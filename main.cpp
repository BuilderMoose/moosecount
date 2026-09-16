#include <iostream>
#include <fstream>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <unordered_set>
#include <cctype>

namespace fs = std::filesystem;

// Supplied by CMake; the fallback keeps a plain `c++ main.cpp` building
#ifndef MOOSECOUNT_VERSION
#define MOOSECOUNT_VERSION "unknown"
#endif

void printUsage(std::ostream &out)
{
  out << "Usage: moosecount [options] <path> [path...]\n"
      << "\n"
      << "Counts lines in C-family source files, separating code, formatting\n"
      << "braces, comments and blanks. With no path given, searches the\n"
      << "current directory.\n"
      << "\n"
      << "Options:\n"
      << "  --exclude <rule>       Skip files or folders matching a rule.\n"
      << "                         May be given more than once.\n"
      << "  --ignore-file <file>   Apply the rules in one gitignore-style file.\n"
      << "  --gitignore            Discover and honor every .gitignore while\n"
      << "                         walking, the way git does.\n"
      << "  --ext <extension>      Add an extension to those scanned. The\n"
      << "                         leading dot is optional.\n"
      << "  --no-defaults          Scan only the extensions added with --ext.\n"
      << "  --sort                 List files by line count, largest first.\n"
      << "  -h, --help             Show this help and exit.\n"
      << "  -v, --version          Show the version and exit.\n"
      << "\n"
      << "Ignore rules:\n"
      << "  build                  that name, at any depth\n"
      << "  build/                 folders only\n"
      << "  *.gen.cpp              wildcards; ? matches one character\n"
      << "  /build                 only at the root of the searched path\n"
      << "  src/generated          a path relative to the searched path\n"
      << "  **/temp_out            at any depth, including the top level\n"
      << "  ./build, ../app/build  a path relative to your current directory\n"
      << "  !keep_me.cpp           put back what an earlier rule excluded\n"
      << "\n"
      << "An --exclude rule that matches nothing is reported on stderr.\n";
}

struct CountTotals
{
  long totalLines = 0;
  long blankLines = 0;
  long codeLines = 0;
  long formatLines = 0;
  long commentLines = 0;
  long fileCount = 0;
};

enum class ParserState
{
  Normal,
  InString,
  InSingleComment,
  InMultiComment
};

const int FLAG_BLANK = 0;
const int FLAG_CODE = 1;
const int FLAG_FORMAT = 2;
const int FLAG_COMMENT = 4;

std::string trim(const std::string &str)
{
  size_t first = str.find_first_not_of(" \t\r\n");
  if (std::string::npos == first)
    return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

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

enum class IgnoreVerdict
{
  None,   // no rule had an opinion
  Ignore, // skip this entry
  Include // a '!' rule put it back
};

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

// A single ignore rule, in one of three flavors:
//   bare name      "build"        matches that name at any depth
//   anchored       "src/gen"      matches a path relative to the directory that declared it
//   absolute       "./x", "../x"  resolved against the working directory when typed
struct IgnoreRule
{
  std::string pattern;
  std::string original; // as the user wrote it, for reporting
  bool negated;         // '!foo' puts back something an earlier rule excluded
  bool anchored;        // had an embedded '/', or a leading one
  bool absolute;        // resolved against the working directory
  bool dirOnly;         // had a trailing '/', so files are left alone
  bool tracked;         // report this one if it never matches anything
  mutable bool matched; // set during evaluation, purely for that report
};

// An ordered rule set. Order matters: as in gitignore, the last rule that
// matches decides, which is what lets a later '!' rule win.
struct IgnoreRules
{
  std::vector<IgnoreRule> rules;
  bool anchoredRules = false;
  bool absoluteRules = false;
  bool negatedRules = false;
  bool trackedRules = false;

  // `track` marks a rule the user typed for this run, so we can tell them when
  // it turned out to be dead weight. Returns false if there was no rule in it.
  bool add(const std::string &item, bool track = false)
  {
    std::string rule = item;

    bool negated = false;
    if (!rule.empty() && rule.front() == '!')
    {
      rule.erase(0, 1);
      negated = true;
    }

    // "./x" and "../x" are how a shell spells a path, so honor that reading and
    // resolve them against the working directory rather than the search root
    bool absolute = rule.rfind("./", 0) == 0 || rule.rfind("../", 0) == 0 || rule == "..";

    // A trailing slash limits the rule to directories
    bool dirOnly = false;
    while (!rule.empty() && rule.back() == '/')
    {
      rule.pop_back();
      dirOnly = true;
    }

    // A leading slash anchors the rule rather than naming a bare folder
    bool anchored = false;
    if (!rule.empty() && rule.front() == '/')
    {
      rule.erase(0, 1);
      anchored = true;
    }

    if (rule.empty())
      return false;

    if (absolute)
    {
      rule = normalizedPathString(rule);
      anchored = false;
    }
    else if (rule.find('/') != std::string::npos)
    {
      anchored = true;
    }

    rules.push_back({rule, item, negated, anchored, absolute, dirOnly, track, false});
    anchoredRules = anchoredRules || anchored;
    absoluteRules = absoluteRules || absolute;
    negatedRules = negatedRules || negated;
    trackedRules = trackedRules || track;
    return true;
  }

  bool empty() const
  {
    return rules.empty();
  }

  bool hasAnchoredRules() const
  {
    return anchoredRules;
  }

  bool hasAbsoluteRules() const
  {
    return absoluteRules;
  }

  IgnoreVerdict evaluate(const std::string &name, const std::string &relativePath,
                         const std::string &absolutePath, bool isDirectory) const
  {
    IgnoreVerdict verdict = IgnoreVerdict::None;

    for (const auto &rule : rules)
    {
      if (rule.dirOnly && !isDirectory)
        continue;

      bool matched;
      if (rule.absolute)
        matched = matchesGlob(rule.pattern, absolutePath, true);
      else if (rule.anchored)
        matched = matchesGlob(rule.pattern, relativePath, true);
      else
        matched = matchesGlob(rule.pattern, name, false);

      if (!matched)
        continue;

      verdict = rule.negated ? IgnoreVerdict::Include : IgnoreVerdict::Ignore;
      rule.matched = true;

      // Without any '!' rule nothing later can overturn this, so stop early.
      // Tracked sets keep scanning so every rule gets a fair chance to match.
      if (!negatedRules && !trackedRules)
        break;
    }

    return verdict;
  }

  // A rule the user typed that never matched is almost always a mistake. When
  // it is a path rule pointing outside every search root, say so specifically —
  // "matched nothing" would send them hunting for a typo that is not there.
  void reportUnmatched(const std::string &flag, const std::vector<std::string> &searchRoots) const
  {
    for (const auto &rule : rules)
    {
      if (!rule.tracked || rule.matched)
        continue;

      // Only a "./" or "../" rule is unambiguously a path, so only those earn
      // the sharper message. A "/..." rule that stayed gitignore-anchored is
      // still a valid rule that simply found nothing, and saying otherwise
      // would mislead anyone who meant it the gitignore way.
      bool outside = rule.absolute;
      for (const auto &root : searchRoots)
      {
        if (isInsideRoot(rule.pattern, root))
        {
          outside = false;
          break;
        }
      }

      std::cerr << "Warning: " << flag << " \"" << rule.original << "\""
                << (outside ? " is outside the paths being searched\n" : " matched nothing\n");
    }
  }
};

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

// One set of rules plus the directory they are anchored to. Frames are stacked
// during traversal so a nested .gitignore only governs its own subtree.
struct IgnoreFrame
{
  fs::path base;
  int depth; // traversal depth of `base`; the search root sits at -1
  IgnoreRules rules;
};

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

std::optional<CountTotals> processFile(const fs::path &path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
  {
    std::cerr << "Error unable to open file: " << path << '\n';
    return std::nullopt;
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  CountTotals fileTotals;
  ParserState state = ParserState::Normal;
  char stringDelimiter = '\0';
  int lineFlag = FLAG_BLANK;

  auto tallyLine = [&]()
  {
    fileTotals.totalLines++;
    if (lineFlag & FLAG_CODE)
      fileTotals.codeLines++;
    if (lineFlag & FLAG_COMMENT)
      fileTotals.commentLines++;

    // A format line is exclusively formatting brackets, with no code or comments
    if ((lineFlag & FLAG_FORMAT) && !(lineFlag & FLAG_CODE) && !(lineFlag & FLAG_COMMENT))
    {
      fileTotals.formatLines++;
    }

    if (lineFlag == FLAG_BLANK)
      fileTotals.blankLines++;

    lineFlag = FLAG_BLANK;
    if (state == ParserState::InSingleComment)
      state = ParserState::Normal;
  };

  for (size_t i = 0; i < content.size(); ++i)
  {
    char c = content[i];
    char next_c = (i + 1 < content.size()) ? content[i + 1] : '\0';

    if (c == '\n')
    {
      tallyLine();
      continue;
    }

    if (std::isspace(static_cast<unsigned char>(c)))
      continue;

    switch (state)
    {
    case ParserState::Normal:
      if (c == '/' && next_c == '/')
      {
        state = ParserState::InSingleComment;
        lineFlag |= FLAG_COMMENT;
        i++;
      }
      else if (c == '/' && next_c == '*')
      {
        state = ParserState::InMultiComment;
        lineFlag |= FLAG_COMMENT;
        i++;
      }
      else if (c == '"' || c == '\'' || c == '`')
      {
        state = ParserState::InString;
        stringDelimiter = c;
        lineFlag |= FLAG_CODE;
      }
      else if (c == '{' || c == '}')
      {
        if (!(lineFlag & FLAG_CODE))
          lineFlag |= FLAG_FORMAT;
      }
      else
      {
        lineFlag |= FLAG_CODE;
      }
      break;

    case ParserState::InString:
      lineFlag |= FLAG_CODE;
      if (c == '\\')
      {
        i++; // skip escaped character, handles \\ correctly
      }
      else if (c == stringDelimiter)
      {
        state = ParserState::Normal;
      }
      break;

    case ParserState::InSingleComment:
      lineFlag |= FLAG_COMMENT;
      break;

    case ParserState::InMultiComment:
      lineFlag |= FLAG_COMMENT;
      if (c == '*' && next_c == '/')
      {
        state = ParserState::Normal;
        i++;
      }
      break;
    }
  }

  // Tally the final line if the file doesn't end with a newline
  if (!content.empty() && content.back() != '\n')
  {
    tallyLine();
  }

  return fileTotals;
}

int main(int argc, char *argv[])
{
  IgnoreRules ignoredItems;
  std::vector<std::string> searchPaths;
  bool sortByCount = false;
  bool useGitignore = false;

  std::unordered_set<std::string> targetExtensions = {
      ".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".m", ".mm",
      ".java", ".cs", ".js", ".ts", ".kt", ".swift", ".go", ".rs"};

  // Parse Command Line Arguments
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];
    if (arg == "--exclude" && i + 1 < argc)
    {
      if (!ignoredItems.add(argv[++i], true))
      {
        std::cerr << "Warning: --exclude \"" << argv[i] << "\" is not a usable rule\n";
      }
    }
    else if (arg == "--ext" && i + 1 < argc)
    {
      std::string ext = argv[++i];
      if (ext[0] != '.')
        ext = "." + ext;
      targetExtensions.insert(ext);
    }
    else if (arg == "--no-defaults")
    {
      targetExtensions.clear();
    }
    else if (arg == "--sort")
    {
      sortByCount = true;
    }
    else if (arg == "--gitignore")
    {
      useGitignore = true;
    }
    else if (arg == "--help" || arg == "-h")
    {
      printUsage(std::cout);
      return 0;
    }
    else if (arg == "--version" || arg == "-v")
    {
      std::cout << "moosecount " << MOOSECOUNT_VERSION << '\n';
      return 0;
    }
    else if (arg == "--ignore-file" && i + 1 < argc)
    {
      if (!loadIgnoreFile(argv[++i], ignoredItems))
      {
        std::cerr << "Warning: Could not open ignore file: " << argv[i] << '\n';
      }
    }
    else if (arg[0] != '-')
    {
      searchPaths.push_back(arg);
    }
    else
    {
      std::cerr << "moosecount: unrecognized option '" << arg << "'\n\n";
      printUsage(std::cerr);
      return 1;
    }
  }

  if (searchPaths.empty())
  {
    searchPaths.push_back(".");
  }

  CountTotals allTotals;
  std::map<std::string, long> extDisplayLines;

  struct FileResult
  {
    std::string path;
    long displayLines;
  };
  std::vector<FileResult> fileResults;

  // Execute File Traversal
  for (const auto &basePath : searchPaths)
  {
    if (!fs::exists(basePath))
    {
      std::cerr << "Path does not exist: " << basePath << '\n';
      continue;
    }

    if (fs::is_regular_file(basePath))
    {
      if (auto result = processFile(basePath))
      {
        long display = result->codeLines + result->formatLines;
        fileResults.push_back({basePath, display});
        extDisplayLines[fs::path(basePath).extension().string()] += display;
        allTotals.totalLines += result->totalLines;
        allTotals.blankLines += result->blankLines;
        allTotals.codeLines += result->codeLines;
        allTotals.formatLines += result->formatLines;
        allTotals.commentLines += result->commentLines;
        allTotals.fileCount++;
      }
      continue;
    }

    // Command line rules are anchored to the search root and outrank .gitignore
    std::vector<IgnoreFrame> frames;

    if (useGitignore)
    {
      IgnoreRules rootRules;
      if (loadIgnoreFile(fs::path(basePath) / ".gitignore", rootRules))
        frames.push_back({basePath, -1, std::move(rootRules)});
    }

    auto it = fs::recursive_directory_iterator(basePath, fs::directory_options::skip_permission_denied);
    auto end = fs::recursive_directory_iterator();

    while (it != end)
    {
      const auto &entry = *it;
      std::string filename = entry.path().filename().string();

      // Drop the rules of any subtree we have already walked back out of
      while (!frames.empty() && frames.back().depth >= it.depth())
        frames.pop_back();

      // Skip hidden folders (like .git) or user-defined excluded folders
      if (entry.is_directory())
      {
        if (filename[0] == '.' || isIgnored(ignoredItems, basePath, frames, entry.path(), filename, true))
        {
          it.disable_recursion_pending();
          ++it;
          continue;
        }

        // A .gitignore here governs this folder and everything beneath it
        if (useGitignore)
        {
          IgnoreRules nestedRules;
          if (loadIgnoreFile(entry.path() / ".gitignore", nestedRules))
            frames.push_back({entry.path(), it.depth(), std::move(nestedRules)});
        }
      }

      if (entry.is_regular_file() && filename[0] != '.')
      {
        if (targetExtensions.count(entry.path().extension().string()) &&
            !isIgnored(ignoredItems, basePath, frames, entry.path(), filename, false))
        {
          if (auto result = processFile(entry.path()))
          {
            long display = result->codeLines + result->formatLines;
            fileResults.push_back({entry.path().string(), display});
            extDisplayLines[entry.path().extension().string()] += display;
            allTotals.totalLines += result->totalLines;
            allTotals.blankLines += result->blankLines;
            allTotals.codeLines += result->codeLines;
            allTotals.formatLines += result->formatLines;
            allTotals.commentLines += result->commentLines;
            allTotals.fileCount++;
          }
        }
      }
      ++it;
    }
  }

  std::vector<std::string> searchRoots;
  for (const auto &basePath : searchPaths)
    searchRoots.push_back(normalizedPathString(basePath));

  ignoredItems.reportUnmatched("--exclude", searchRoots);

  if (sortByCount)
  {
    std::sort(fileResults.begin(), fileResults.end(),
              [](const FileResult &a, const FileResult &b)
              { return a.displayLines > b.displayLines; });
  }

  for (const auto &fr : fileResults)
  {
    std::cout << fr.displayLines << "\t" << fr.path << '\n';
  }

  // Display Totals
  std::cout << "\n\nTOTALS...\n\n";
  std::cout << "Lines of Code   = " << (allTotals.codeLines + allTotals.formatLines) << '\n';
  std::cout << "File Count      = " << allTotals.fileCount << '\n';
  std::cout << "Code Lines      = " << allTotals.codeLines << '\n';
  std::cout << "Format Lines    = " << allTotals.formatLines << '\n';
  std::cout << "Comment Lines   = " << allTotals.commentLines << '\n';
  std::cout << "Blank Lines     = " << allTotals.blankLines << '\n';
  std::cout << "Total Lines     = " << allTotals.totalLines << '\n';

  if (extDisplayLines.size() > 1)
  {
    std::cout << "\nBy Extension\n";
    for (const auto &[ext, lines] : extDisplayLines)
    {
      std::cout << "  " << ext << "\t= " << lines << '\n';
    }
  }

  return 0;
}
