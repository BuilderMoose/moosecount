# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Moose Metrics Suite: two independent metrics tools shipped from one repo.

- **`moosecount`** — C++17 single-file tool (`main.cpp`) that counts lines in C-family/curly-brace source files.
- **`moosemetrics`** — Python 3 single-file script (`moosemetrics.py`) that counts structure in Markdown/Text and PlantUML files.

They share no code. The only coupling is the CLI convention (`--exclude <folder>`, positional paths defaulting to `.`) and the `Makefile` that builds, tests, and installs both.

## Build & Install

```bash
make                # same as `make build`
make build          # cmake configure + build into bin/ (Release by default)
make BUILD_TYPE=Debug
make test           # builds, then runs tests/run_tests.py
make clean          # removes build/ and bin/
make install        # bin/moosecount and moosemetrics.py -> ~/.local/bin (PREFIX overridable)
make uninstall
```

The C++ binary is built via CMake, wrapped by the Makefile. CMake outputs `moosecount` to `bin/` during the build; `make install` is what places both tools in `~/.local/bin` (the Python script is copied there as `moosemetrics`, without the `.py`, and chmod'd executable).

## Run Metrics Against This Repo

```bash
make code-metrics   # runs moosecount excluding build/, bin/, scratch/
make doc-metrics    # runs moosemetrics.py excluding scratch/
make metrics        # runs both
```

## moosecount architecture

`processFile()` is a single-pass character state machine over the whole file contents (`ParserState`: Normal / InString / InSingleComment / InMultiComment). Per line it accumulates a bitmask of `FLAG_CODE | FLAG_FORMAT | FLAG_COMMENT`; `tallyLine()` fires on `\n` (plus once more at EOF if the file lacks a trailing newline) and resolves the mask into the counters.

Classification rules that matter when changing the parser:

- A **format line** is counted only if the line has `{`/`}` and *no* code and *no* comment. A line can be both code and comment.
- The headline "Lines of Code" is `codeLines + formatLines`; that same sum is the per-file number printed in the left column and in the by-extension breakdown.
- Strings are delimited by `"`, `'`, or backtick — all three are treated identically, so language-specific quoting (e.g. Python) is not modeled. This is why the tool targets curly-brace languages.
- The `//`, `/*`, `*/`, and escape handling all advance `i` manually inside the loop; watch for double-advance bugs when editing.

Traversal in `main()` skips any directory whose name starts with `.` (so `.git` needs no flag) or that matches `ignoredItems`, and skips dotfiles.

Both `--exclude` and `--ignore-file` feed the same `IgnoreRules` struct. `add()` normalizes a rule into an `IgnoreRule` — stripping a leading `!` (`negated`), a trailing `/` (`dirOnly`), and a leading `/` (`anchored`, also set by any embedded separator) — and appends it to an **ordered** vector. A rule has exactly one of three flavors, and getting the flavor wrong is how this code has broken before:

- **bare name** (`build`) — matched against the entry's filename at any depth.
- **anchored** (`src/gen`, `/build`) — matched against `lexically_relative(frame base)`.
- **absolute** (`./x`, `../x`) — resolved against the working directory at parse time via `normalizedPathString()`, then matched against the entry's own absolute normalized path.

The absolute flavor exists because a path-shaped argument means a filesystem path everywhere else in a shell. Without it, `../../app/junk` keeps its embedded separator, is classified as anchored, and is then compared against a relative path that can never carry a `../..` prefix — so it silently matches nothing. `normalizedPathString()` is deliberately used on *both* sides (rule and entry) so the two are always comparable; it is lexical (`lexically_normal`, no filesystem access) so wildcards survive resolution.

A leading `/` always keeps its gitignore meaning, even when the rule looks like an absolute filesystem path. Disambiguating those two readings was tried and deliberately reverted: it made one rule string mean different things depending on which search paths accompanied it, which is more cleverness than the case is worth. `reportUnmatched()` makes the failure visible instead, and `./`/`../` remain the supported way to name a path.

That warning keys its sharper "is outside the paths being searched" message on `rule.absolute` alone, since only `./` and `../` rules are unambiguously paths. Anything else gets the plain "matched nothing", which stays accurate whichever way the user meant the rule. The order is load-bearing: `evaluate()` walks every rule and the last match wins, which is what makes `!` work. It returns an `IgnoreVerdict` of `None`/`Ignore`/`Include` rather than a bool, so "no opinion" stays distinct from "explicitly put back".

Three flags cache properties of the set: `negatedRules` lets `evaluate()` break on first match when no `!` rule could overturn it, `hasAnchoredRules()` keeps the relative-path computation out of the common case, and `trackedRules` marks a set holding rules the user typed.

Rules added with `add(item, true)` — only `--exclude` does this — carry `tracked`, their `original` spelling, and a `mutable matched` flag that `evaluate()` sets. After traversal, `reportUnmatched()` warns on stderr about any tracked rule that never matched, which catches typos and wrong spellings that would otherwise just produce a quietly wrong count. It takes the search roots so an absolute rule pointing outside all of them gets the specific "is outside the paths being searched" message instead of sending the user hunting for a typo that is not there. Two consequences to preserve when editing:

- A tracked set disables the early `break` in `evaluate()`, so a rule shadowed by an earlier one still gets its chance to match. Non-tracked sets (every `.gitignore`) keep the fast path.
- `isIgnored()` takes the command-line rules **by reference**, not as a copied `IgnoreFrame`, because match marks have to survive across search paths. Copying them back into a frame would silently break the report.

`globMatch()` is a recursive backtracking matcher, switched by a `pathMode` flag. In path mode `*` and `?` stop at `/` and only `**` spans directories; a `**/` segment may also match zero directories, which is what lets `**/temp_out` match at the top level. Anchored rules match in path mode against the path relative to the frame's base; the rest match the bare name.

Precedence lives in `isIgnored()`: command-line rules are consulted first and are final if they have an opinion, then `.gitignore` frames deepest-first. Both directories and files are filtered, so a rule like `*.gen.cpp` works. A `!` rule cannot rescue anything inside an excluded directory, because the walker never descends into it — same as git.

`--gitignore` turns on per-directory discovery. Rules live in a stack of `IgnoreFrame`s, each pairing an `IgnoreRules` with the directory it is anchored to and the traversal depth of that directory:

- Command-line rules sit outside the stack in their own frame, so a `.gitignore` can never override an explicit `--exclude`.
- The search root's own `.gitignore` is pushed at depth `-1`, so it is never popped.
- Entering a directory that holds a `.gitignore` pushes a frame at `it.depth()`.
- Before each entry is examined, frames with `depth >= it.depth()` are popped — that single `while` loop unwinds however many levels the walk just left, which is what keeps a submodule's rules from leaking into a sibling.
- `isIgnored()` walks the live frames in reverse and stops at the first frame with a verdict, recomputing the relative path per frame so each frame's anchored rules resolve against *its own* base.

A skipped directory is never read for a `.gitignore`, since the `continue` happens before the push.

`printUsage()` serves both `--help` (stdout, exit 0) and an unrecognized flag (stderr after an error line, exit 1) — the stream is the parameter, so the two can never drift apart. The version comes from `project(MooseCount VERSION ...)` in `CMakeLists.txt`, passed through `target_compile_definitions` as `MOOSECOUNT_VERSION`; `main.cpp` defines a fallback of `"unknown"` so a direct `c++ main.cpp` still compiles. A test asserts the binary's reported version matches the one declared in `CMakeLists.txt`, so bumping it in one place and not the other fails the suite.

Extension filtering starts from a hardcoded default set in `main()` (C-family plus `.java`, `.cs`, `.js`, `.ts`, `.kt`, `.swift`, `.go`, `.rs`); `--ext` adds to it and `--no-defaults` empties it first. Explicitly named files on the command line bypass the extension filter entirely.

The README documents every flag and shows the exact output format, including the `By Extension` block that only prints when more than one extension matched. Keep it in sync when touching argument parsing or the totals output.

## moosemetrics architecture

Single-file CLI using `argparse`. Parses `.md`/`.txt` files (lines, words, headers, open/completed tasks via `- [ ]`/`- [x]`) and `.puml`/`.pu`/`.wsd` files (UML entities and relationships, both by regex). Default exclusions are `build`, `bin`, `.git`, `.vscode`, plus anything passed via `--exclude`; unlike moosecount, exclusions are checked against *all* path parts, not just the directory being traversed.

## Testing

`tests/run_tests.py` runs the built binary against `tests/data/` and regex-matches the `Key = Value` totals block in stdout against a hardcoded `EXPECTED_TOTALS` dict. Two consequences:

- Changing the totals output format (labels, `=` spacing) breaks the test parser, not just the values.
- Adding or editing a file in `tests/data/` requires updating `EXPECTED_TOTALS` by hand.

`sample1.cpp` deliberately embeds `/* */` and `//` inside a string literal to pin the string-vs-comment precedence.

There is no single-test runner — the suite is this one integration test. Run it directly with `python3 tests/run_tests.py` if `bin/moosecount` is already built.

## Conventions

- `bin/` and `build/` are gitignored build output. Several test fixtures, however, are force-added: the fixture trees contain their own `.gitignore` files, so `git add` would silently skip the files those rules match. After touching fixtures, check `git ls-files --others --ignored --exclude-standard tests/` comes back empty.
- CMake sets `-Wall -Wextra -pedantic -Wformat-security`; Release adds `-O3 -flto`.
- `main.cpp` uses Allman braces and 2-space indent.
