#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "document.hpp"
#include "ignore.hpp"
#include "options.hpp"
#include "report.hpp"
#include "walker.hpp"

namespace
{
const char *USAGE =
    "Usage: moosemetrics [options] <path> [path...]\n"
    "\n"
    "Extracts structural metrics from Markdown, Text and PlantUML files:\n"
    "words, headers, open and completed tasks, UML entities and their\n"
    "relationships. With no path given, searches the current directory.\n"
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

void renderDocumentText(std::ostream &out, const DocumentReport &report)
{
  out << std::string(50, ' ').replace(0, 4, "File") << " | Metrics\n";
  out << std::string(80, '-') << "\n";

  for (const auto &file : report.files)
  {
    std::string label = file.path;
    if (label.size() < 50)
      label.resize(50, ' ');

    if (file.kind == "PlantUML")
    {
      out << label << " | Entities: " << file.totals.umlEntities
          << " | Relationships: " << file.totals.relationships << '\n';
    }
    else
    {
      out << label << " | Words: " << file.totals.words << " | Tasks: "
          << file.totals.completedTasks << "/"
          << (file.totals.openTasks + file.totals.completedTasks) << '\n';
    }
  }

  const DocumentTotals &markdown = report.byKind.count("Markdown/Text")
                                       ? report.byKind.at("Markdown/Text")
                                       : DocumentTotals();
  const DocumentTotals &uml =
      report.byKind.count("PlantUML") ? report.byKind.at("PlantUML") : DocumentTotals();

  out << "\n" << std::string(80, '=') << "\n";
  out << "TOTALS\n";
  out << std::string(80, '=') << "\n";

  // Sub-totals are omitted for a kind with no files, as the script did
  out << "Markdown & Text Files: " << markdown.fileCount << '\n';
  if (markdown.fileCount > 0)
  {
    out << "  - Lines          : " << markdown.lines << '\n';
    out << "  - Words          : " << markdown.words << '\n';
    out << "  - Headers        : " << markdown.headers << '\n';
    out << "  - Open Tasks     : " << markdown.openTasks << '\n';
    out << "  - Completed Tasks: " << markdown.completedTasks << '\n';
  }

  out << "\nPlantUML Files: " << uml.fileCount << '\n';
  if (uml.fileCount > 0)
  {
    out << "  - Lines          : " << uml.lines << '\n';
    out << "  - UML Entities   : " << uml.umlEntities << '\n';
    out << "  - Relationships  : " << uml.relationships << '\n';
  }
}

void renderDocumentJson(std::ostream &out, const DocumentReport &report,
                        const std::vector<ReportWarning> &warnings)
{
  auto counts = [&out](const DocumentTotals &totals, const std::string &indent)
  {
    out << indent << "\"lines\": " << totals.lines << ",\n"
        << indent << "\"words\": " << totals.words << ",\n"
        << indent << "\"headers\": " << totals.headers << ",\n"
        << indent << "\"openTasks\": " << totals.openTasks << ",\n"
        << indent << "\"completedTasks\": " << totals.completedTasks << ",\n"
        << indent << "\"umlEntities\": " << totals.umlEntities << ",\n"
        << indent << "\"relationships\": " << totals.relationships;
  };

  out << "{\n";
  out << "  \"schemaVersion\": 1,\n";
  out << "  \"tool\": \"moosemetrics\",\n";
  out << "  \"version\": \"" << MOOSECOUNT_VERSION << "\",\n";

  out << "  \"totals\": {\n";
  out << "    \"files\": " << report.totals.fileCount << ",\n";
  counts(report.totals, "    ");
  out << "\n  },\n";

  out << "  \"kinds\": [";
  bool first = true;
  for (const auto &[kind, totals] : report.byKind)
  {
    out << (first ? "\n" : ",\n");
    first = false;
    out << "    {\n";
    out << "      \"name\": \"" << jsonEscape(kind) << "\",\n";
    out << "      \"files\": " << totals.fileCount << ",\n";
    counts(totals, "      ");
    out << "\n    }";
  }
  out << (first ? "],\n" : "\n  ],\n");

  out << "  \"files\": [";
  first = true;
  for (const auto &file : report.files)
  {
    out << (first ? "\n" : ",\n");
    first = false;
    out << "    {\n";
    out << "      \"path\": \"" << jsonEscape(file.path) << "\",\n";
    out << "      \"kind\": \"" << jsonEscape(file.kind) << "\",\n";
    counts(file.totals, "      ");
    out << "\n    }";
  }
  out << (first ? "],\n" : "\n  ],\n");

  out << "  \"warnings\": [";
  first = true;
  for (const auto &warning : warnings)
  {
    out << (first ? "\n" : ",\n");
    first = false;
    out << "    {\n";
    out << "      \"kind\": \"" << jsonEscape(warning.kind) << "\",\n";
    out << "      \"subject\": \"" << jsonEscape(warning.subject) << "\",\n";
    out << "      \"message\": \"" << jsonEscape(warning.message) << "\"\n";
    out << "    }";
  }
  out << (first ? "]\n" : "\n  ]\n");

  out << "}\n";
}
} // namespace

int main(int argc, char *argv[])
{
  CommonOptions options;
  for (const auto &extension : defaultDocumentExtensions())
    options.extensions.insert(extension);

  // The exclusions moosemetrics has always applied by default
  for (const char *name : {"build", "bin", ".git", ".vscode"})
    options.ignoreRules.add(name);

  ParseOutcome outcome = ParseOutcome::Run;
  if (!parseCommonOptions(argc, argv, "moosemetrics", USAGE, options, outcome))
    return 1;
  if (outcome == ParseOutcome::Finished)
    return 0;

  WalkOptions walkOptions;
  walkOptions.useGitignore = options.useGitignore;
  walkOptions.extensions = options.extensions;

  DocumentReport report =
      countDocumentPaths(options.searchPaths, options.ignoreRules, walkOptions);

  if (options.sortByCount)
  {
    std::sort(report.files.begin(), report.files.end(),
              [](const DocumentFileResult &a, const DocumentFileResult &b)
              { return a.totals.lines != b.totals.lines ? a.totals.lines > b.totals.lines
                                                        : a.path < b.path; });
  }
  else
  {
    std::sort(report.files.begin(), report.files.end(),
              [](const DocumentFileResult &a, const DocumentFileResult &b)
              { return a.path < b.path; });
  }

  std::vector<std::string> searchRoots;
  for (const auto &basePath : options.searchPaths)
    searchRoots.push_back(normalizedPathString(basePath));

  std::vector<ReportWarning> warnings;
  for (const auto &[rule, message] : options.ignoreRules.unmatchedRules("--exclude", searchRoots))
    warnings.push_back({"unmatchedRule", rule, message});

  for (const auto &warning : warnings)
    std::cerr << "Warning: " << warning.message << '\n';

  if (options.asJson)
    renderDocumentJson(std::cout, report, warnings);
  else
    renderDocumentText(std::cout, report);

  return 0;
}
