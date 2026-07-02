# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Install

```bash
make            # build (Release by default, outputs to bin/)
make install    # build + copy to ~/.local/bin
make uninstall  # remove from ~/.local/bin
make clean      # remove build/ and bin/
```

Debug build:
```bash
make BUILD_TYPE=Debug
```

The C++ binary is built via CMake, wrapped by the Makefile. CMake outputs the `moosecount` executable to `bin/` during the build, but `make install` is what places both tools in `~/.local/bin`.

## Run Metrics Against This Repo

```bash
make code-metrics   # runs moosecount excluding build/, bin/, scratch/
make doc-metrics    # runs moosemetrics.py excluding scratch/
make metrics        # runs both
```

## Architecture

Two independent tools, no shared code:

**`moosecount` (C++17, `main.cpp`)** — single-file CLI. Walks directories recursively using `std::filesystem`, processes `.c/.cc/.cpp/.h/.hh/.hpp/.m/.mm` files, and classifies each line via a character-by-character state machine (`ParserState`: Normal, InString, InSingleComment, InMultiComment). Line classification uses bitmask flags (`FLAG_CODE`, `FLAG_FORMAT`, `FLAG_COMMENT`, `FLAG_BLANK`). "Format lines" are lines with only `{` or `}` and no code or comments — tracked separately from code lines.

**`moosemetrics` (Python 3, `moosemetrics.py`)** — single-file CLI using `argparse`. Parses `.md`/`.txt` files (words, headers, open/completed tasks via `- [ ]`/`- [x]`) and `.puml`/`.pu`/`.wsd` files (UML entities, relationships via regex). Default exclusions: `build`, `bin`, `.git`, `.vscode`.

## Key Behaviors to Know

- `moosecount` skips hidden directories (any folder starting with `.`) automatically — no flag needed for `.git`.
- `--ignore-file` in `moosecount` reads a gitignore-style file, strips trailing slashes, and ignores comment lines (`#`).
- `moosemetrics` exclusions are checked against *all* path parts, not just the top-level directory name.
- "Lines of Code" in moosecount output = `codeLines + formatLines` (the per-file line and the TOTALS summary both use this combined value).
