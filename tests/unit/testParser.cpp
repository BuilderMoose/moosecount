/**
 * @file testParser.cpp
 *
 * Line classification, driven straight from source held in memory.
 */

#include <gtest/gtest.h>
#include <string>

#include "language.hpp"
#include "parser.hpp"

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
class TestParserFixture : public ::testing::Test
{
public:
  // The C family spec, which is what these cases are written against
  static const LanguageSpec &cpp()
  {
    return *languageForExtension(".cpp");
  }

  TestParserFixture()
  {
    /* Empty */
  }

  void SetUp(void)
  {
    /* Empty */
  }

  void TearDown(void)
  {
    /* Empty */
  }
};

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, BlankLines)
{
  CountTotals totals = countSource("\n\n\n", cpp());

  EXPECT_EQ(3, totals.totalLines);
  EXPECT_EQ(3, totals.blankLines);
  EXPECT_EQ(0, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, WhitespaceOnlyCountsAsBlank)
{
  CountTotals totals = countSource("   \n\t\n", cpp());

  EXPECT_EQ(2, totals.totalLines);
  EXPECT_EQ(2, totals.blankLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, FormatLinesAreBracesAlone)
{
  CountTotals totals = countSource("int main()\n{\n  return 0;\n}\n", cpp());

  EXPECT_EQ(4, totals.totalLines);
  EXPECT_EQ(2, totals.codeLines);
  EXPECT_EQ(2, totals.formatLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, ABraceSharingALineWithCodeIsNotFormat)
{
  CountTotals totals = countSource("int main() {\n  return 0;\n}\n", cpp());

  EXPECT_EQ(2, totals.codeLines);
  EXPECT_EQ(1, totals.formatLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, LineComments)
{
  CountTotals totals = countSource("// a comment\nint x = 1; // trailing\n", cpp());

  EXPECT_EQ(2, totals.commentLines);

  // The second line is both code and comment
  EXPECT_EQ(1, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, BlockComments)
{
  CountTotals totals = countSource("/*\n * spanning\n */\nint x = 1;\n", cpp());

  EXPECT_EQ(3, totals.commentLines);
  EXPECT_EQ(1, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, CommentMarkersInsideAStringAreNotComments)
{
  CountTotals totals = countSource("puts(\"/* not a comment */ // nor this\");\n", cpp());

  EXPECT_EQ(1, totals.codeLines);
  EXPECT_EQ(0, totals.commentLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, QuotesInsideACommentDoNotOpenAString)
{
  CountTotals totals = countSource("// it's fine\nint x = 1;\n", cpp());

  EXPECT_EQ(1, totals.commentLines);
  EXPECT_EQ(1, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, EscapedQuoteDoesNotEndTheString)
{
  CountTotals totals = countSource("puts(\"a \\\" b\"); // after\n", cpp());

  EXPECT_EQ(1, totals.codeLines);
  EXPECT_EQ(1, totals.commentLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, LastLineWithoutANewlineStillCounts)
{
  CountTotals totals = countSource("int x = 1;", cpp());

  EXPECT_EQ(1, totals.totalLines);
  EXPECT_EQ(1, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestParserFixture, EmptyInput)
{
  CountTotals totals = countSource("", cpp());

  EXPECT_EQ(0, totals.totalLines);
  EXPECT_EQ(0, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
// A Rust lifetime used to open a string that never closed, swallowing the
// rest of the file. This is the defect that motivated 0.3.0.
TEST_F(TestParserFixture, RustLifetimeIsNotAString)
{
  const LanguageSpec &rust = *languageForExtension(".rs");

  CountTotals totals = countSource(
      "fn main() {\n"
      "    let s: &'static str = \"hello\";\n"
      "}\n"
      "\n"
      "// a comment\n",
      rust);

  EXPECT_EQ(5, totals.totalLines);
  EXPECT_EQ(2, totals.codeLines);
  EXPECT_EQ(1, totals.formatLines);
  EXPECT_EQ(1, totals.commentLines);
  EXPECT_EQ(1, totals.blankLines);
}
