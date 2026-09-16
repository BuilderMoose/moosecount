# Changelog

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
