#include "MiniGitSystem.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <filesystem> // Required for fs::current_path() if you want to use it for debugging

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    // MiniGitSystem operates on the current directory, so no path argument is needed for the constructor.
    MiniGitSystem git;

    if (argc < 2) {
        std::cout << "Usage: minigit <command> [args...]\n";
        std::cout << "Commands:\n";
        std::cout << "  init                      - Initialize a new MiniGit repository.\n";
        std::cout << "  add <file>                - Add file content to the staging area.\n";
        std::cout << "  commit <message>          - Record changes to the repository.\n";
        std::cout << "  log                       - Show commit history.\n";
        std::cout << "  branch <name>             - Create a new branch.\n";
        std::cout << "  checkout <target>         - Switch branches or restore working tree files.\n";
        std::cout << "  status                    - Show the working tree status.\n";
        std::cout << "  diff [arg1] [arg2]        - Show changes between commits, staging, or working tree.\n";
        return 1;
    }

    std::string command = argv[1];

    if (command == "init") {
        git.init();
    } else if (command == "add") {
        if (argc < 3) {
            std::cout << "Usage: minigit add <filename>\n";
            return 1;
        }
        git.add(argv[2]);
    } else if (command == "commit") {
        if (argc < 3) {
            std::cout << "Usage: minigit commit \"<message>\"\n"; // Emphasize quotes for multi-word messages
            return 1;
        }
        std::string message = argv[2];
        // Combine all arguments into a single message string for messages with spaces
        for (int i = 3; i < argc; ++i) {
            message += " " + std::string(argv[i]);
        }
        git.commit(message);
    } else if (command == "log") {
        git.log();
    } else if (command == "branch") {
        if (argc < 3) {
            std::cout << "Usage: minigit branch <name>\n";
            return 1;
        }
        git.branch(argv[2]);
    } else if (command == "checkout") {
        if (argc < 3) {
            std::cout << "Usage: minigit checkout <branch_name_or_commit_hash>\n";
            return 1;
        }
        git.checkout(argv[2]);
    } else if (command == "status") {
        git.status();
    } else if (command == "diff") {
        if (argc == 2) { // minigit diff (WD vs staging)
            git.diff();
        } else if (argc == 3) { // minigit diff <commit> (WD vs commit) OR minigit diff --staged
            git.diff(argv[2]);
        } else if (argc == 4) { // minigit diff <commit1> <commit2> (commit vs commit)
            git.diff(argv[2], argv[3]);
        } else {
            std::cout << "Usage:\n";
            std::cout << "  minigit diff                          # Show diff between working directory and staging\n";
            std::cout << "  minigit diff --staged (or --cached) # Show diff between staging and HEAD commit\n";
            std::cout << "  minigit diff <commit>                 # Show diff between working directory and a commit\n";
            std::cout << "  minigit diff <commit1> <commit2>      # Show diff between two commits\n";
            return 1;
        }
    } else {
        std::cout << "Unknown command: " << command << "\n";
        std::cout << "Commands: init, add <file>, commit <message>, log, branch <name>, checkout <target>, status, diff\n";
        return 1;
    }

    return 0;
}