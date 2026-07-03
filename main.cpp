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
  std::unordered_set<std::string> ignoredItems;
  std::vector<std::string> searchPaths;
  bool sortByCount = false;

  std::unordered_set<std::string> targetExtensions = {
      ".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".m", ".mm",
      ".java", ".cs", ".js", ".ts", ".kt", ".swift", ".go", ".rs"};

  // Parse Command Line Arguments
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];
    if (arg == "--exclude" && i + 1 < argc)
    {
      ignoredItems.insert(argv[++i]);
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
    else if (arg == "--ignore-file" && i + 1 < argc)
    {
      std::ifstream ignoreFile(argv[++i]);
      if (ignoreFile.is_open())
      {
        std::string line;
        while (std::getline(ignoreFile, line))
        {
          std::string trimmed = trim(line);
          if (!trimmed.empty() && trimmed[0] != '#')
          {
            // Strip trailing slashes commonly found in gitignore
            if (trimmed.back() == '/')
              trimmed.pop_back();
            ignoredItems.insert(trimmed);
          }
        }
      }
      else
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
      std::cerr << "Usage: moosecount [--exclude <folder>] [--ignore-file <filename>] [--ext <extension>] [--no-defaults] [--sort] <path1> <path2> ...\n";
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

    auto it = fs::recursive_directory_iterator(basePath, fs::directory_options::skip_permission_denied);
    auto end = fs::recursive_directory_iterator();

    while (it != end)
    {
      const auto &entry = *it;
      std::string filename = entry.path().filename().string();

      // Skip hidden folders (like .git) or user-defined excluded folders
      if (entry.is_directory() && (filename[0] == '.' || ignoredItems.count(filename)))
      {
        it.disable_recursion_pending();
        ++it;
        continue;
      }

      if (entry.is_regular_file() && filename[0] != '.')
      {
        if (targetExtensions.count(entry.path().extension().string()))
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
