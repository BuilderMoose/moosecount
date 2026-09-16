#pragma once

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "counting.hpp"
#include "ignore.hpp"

struct FileResult
{
  std::string path;
  long displayLines;
};

struct CountOptions
{
  bool useGitignore = false;
  std::unordered_set<std::string> extensions;
};

struct CountReport
{
  CountTotals totals;
  std::vector<FileResult> files;
  std::map<std::string, long> byLanguage;

  // Extensions that were scanned but that no language spec claims. They were
  // counted generically, and the caller reports them.
  std::set<std::string> unknownExtensions;
};

// Walks every search path, counting what the ignore rules leave behind.
// `ignoreRules` is non-const because it records which of its rules matched.
CountReport countPaths(const std::vector<std::string> &searchPaths, IgnoreRules &ignoreRules,
                       const CountOptions &options);
