#include "walker.hpp"

#include <iostream>

#include "language.hpp"
#include "parser.hpp"

CountReport countPaths(const std::vector<std::string> &searchPaths, IgnoreRules &ignoreRules,
                       const CountOptions &options)
{
  CountReport report;
  CountTotals &allTotals = report.totals;
  std::map<std::string, long> &byLanguage = report.byLanguage;

  // An extension no spec claims is counted generically, and named once
  auto specFor = [&report](const fs::path &path) -> const LanguageSpec &
  {
    const std::string extension = path.extension().string();
    if (const LanguageSpec *found = languageForExtension(extension))
      return *found;

    report.unknownExtensions.insert(extension);
    return genericLanguage();
  };
  std::vector<FileResult> &fileResults = report.files;
  const bool useGitignore = options.useGitignore;
  const std::unordered_set<std::string> &targetExtensions = options.extensions;

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
      const LanguageSpec &language = specFor(basePath);
      if (auto result = processFile(basePath, language))
      {
        long display = result->codeLines + result->formatLines;
        fileResults.push_back({basePath, display});
        byLanguage[language.name] += display;
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
        if (filename[0] == '.' || isIgnored(ignoreRules, basePath, frames, entry.path(), filename, true))
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
            !isIgnored(ignoreRules, basePath, frames, entry.path(), filename, false))
        {
          const LanguageSpec &language = specFor(entry.path());
          if (auto result = processFile(entry.path(), language))
          {
            long display = result->codeLines + result->formatLines;
            fileResults.push_back({entry.path().string(), display});
            byLanguage[language.name] += display;
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


  return report;
}
