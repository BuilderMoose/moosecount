# Changelog

## 0.4.0

One library, two tools, and output a machine can read.

### Added

- `--json` on both tools, carrying the same numbers as the table plus a
  per-file breakdown, a per-language (or per-document-kind) rollup, and any
  warnings. `schemaVersion` heads the document.
- moosemetrics gains the entire ignore engine: wildcards, anchored rules,
  negation, `--gitignore` discovery, `--ignore-file`, `--ext`, `--no-defaults`
  and `--sort`. Previously its `--exclude` took a bare folder name and nothing
  else.
- Unit tests for document metrics and for the renderers, including JSON
  escaping. 60 unit tests in total.

### Changed

- moosemetrics is a C++ binary built from the shared library rather than a
  Python script. `moosemetrics.py` is deleted; `make install` installs two
  binaries. Its output and its numbers are unchanged, verified by diffing the
  two implementations over this repository before the script was removed.
- Option parsing, traversal and the ignore rules are shared by both tools, so
  a flag cannot come to mean two different things again.
- moosemetrics lists files in sorted order and prefixes paths the way
  moosecount always has, so the two agree.

## 0.3.0

Language-aware counting. Every language is now parsed by its own rules rather
than by one C-family state machine pointed at sixteen extensions.

### Fixed

- Rust lifetimes (`&'static str`) opened a string that never closed, so every
  comment and brace after it in the file was counted as code.
- Nested block comments, which Rust, Swift and Kotlin all allow, ended early at
  the first `*/`.
- An unterminated string no longer runs past the end of its line.
- Raw strings are understood: Rust's `r#"..."#` and C++'s `R"delim(...)delim"`.
- Files reached with `--ext` for a language with no support were parsed as C
  and silently miscounted. They are now counted blank versus non-blank, with a
  warning naming the extension.

### Added

- Python and Shell support, alongside the languages already counted.
- Unit tests (googletest, vendored as a submodule under `google/`), covering
  line classification per language, the ignore rule engine and the wildcard
  matcher. `make test` runs these and the existing integration suite.

### Changed

- The per-file listing is sorted: by path normally, by count with `--sort`,
  with ties broken on path. Traversal order is unspecified by the standard, so
  two machines could previously print the same tree in different orders.
- The `By Extension` breakdown is now `By Language`, so `.cc`, `.cpp` and
  `.hpp` aggregate under one heading.
- `.py`, `.pyw`, `.sh`, `.bash`, `.zsh`, `.cxx`, `.hxx`, `.jsx`, `.mjs`,
  `.tsx` and `.kts` are counted by default, being languages with real support.
- Format lines are a property of each language. Python reports none, because
  it expresses structure through indentation rather than braces.
- `main.cpp` is now the command line only; counting lives in `MoosecountLib`
  so the tests can link it.

### Known limitations

- JavaScript regular expression literals cannot be told from division without
  parsing, so a regex containing a quote will confuse string tracking.
- Shell heredocs are not recognized.
- Python docstrings count as code rather than comments. A state machine cannot
  distinguish a docstring from any other triple-quoted string. cloc treats them
  as comments; moosecount does not.

## 0.2.0

- `--help` and `--version`, with usage going to stdout on success and to stderr
  with a non-zero exit for an unrecognized flag.

## Earlier

Ignore handling was reworked to follow git's model: wildcards, anchored rules,
negation, file-level rules, per-directory `.gitignore` discovery via
`--gitignore`, and a warning when an `--exclude` rule matches nothing.
