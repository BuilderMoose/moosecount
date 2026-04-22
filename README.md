# Project Metrics: Code & Document Counters

This repository contains two lightweight, dependency-free tools designed to track project metrics across both executable code and design documentation.

1. **`codecount`**: A fast, C++17 command-line tool for parsing C-style codebases (`.c`, `.cpp`, `.h`, `.m`, etc.).
2. **`doc_metrics.py`**: A Python 3 script for extracting structural metrics from Markdown, Text, and PlantUML design files.

---

## 1. C++17 Line of Code Counter (`codecount`)

Accurately tracks and separates metrics into:
* **Code Lines:** Core executable logic.
* **Format Lines:** Lines exclusively containing structural brackets (`{` or `}`).
* **Comment Lines:** `//` and `/* */` comments.
* **Blank Lines:** Empty lines or lines with only whitespace.

### Building

This project utilizes CMake wrapped in a standard Makefile.

```bash
make          # Builds the project in Release mode
make clean    # Removes build artifacts and the bin folder
```

The compiled binary will be placed in `./bin/codecount`.

### Usage

```bash
./bin/codecount [options] <path1> <path2> ...
```

If no path is provided, it defaults to recursively searching the current directory (`.`).

#### Options
* `--exclude <folder>` : Skips a specific folder name during directory traversal. Can be used multiple times.
* `--ignore-file <filename>` : Reads a file (like `.gitignore`) and skips any directories listed inside it.

#### Example Commands
```bash
# Standard Run (Ignoring specific folders)
./bin/codecount --exclude build --exclude bin --exclude scratch .

# Using an existing .gitignore
./bin/codecount --ignore-file .gitignore .
```

#### Example Output
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

## 2. Document Metrics Tracker (`doc_metrics.py`)

A Python script that tracks architectural complexity and task completion rather than just raw line counts. It extracts:
* **Markdown/Text:** Word counts, header counts, and Open vs. Completed tasks (`- [ ]` vs `- [x]`).
* **PlantUML:** Number of UML entities (actors, components, states) and relationship connectors (`->`, `..>`).

### Usage

```bash
python3 doc_metrics.py [options] <path1> <path2> ...
```

#### Options
* `--exclude <folder>` : Skips a specific folder name during traversal (defaults to ignoring `build`, `bin`, `.git`, and `.vscode`). Can be used multiple times.

#### Example Command
```bash
python3 doc_metrics.py --exclude scratch .
```

#### Example Output
```text
File                                               | Metrics
--------------------------------------------------------------------------------
./design/architecture.puml                         | Entities: 12 | Relationships: 18
./devlog/2026-04-21-update.md                      | Words: 450 | Tasks: 3/5
./devlog/todo.md                                   | Words: 120 | Tasks: 10/12

================================================================================
TOTALS
================================================================================
Markdown & Text Files: 2
  - Lines          : 145
  - Words          : 570
  - Headers        : 8
  - Open Tasks     : 4
  - Completed Tasks: 13

PlantUML Files: 1
  - Lines          : 45
  - UML Entities   : 12
  - Relationships  : 18
```

---

## Recommended Bash Aliases

To make these tools easily accessible from any branch or directory on your machine, you can add the following aliases to your `~/.bashrc` or `~/.zshrc` profile. 

*(Update the `/Users/username/...` path to match your actual home directory and project location)*

```bash
# Code Counter Alias
alias codecount='/Users/username/projects/moose/ccount/bin/codecount'

# Document Metrics Alias
alias docmetrics='python3 /Users/username/projects/moose/ccount/doc_metrics.py'
```
