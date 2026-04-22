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

```
```text?code_stdout&code_event_index=2
README.md created successfully

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

#### Examples
```bash
# Standard Run (Ignoring specific folders)
./bin/codecount --exclude build --exclude bin --exclude scratch .

# Using an existing .gitignore
./bin/codecount --ignore-file .gitignore .
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

#### Example
```bash
python3 doc_metrics.py --exclude scratch .
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
