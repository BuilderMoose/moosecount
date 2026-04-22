#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>
#include <cctype>

namespace fs = std::filesystem;

struct CountTotals {
    long totalLines = 0;
    long blankLines = 0;
    long codeLines = 0;
    long formatLines = 0;
    long commentLines = 0;
    long fileCount = 0;
};

enum class ParserState { Normal, InString, InSingleComment, InMultiComment };

const int FLAG_BLANK   = 0;
const int FLAG_CODE    = 1;
const int FLAG_FORMAT  = 2;
const int FLAG_COMMENT = 4;

// Trims whitespace from both ends of a string (useful for parsing ignore files)
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool processFile(const fs::path& path, CountTotals& allTotals) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error unable to open file: " << path << '\n';
        return false;
    }

    // Load entire file into memory for fast processing
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    CountTotals fileTotals;
    ParserState state = ParserState::Normal;
    int lineFlag = FLAG_BLANK;

    auto tallyLine = [&]() {
        fileTotals.totalLines++;
        if (lineFlag & FLAG_CODE) fileTotals.codeLines++;
        if (lineFlag & FLAG_COMMENT) fileTotals.commentLines++;
        
        // A format line is exclusively formatting brackets, with no code or comments
        if ((lineFlag & FLAG_FORMAT) && !(lineFlag & FLAG_CODE) && !(lineFlag & FLAG_COMMENT)) {
            fileTotals.formatLines++;
        }
        
        if (lineFlag == FLAG_BLANK) fileTotals.blankLines++;
        
        // Reset for the next line
        lineFlag = FLAG_BLANK;
        if (state == ParserState::InSingleComment) state = ParserState::Normal;
    };

    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        char next_c = (i + 1 < content.size()) ? content[i+1] : '\0';

        if (c == '\n') {
            tallyLine();
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c))) continue;

        switch (state) {
            case ParserState::Normal:
                if (c == '/' && next_c == '/') {
                    state = ParserState::InSingleComment;
                    lineFlag |= FLAG_COMMENT;
                    i++; // Skip next char
                } else if (c == '/' && next_c == '*') {
                    state = ParserState::InMultiComment;
                    lineFlag |= FLAG_COMMENT;
                    i++; // Skip next char
                } else if (c == '"') {
                    state = ParserState::InString;
                    lineFlag |= FLAG_CODE;
                } else if (c == '{' || c == '}') {
                    if (!(lineFlag & FLAG_CODE)) lineFlag |= FLAG_FORMAT;
                } else {
                    lineFlag |= FLAG_CODE;
                }
                break;

            case ParserState::InString:
                lineFlag |= FLAG_CODE;
                // Check for end of string, ignoring escaped quotes
                if (c == '"' && content[i-1] != '\\') {
                    state = ParserState::Normal;
                }
                break;

            case ParserState::InSingleComment:
                lineFlag |= FLAG_COMMENT;
                break;

            case ParserState::InMultiComment:
                lineFlag |= FLAG_COMMENT;
                if (c == '*' && next_c == '/') {
                    state = ParserState::Normal;
                    i++; // Skip next char
                }
                break;
        }
    }

    // Tally the final line if the file doesn't end with a newline
    if (!content.empty() && content.back() != '\n') {
        tallyLine();
    }

    std::cout << (fileTotals.codeLines + fileTotals.formatLines) << "\t" << path.string() << '\n';

    allTotals.totalLines += fileTotals.totalLines;
    allTotals.blankLines += fileTotals.blankLines;
    allTotals.codeLines += fileTotals.codeLines;
    allTotals.formatLines += fileTotals.formatLines;
    allTotals.commentLines += fileTotals.commentLines;
    allTotals.fileCount++;

    return true;
}

int main(int argc, char* argv[]) {
    std::unordered_set<std::string> ignoredItems;
    std::vector<std::string> searchPaths;
    
    // Default extensions
    std::unordered_set<std::string> targetExtensions = {
        ".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".m", ".mm"
    };

    // Parse Command Line Arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--exclude" && i + 1 < argc) {
            ignoredItems.insert(argv[++i]);
        } else if (arg == "--ignore-file" && i + 1 < argc) {
            std::ifstream ignoreFile(argv[++i]);
            if (ignoreFile.is_open()) {
                std::string line;
                while (std::getline(ignoreFile, line)) {
                    std::string trimmed = trim(line);
                    if (!trimmed.empty() && trimmed[0] != '#') {
                        // Strip trailing slashes commonly found in gitignore
                        if (trimmed.back() == '/') trimmed.pop_back();
                        ignoredItems.insert(trimmed);
                    }
                }
            } else {
                std::cerr << "Warning: Could not open ignore file: " << argv[i] << '\n';
            }
        } else if (arg[0] != '-') {
            searchPaths.push_back(arg);
        } else {
            std::cerr << "Usage: codecount [--exclude <folder>] [--ignore-file <filename>] <path1> <path2> ...\n";
            return 1;
        }
    }

    if (searchPaths.empty()) {
        searchPaths.push_back("."); // Default to current directory
    }

    CountTotals allTotals;

    // Execute File Traversal
    for (const auto& basePath : searchPaths) {
        if (!fs::exists(basePath)) {
            std::cerr << "Path does not exist: " << basePath << '\n';
            continue;
        }

        if (fs::is_regular_file(basePath)) {
            processFile(basePath, allTotals);
            continue;
        }

        auto it = fs::recursive_directory_iterator(basePath, fs::directory_options::skip_permission_denied);
        auto end = fs::recursive_directory_iterator();

        while (it != end) {
            const auto& entry = *it;
            std::string filename = entry.path().filename().string();

            // Skip hidden folders (like .git) or user-defined excluded folders
            if (entry.is_directory() && (filename[0] == '.' || ignoredItems.count(filename))) {
                it.disable_recursion_pending();
                ++it;
                continue;
            }

            // Process matching files
            if (entry.is_regular_file() && filename[0] != '.') {
                if (targetExtensions.count(entry.path().extension().string())) {
                    processFile(entry.path(), allTotals);
                }
            }
            ++it;
        }
    }

    // Display Totals
    std::cout << "\n\nTOTALS...\n\n";
    std::cout << "Lines of Code   = " << (allTotals.codeLines + allTotals.formatLines) << '\n';
    std::cout << "File Count      = " << allTotals.fileCount << '\n';
    std::cout << "Code Lines      = " << allTotals.codeLines << '\n';
    std::cout << "Format Lines    = " << allTotals.formatLines << '\n';
    std::cout << "Comment Lines   = " << allTotals.commentLines << '\n';
    std::cout << "Blank Lines     = " << allTotals.blankLines << '\n';
    std::cout << "Total Lines     = " << allTotals.totalLines << '\n';

    return 0;
}