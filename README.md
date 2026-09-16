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

_(Note: You only need to do this once. After this, `moosecount` and `moosemetrics` will be available in every new terminal window you open)._

---

## 1. MooseCount (C++ Line Counter)

Accurately tracks and separates metrics into Code Lines, Format Lines (isolated brackets `{` or `}`), Comment Lines, and Blank Lines.

### Usage

```bash
moosecount [options] <path1> <path2> ...
```

_(If no path is provided, it searches the current directory)._

**Options:**

- `--exclude <rule>` : Skips a file or folder during traversal. Accepts the same rule syntax as `--ignore-file` (see below), so `--exclude "*build*"` skips `build`, `prebuild_out`, and `build_x64` alike. Can be used multiple times.
- `--ignore-file <filename>` : Reads a file (like `.gitignore`) and applies the rules inside it. Comment lines (`#`) are handled for you. Reads exactly the one file you name.
- `--gitignore` : Discovers and honors every `.gitignore` in the tree as it walks, the way git itself does. Rules in a nested `.gitignore` apply only to that folder and below, and anchored rules inside it are relative to that folder — so a submodule's `.gitignore` no longer leaks into its siblings. Combines with `--exclude` and `--ignore-file`.
- `--ext <extension>` : Adds an extension to the scanned set (the leading dot is optional, so `--ext py` and `--ext .py` are equivalent). Can be used multiple times.
- `--no-defaults` : Clears the built-in extension list so that only extensions added with `--ext` are scanned.
- `--sort` : Orders the per-file listing by line count, largest first, instead of traversal order.
- `-h`, `--help` : Prints the full option list and rule syntax, then exits.
- `-v`, `--version` : Prints the version, then exits.

### Ignore rule syntax

The same syntax is shared by `--exclude`, `--ignore-file`, and `--gitignore`:

| Rule | Matches |
| --- | --- |
| `build` | anything named `build`, file or folder, at any depth |
| `build/` | only folders named `build`, at any depth |
| `*.gen.cpp` | any file whose name ends in `.gen.cpp` |
| `*build*` | any name containing `build` |
| `build_?` | `build_1`, `build_x`, … |
| `/build` | only `build` at the root of the searched path |
| `./build` | `build` inside your current directory |
| `../app/build` | that folder, resolved from your current directory |
| `src/generated` | only that exact path, relative to the searched path |
| `**/temp_out` | a `temp_out` folder at any depth, including the top level |
| `!keep_me.cpp` | puts back something an earlier rule excluded |

A rule containing a `/` is anchored to the path you searched rather than matching a bare name, which is how gitignore behaves. In those anchored rules `*` and `?` stop at a `/` boundary, while `**` spans directories. A trailing `/` limits a rule to folders.

Rules that start with `./` or `../` are the exception: those are read as paths relative to your **current directory**, not to the path being searched, because that is what they mean everywhere else in a shell. So counting a project that lives above you works the way you would type it:

```bash
moosecount --gitignore --exclude ../../other_app/junk ../../other_app
```

If such a rule resolves to somewhere outside every path you asked to count, it cannot exclude anything, and moosecount tells you rather than leaving it to be discovered in the numbers:

```text
Warning: --exclude "../../junk/" is outside the paths being searched
```

A bare `src/generated` keeps its gitignore meaning — relative to each searched path — so `moosecount --exclude src/gen dirA dirB` still means "`src/gen` inside each of them".

A rule starting with a single `/` always keeps its gitignore meaning — the root of the searched path — even when it looks like a filesystem path. To exclude by absolute path, use a bare name, or spell it relative to where you are standing with `./` or `../`.

**Order matters.** Within one rule set the last rule that matches wins, which is what lets a `!` rule override an earlier exclusion. Between rule sets, a `.gitignore` deeper in the tree overrides the one above it, and rules given on the command line override both.

As in git, a `!` rule cannot rescue a file whose folder was already excluded — the folder is never walked into, so nothing inside it can be put back.

If an `--exclude` rule you typed never matches anything, moosecount says so on stderr rather than leaving you to wonder why your counts look wrong:

```text
Warning: --exclude "tetss" matched nothing
```

Rules read from an ignore file stay quiet, since a shared `.gitignore` listing folders that do not exist in the tree you are counting is perfectly normal.

By default the following extensions are scanned: `.c`, `.cc`, `.cpp`, `.h`, `.hh`, `.hpp`, `.m`, `.mm`, `.java`, `.cs`, `.js`, `.ts`, `.kt`, `.swift`, `.go`, `.rs`. Files named directly on the command line are always counted, regardless of extension.

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

By Extension
  .cpp  = 227
  .hpp  = 24
```

_(The `By Extension` breakdown is only printed when more than one extension was matched. Both it and the per-file column report Code Lines + Format Lines.)_

---

## 2. MooseMetrics (Document Tracker)

Tracks architectural complexity and task completion. It extracts Word counts, Open vs. Completed tasks (`- [ ]` vs `- [x]`), and PlantUML entities/relationships.

### Usage

```bash
moosemetrics [options] <path1> <path2> ...
```

**Options:**

- `--exclude <folder>` : Skips a specific folder name during traversal (defaults to ignoring `build`, `bin`, `.git`, and `.vscode`). Can be used multiple times.

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

---

## 3. Testing

The project uses an integration test wrapper to verify the output of `moosecount`.
Sample files are stored in `tests/data/` and the wrapper script (`tests/run_tests.py`) runs the compiled binary against this data to ensure the parsing and counting logic remains accurate.

To run the integration test suite:

```bash
make test
```
