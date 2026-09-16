#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "counting.hpp"
#include "language.hpp"

namespace fs = std::filesystem;

// Counts source held in memory, according to one language's rules. This is
// what the unit tests drive; everything else is a wrapper around it.
CountTotals countSource(const std::string &content, const LanguageSpec &language);

// Counts one file. Returns nothing if the file could not be opened.
std::optional<CountTotals> processFile(const fs::path &path, const LanguageSpec &language);
