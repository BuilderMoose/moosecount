#include "report.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>

std::string jsonEscape(const std::string &text)
{
  std::string escaped;
  escaped.reserve(text.size() + 8);

  for (const char c : text)
  {
    switch (c)
    {
    case '"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '\b':
      escaped += "\\b";
      break;
    case '\f':
      escaped += "\\f";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20)
      {
        std::ostringstream unicode;
        unicode << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<int>(static_cast<unsigned char>(c));
        escaped += unicode.str();
      }
      else
      {
        escaped += c;
      }
      break;
    }
  }

  return escaped;
}

void renderText(std::ostream &out, const CountReport &report)
{
  for (const auto &file : report.files)
  {
    out << file.displayLines() << "\t" << file.path << '\n';
  }

  const CountTotals &totals = report.totals;

  out << "\n\nTOTALS...\n\n";
  out << "Lines of Code   = " << (totals.codeLines + totals.formatLines) << '\n';
  out << "File Count      = " << totals.fileCount << '\n';
  out << "Code Lines      = " << totals.codeLines << '\n';
  out << "Format Lines    = " << totals.formatLines << '\n';
  out << "Comment Lines   = " << totals.commentLines << '\n';
  out << "Blank Lines     = " << totals.blankLines << '\n';
  out << "Total Lines     = " << totals.totalLines << '\n';

  if (report.byLanguage.size() > 1)
  {
    out << "\nBy Language\n";
    for (const auto &[language, rollup] : report.byLanguage)
    {
      out << "  " << language << "\t= " << rollup.linesOfCode() << '\n';
    }
  }
}

namespace
{
void writeCounts(std::ostream &out, const CountTotals &totals, const std::string &indent)
{
  out << indent << "\"linesOfCode\": " << (totals.codeLines + totals.formatLines) << ",\n"
      << indent << "\"code\": " << totals.codeLines << ",\n"
      << indent << "\"format\": " << totals.formatLines << ",\n"
      << indent << "\"comment\": " << totals.commentLines << ",\n"
      << indent << "\"blank\": " << totals.blankLines << ",\n"
      << indent << "\"total\": " << totals.totalLines;
}
} // namespace

void renderJson(std::ostream &out, const CountReport &report,
                const std::string &toolName, const std::string &version,
                const std::vector<ReportWarning> &warnings)
{
  out << "{\n";
  out << "  \"schemaVersion\": 1,\n";
  out << "  \"tool\": \"" << jsonEscape(toolName) << "\",\n";
  out << "  \"version\": \"" << jsonEscape(version) << "\",\n";

  out << "  \"totals\": {\n";
  out << "    \"files\": " << report.totals.fileCount << ",\n";
  writeCounts(out, report.totals, "    ");
  out << "\n  },\n";

  out << "  \"languages\": [";
  bool first = true;
  for (const auto &[language, rollup] : report.byLanguage)
  {
    out << (first ? "\n" : ",\n");
    first = false;
    out << "    {\n";
    out << "      \"name\": \"" << jsonEscape(language) << "\",\n";
    out << "      \"files\": " << rollup.files << ",\n";
    writeCounts(out, rollup.totals, "      ");
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
    out << "      \"language\": \"" << jsonEscape(file.language) << "\",\n";
    writeCounts(out, file.totals, "      ");
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
