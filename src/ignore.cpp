#include "ignore.hpp"

#include <fstream>

namespace
{
std::string trim(const std::string &str)
{
  size_t first = str.find_first_not_of(" \t\r\n");
  if (std::string::npos == first)
    return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}
} // namespace

// Wildcard match supporting '*' (any run of characters) and '?' (one character).
// In pathMode, '*' and '?' stop at a '/' boundary and only '**' spans directories.
bool globMatch(const char *pattern, const char *text, bool pathMode)
{
  if (*pattern == '\0')
    return *text == '\0';

  if (pattern[0] == '*' && pattern[1] == '*')
  {
    const char *rest = pattern + 2;

    // A "**/" segment is allowed to match zero directories
    if (rest[0] == '/' && globMatch(rest + 1, text, pathMode))
      return true;

    if (globMatch(rest, text, pathMode))
      return true;

    for (const char *t = text; *t != '\0'; ++t)
    {
      if (globMatch(rest, t + 1, pathMode))
        return true;
    }
    return false;
  }

  if (pattern[0] == '*')
  {
    if (globMatch(pattern + 1, text, pathMode))
      return true;

    for (const char *t = text; *t != '\0' && !(pathMode && *t == '/'); ++t)
    {
      if (globMatch(pattern + 1, t + 1, pathMode))
        return true;
    }
    return false;
  }

  if (*text == '\0')
    return false;

  if (pattern[0] == '?')
    return !(pathMode && *text == '/') && globMatch(pattern + 1, text + 1, pathMode);

  return pattern[0] == *text && globMatch(pattern + 1, text + 1, pathMode);
}

bool matchesGlob(const std::string &pattern, const std::string &text, bool pathMode)
{
  return globMatch(pattern.c_str(), text.c_str(), pathMode);
}

// Absolute, lexically normalized form of a path, with no trailing slash. Used
// for both sides of an absolute rule so the two are always comparable.
std::string normalizedPathString(const fs::path &path)
{
  std::string text = fs::absolute(path).lexically_normal().generic_string();
  while (text.size() > 1 && text.back() == '/')
    text.pop_back();

  return text;
}

bool isInsideRoot(const std::string &candidate, const std::string &root)
{
  if (candidate == root)
    return true;

  std::string prefix = (root == "/") ? root : root + "/";
  return candidate.rfind(prefix, 0) == 0;
}

// Reads gitignore-shaped rules into an existing rule set
bool loadIgnoreFile(const fs::path &path, IgnoreRules &rules)
{
  std::ifstream ignoreFile(path);
  if (!ignoreFile.is_open())
    return false;

  std::string line;
  while (std::getline(ignoreFile, line))
  {
    std::string trimmed = trim(line);
    if (!trimmed.empty() && trimmed[0] != '#')
    {
      rules.add(trimmed);
    }
  }
  return true;
}

IgnoreVerdict evaluateRules(const IgnoreRules &rules, const fs::path &base, const fs::path &path,
                            const std::string &name, bool isDirectory)
{
  std::string relativePath;
  if (rules.hasAnchoredRules())
    relativePath = path.lexically_relative(base).generic_string();

  std::string absolutePath;
  if (rules.hasAbsoluteRules())
    absolutePath = normalizedPathString(path);

  return rules.evaluate(name, relativePath, absolutePath, isDirectory);
}

// `commandLine` is taken by reference on purpose: it records which of its rules
// matched, and that has to survive across every search path.
bool isIgnored(const IgnoreRules &commandLine, const fs::path &commandLineBase,
               const std::vector<IgnoreFrame> &frames, const fs::path &path,
               const std::string &name, bool isDirectory)
{
  // Rules given on the command line outrank anything found in a .gitignore
  IgnoreVerdict verdict = evaluateRules(commandLine, commandLineBase, path, name, isDirectory);

  // Otherwise the deepest .gitignore decides, the way git resolves it
  for (auto frame = frames.rbegin(); verdict == IgnoreVerdict::None && frame != frames.rend(); ++frame)
  {
    verdict = evaluateRules(frame->rules, frame->base, path, name, isDirectory);
  }

  return verdict == IgnoreVerdict::Ignore;
}
