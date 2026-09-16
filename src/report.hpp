#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "walker.hpp"

// A warning worth carrying into the output rather than only onto stderr: a
// consumer capturing stdout alone would otherwise never see it.
struct ReportWarning
{
  std::string kind;
  std::string subject;
  std::string message;
};

// The human-readable table, unchanged from what the tool has always printed.
void renderText(std::ostream &out, const CountReport &report);

// The same report as JSON. `toolName` and `version` head the document so one
// consumer can tell moosecount and moosemetrics apart.
void renderJson(std::ostream &out, const CountReport &report,
                const std::string &toolName, const std::string &version,
                const std::vector<ReportWarning> &warnings);

// Exposed for its own tests: paths carry quotes and backslashes on some
// platforms, and control characters have to survive the trip.
std::string jsonEscape(const std::string &text);
