# Moose Metrics Suite

A personalized, lightweight toolchain for tracking project metrics across both executable code and design documentation.

1. **`moosecount`**: A fast, C++17 command-line tool for parsing C-style codebases (`.c`, `.cpp`, `.java`, `.ts`, and more).
2. **`moosemetrics`**: A Python 3 script for extracting structural metrics from Markdown, Text, and PlantUML design files.

---

## Installation

This project utilizes CMake wrapped in a standard Makefile. You can install these tools directly to your user's local binary folder (`~/.local/bin`) so they act like native system commands across all your projects.

```bash
make install
```

To remove the tools from your system:
```bash
make uninstall
```

### Post-Installation: Adding to your PATH

Because these tools are installed to your user directory (`~/.local/bin`) rather than the root system directory, you need to ensure your terminal knows where to find them.

**For macOS (Default Zsh):**
Run the following commands to add the path to your profile and reload it:
```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

**For WSL / Ubuntu (Default Bash):**
Run the following commands to add the path to your profile and reload it:
```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

*(Note: You only need to do this once. After this, `moosecount` and `moosemetrics` will be available in every new terminal window you open).*

---

## 1. MooseCount (C++ Line Counter)

Accurately tracks and separates metrics into Code Lines, Format Lines (isolated brackets `{` or `}`), Comment Lines, and Blank Lines.

### Usage
```bash
moosecount [options] <path1> <path2> ...
```
*(If no path is provided, it searches the current directory).*

**Default Extensions:**
`.c` `.cc` `.cpp` `.h` `.hh` `.hpp` `.m` `.mm` `.java` `.cs` `.js` `.ts` `.kt` `.swift` `.go` `.rs`

**Options:**
* `--exclude <folder>` : Skips a specific folder name during directory traversal. Can be used multiple times.
* `--ignore-file <filename>` : Reads a file (like `.gitignore`) and skips any directories listed inside it.
* `--ext <extension>` : Adds a file extension to the search (e.g. `--ext .lua` or `--ext lua`). Can be used multiple times.
* `--no-defaults` : Clears the default extension set. Use with `--ext` to target only specific languages (e.g. `--no-defaults --ext .java`).
* `--sort` : Sorts the per-file output by line count, highest first.

**Example Run & Output:**
```bash
moosecount --exclude build --exclude scratch .
```
```text
145     ./src/main.cpp
82      ./src/logger.cpp
24      ./include/logger.hpp

TOTALS...

Lines of Code   = 251
File Count      = 3
Code Lines      = 220
Format Lines    = 31
Comment Lines   = 45
Blank Lines     = 60
Total Lines     = 356
```

---

## 2. MooseMetrics (Document Tracker)

Tracks architectural complexity and task completion. It extracts Word counts, Open vs. Completed tasks (`- [ ]` vs `- [x]`), and PlantUML entities/relationships.

### Usage
```bash
moosemetrics [options] <path1> <path2> ...
```

**Options:**
* `--exclude <folder>` : Skips a specific folder name during traversal (defaults to ignoring `build`, `bin`, `.git`, and `.vscode`). Can be used multiple times.

**Example Run & Output:**
```bash
moosemetrics --exclude scratch .
```
```text
File                                               | Metrics
--------------------------------------------------------------------------------
./design/architecture.puml                         | Entities: 12 | Relationships: 18
./devlog/todo.md                                   | Words: 120 | Tasks: 10/12

================================================================================
TOTALS
================================================================================
Markdown & Text Files: 1
  - Lines          : 45
  - Words          : 120
  - Headers        : 2
  - Open Tasks     : 2
  - Completed Tasks: 10

PlantUML Files: 1
  - Lines          : 45
  - UML Entities   : 12
  - Relationships  : 18
```