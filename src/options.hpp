#pragma once

#include <iosfwd>
#include <string>
#include <unordered_set>
#include <vector>

#include "ignore.hpp"

// Supplied by CMake; the fallback keeps a direct compile working
#ifndef MOOSECOUNT_VERSION
#define MOOSECOUNT_VERSION "unknown"
#endif

// Flags both tools share. Parsing them once keeps moosecount and moosemetrics
// from drifting apart the way their --exclude handling already had.
struct CommonOptions
{
  IgnoreRules ignoreRules;
  std::vector<std::string> searchPaths;
  std::unordered_set<std::string> extensions;
  bool useGitignore = false;
  bool sortByCount = false;
  bool asJson = false;
};

enum class ParseOutcome
{
  Run,     // carry on
  Finished // --help or --version was handled; exit 0
};

// Parses the shared flags. `usageBody` is the tool-specific description and
// option list; this adds the ignore rule reference every tool shares.
// Returns false on an unrecognized flag, having printed usage to stderr.
bool parseCommonOptions(int argc, char *argv[], const std::string &toolName,
                        const std::string &usageBody, CommonOptions &options,
                        ParseOutcome &outcome);

void printIgnoreRuleHelp(std::ostream &out);
