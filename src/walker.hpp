#pragma once

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <functional>

#include "counting.hpp"
#include "document.hpp"
#include "ignore.hpp"

struct FileResult
{
  std::string path;
  std::string language;
  CountTotals totals;

  // What the text output prints in its left column
  long displayLines() const
  {
    return totals.codeLines + totals.formatLines;
  }
};

// Per-language rollup. `linesOfCode` is the same code + format sum.
struct LanguageTotals
{
  long files = 0;
  CountTotals totals;

  long linesOfCode() const
  {
    return totals.codeLines + totals.formatLines;
  }
};

struct WalkOptions
{
  bool useGitignore = false;
  std::unordered_set<std::string> extensions;
};

using CountOptions = WalkOptions;

using FileVisitor = std::function<void(const fs::path &)>;

// Traversal, ignore rules and extension filtering, shared by both tools.
// Every surviving file is handed to `onFile`.
void walkFiles(const std::vector<std::string> &searchPaths, IgnoreRules &ignoreRules,
               const WalkOptions &options, const FileVisitor &onFile);

struct CountReport
{
  CountTotals totals;
  std::vector<FileResult> files;
  std::map<std::string, LanguageTotals> byLanguage;

  // Extensions that were scanned but that no language spec claims. They were
  // counted generically, and the caller reports them.
  std::set<std::string> unknownExtensions;
};

struct DocumentFileResult
{
  std::string path;
  std::string kind;
  DocumentTotals totals;
};

struct DocumentReport
{
  DocumentTotals totals;
  std::vector<DocumentFileResult> files;
  std::map<std::string, DocumentTotals> byKind;
};

// The document counterpart: same traversal, different metrics.
DocumentReport countDocumentPaths(const std::vector<std::string> &searchPaths,
                                  IgnoreRules &ignoreRules, const WalkOptions &options);

// Walks every search path, counting what the ignore rules leave behind.
// `ignoreRules` is non-const because it records which of its rules matched.
CountReport countPaths(const std::vector<std::string> &searchPaths, IgnoreRules &ignoreRules,
                       const CountOptions &options);
