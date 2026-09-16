# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Moose Metrics Suite: two independent metrics tools shipped from one repo.

- **`moosecount`** — counts lines of source in eleven languages, each parsed by its own `LanguageSpec`.
- **`moosemetrics`** — counts structure in Markdown/Text and PlantUML documents.

Both are thin CLIs over `MoosecountLib` (`src/`), which owns traversal, the ignore rules, option parsing and the renderers. That sharing is the point: the two tools previously diverged on what `--exclude` meant, and the library is what stops it happening again. `main.cpp` and `metrics_main.cpp` hold only what is specific to each.

| File | Holds |
| --- | --- |
| `src/counting.hpp` | `CountTotals`, the four code categories |
| `src/language.hpp/.cpp` | `LanguageSpec`, the registry, extension lookup |
| `src/parser.hpp/.cpp` | one state machine, driven by a `LanguageSpec` |
| `src/document.hpp/.cpp` | `DocumentTotals`, the Markdown and PlantUML scanners |
| `src/ignore.hpp/.cpp` | the rule engine |
| `src/walker.hpp/.cpp` | `walkFiles()` plus the two report builders |
| `src/options.hpp/.cpp` | shared flag parsing, help, version |
| `src/report.hpp/.cpp` | `renderText()`, `renderJson()`, `jsonEscape()` |

`walkFiles()` is the seam: it does traversal, ignore rules and extension filtering, then hands each surviving file to a callback. `countPaths()` and `countDocumentPaths()` are both built on it, so neither tool has its own copy of the hard part.

## Build & Install

```bash
make                # same as `make build`
make build          # cmake configure + build into bin/ (Release by default)
make BUILD_TYPE=Debug
make test           # builds, then runs bin/tester and tests/run_tests.py
make clean          # removes build/ and bin/
make install        # bin/moosecount and moosemetrics.py -> ~/.local/bin (PREFIX overridable)
make uninstall
```

The C++ binary is built via CMake, wrapped by the Makefile. CMake outputs `moosecount` to `bin/` during the build; `make install` is what places both tools in `~/.local/bin` (the Python script is copied there as `moosemetrics`, without the `.py`, and chmod'd executable).

## Run Metrics Against This Repo

```bash
make code-metrics   # runs moosecount excluding build/, bin/, google/, scratch/
make doc-metrics    # runs moosemetrics.py excluding scratch/
make metrics        # runs both
```

## moosecount architecture

Everything except the command lines lives in `MoosecountLib`, so the unit tests link it rather than shelling out to a binary.

`countSource(content, language)` is a single-pass character state machine (Normal / InString / InLineComment / InBlockComment). Per line it accumulates a bitmask of `FLAG_CODE | FLAG_FORMAT | FLAG_COMMENT`; `tallyLine()` fires on `\n` (plus once more at EOF if the file lacks a trailing newline) and resolves the mask into the counters. `processFile()` is a thin wrapper that reads a file and calls it — the in-memory form is what the tests drive.

Classification rules that matter when changing the parser:

- A **format line** holds only `formatChars` and *no* code and *no* comment. A line can be both code and comment.
- The headline "Lines of Code" is `codeLines + formatLines`; that same sum is the per-file number printed in the left column and in the by-language breakdown.
- Whitespace is skipped before the state dispatch, so it never sets a flag. That is what makes "blank" mean "no non-whitespace characters" even inside a multi-line string, and it matches the spec in `design/0.3.0-requirements.md`.
- An unterminated single-line string is closed at the newline rather than running on. Without that, one stray quote silently reclassifies the rest of a file.
- `formatChars` is per language and empty for Python, so Python reports zero format lines by design, not by accident.
- Comment tokens, string openers and escapes all advance `i` manually inside the loop; watch for double-advance bugs when editing.

Adding a language is a `LanguageSpec` in `src/language.cpp` and a test in `tests/unit/testLanguages.cpp`; nothing in the parser should need to change. The exceptions are gathered in `LanguageQuirks` — Rust lifetimes, hash raw strings, paren raw strings — so anything a table genuinely cannot express is visible in the data rather than buried in the state machine.

Traversal in `countPaths()` (`src/walker.cpp`) skips any directory whose name starts with `.` (so `.git` needs no flag) or that matches the ignore rules, and skips dotfiles. It looks each file's extension up in the language registry, falling back to `genericLanguage()` and recording the extension in `report.unknownExtensions` so `main.cpp` can warn once.

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

`src/document.cpp` scans `.md`/`.txt` for lines, words, headers and open/completed tasks, and `.puml`/`.pu`/`.wsd` for UML entities and relationships. These are line-oriented scans, not `std::regex`, and the rules were transcribed from the Python script this replaced so the numbers would not move.

Two details that a rewrite gets wrong by default, both pinned by tests:

- A bare `#` **is** a header. CommonMark allows an empty ATX heading, and the old script's `^#{1,6}\s` matched the newline. Requiring content after the hashes silently drops them.
- Sub-totals are suppressed for a document kind with no files, because the script's `defaultdict` had no keys to print.

Default exclusions (`build`, `bin`, `.git`, `.vscode`) are seeded as ordinary ignore rules in `metrics_main.cpp`, so they behave like every other rule.

## Testing

Two suites, both run by `make test`.

**Unit tests** — `tests/unit/`, googletest vendored as a submodule under `google/`. They link `MoosecountLib` directly: `testParser.cpp` and `testLanguages.cpp` drive `countSource()` with source held in memory, `testDocument.cpp` does the same for document metrics, `testIgnoreRules.cpp` and `testGlobMatch.cpp` cover the rule engine, and `testReport.cpp` covers rendering and JSON escaping. Run `./bin/tester` alone, and `--gtest_filter=TestLanguagesFixture.*` for one suite.

**Integration tests** — `tests/run_tests.py` runs the built binary against the fixture trees and regex-matches the `Key = Value` totals block in stdout against hardcoded expectations. Two consequences:

- Changing the totals output format (labels, `=` spacing) breaks the test parser, not just the values.
- Adding or editing a file under `tests/data*/` requires updating the expected counts by hand.

`sample1.cpp` deliberately embeds `/* */` and `//` inside a string literal to pin the string-vs-comment precedence.

## Conventions

- `bin/` and `build/` are gitignored build output. Several test fixtures, however, are force-added: the fixture trees contain their own `.gitignore` files, so `git add` would silently skip the files those rules match. After touching fixtures, check `git ls-files --others --ignored --exclude-standard tests/` comes back empty.
- Warnings (`-Wall -Wextra -pedantic -Wformat-security`) are `target_compile_options` on our targets only, so googletest builds quietly. Release adds `-O3 -flto`.
- The googletest submodule means a fresh clone needs `git submodule update --init --recursive`, and `make code-metrics` excludes `google/` so the vendored library is not counted as ours.
- Sources use Allman braces and 2-space indent. Unit tests follow the mooseworks house style: a `TestXFixture` with ctor/SetUp/TearDown and `// -- --- --- ... . .......` separators.
