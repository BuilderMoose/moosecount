#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Documents are counted differently from code: what matters is how much has
// been written and how much of it is done, not how much is comment.
struct DocumentTotals
{
  long lines = 0;
  long words = 0;
  long headers = 0;
  long openTasks = 0;
  long completedTasks = 0;
  long umlEntities = 0;
  long relationships = 0;
  long fileCount = 0;
};

enum class DocumentKind
{
  Markdown,
  PlantUml
};

struct DocumentSpec
{
  std::string name;
  DocumentKind kind;
  std::vector<std::string> extensions;
};

const std::vector<DocumentSpec> &allDocumentSpecs();
const DocumentSpec *documentSpecForExtension(const std::string &extension);
std::vector<std::string> defaultDocumentExtensions();

// Counts a document held in memory. This is what the unit tests drive.
DocumentTotals countDocument(const std::string &content, DocumentKind kind);

DocumentTotals processDocument(const fs::path &path, DocumentKind kind, bool &opened);
