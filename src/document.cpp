#include "document.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
const std::vector<DocumentSpec> &buildSpecs()
{
  static const std::vector<DocumentSpec> specs = {
      {"Markdown/Text", DocumentKind::Markdown, {".md", ".txt"}},
      {"PlantUML", DocumentKind::PlantUml, {".puml", ".pu", ".wsd"}}};

  return specs;
}

std::string lowered(const std::string &text)
{
  std::string copy = text;
  for (char &c : copy)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  return copy;
}

size_t firstNonSpace(const std::string &line)
{
  size_t at = 0;
  while (at < line.size() && std::isspace(static_cast<unsigned char>(line[at])))
    ++at;

  return at;
}

// One to six '#' followed by whitespace, or by nothing at all: CommonMark
// allows an empty ATX heading, so a bare "#" counts.
bool isHeader(const std::string &line)
{
  size_t hashes = 0;
  while (hashes < line.size() && line[hashes] == '#')
    ++hashes;

  if (hashes == 0 || hashes > 6)
    return false;

  return hashes == line.size() || std::isspace(static_cast<unsigned char>(line[hashes])) != 0;
}

// "- [ ]" open, "- [x]" or "- [X]" completed, after any indentation
int taskState(const std::string &line)
{
  size_t at = firstNonSpace(line);
  if (at >= line.size() || line[at] != '-')
    return 0;

  ++at;
  while (at < line.size() && (line[at] == ' ' || line[at] == '\t'))
    ++at;

  if (at + 2 >= line.size() || line[at] != '[' || line[at + 2] != ']')
    return 0;

  const char mark = line[at + 1];
  if (mark == ' ')
    return 1;
  if (mark == 'x' || mark == 'X')
    return 2;

  return 0;
}

// class, component, actor, usecase, state, node or interface, at line start
bool isUmlEntity(const std::string &line)
{
  static const std::vector<std::string> keywords = {
      "class", "component", "actor", "usecase", "state", "node", "interface"};

  const size_t at = firstNonSpace(line);
  const std::string rest = lowered(line.substr(at));

  for (const auto &keyword : keywords)
  {
    if (rest.compare(0, keyword.size(), keyword) == 0 && rest.size() > keyword.size() &&
        std::isspace(static_cast<unsigned char>(rest[keyword.size()])))
    {
      return true;
    }
  }

  return false;
}

// Arrow forms: -> --> ..> and their mirrors <- <-- <..
long countRelationships(const std::string &line)
{
  long found = 0;

  for (size_t i = 0; i < line.size(); ++i)
  {
    if (line[i] == '>' && i > 0 && (line[i - 1] == '-' || line[i - 1] == '.'))
      found++;
    else if (line[i] == '<' && i + 1 < line.size() && (line[i + 1] == '-' || line[i + 1] == '.'))
      found++;
  }

  return found;
}

long countWords(const std::string &content)
{
  long words = 0;
  bool inWord = false;

  for (const char c : content)
  {
    if (std::isspace(static_cast<unsigned char>(c)))
    {
      inWord = false;
    }
    else if (!inWord)
    {
      inWord = true;
      words++;
    }
  }

  return words;
}
} // namespace

const std::vector<DocumentSpec> &allDocumentSpecs()
{
  return buildSpecs();
}

const DocumentSpec *documentSpecForExtension(const std::string &extension)
{
  for (const auto &spec : buildSpecs())
  {
    for (const auto &candidate : spec.extensions)
    {
      if (candidate == extension)
        return &spec;
    }
  }

  return nullptr;
}

std::vector<std::string> defaultDocumentExtensions()
{
  std::vector<std::string> extensions;
  for (const auto &spec : buildSpecs())
    extensions.insert(extensions.end(), spec.extensions.begin(), spec.extensions.end());

  return extensions;
}

DocumentTotals countDocument(const std::string &content, DocumentKind kind)
{
  DocumentTotals totals;
  totals.fileCount = 1;

  std::istringstream stream(content);
  std::string line;

  while (std::getline(stream, line))
  {
    totals.lines++;

    if (kind == DocumentKind::Markdown)
    {
      if (isHeader(line))
        totals.headers++;

      const int task = taskState(line);
      if (task == 1)
        totals.openTasks++;
      else if (task == 2)
        totals.completedTasks++;
    }
    else
    {
      if (isUmlEntity(line))
        totals.umlEntities++;

      totals.relationships += countRelationships(line);
    }
  }

  if (kind == DocumentKind::Markdown)
    totals.words = countWords(content);

  return totals;
}

DocumentTotals processDocument(const fs::path &path, DocumentKind kind, bool &opened)
{
  std::ifstream file(path, std::ios::binary);
  opened = file.is_open();
  if (!opened)
  {
    std::cerr << "Error unable to open file: " << path << '\n';
    return DocumentTotals();
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  return countDocument(content, kind);
}
