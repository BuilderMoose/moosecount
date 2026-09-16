#pragma once

#include <string>
#include <unordered_set>
#include <vector>

// A block comment's opening and closing tokens.
struct BlockComment
{
  std::string open;
  std::string close;
};

// One kind of string literal. `close` is usually the same token as `open`;
// Python's triple quotes and Go's backticks are the interesting exceptions.
struct StringDelimiter
{
  std::string open;
  std::string close;
  char escape = '\\'; // '\0' when the language has no escape character
  bool multiline = false;
};

// Quirks that a table cannot express. Each is named so the limitation is
// visible in the data rather than buried in the parser.
struct LanguageQuirks
{
  // Rust: a bare ' is a lifetime unless it closes as a char literal. This is
  // the defect that motivated 0.3.0.
  bool lifetimeApostrophe = false;

  // Rust: r"...", r#"..."#, with the hash count deciding the terminator.
  bool hashRawStrings = false;

  // C++: R"delim( ... )delim", with an arbitrary delimiter.
  bool parenRawStrings = false;
};

struct LanguageSpec
{
  std::string name;
  std::vector<std::string> extensions;
  std::vector<std::string> lineComments;
  std::vector<BlockComment> blockComments;
  bool blockCommentsNest = false;
  std::vector<StringDelimiter> strings;

  // Characters that count as pure structure when alone on a line. Empty for
  // languages that express structure some other way, which is why Python
  // reports no format lines rather than misreporting them.
  std::string formatChars;

  LanguageQuirks quirks;
};

// Every language the tool knows about.
const std::vector<LanguageSpec> &allLanguages();

// The spec for an extension (".cpp"), or nullptr when nothing claims it.
const LanguageSpec *languageForExtension(const std::string &extension);

// Blank versus non-blank, no comment or string handling. Used for files whose
// extension was added with --ext but which no spec claims.
const LanguageSpec &genericLanguage();

// Every extension claimed by a spec; the default set that gets scanned.
std::unordered_set<std::string> defaultExtensions();
