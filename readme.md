# MiniGit
---
## 👨‍💻 Team Members – Developers & Report Authors

| Name               | ID           |            GitHub repository link             |
|--------------------|--------------|-----------------------------------------------|
| **Tamene Wolde**   | ATE/5140/15  | https://github.com/TAME911/my-minigit-trial   |
| **Sadik Awel**     | ATE/4047/15  | https://github.com/SadikAwel/my-minigit-trial |
| **Mubarek Jemal**  | ATE/2478/15  | https://github.com/mubarekdevv/my-minigit-trial |
| **Terefe Mulushewa** | ATE/9042/15 |                                              |

---
_A Simplified MiniGit Version Control System_

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Example Workflow](#example-workflow)
- [Technical Documentation](#technical-documentation)

  - [Data Structures](#data-structures)
  - [Design Decisions](#design-decisions)
  - [Limitations](#limitations)
  - [Future Improvements](#future-improvements)

- [About](#About)

---

## Overview

**MiniGit** is a lightweight, educational version control system that mimics essential Git features such as **commits**, **branching**, and **diffing**. It's specifically designed for students and developers to explore core concepts of version control—without the complexity of Git's internals.

---

## Features

- Track and commit file changes with messages
- Create, list, and switch between branches
- View diffs between working, staged, and committed versions
- Checkout previous commits and branches
- Visualize commit history
- Inspect repository status

Note: Merge support is listed as a planned feature unless implemented in your version.

---

## Installation

### 1. Clone the repository

```bash
git clone https://github.com/sadikawel/minigit.git
cd minigit
```

### 2. Compile the project (C++17 required)

CMD / PowerShell:

```cmd
g++ -std=c++17 main.cpp MiniGitSystem.cpp -o minigit
```

---

## Usage

### Repository operations:

```cmd
./minigit init                     # Initialize a repository
./minigit add <filename>          # Add file to staging area
./minigit commit "message"         # Commit staged changes
./minigit status                  # View current status
./minigit log                     # View commit history
```

### Branching:

```cmd
./minigit branch <branch-name>    # Create a new branch
./minigit branch                  # List all branches
./minigit checkout <name|hash>    # Switch to a branch or commit
```

### Diffing:

```cmd
./minigit diff                    # Working directory vs. staging
./minigit diff --staged          # Staging vs. HEAD
./minigit diff <commit>          # Working dir vs. specific commit
./minigit diff <commit1> <commit2> # Between two commits
```

---

## Example Workflow

### Clean start and initialize

```cmd
rmdir /s /q .minigit
./minigit init
```

### Create and stage a file

```cmd
echo Line 1 > file.txt
echo Line 2 >> file.txt
type file.txt
./minigit add file.txt
```

### First commit

```cmd
./minigit commit "Add file.txt"
./minigit log
```

### Modify file and diff changes

```cmd
echo Modified Line 1 > file.txt
echo Line 2 >> file.txt
echo New Line 3 >> file.txt
./minigit status
./minigit diff
```

### Stage and commit modified file

```cmd
./minigit add file.txt
./minigit diff --staged
./minigit commit "Modify file.txt and add line"
```

### Create another file and branch

```cmd
echo Branch file content > branch_file.txt
./minigit add branch_file.txt
./minigit commit "Add branch_file.txt"
./minigit branch feature
./minigit checkout feature
```

### Work on feature branch

```cmd
echo Modified on feature > file.txt
./minigit add file.txt
./minigit commit "Modify file on feature branch"
```

### Switch back to master

```cmd
./minigit checkout master
type file.txt
```

### Compare master vs. feature (PowerShell or Command Prompt)

```cmd
set /p master_head=<.minigit\refs\heads\master
set /p feature_head=<.minigit\refs\heads\feature
echo Diffing master (%master_head%) vs feature (%feature_head%)
./minigit diff %master_head% %feature_head%
```

### Diff against first commit

```cmd
set "first_commit_hash=YOUR_ACTUAL_HASH"
./minigit diff %first_commit_hash%
```
### 📽️ MiniGit Demonstration Video

Want to see MiniGit in action?  
Check out this short demo video showing a simple example of initializing a repository, adding files, making commits, and switching branches:

<a href="https://youtu.be/kGN5GuwusAw" target="_blank">
  <img src="https://img.youtube.com/vi/kGN5GuwusAw/0.jpg" alt="MiniGit Demo Video" width="500">
</a>

> 🎬 _This video provides a quick walkthrough of MiniGit's core features using actual terminal commands._

---

## Technical Documentation

### Data Structures

```cpp
struct Commit {
    std::string hash;
    std::string message;
    std::string timestamp;
    std::vector<std::string> parentHashes;
    std::unordered_map<std::string, std::string> fileBlobs;
};
```

### Design Decisions

- `.minigit/commits/` — Commit objects
- `.minigit/objects/` — File content blobs
- `.minigit/refs/heads/` — Branch references
- `.minigit/index` — Tracks staged files and blob hashes
- `.minigit/HEAD` — Current branch pointer

---

## Limitations

### Functional

- No support for subdirectories
- Basic or no conflict resolution
- No `undo` or `reset` commands

### Technical

- Memory-bound for large repositories
- No delta compression or packfiles
- Single-threaded only

---

## Future Improvements

### Short-term

- Enhanced diffing (e.g., Myers algorithm)
- Merge conflict UI
- Atomic file operations

### Long-term

- Delta compression
- Remote sync capabilities
- Graphical history viewer

---

## About

Built as an educational project to understand version control from the inside out.

[a]: #About
