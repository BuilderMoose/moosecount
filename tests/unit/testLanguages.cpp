/**
 * @file testLanguages.cpp
 *
 * One case per language construct that a single C-family parser got wrong.
 */

#include <gtest/gtest.h>
#include <string>

#include "language.hpp"
#include "parser.hpp"

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
class TestLanguagesFixture : public ::testing::Test
{
public:
  TestLanguagesFixture()
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

  const LanguageSpec &spec(const std::string &extension)
  {
    const LanguageSpec *found = languageForExtension(extension);
    EXPECT_NE(nullptr, found) << "no spec claims " << extension;

    return found == nullptr ? genericLanguage() : *found;
  }
};

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, ExtensionsResolveToTheirLanguage)
{
  EXPECT_EQ("C/C++", spec(".cpp").name);
  EXPECT_EQ("C/C++", spec(".h").name);
  EXPECT_EQ("Rust", spec(".rs").name);
  EXPECT_EQ("Python", spec(".py").name);
  EXPECT_EQ("Go", spec(".go").name);

  EXPECT_EQ(nullptr, languageForExtension(".xyz"));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, PythonHashIsAComment)
{
  CountTotals totals = countSource("# a comment\ndef add(a, b):\n    return a + b\n", spec(".py"));

  EXPECT_EQ(3, totals.totalLines);
  EXPECT_EQ(1, totals.commentLines);
  EXPECT_EQ(2, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, PythonReportsNoFormatLines)
{
  // Structure is indentation, which occupies no lines of its own
  CountTotals totals = countSource("def add(a, b):\n    return a + b\n", spec(".py"));

  EXPECT_EQ(0, totals.formatLines);
  EXPECT_EQ(2, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, PythonTripleQuotesSpanLines)
{
  // Per D1 a docstring is code, not a comment
  CountTotals totals = countSource("x = \"\"\"\n# not a comment\n\"\"\"\n", spec(".py"));

  EXPECT_EQ(3, totals.totalLines);
  EXPECT_EQ(0, totals.commentLines);
  EXPECT_EQ(3, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, PythonApostropheInACommentIsHarmless)
{
  CountTotals totals = countSource("# it's fine\nx = 1\n", spec(".py"));

  EXPECT_EQ(1, totals.commentLines);
  EXPECT_EQ(1, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, RustCharLiteralIsStillAString)
{
  CountTotals totals = countSource("let c = '\\n';\nlet d = 'x';\n", spec(".rs"));

  EXPECT_EQ(2, totals.codeLines);
  EXPECT_EQ(0, totals.commentLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, RustLifetimesDoNotOpenStrings)
{
  CountTotals totals = countSource("struct Foo<'a> {\n"
                                   "    name: &'a str,\n"
                                   "}\n"
                                   "// still a comment\n",
                                   spec(".rs"));

  EXPECT_EQ(4, totals.totalLines);
  EXPECT_EQ(2, totals.codeLines);
  EXPECT_EQ(1, totals.formatLines);
  EXPECT_EQ(1, totals.commentLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, RustBlockCommentsNest)
{
  CountTotals totals = countSource("/* outer /* inner */ still a comment */\nlet x = 1;\n",
                                   spec(".rs"));

  EXPECT_EQ(1, totals.commentLines);
  EXPECT_EQ(1, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, CBlockCommentsDoNotNest)
{
  // The first */ closes it, so the tail is code
  CountTotals totals = countSource("/* outer /* inner */ x = 1;\n", spec(".cpp"));

  EXPECT_EQ(1, totals.commentLines);
  EXPECT_EQ(1, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, RustRawStringsCarryTheirHashCount)
{
  CountTotals totals = countSource("let s = r#\"a \" quote // not a comment\"#;\n", spec(".rs"));

  EXPECT_EQ(1, totals.codeLines);
  EXPECT_EQ(0, totals.commentLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, CppRawStringsUseTheirDelimiter)
{
  CountTotals totals = countSource("auto s = R\"json({\"a\": 1} // no comment)json\";\n", spec(".cpp"));

  EXPECT_EQ(1, totals.codeLines);
  EXPECT_EQ(0, totals.commentLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, GoBackticksSpanLines)
{
  CountTotals totals = countSource("s := `line one\n// not a comment\n`\n", spec(".go"));

  EXPECT_EQ(3, totals.totalLines);
  EXPECT_EQ(0, totals.commentLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, ShellHashIsAComment)
{
  CountTotals totals = countSource("#!/bin/sh\necho hi\n", spec(".sh"));

  EXPECT_EQ(1, totals.commentLines);
  EXPECT_EQ(1, totals.codeLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, UnterminatedStringEndsWithItsLine)
{
  // Rather than swallowing everything after it
  CountTotals totals = countSource("puts(\"oops;\n// still a comment\n", spec(".cpp"));

  EXPECT_EQ(1, totals.commentLines);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestLanguagesFixture, GenericSpecCountsBlankVersusNonBlank)
{
  CountTotals totals = countSource("# whatever\n\nanything at all\n", genericLanguage());

  EXPECT_EQ(3, totals.totalLines);
  EXPECT_EQ(1, totals.blankLines);
  EXPECT_EQ(2, totals.codeLines);
  EXPECT_EQ(0, totals.commentLines);
}
