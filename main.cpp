#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "ignore.hpp"
#include "language.hpp"
#include "walker.hpp"

// Supplied by CMake; the fallback keeps a plain `c++ main.cpp` building
#ifndef MOOSECOUNT_VERSION
#define MOOSECOUNT_VERSION "unknown"
#endif

void printUsage(std::ostream &out)
{
  out << "Usage: moosecount [options] <path> [path...]\n"
      << "\n"
      << "Counts lines of source, separating code, formatting braces,\n"
      << "comments and blanks, according to each language's own rules.\n"
      << "With no path given, searches the current directory.\n"
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

int main(int argc, char *argv[])
{
  IgnoreRules ignoredItems;
  std::vector<std::string> searchPaths;
  bool sortByCount = false;
  bool useGitignore = false;

  std::unordered_set<std::string> targetExtensions = defaultExtensions();

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

  CountOptions options;
  options.useGitignore = useGitignore;
  options.extensions = targetExtensions;

  CountReport report = countPaths(searchPaths, ignoredItems, options);
  const CountTotals &allTotals = report.totals;

  std::vector<std::string> searchRoots;
  for (const auto &basePath : searchPaths)
    searchRoots.push_back(normalizedPathString(basePath));

  ignoredItems.reportUnmatched("--exclude", searchRoots);

  for (const auto &extension : report.unknownExtensions)
  {
    std::cerr << "Warning: no language support for '" << extension
              << "', counted as blank versus non-blank only\n";
  }

  if (sortByCount)
  {
    std::sort(report.files.begin(), report.files.end(),
              [](const FileResult &a, const FileResult &b)
              { return a.displayLines > b.displayLines; });
  }

  for (const auto &fr : report.files)
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

  if (report.byLanguage.size() > 1)
  {
    std::cout << "\nBy Language\n";
    for (const auto &[language, lines] : report.byLanguage)
    {
      std::cout << "  " << language << "\t= " << lines << '\n';
    }
  }

  return 0;
}
