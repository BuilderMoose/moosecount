#include "parser.hpp"

#include <cctype>
#include <fstream>
#include <iostream>

namespace
{
const int FLAG_BLANK = 0;
const int FLAG_CODE = 1;
const int FLAG_FORMAT = 2;
const int FLAG_COMMENT = 4;

bool startsWith(const std::string &content, size_t at, const std::string &token)
{
  return !token.empty() && content.compare(at, token.size(), token) == 0;
}

// A Rust apostrophe opens a char literal only if it closes like one. Anything
// else is a lifetime, and treating it as a string is what used to swallow the
// rest of the file.
bool opensCharLiteral(const std::string &content, size_t at)
{
  const size_t limit = std::min(content.size(), at + 12);

  for (size_t scan = at + 1; scan < limit; ++scan)
  {
    if (content[scan] == '\\')
    {
      ++scan; // the escaped character cannot be the closing quote
      continue;
    }
    if (content[scan] == '\'')
      return true;
    if (content[scan] == '\n')
      break;
  }

  return false;
}

// Rust raw strings: r"...", r#"..."#, rb##"..."##. The terminator carries the
// same number of hashes the opener did.
bool opensHashRawString(const std::string &content, size_t at, std::string &closeToken, size_t &openLength)
{
  if (content[at] != 'r')
    return false;

  size_t scan = at + 1;
  size_t hashes = 0;
  while (scan < content.size() && content[scan] == '#')
  {
    ++hashes;
    ++scan;
  }

  if (scan >= content.size() || content[scan] != '"')
    return false;

  closeToken = "\"" + std::string(hashes, '#');
  openLength = (scan + 1) - at;

  return true;
}

// C++ raw strings: R"delim( ... )delim", the delimiter being whatever sits
// between the quote and the paren.
bool opensParenRawString(const std::string &content, size_t at, std::string &closeToken, size_t &openLength)
{
  if (content[at] != 'R' || at + 1 >= content.size() || content[at + 1] != '"')
    return false;

  size_t open = content.find('(', at + 2);
  if (open == std::string::npos)
    return false;

  std::string delimiter = content.substr(at + 2, open - (at + 2));
  closeToken = ")" + delimiter + "\"";
  openLength = (open + 1) - at;

  return true;
}
} // namespace

CountTotals countSource(const std::string &content, const LanguageSpec &language)
{
  CountTotals fileTotals;
  int lineFlag = FLAG_BLANK;

  enum class State
  {
    Normal,
    InString,
    InLineComment,
    InBlockComment
  };

  State state = State::Normal;
  std::string closeToken;   // what ends the current string or block comment
  char escapeChar = '\0';   // escape character of the current string
  bool stringMultiline = false;
  int commentDepth = 0;

  auto tallyLine = [&]()
  {
    fileTotals.totalLines++;
    if (lineFlag & FLAG_CODE)
      fileTotals.codeLines++;
    if (lineFlag & FLAG_COMMENT)
      fileTotals.commentLines++;

    // A format line is exclusively formatting characters, with no code or comments
    if ((lineFlag & FLAG_FORMAT) && !(lineFlag & FLAG_CODE) && !(lineFlag & FLAG_COMMENT))
    {
      fileTotals.formatLines++;
    }

    if (lineFlag == FLAG_BLANK)
      fileTotals.blankLines++;

    lineFlag = FLAG_BLANK;

    if (state == State::InLineComment)
      state = State::Normal;

    // An unterminated single-line string ends with its line rather than
    // running on and swallowing everything after it
    if (state == State::InString && !stringMultiline)
      state = State::Normal;
  };

  for (size_t i = 0; i < content.size(); ++i)
  {
    const char c = content[i];

    if (c == '\n')
    {
      tallyLine();
      continue;
    }

    if (std::isspace(static_cast<unsigned char>(c)))
      continue;

    switch (state)
    {
    case State::Normal:
    {
      bool consumed = false;

      for (const auto &token : language.lineComments)
      {
        if (startsWith(content, i, token))
        {
          state = State::InLineComment;
          lineFlag |= FLAG_COMMENT;
          i += token.size() - 1;
          consumed = true;
          break;
        }
      }
      if (consumed)
        break;

      for (const auto &block : language.blockComments)
      {
        if (startsWith(content, i, block.open))
        {
          state = State::InBlockComment;
          closeToken = block.close;
          commentDepth = 1;
          lineFlag |= FLAG_COMMENT;
          i += block.open.size() - 1;
          consumed = true;
          break;
        }
      }
      if (consumed)
        break;

      if (language.quirks.hashRawStrings || language.quirks.parenRawStrings)
      {
        size_t openLength = 0;
        std::string rawClose;
        const bool raw = (language.quirks.hashRawStrings && opensHashRawString(content, i, rawClose, openLength)) ||
                         (language.quirks.parenRawStrings && opensParenRawString(content, i, rawClose, openLength));
        if (raw)
        {
          state = State::InString;
          closeToken = rawClose;
          escapeChar = '\0';
          stringMultiline = true;
          lineFlag |= FLAG_CODE;
          i += openLength - 1;
          break;
        }
      }

      for (const auto &string : language.strings)
      {
        if (!startsWith(content, i, string.open))
          continue;

        // In Rust this apostrophe is far more likely to be a lifetime
        if (language.quirks.lifetimeApostrophe && string.open == "'" && !opensCharLiteral(content, i))
          continue;

        state = State::InString;
        closeToken = string.close;
        escapeChar = string.escape;
        stringMultiline = string.multiline;
        lineFlag |= FLAG_CODE;
        i += string.open.size() - 1;
        consumed = true;
        break;
      }
      if (consumed)
        break;

      if (language.formatChars.find(c) != std::string::npos)
      {
        if (!(lineFlag & FLAG_CODE))
          lineFlag |= FLAG_FORMAT;
      }
      else
      {
        lineFlag |= FLAG_CODE;
      }
      break;
    }

    case State::InString:
      lineFlag |= FLAG_CODE;
      if (escapeChar != '\0' && c == escapeChar)
      {
        i++; // skip the escaped character, which handles \\ correctly
      }
      else if (startsWith(content, i, closeToken))
      {
        i += closeToken.size() - 1;
        state = State::Normal;
      }
      break;

    case State::InLineComment:
      lineFlag |= FLAG_COMMENT;
      break;

    case State::InBlockComment:
    {
      lineFlag |= FLAG_COMMENT;

      if (language.blockCommentsNest)
      {
        bool opened = false;
        for (const auto &block : language.blockComments)
        {
          if (block.close == closeToken && startsWith(content, i, block.open))
          {
            commentDepth++;
            i += block.open.size() - 1;
            opened = true;
            break;
          }
        }
        if (opened)
          break;
      }

      if (startsWith(content, i, closeToken))
      {
        i += closeToken.size() - 1;
        if (--commentDepth <= 0)
          state = State::Normal;
      }
      break;
    }
    }
  }

  // Tally the final line if the file doesn't end with a newline
  if (!content.empty() && content.back() != '\n')
  {
    tallyLine();
  }

  return fileTotals;
}

std::optional<CountTotals> processFile(const fs::path &path, const LanguageSpec &language)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
  {
    std::cerr << "Error unable to open file: " << path << '\n';
    return std::nullopt;
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  return countSource(content, language);
}
