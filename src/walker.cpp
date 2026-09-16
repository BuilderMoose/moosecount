#include "walker.hpp"

#include <iostream>

#include "language.hpp"
#include "parser.hpp"

void walkFiles(const std::vector<std::string> &searchPaths, IgnoreRules &ignoreRules,
               const WalkOptions &options, const FileVisitor &onFile)
{
  const bool useGitignore = options.useGitignore;
  const std::unordered_set<std::string> &targetExtensions = options.extensions;

  for (const auto &basePath : searchPaths)
  {
    if (!fs::exists(basePath))
    {
      std::cerr << "Path does not exist: " << basePath << '\n';
      continue;
    }

    if (fs::is_regular_file(basePath))
    {
      onFile(basePath);
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
          onFile(entry.path());
        }
      }
      ++it;
    }
  }

}

#include "walker.hpp"

#include <iostream>

#include "language.hpp"
#include "parser.hpp"

CountReport countPaths(const std::vector<std::string> &searchPaths, IgnoreRules &ignoreRules,
                       const CountOptions &options)
{
  CountReport report;

  // An extension no spec claims is counted generically, and named once
  auto accumulate = [&](const std::string &path, const LanguageSpec &language, const CountTotals &counts)
  {
    report.files.push_back({path, language.name, counts});

    LanguageTotals &rollup = report.byLanguage[language.name];
    rollup.files++;
    rollup.totals.totalLines += counts.totalLines;
    rollup.totals.blankLines += counts.blankLines;
    rollup.totals.codeLines += counts.codeLines;
    rollup.totals.formatLines += counts.formatLines;
    rollup.totals.commentLines += counts.commentLines;

    report.totals.totalLines += counts.totalLines;
    report.totals.blankLines += counts.blankLines;
    report.totals.codeLines += counts.codeLines;
    report.totals.formatLines += counts.formatLines;
    report.totals.commentLines += counts.commentLines;
    report.totals.fileCount++;
  };

  auto specFor = [&report](const fs::path &path) -> const LanguageSpec &
  {
    const std::string extension = path.extension().string();
    if (const LanguageSpec *found = languageForExtension(extension))
      return *found;

    report.unknownExtensions.insert(extension);
    return genericLanguage();
  };

  WalkOptions walkOptions;
  walkOptions.useGitignore = options.useGitignore;
  walkOptions.extensions = options.extensions;

  walkFiles(searchPaths, ignoreRules, walkOptions,
            [&](const fs::path &path)
            {
              const LanguageSpec &language = specFor(path);
              if (auto result = processFile(path, language))
                accumulate(path.string(), language, *result);
            });

  return report;
}

namespace
{
void addDocument(DocumentTotals &into, const DocumentTotals &from)
{
  into.lines += from.lines;
  into.words += from.words;
  into.headers += from.headers;
  into.openTasks += from.openTasks;
  into.completedTasks += from.completedTasks;
  into.umlEntities += from.umlEntities;
  into.relationships += from.relationships;
  into.fileCount += from.fileCount;
}
} // namespace

DocumentReport countDocumentPaths(const std::vector<std::string> &searchPaths,
                                  IgnoreRules &ignoreRules, const WalkOptions &options)
{
  DocumentReport report;

  walkFiles(searchPaths, ignoreRules, options,
            [&](const fs::path &path)
            {
              const DocumentSpec *spec = documentSpecForExtension(path.extension().string());
              if (spec == nullptr)
                return;

              bool opened = false;
              DocumentTotals counts = processDocument(path, spec->kind, opened);
              if (!opened)
                return;

              report.files.push_back({path.string(), spec->name, counts});
              addDocument(report.byKind[spec->name], counts);
              addDocument(report.totals, counts);
            });

  return report;
}
