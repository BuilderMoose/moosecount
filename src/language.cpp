#include "language.hpp"

#include <unordered_map>

namespace
{
const StringDelimiter DOUBLE_QUOTE{"\"", "\"", '\\', false};
const StringDelimiter SINGLE_QUOTE{"'", "'", '\\', false};
const BlockComment C_BLOCK{"/*", "*/"};

const std::vector<LanguageSpec> &buildLanguages()
{
  static const std::vector<LanguageSpec> languages = [] {
    std::vector<LanguageSpec> list;

    LanguageSpec c;
    c.name = "C/C++";
    c.extensions = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"};
    c.lineComments = {"//"};
    c.blockComments = {C_BLOCK};
    c.strings = {DOUBLE_QUOTE, SINGLE_QUOTE};
    c.formatChars = "{}";
    c.quirks.parenRawStrings = true;
    list.push_back(c);

    LanguageSpec objc;
    objc.name = "Objective-C";
    objc.extensions = {".m", ".mm"};
    objc.lineComments = {"//"};
    objc.blockComments = {C_BLOCK};
    objc.strings = {DOUBLE_QUOTE, SINGLE_QUOTE};
    objc.formatChars = "{}";
    list.push_back(objc);

    LanguageSpec java;
    java.name = "Java";
    java.extensions = {".java"};
    java.lineComments = {"//"};
    java.blockComments = {C_BLOCK};
    java.strings = {{"\"\"\"", "\"\"\"", '\\', true}, DOUBLE_QUOTE, SINGLE_QUOTE};
    java.formatChars = "{}";
    list.push_back(java);

    LanguageSpec csharp;
    csharp.name = "C#";
    csharp.extensions = {".cs"};
    csharp.lineComments = {"//"};
    csharp.blockComments = {C_BLOCK};
    csharp.strings = {DOUBLE_QUOTE, SINGLE_QUOTE};
    csharp.formatChars = "{}";
    list.push_back(csharp);

    LanguageSpec js;
    js.name = "JavaScript/TypeScript";
    js.extensions = {".js", ".jsx", ".mjs", ".ts", ".tsx"};
    js.lineComments = {"//"};
    js.blockComments = {C_BLOCK};
    js.strings = {DOUBLE_QUOTE, SINGLE_QUOTE, {"`", "`", '\\', true}};
    js.formatChars = "{}";
    list.push_back(js);

    LanguageSpec kotlin;
    kotlin.name = "Kotlin";
    kotlin.extensions = {".kt", ".kts"};
    kotlin.lineComments = {"//"};
    kotlin.blockComments = {C_BLOCK};
    kotlin.blockCommentsNest = true;
    kotlin.strings = {{"\"\"\"", "\"\"\"", '\0', true}, DOUBLE_QUOTE, SINGLE_QUOTE};
    kotlin.formatChars = "{}";
    list.push_back(kotlin);

    LanguageSpec swift;
    swift.name = "Swift";
    swift.extensions = {".swift"};
    swift.lineComments = {"//"};
    swift.blockComments = {C_BLOCK};
    swift.blockCommentsNest = true;
    swift.strings = {{"\"\"\"", "\"\"\"", '\\', true}, DOUBLE_QUOTE};
    swift.formatChars = "{}";
    list.push_back(swift);

    LanguageSpec go;
    go.name = "Go";
    go.extensions = {".go"};
    go.lineComments = {"//"};
    go.blockComments = {C_BLOCK};
    go.strings = {DOUBLE_QUOTE, SINGLE_QUOTE, {"`", "`", '\0', true}};
    go.formatChars = "{}";
    list.push_back(go);

    LanguageSpec rust;
    rust.name = "Rust";
    rust.extensions = {".rs"};
    rust.lineComments = {"//"};
    rust.blockComments = {C_BLOCK};
    rust.blockCommentsNest = true;
    rust.strings = {DOUBLE_QUOTE};
    rust.formatChars = "{}";
    rust.quirks.lifetimeApostrophe = true;
    rust.quirks.hashRawStrings = true;
    list.push_back(rust);

    LanguageSpec python;
    python.name = "Python";
    python.extensions = {".py", ".pyw"};
    python.lineComments = {"#"};
    python.strings = {{"\"\"\"", "\"\"\"", '\\', true},
                      {"'''", "'''", '\\', true},
                      DOUBLE_QUOTE,
                      SINGLE_QUOTE};
    // Structure is indentation, which occupies no lines of its own
    python.formatChars = "";
    list.push_back(python);

    LanguageSpec shell;
    shell.name = "Shell";
    shell.extensions = {".sh", ".bash", ".zsh"};
    shell.lineComments = {"#"};
    shell.strings = {DOUBLE_QUOTE, SINGLE_QUOTE};
    shell.formatChars = "{}";
    list.push_back(shell);

    return list;
  }();

  return languages;
}

const std::unordered_map<std::string, const LanguageSpec *> &extensionIndex()
{
  static const std::unordered_map<std::string, const LanguageSpec *> index = [] {
    std::unordered_map<std::string, const LanguageSpec *> map;
    for (const auto &language : buildLanguages())
    {
      for (const auto &extension : language.extensions)
        map[extension] = &language;
    }
    return map;
  }();

  return index;
}
} // namespace

const std::vector<LanguageSpec> &allLanguages()
{
  return buildLanguages();
}

const LanguageSpec *languageForExtension(const std::string &extension)
{
  const auto &index = extensionIndex();
  auto found = index.find(extension);

  return found == index.end() ? nullptr : found->second;
}

const LanguageSpec &genericLanguage()
{
  static const LanguageSpec generic = [] {
    LanguageSpec spec;
    spec.name = "Other";
    return spec;
  }();

  return generic;
}

std::unordered_set<std::string> defaultExtensions()
{
  std::unordered_set<std::string> extensions;
  for (const auto &language : allLanguages())
  {
    for (const auto &extension : language.extensions)
      extensions.insert(extension);
  }

  return extensions;
}
