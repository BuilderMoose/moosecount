/**
 * @file testReport.cpp
 *
 * JSON escaping, which paths exercise in ways ordinary text does not.
 */

#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "report.hpp"

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
class TestReportFixture : public ::testing::Test
{
public:
  TestReportFixture()
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
TEST_F(TestReportFixture, PlainTextIsUntouched)
{
  EXPECT_EQ("src/parser.cpp", jsonEscape("src/parser.cpp"));
  EXPECT_EQ("", jsonEscape(""));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestReportFixture, QuotesAndBackslashes)
{
  EXPECT_EQ("a\\\"b", jsonEscape("a\"b"));
  EXPECT_EQ("C:\\\\src", jsonEscape("C:\\src"));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestReportFixture, ControlCharacters)
{
  EXPECT_EQ("a\\nb", jsonEscape("a\nb"));
  EXPECT_EQ("a\\tb", jsonEscape("a\tb"));
  EXPECT_EQ("a\\rb", jsonEscape("a\rb"));

  // Anything else below 0x20 goes out as a \u escape
  EXPECT_EQ("a\\u0001b", jsonEscape(std::string("a\x01"
                                                "b")));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestReportFixture, TextRenderOfAnEmptyReport)
{
  CountReport report;
  std::ostringstream out;
  renderText(out, report);

  // No files, but the totals block is still printed
  EXPECT_NE(std::string::npos, out.str().find("File Count      = 0"));

  // The by-language block needs more than one language to be worth printing
  EXPECT_EQ(std::string::npos, out.str().find("By Language"));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestReportFixture, JsonRenderOfAnEmptyReport)
{
  CountReport report;
  std::ostringstream out;
  renderJson(out, report, "moosecount", "9.9.9", {});

  const std::string json = out.str();
  EXPECT_NE(std::string::npos, json.find("\"schemaVersion\": 1"));
  EXPECT_NE(std::string::npos, json.find("\"tool\": \"moosecount\""));
  EXPECT_NE(std::string::npos, json.find("\"version\": \"9.9.9\""));

  // Empty collections still have to be valid JSON
  EXPECT_NE(std::string::npos, json.find("\"languages\": []"));
  EXPECT_NE(std::string::npos, json.find("\"files\": []"));
  EXPECT_NE(std::string::npos, json.find("\"warnings\": []"));
}
