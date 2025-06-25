#ifndef MINIGIT_SYSTEM_HPP
#define MINIGIT_SYSTEM_HPP

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip> // For std::put_time
#include <algorithm> // For std::remove_if

namespace fs = std::filesystem;

class MiniGitSystem {
private:
    // --- Data Structures ---
    struct Commit {
        std::string hash;
        std::string message;
        std::string timestamp;
        std::vector<std::string> parentHashes;
        std::unordered_map<std::string, std::string> fileBlobs; // filename -> blob hash
    };

    // Core data:
    std::unordered_map<std::string, Commit> commits;       // hash -> Commit object
    std::unordered_map<std::string, std::string> branches; // branch name -> commit hash
    std::unordered_map<std::string, std::string> stagingArea; // filename -> blob hash

    // Current state:
    std::string headBranch = "master";      // The currently active branch (e.g., "master", "feature-a")
    std::string headCommitHash;             // The hash of the commit HEAD currently points to

    // --- Private Utility Methods ---

    // Generates a formatted timestamp for commits.
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // A simple hashing function for content.
    // In a real VCS, you'd use a cryptographic hash like SHA-1 or SHA-256.
    std::string hashFileContent(const std::string& content) {
        std::hash<std::string> hasher;
        return std::to_string(hasher(content));
    }

    // Reads the entire content of a file into a string.
    std::string readFileContent(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary); // Use binary mode to avoid issues with text conversions
        if (!file.is_open()) {
            // std::cerr << "Error: Could not open file for reading: " << filename << "\n"; // Suppress for status
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Writes content to a 'blob' file in the .minigit/objects directory.
    void saveBlob(const std::string& hash, const std::string& content) {
        fs::path blobPath = ".minigit/objects/" + hash;
        std::ofstream out(blobPath, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "Error: Could not save blob to " << blobPath << "\n";
            return;
        }
        out << content;
        out.close();
    }

    // Reads content from a 'blob' file.
    std::string loadBlob(const std::string& hash) {
        fs::path blobPath = ".minigit/objects/" + hash;
        std::ifstream in(blobPath, std::ios::binary);
        if (!in.is_open()) {
            // std::cerr << "Error: Could not load blob from " << blobPath << "\n"; // Suppress for cases where blob might not exist (e.g. initial diffs)
            return "";
        }
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    // Writes commit metadata to a file in the .minigit/commits directory.
    void writeCommitToFile(const Commit& commit) {
        fs::path commitPath = ".minigit/commits/" + commit.hash;
        std::ofstream file(commitPath);
        if (!file.is_open()) {
            std::cerr << "Error: Could not write commit to " << commitPath << "\n";
            return;
        }
        file << "message:" << commit.message << "\n";
        file << "timestamp:" << commit.timestamp << "\n";
        file << "parents:";
        for (const auto& parent : commit.parentHashes) {
            file << parent << " ";
        }
        file << "\n";
        file << "files:\n";
        for (const auto& [f, b] : commit.fileBlobs) {
            file << f << ":" << b << "\n";
        }
        file.close();
    }

    // Loads a commit from its file representation.
    Commit loadCommitFromFile(const std::string& commitHash) {
        fs::path commitPath = ".minigit/commits/" + commitHash;
        std::ifstream file(commitPath);
        Commit c;
        c.hash = commitHash;

        if (!file.is_open()) {
            // std::cerr << "Error: Could not load commit from " << commitPath << "\n";
            c.hash = ""; // Indicate failure
            return c;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.rfind("message:", 0) == 0) {
                c.message = line.substr(8);
            } else if (line.rfind("timestamp:", 0) == 0) {
                c.timestamp = line.substr(10);
            } else if (line.rfind("parents:", 0) == 0) {
                std::string parentsStr = line.substr(8);
                std::stringstream ss(parentsStr);
                std::string parentHash;
                while (ss >> parentHash) {
                    c.parentHashes.push_back(parentHash);
                }
            } else if (line.rfind("files:", 0) == 0) {
                while (std::getline(file, line) && !line.empty()) {
                    size_t colonPos = line.find(':');
                    if (colonPos != std::string::npos) {
                        std::string filename = line.substr(0, colonPos);
                        std::string blobHash = line.substr(colonPos + 1);
                        c.fileBlobs[filename] = blobHash;
                    }
                }
            }
        }
        file.close();
        return c;
    }

    // Persist current branch state to a file and update HEAD.
    void saveHeadAndBranchRefs() {
        // Save current branch's commit hash
        if (!headBranch.empty() && !headCommitHash.empty()) { // Only save if on a branch and points to a commit
            fs::create_directories(".minigit/refs/heads");
            std::ofstream branchRefFile(".minigit/refs/heads/" + headBranch);
            if (!branchRefFile.is_open()) {
                std::cerr << "Error: Could not save branch ref for " << headBranch << "\n";
                return;
            }
            branchRefFile << headCommitHash << "\n";
            branchRefFile.close();
        }

        // Update .minigit/HEAD
        std::ofstream headFile(".minigit/HEAD");
        if (!headFile.is_open()) {
            std::cerr << "Error: Could not save HEAD file.\n";
            return;
        }
        if (!headBranch.empty()) { // On a branch
            headFile << "ref: refs/heads/" << headBranch << "\n";
        } else { // Detached HEAD
            headFile << headCommitHash << "\n";
        }
        headFile.close();
    }

    // Load branch state from HEAD file and all branch refs.
    void loadRepoState() {
        // Load HEAD
        std::ifstream headFile(".minigit/HEAD");
        if (headFile.is_open()) {
            std::string line;
            std::getline(headFile, line);
            if (line.rfind("ref: refs/heads/", 0) == 0) {
                headBranch = line.substr(16);
                // Load the commit pointed to by the branch
                std::ifstream branchRef(".minigit/refs/heads/" + headBranch);
                if (branchRef.is_open()) {
                    std::getline(branchRef, headCommitHash);
                    branchRef.close();
                } else {
                    headCommitHash = ""; // Branch exists but points to nothing (e.g., brand new repo before first commit)
                }
            } else {
                // Detached HEAD state
                headBranch = ""; // Indicate detached HEAD
                headCommitHash = line;
            }
            headFile.close();
        } else {
            // No HEAD file (first init case)
            headBranch = "master";
            headCommitHash = "";
        }

        // Load all commits
        if (fs::exists(".minigit/commits")) {
            for (const auto& entry : fs::directory_iterator(".minigit/commits")) {
                if (entry.is_regular_file()) {
                    std::string commitHash = entry.path().filename().string();
                    commits[commitHash] = loadCommitFromFile(commitHash);
                }
            }
        }

        // Load all branch refs
        fs::path branchesPath = ".minigit/refs/heads";
        if (fs::exists(branchesPath)) {
            for (const auto& entry : fs::directory_iterator(branchesPath)) {
                if (entry.is_regular_file()) {
                    std::string branchName = entry.path().filename().string();
                    std::ifstream branchFile(entry.path());
                    std::string commitHash;
                    std::getline(branchFile, commitHash);
                    branches[branchName] = commitHash;
                    branchFile.close();
                }
            }
        }
        // Clear staging area on load, simulating .git/index not being loaded
        stagingArea.clear();
    }


    // Populate the working directory with files from a given commit snapshot.
    // Handles creating, updating, and deleting files.
    void populateWorkingDirectory(const Commit& commit) {
        // Get list of all files in current working directory (excluding .minigit)
        std::unordered_set<std::string> wdFiles;
        for (const auto& entry : fs::directory_iterator(".")) {
            std::string filename = entry.path().filename().string(); // Corrected: Get filename as string
            if (entry.is_regular_file() && filename != ".minigit" && filename[0] != '.') { // Corrected: Access first char of string
                wdFiles.insert(filename);
            }
        }

        // 1. Create/Update files from the target commit
        for (const auto& [filename, blobHash] : commit.fileBlobs) {
            std::string content = loadBlob(blobHash);
            if (content.empty() && !fs::exists(".minigit/objects/" + blobHash)) {
                 std::cerr << "Warning: Blob for " << filename << " (" << blobHash.substr(0,7) << ") not found. Skipping.\n";
                 continue;
            }

            std::ofstream outFile(filename, std::ios::binary);
            if (outFile.is_open()) {
                outFile << content;
                outFile.close();
            } else {
                std::cerr << "Warning: Could not write file " << filename << ". Skipping.\n";
            }
            wdFiles.erase(filename); // Mark as handled
        }

        // 2. Delete files that are in the working directory but NOT in the target commit
        for (const std::string& wdFilename : wdFiles) {
            // Skip .minigit folder itself
            if (wdFilename == ".minigit" || wdFilename[0] == '.') continue; // Also skip other hidden files potentially

            if (commit.fileBlobs.find(wdFilename) == commit.fileBlobs.end()) {
                try {
                    fs::remove(wdFilename);
                    std::cout << "Removed: " << wdFilename << "\n";
                } catch (const fs::filesystem_error& e) {
                    std::cerr << "Error removing file " << wdFilename << ": " << e.what() << "\n";
                }
            }
        }
        std::cout << "Working directory updated to commit " << commit.hash.substr(0, 7) << ".\n";
    }

    // Compares working directory files with the current HEAD commit or staging area.
    // Returns a tuple of (modified, deleted, untracked) files.
    // This is a helper for status().
    struct WorkingDirChanges {
        std::vector<std::string> modified;
        std::vector<std::string> deleted;
        std::vector<std::string> untracked;
    };

    WorkingDirChanges getUnstagedChanges(const Commit* headCommit) {
        WorkingDirChanges changes;
        std::unordered_map<std::string, std::string> commitFiles;
        if (headCommit) {
            commitFiles = headCommit->fileBlobs;
        }

        // Files in working directory
        std::unordered_set<std::string> wdFiles;
        for (const auto& entry : fs::directory_iterator(".")) {
            std::string filename = entry.path().filename().string(); // Corrected: Get filename as string
            // Exclude .minigit directory and other hidden/system files if necessary
            if (entry.is_regular_file() && filename != ".minigit" && filename[0] != '.') { // Corrected: Access first char of string
                wdFiles.insert(filename);
                std::string currentContent = readFileContent(filename);
                std::string currentHash = hashFileContent(currentContent);

                if (stagingArea.count(filename)) { // File is in staging
                    // Check if WD content differs from staged content
                    if (stagingArea.at(filename) != currentHash) {
                        changes.modified.push_back(filename + " (not staged - staged version differs from WD)");
                    }
                    // If content matches staged, it's not an unstaged modification from staged.
                    // But if staged differs from commit, it would be a staged change.
                } else if (commitFiles.count(filename)) { // File is tracked by current commit
                    // Check if WD content differs from committed content
                    if (commitFiles.at(filename) != currentHash) {
                        changes.modified.push_back(filename);
                    }
                } else { // Not in staging and not in commit -> Untracked
                    changes.untracked.push_back(filename);
                }
            }
        }

        // Files deleted from working directory (but present in commit or staging)
        // Check for files that were in the last commit (tracked) but are now missing from WD
        for (const auto& [filename, blobHash] : commitFiles) {
            if (!wdFiles.count(filename) && !stagingArea.count(filename)) { // Not in WD and not in staging
                changes.deleted.push_back(filename);
            }
            // If it's in staging but not in WD, it's a staged deletion (handled by getStagedChanges)
            // If it's in WD but not in staging (and was in commit), it's handled above as a modified/unchanged.
        }

        return changes;
    }

    // Checks if there are changes staged for commit.
    // Returns a tuple of (added, modified, deleted) staged files.
    struct StagedChanges {
        std::vector<std::string> added;
        std::vector<std::string> modified;
        std::vector<std::string> deleted; // Files removed from tracking and then added to staging (via git rm)
    };

    StagedChanges getStagedChanges(const Commit* headCommit) {
        StagedChanges changes;
        std::unordered_map<std::string, std::string> commitFiles;
        if (headCommit) {
            commitFiles = headCommit->fileBlobs;
        }

        // Check staged files (added/modified)
        for (const auto& [filename, stagedBlobHash] : stagingArea) {
            if (commitFiles.count(filename)) {
                // File exists in both staged and committed, check if content differs
                if (commitFiles.at(filename) != stagedBlobHash) {
                    changes.modified.push_back(filename);
                }
            } else {
                // New file added to staging
                changes.added.push_back(filename);
            }
        }

        // Check for files that were in the current commit but are now missing from staging
        // This implies they were "removed" (like `git rm` but we don't have rm, so implicitly by user deletion + add)
        for (const auto& [filename, blobHash] : commitFiles) {
            if (!stagingArea.count(filename)) { // If file was in commit, but not in staging
                // This means it's either deleted from staging, or it was deleted from WD and never added to staging.
                // For simplicity, we assume if it's missing from staging, it's considered deleted if it's also missing from WD.
                // A true `git rm` would stage the deletion.
                if (!fs::exists(filename)) { // If it's also not in the working directory
                     changes.deleted.push_back(filename);
                }
            }
        }
        return changes;
    }

    // Helper function to compare two strings line by line and print differences
    // This is a very basic diff, not a sophisticated algorithm like Myers'
    void displayLineDiff(const std::string& oldContent, const std::string& newContent, const std::string& filename) {
        std::cout << "--- Diff for: " << filename << " ---\n";
        std::stringstream oldSs(oldContent);
        std::stringstream newSs(newContent);
        std::string oldLine, newLine;

        std::vector<std::string> oldLines, newLines;
        while (std::getline(oldSs, oldLine)) {
            oldLines.push_back(oldLine);
        }
        while (std::getline(newSs, newLine)) {
            newLines.push_back(newLine);
        }

        size_t oldIdx = 0;
        size_t newIdx = 0;

        // This is a very simple diff, primarily showing additions/deletions for lines that
        // don't have a direct 1:1 match. It won't handle moved blocks well.
        // For a more robust diff, a true longest common subsequence (LCS) algorithm is needed.
        while (oldIdx < oldLines.size() || newIdx < newLines.size()) {
            bool matched = false;
            if (oldIdx < oldLines.size() && newIdx < newLines.size()) {
                if (oldLines[oldIdx] == newLines[newIdx]) {
                    std::cout << "  " << oldLines[oldIdx] << "\n";
                    oldIdx++;
                    newIdx++;
                    matched = true;
                }
            }

            if (!matched) {
                // Try to find if current oldLine exists later in newLines
                // or if current newLine exists later in oldLines
                size_t tempOldIdx = oldIdx;
                size_t tempNewIdx = newIdx;
                bool oldFoundInNew = false;
                bool newFoundInOld = false;

                if (oldIdx < oldLines.size()) {
                    for (size_t i = newIdx; i < newLines.size(); ++i) {
                        if (oldLines[oldIdx] == newLines[i]) {
                            oldFoundInNew = true;
                            break;
                        }
                    }
                }
                if (newIdx < newLines.size()) {
                    for (size_t i = oldIdx; i < oldLines.size(); ++i) {
                        if (newLines[newIdx] == oldLines[i]) {
                            newFoundInOld = true;
                            break;
                        }
                    }
                }

                if (oldIdx < oldLines.size() && !oldFoundInNew) {
                    std::cout << "- " << oldLines[oldIdx] << "\n";
                    oldIdx++;
                } else if (newIdx < newLines.size() && !newFoundInOld) {
                    std::cout << "+ " << newLines[newIdx] << "\n";
                    newIdx++;
                } else { // Fallback for when both might be different or complex sequence
                    if (oldIdx < oldLines.size() && !newFoundInOld) { // If old line is not found in remaining new, it's a deletion
                         std::cout << "- " << oldLines[oldIdx] << "\n";
                         oldIdx++;
                    }
                    if (newIdx < newLines.size() && !oldFoundInNew) { // If new line is not found in remaining old, it's an addition
                         std::cout << "+ " << newLines[newIdx] << "\n";
                         newIdx++;
                    }
                     // If both are still remaining and seem to be a change, prioritize one or the other.
                     // This simple diff prioritizes printing deletions then additions if ambiguous.
                }
            }
        }
        std::cout << "---------------------------\n";
    }


public:
    // --- Public API ---

    // Constructor: Attempts to load existing repository state.
    MiniGitSystem() {
        if (fs::exists(".minigit")) {
            std::cout << "Loading existing MiniGit repository...\n";
            loadRepoState(); // Consolidate loading logic
            std::cout << "MiniGit repository loaded.\n";
        } else {
            std::cout << "No existing MiniGit repository found. Call 'init' to create one.\n";
            // Set default initial state for a new repo that hasn't been init'd
            headBranch = "master";
            headCommitHash = "";
            stagingArea.clear();
            commits.clear();
            branches.clear();
        }
    }

    // Initializes a new MiniGit repository.
    void init() {
        if (fs::exists(".minigit")) {
            std::cout << "MiniGit repository already initialized in .minigit\n";
            return;
        }
        try {
            fs::create_directory(".minigit");
            fs::create_directory(".minigit/objects");
            fs::create_directory(".minigit/commits");
            fs::create_directories(".minigit/refs/heads"); // For branch pointers

            headBranch = "master";
            headCommitHash = ""; // No commits yet
            branches["master"] = ""; // Master points to no commit initially
            saveHeadAndBranchRefs(); // Create the HEAD file and master ref
            std::cout << "Initialized empty MiniGit repository in .minigit\n";
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error initializing MiniGit repository: " << e.what() << "\n";
        }
    }

    // Adds a file's current content to the staging area.
    void add(const std::string& filename) {
        if (!fs::exists(".minigit")) {
            std::cout << "Not a MiniGit repository. Please run 'init' first.\n";
            return;
        }
        if (!fs::exists(filename)) {
            std::cout << "Error: File does not exist: " << filename << "\n";
            return;
        }
        if (!fs::is_regular_file(filename)) {
            std::cout << "Error: Not a regular file: " << filename << "\n";
            return;
        }

        std::string content = readFileContent(filename);
        if (content.empty() && !fs::is_empty(filename)) { // Check for read failure on non-empty files
            std::cerr << "Warning: Could not read content of file: " << filename << ". Not added.\n";
            return;
        }
        std::string hash = hashFileContent(content);

        // Optimization: Don't re-add if content hasn't changed and is already staged
        if (stagingArea.count(filename) && stagingArea[filename] == hash) {
            std::cout << "File already up to date in staging: " << filename << "\n";
            return;
        }

        stagingArea[filename] = hash;
        saveBlob(hash, content);
        std::cout << "Added file to staging: " << filename << " (" << hash.substr(0, 7) << ")\n";
    }

    // Commits staged changes with a given message.
    void commit(const std::string& message) {
        if (!fs::exists(".minigit")) {
            std::cout << "Not a MiniGit repository. Please run 'init' first.\n";
            return;
        }

        const Commit* currentHeadCommit = nullptr;
        if (!headCommitHash.empty()) {
            // Ensure the head commit is loaded if it wasn't already (e.g. after init/clone)
            if (commits.find(headCommitHash) == commits.end()) {
                commits[headCommitHash] = loadCommitFromFile(headCommitHash);
            }
            currentHeadCommit = &commits[headCommitHash];
        }

        // Get staged changes to check if there's anything to commit
        StagedChanges staged = getStagedChanges(currentHeadCommit);

        // Check if there are any staged changes at all
        if (staged.added.empty() && staged.modified.empty() && staged.deleted.empty()) {
            std::cout << "No changes to commit. Staging area is empty or identical to HEAD.\n";
            stagingArea.clear(); // Ensure staging is clear if no effective changes
            return;
        }


        Commit newCommit;
        newCommit.timestamp = getCurrentTime();
        newCommit.message = message;

        if (!headCommitHash.empty()) {
            newCommit.parentHashes.push_back(headCommitHash);
            // Start the new commit's file snapshot by copying from the parent
            newCommit.fileBlobs = currentHeadCommit->fileBlobs;
        }

        // Apply staged changes to the new commit's file snapshot
        for (const auto& [filename, blob] : stagingArea) {
            // Add or update file in the new commit's snapshot
            newCommit.fileBlobs[filename] = blob;
        }

        // Handle explicitly 'removed' files from the commit snapshot
        // Files marked as 'deleted' in staged changes should be removed from the new commit's snapshot
        for(const auto& filename : staged.deleted) {
            newCommit.fileBlobs.erase(filename);
        }

        // Generate commit hash based on its content (message, timestamp, parent, and file tree hashes)
        std::string commitContentToHash = newCommit.message + newCommit.timestamp;
        for (const auto& parent : newCommit.parentHashes) commitContentToHash += parent;
        for (const auto& [f, b] : newCommit.fileBlobs) commitContentToHash += f + b;

        newCommit.hash = hashFileContent(commitContentToHash);

        commits[newCommit.hash] = newCommit;
        headCommitHash = newCommit.hash;

        if (!headBranch.empty()) { // Update current branch if not in detached HEAD
            branches[headBranch] = newCommit.hash;
        }
        saveHeadAndBranchRefs(); // Update branch ref file and HEAD file

        writeCommitToFile(newCommit);
        stagingArea.clear(); // Clear staging area after successful commit
        std::cout << "Committed as " << newCommit.hash.substr(0, 7) << "\n";
    }

    // Displays the commit history.
    void log() {
        if (!fs::exists(".minigit")) {
            std::cout << "Not a MiniGit repository. Please run 'init' first.\n";
            return;
        }
        if (headCommitHash.empty()) {
            std::cout << "No commits yet.\n";
            return;
        }
        std::cout << "--- Commit History ---\n";
        std::string current = headCommitHash;
        std::unordered_set<std::string> visitedCommits; // To prevent infinite loops in complex graphs

        while (!current.empty() && visitedCommits.find(current) == visitedCommits.end()) {
            if (commits.find(current) == commits.end()) {
                // If a commit is referenced but not loaded, try to load it
                commits[current] = loadCommitFromFile(current);
                if (commits.find(current) == commits.end() || commits[current].hash.empty()) {
                    std::cerr << "Error: Corrupt commit reference " << current << ". Stopping log.\n";
                    break;
                }
            }
            Commit& c = commits[current];
            std::cout << "Commit: " << c.hash.substr(0, 7);
            if (!headBranch.empty() && branches[headBranch] == current) {
                std::cout << " (HEAD -> " << headBranch << ")";
            } else if (headBranch.empty() && headCommitHash == current) {
                std::cout << " (HEAD, detached)";
            }
            // Also show other branches pointing to this commit
            for (const auto& [branchName, commitHash] : branches) {
                if (branchName != headBranch && commitHash == current) {
                    std::cout << ", " << branchName;
                }
            }
            std::cout << "\n";

            if (!c.parentHashes.empty()) {
                std::cout << "Parents: ";
                for (const auto& p : c.parentHashes) {
                    std::cout << p.substr(0, 7) << " ";
                }
                std::cout << "\n";
            }
            std::cout << "Date:    " << c.timestamp << "\n";
            std::cout << "Message: " << c.message << "\n\n";
            visitedCommits.insert(current);
            current = c.parentHashes.empty() ? "" : c.parentHashes[0]; // Follow first parent for simplicity
        }
        std::cout << "----------------------\n";
    }

    // Creates a new branch pointing to the current HEAD commit.
    void branch(const std::string& name) {
        if (!fs::exists(".minigit")) {
            std::cout << "Not a MiniGit repository. Please run 'init' first.\n";
            return;
        }
        if (headCommitHash.empty()) {
            std::cout << "Cannot create branch: No commits yet.\n";
            return;
        }
        if (branches.count(name)) {
            std::cout << "Error: Branch '" << name << "' already exists.\n";
            return;
        }
        branches[name] = headCommitHash;
        saveHeadAndBranchRefs(); // Persist the new branch reference
        std::cout << "Created branch: " << name << " pointing to " << headCommitHash.substr(0, 7) << "\n";
    }

    // Switches between branches or checks out a specific commit.
    void checkout(const std::string& target) {
        if (!fs::exists(".minigit")) {
            std::cout << "Not a MiniGit repository. Please run 'init' first.\n";
            return;
        }

        // Check for uncommitted changes (both staged and unstaged)
        const Commit* currentHeadCommit = nullptr;
        if (!headCommitHash.empty()) {
            if (commits.find(headCommitHash) == commits.end()) {
                commits[headCommitHash] = loadCommitFromFile(headCommitHash);
            }
            currentHeadCommit = &commits[headCommitHash];
        }
        StagedChanges staged = getStagedChanges(currentHeadCommit);
        WorkingDirChanges unstaged = getUnstagedChanges(currentHeadCommit);

        if (!staged.added.empty() || !staged.modified.empty() || !staged.deleted.empty() ||
            !unstaged.modified.empty() || !unstaged.deleted.empty() || !unstaged.untracked.empty()) {
            std::cout << "Error: Your working directory has uncommitted changes. Please commit or stash them before checking out.\n";
            status(); // Show the user what changes are pending
            return;
        }

        std::string targetCommitHash = "";
        std::string newHeadBranch = "";

        // Try to checkout a branch
        if (branches.count(target)) {
            targetCommitHash = branches[target];
            newHeadBranch = target;
        } else { // Try to checkout a commit (by its full hash or first 7 chars)
            std::string resolvedCommitHash = "";
            if (commits.count(target)) { // Exact match
                resolvedCommitHash = target;
            } else { // Try partial match (first 7 chars)
                for (const auto& pair : commits) {
                    if (pair.first.rfind(target, 0) == 0 && target.length() >= 4) { // Use at least 4 chars for short hash
                        resolvedCommitHash = pair.first;
                        break;
                    }
                }
            }
            targetCommitHash = resolvedCommitHash;
            // newHeadBranch remains empty for detached HEAD
        }

        if (targetCommitHash.empty() && !newHeadBranch.empty()) {
            // Special case: checking out an empty branch (e.g., master after init, before first commit)
            if (headBranch == newHeadBranch) {
                std::cout << "Already on branch '" << newHeadBranch << "'.\n";
                return;
            }
            std::cout << "Switched to branch: " << newHeadBranch << " (empty branch, no files restored).\n";
            // Clear working directory as there's no snapshot to restore.
             for (const auto& entry : fs::directory_iterator(".")) {
                std::string filename = entry.path().filename().string(); // Corrected: Get filename as string
                if (entry.is_regular_file() && filename != ".minigit" && filename[0] != '.') { // Corrected: Access first char of string
                    try {
                        fs::remove(entry.path());
                    } catch (const fs::filesystem_error& e) {
                        std::cerr << "Warning: Could not remove file " << filename << " during empty branch checkout: " << e.what() << "\n";
                    }
                }
            }
            headBranch = newHeadBranch;
            headCommitHash = targetCommitHash; // Will be empty
            saveHeadAndBranchRefs();
            stagingArea.clear();
            return;
        }

        if (targetCommitHash.empty()) {
            std::cout << "Error: Branch or commit not found: " << target << "\n";
            return;
        }

        // Ensure the target commit is loaded
        if (commits.find(targetCommitHash) == commits.end()) {
            commits[targetCommitHash] = loadCommitFromFile(targetCommitHash);
            if (commits[targetCommitHash].hash.empty()) {
                std::cerr << "Error: Target " << target << " points to a corrupt commit. Cannot checkout.\n";
                return;
            }
        }

        // Check if we are already at the target
        if (headCommitHash == targetCommitHash && ((newHeadBranch.empty() && headBranch.empty()) || (newHeadBranch == headBranch))) {
            std::cout << "Already on " << (newHeadBranch.empty() ? "commit " + targetCommitHash.substr(0,7) + " (detached HEAD)" : "branch '" + newHeadBranch + "'") << ".\n";
            return;
        }


        headBranch = newHeadBranch;
        headCommitHash = targetCommitHash;
        saveHeadAndBranchRefs(); // Update HEAD file and branch ref (if applicable)
        populateWorkingDirectory(commits[headCommitHash]);
        if (newHeadBranch.empty()) {
            std::cout << "Checked out commit: " << headCommitHash.substr(0, 7) << " (detached HEAD)\n";
        } else {
            std::cout << "Switched to branch: " << newHeadBranch << "\n";
        }
        stagingArea.clear(); // Clear staging area on checkout
    }

    // Displays the current status of the repository (staged, unstaged, untracked files).
    void status() {
        if (!fs::exists(".minigit")) {
            std::cout << "Not a MiniGit repository. Please run 'init' first.\n";
            return;
        }
        std::cout << "--- MiniGit Status ---\n";
        std::cout << "On branch " << (headBranch.empty() ? "(detached HEAD)" : headBranch) << "\n";
        std::cout << "HEAD points to: " << (headCommitHash.empty() ? "No commits yet" : headCommitHash.substr(0, 7)) << "\n\n";

        const Commit* currentHeadCommit = nullptr;
        if (!headCommitHash.empty()) {
            if (commits.find(headCommitHash) == commits.end()) {
                // If commit not in memory, try loading it (should ideally be loaded by loadRepoState)
                commits[headCommitHash] = loadCommitFromFile(headCommitHash);
                if (commits[headCommitHash].hash.empty()) {
                    std::cerr << "Warning: HEAD commit " << headCommitHash.substr(0,7) << " not found/corrupt during status check.\n";
                    currentHeadCommit = nullptr;
                } else {
                    currentHeadCommit = &commits[headCommitHash];
                }
            } else {
                currentHeadCommit = &commits[headCommitHash];
            }
        }

        StagedChanges staged = getStagedChanges(currentHeadCommit);
        if (!staged.added.empty() || !staged.modified.empty() || !staged.deleted.empty()) {
            std::cout << "Changes to be committed:\n";
            for (const auto& file : staged.added) {
                std::cout << "    New file:   " << file << "\n";
            }
            for (const auto& file : staged.modified) {
                std::cout << "    Modified:   " << file << "\n";
            }
             for (const auto& file : staged.deleted) {
                std::cout << "    Deleted:    " << file << "\n";
            }
            std::cout << "\n";
        } else {
            std::cout << "No changes to be committed.\n\n";
        }

        WorkingDirChanges unstaged = getUnstagedChanges(currentHeadCommit);
        bool hasUnstagedChanges = !unstaged.modified.empty() || !unstaged.deleted.empty();

        if (hasUnstagedChanges) {
            std::cout << "Changes not staged for commit:\n";
            for (const auto& file : unstaged.modified) {
                std::cout << "    Modified:   " << file << "\n";
            }
            for (const auto& file : unstaged.deleted) {
                std::cout << "    Deleted:    " << file << "\n";
            }
            std::cout << "\n";
        } else {
             std::cout << "No changes not staged for commit.\n\n";
        }

        if (!unstaged.untracked.empty()) {
            std::cout << "Untracked files:\n";
            std::cout << "  (use \"minigit add <file>...\" to include in what will be committed)\n";
            for (const auto& file : unstaged.untracked) {
                std::cout << "    " << file << "\n";
            }
            std::cout << "\n";
        } else {
            std::cout << "No untracked files.\n\n";
        }

        if (staged.added.empty() && staged.modified.empty() && staged.deleted.empty() &&
            unstaged.modified.empty() && unstaged.deleted.empty() && unstaged.untracked.empty()) {
            std::cout << "Your working directory is clean.\n";
        }
        std::cout << "----------------------\n";
    }

    // Displays differences between various states (WD, staging, commits).
    void diff(const std::string& arg1 = "", const std::string& arg2 = "") {
        if (!fs::exists(".minigit")) {
            std::cout << "Not a MiniGit repository. Please run 'init' first.\n";
            return;
        }

        // Helper to resolve short hashes to full hashes
        auto resolveHash = [&](std::string hash) {
            if (hash.length() == 40) return hash; // Already full hash
            for (const auto& pair : commits) {
                if (pair.first.rfind(hash, 0) == 0 && hash.length() >= 4) { // Match prefix
                    return pair.first;
                }
            }
            return std::string(""); // Not found
        };

        // Scenario 1: diff working directory vs staging area (like 'git diff' without arguments)
        if (arg1.empty() && arg2.empty()) {
            std::cout << "Diff: Working Directory vs Staging Area (unstaged changes)\n";
            std::unordered_map<std::string, std::string> compareToFiles = stagingArea; // Compare WD to staged

            bool foundDiff = false;
            // Iterate through files in working directory
            for (const auto& entry : fs::directory_iterator(".")) {
                std::string filename = entry.path().filename().string(); // Corrected: Declare filename here
                if (entry.is_regular_file() && filename != ".minigit" && filename[0] != '.') {
                    std::string wdContent = readFileContent(filename);
                    std::string wdHash = hashFileContent(wdContent);

                    if (compareToFiles.count(filename)) { // File exists in staging
                        std::string stagedBlobHash = compareToFiles.at(filename);
                        std::string stagedContent = loadBlob(stagedBlobHash);
                        if (wdContent != stagedContent) {
                            displayLineDiff(stagedContent, wdContent, filename);
                            foundDiff = true;
                        }
                    } else { // File in WD, but not in staging (untracked, or deleted from staging)
                        // If it's a new untracked file, it won't be in staging.
                        // We only show diffs for files that are 'known' to git in some way (tracked/staged)
                        // This behavior mimics `git diff` which usually ignores untracked files by default.
                    }
                }
            }
            // Check for files that were in staging but are no longer in WD (deleted)
            for (const auto& pair : compareToFiles) {
                std::string filename = pair.first;
                if (!fs::exists(filename)) {
                    displayLineDiff(loadBlob(pair.second), "", filename + " (deleted from WD)");
                    foundDiff = true;
                }
            }


            if (!foundDiff) {
                std::cout << "No differences in working directory compared to staged area.\n";
            }
        }
        // Scenario 2: diff staging area vs HEAD commit (like 'git diff --staged' or 'git diff --cached')
        else if (arg1 == "--staged" || arg1 == "--cached") {
            std::cout << "Diff: Staging Area vs HEAD commit (staged changes)\n";
            if (headCommitHash.empty()) {
                std::cout << "No HEAD commit to compare against. Use `commit` first.\n";
                return;
            }
            if (commits.find(headCommitHash) == commits.end()) {
                commits[headCommitHash] = loadCommitFromFile(headCommitHash);
            }
            const Commit& headCommit = commits[headCommitHash];

            std::unordered_set<std::string> allFiles;
            for (const auto& pair : stagingArea) allFiles.insert(pair.first);
            for (const auto& pair : headCommit.fileBlobs) allFiles.insert(pair.first);

            bool foundDiff = false;
            for (const std::string& filename : allFiles) {
                bool inStaging = stagingArea.count(filename);
                bool inHead = headCommit.fileBlobs.count(filename);

                if (inStaging && inHead) {
                    std::string stagedContent = loadBlob(stagingArea.at(filename));
                    std::string headContent = loadBlob(headCommit.fileBlobs.at(filename));
                    if (stagedContent != headContent) {
                        displayLineDiff(headContent, stagedContent, filename);
                        foundDiff = true;
                    }
                } else if (inHead && !inStaging) { // Deleted from staging
                    displayLineDiff(loadBlob(headCommit.fileBlobs.at(filename)), "", filename + " (deleted from staged)");
                    foundDiff = true;
                } else if (!inHead && inStaging) { // Added to staging
                    displayLineDiff("", loadBlob(stagingArea.at(filename)), filename + " (new file staged)");
                    foundDiff = true;
                }
            }
            if (!foundDiff) {
                std::cout << "No staged changes to show.\n";
            }
        }
        // Scenario 3: diff two commits (e.g., 'minigit diff <commit1> <commit2>')
        else if (!arg1.empty() && !arg2.empty()) {
            std::string commit1Hash = resolveHash(arg1);
            std::string commit2Hash = resolveHash(arg2);

            if (commit1Hash.empty() || commits.find(commit1Hash) == commits.end() || commits[commit1Hash].hash.empty()) {
                std::cerr << "Error: Commit " << arg1 << " not found or corrupt.\n";
                return;
            }
            if (commit2Hash.empty() || commits.find(commit2Hash) == commits.end() || commits[commit2Hash].hash.empty()) {
                std::cerr << "Error: Commit " << arg2 << " not found or corrupt.\n";
                return;
            }

            const Commit& c1 = commits[commit1Hash];
            const Commit& c2 = commits[commit2Hash];

            std::cout << "Diff between " << c1.hash.substr(0, 7) << " and " << c2.hash.substr(0, 7) << "\n";

            std::unordered_set<std::string> allFiles;
            for (const auto& pair : c1.fileBlobs) allFiles.insert(pair.first);
            for (const auto& pair : c2.fileBlobs) allFiles.insert(pair.first);

            bool foundDiff = false;
            for (const std::string& filename : allFiles) {
                bool inC1 = c1.fileBlobs.count(filename);
                bool inC2 = c2.fileBlobs.count(filename);

                if (inC1 && inC2) {
                    if (c1.fileBlobs.at(filename) != c2.fileBlobs.at(filename)) {
                        // File modified
                        displayLineDiff(loadBlob(c1.fileBlobs.at(filename)), loadBlob(c2.fileBlobs.at(filename)), filename);
                        foundDiff = true;
                    }
                } else if (inC1 && !inC2) {
                    // File deleted in c2
                    displayLineDiff(loadBlob(c1.fileBlobs.at(filename)), "", filename + " (deleted)");
                    foundDiff = true;
                } else if (!inC1 && inC2) {
                    // File added in c2
                    displayLineDiff("", loadBlob(c2.fileBlobs.at(filename)), filename + " (new file)");
                    foundDiff = true;
                }
            }
            if (!foundDiff) {
                std::cout << "No differences between commits.\n";
            }
        }
        // Scenario 4: diff working directory vs a specific commit (e.g., 'minigit diff <commit>')
        else if (!arg1.empty() && arg2.empty()) {
            std::string targetCommitHash = resolveHash(arg1);
            if (targetCommitHash.empty() || commits.find(targetCommitHash) == commits.end() || commits[targetCommitHash].hash.empty()) {
                std::cerr << "Error: Commit " << arg1 << " not found or corrupt.\n";
                return;
            }
            const Commit& targetCommit = commits[targetCommitHash];

            std::cout << "Diff: Working Directory vs Commit " << targetCommit.hash.substr(0, 7) << "\n";

            std::unordered_set<std::string> allFiles;
            for (const auto& entry : fs::directory_iterator(".")) {
                std::string filename = entry.path().filename().string(); // Corrected: Declare filename here
                if (entry.is_regular_file() && filename != ".minigit" && filename[0] != '.') {
                    allFiles.insert(filename);
                }
            }
            for (const auto& pair : targetCommit.fileBlobs) {
                allFiles.insert(pair.first);
            }

            bool foundDiff = false;
            for (const std::string& filename : allFiles) {
                bool inWD = fs::exists(filename) && fs::is_regular_file(filename);
                bool inCommit = targetCommit.fileBlobs.count(filename);

                if (inWD && inCommit) {
                    std::string wdContent = readFileContent(filename);
                    std::string commitContent = loadBlob(targetCommit.fileBlobs.at(filename));
                    if (wdContent != commitContent) {
                        displayLineDiff(commitContent, wdContent, filename);
                        foundDiff = true;
                    }
                } else if (inCommit && !inWD) {
                    // File deleted in WD
                    displayLineDiff(loadBlob(targetCommit.fileBlobs.at(filename)), "", filename + " (deleted in WD)");
                    foundDiff = true;
                } else if (inWD && !inCommit) {
                    // File added in WD (untracked from commit's perspective)
                    displayLineDiff("", readFileContent(filename), filename + " (new in WD)");
                    foundDiff = true;
                }
            }
            if (!foundDiff) {
                std::cout << "No differences in working directory compared to commit " << targetCommit.hash.substr(0, 7) << ".\n";
            }
        }
        else {
            std::cout << "Usage:\n";
            std::cout << "  minigit diff                          # Show diff between working directory and staging\n";
            std::cout << "  minigit diff --staged (or --cached) # Show diff between staging and HEAD commit\n";
            std::cout << "  minigit diff <commit>                 # Show diff between working directory and a commit\n";
            std::cout << "  minigit diff <commit1> <commit2>      # Show diff between two commits\n";
        }
    }
};

#endif // MINIGIT_SYSTEM_HPP