#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "ignore.hpp"
#include "language.hpp"
#include "options.hpp"
#include "report.hpp"
#include "walker.hpp"

namespace
{
const char *USAGE =
    "Usage: moosecount [options] <path> [path...]\n"
    "\n"
    "Counts lines of source, separating code, formatting braces, comments\n"
    "and blanks, according to each language's own rules. With no path given,\n"
    "searches the current directory.\n"
    "\n"
    "Options:\n"
    "  --exclude <rule>       Skip files or folders matching a rule.\n"
    "                         May be given more than once.\n"
    "  --ignore-file <file>   Apply the rules in one gitignore-style file.\n"
    "  --gitignore            Discover and honor every .gitignore while\n"
    "                         walking, the way git does.\n"
    "  --ext <extension>      Add an extension to those scanned. The\n"
    "                         leading dot is optional.\n"
    "  --no-defaults          Scan only the extensions added with --ext.\n"
    "  --sort                 List files by line count, largest first.\n"
    "  --json                 Emit the report as JSON instead of a table.\n"
    "  -h, --help             Show this help and exit.\n"
    "  -v, --version          Show the version and exit.\n";
} // namespace

int main(int argc, char *argv[])
{
  CommonOptions options;
  options.extensions = defaultExtensions();

  ParseOutcome outcome = ParseOutcome::Run;
  if (!parseCommonOptions(argc, argv, "moosecount", USAGE, options, outcome))
    return 1;
  if (outcome == ParseOutcome::Finished)
    return 0;

  CountOptions countOptions;
  countOptions.useGitignore = options.useGitignore;
  countOptions.extensions = options.extensions;

  CountReport report = countPaths(options.searchPaths, options.ignoreRules, countOptions);

  // Traversal order is whatever the filesystem hands back, which the standard
  // leaves unspecified, so two machines can disagree. Sorting unconditionally
  // makes one run's output diffable against another's.
  if (options.sortByCount)
  {
    std::sort(report.files.begin(), report.files.end(),
              [](const FileResult &a, const FileResult &b)
              { return a.displayLines() != b.displayLines() ? a.displayLines() > b.displayLines()
                                                            : a.path < b.path; });
  }
  else
  {
    std::sort(report.files.begin(), report.files.end(),
              [](const FileResult &a, const FileResult &b)
              { return a.path < b.path; });
  }

  std::vector<std::string> searchRoots;
  for (const auto &basePath : options.searchPaths)
    searchRoots.push_back(normalizedPathString(basePath));

  std::vector<ReportWarning> warnings;
  for (const auto &[rule, message] : options.ignoreRules.unmatchedRules("--exclude", searchRoots))
  {
    warnings.push_back({"unmatchedRule", rule, message});
  }
  for (const auto &extension : report.unknownExtensions)
  {
    warnings.push_back({"noLanguageSupport", extension,
                        "no language support for '" + extension +
                            "', counted as blank versus non-blank only"});
  }

  // Warnings go to stderr whatever the format, so a terminal user sees them
  // even when stdout is being captured
  for (const auto &warning : warnings)
    std::cerr << "Warning: " << warning.message << '\n';

  if (options.asJson)
    renderJson(std::cout, report, "moosecount", MOOSECOUNT_VERSION, warnings);
  else
    renderText(std::cout, report);

  return 0;
}
