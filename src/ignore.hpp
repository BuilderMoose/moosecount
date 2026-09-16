#pragma once

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Wildcard match supporting '*' (any run of characters) and '?' (one character).
// In pathMode, '*' and '?' stop at a '/' boundary and only '**' spans directories.
bool globMatch(const char *pattern, const char *text, bool pathMode);
bool matchesGlob(const std::string &pattern, const std::string &text, bool pathMode);

// Absolute, lexically normalized form of a path, with no trailing slash. Used
// for both sides of an absolute rule so the two are always comparable.
std::string normalizedPathString(const fs::path &path);
bool isInsideRoot(const std::string &candidate, const std::string &root);

enum class IgnoreVerdict
{
  None,   // no rule had an opinion
  Ignore, // skip this entry
  Include // a '!' rule put it back
};

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
bool loadIgnoreFile(const fs::path &path, IgnoreRules &rules);

// One set of rules plus the directory they are anchored to. Frames are stacked
// during traversal so a nested .gitignore only governs its own subtree.
struct IgnoreFrame
{
  fs::path base;
  int depth; // traversal depth of `base`; the search root sits at -1
  IgnoreRules rules;
};

IgnoreVerdict evaluateRules(const IgnoreRules &rules, const fs::path &base, const fs::path &path,
                            const std::string &name, bool isDirectory);

// `commandLine` is taken by reference on purpose: it records which of its rules
// matched, and that has to survive across every search path.
bool isIgnored(const IgnoreRules &commandLine, const fs::path &commandLineBase,
               const std::vector<IgnoreFrame> &frames, const fs::path &path,
               const std::string &name, bool isDirectory);
