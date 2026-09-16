/**
 * @file testGlobMatch.cpp
 *
 * The wildcard matcher behind every ignore rule. Until now it was only
 * reachable by running the binary against a fixture tree.
 */

#include <gtest/gtest.h>
#include <string>

#include "ignore.hpp"

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
class TestGlobMatchFixture : public ::testing::Test
{
public:
  TestGlobMatchFixture()
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
TEST_F(TestGlobMatchFixture, LiteralNames)
{
  EXPECT_TRUE(matchesGlob("build", "build", false));
  EXPECT_FALSE(matchesGlob("build", "builds", false));
  EXPECT_FALSE(matchesGlob("build", "prebuild", false));
  EXPECT_FALSE(matchesGlob("build", "", false));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestGlobMatchFixture, StarMatchesAnyRun)
{
  EXPECT_TRUE(matchesGlob("*build*", "build", false));
  EXPECT_TRUE(matchesGlob("*build*", "prebuild_out", false));
  EXPECT_TRUE(matchesGlob("*build*", "build_x64", false));
  EXPECT_FALSE(matchesGlob("*build*", "bulid", false));

  // A bare star takes everything, including nothing at all
  EXPECT_TRUE(matchesGlob("*", "anything", false));
  EXPECT_TRUE(matchesGlob("*", "", false));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestGlobMatchFixture, QuestionMarkMatchesExactlyOne)
{
  EXPECT_TRUE(matchesGlob("build_?", "build_1", false));
  EXPECT_TRUE(matchesGlob("build_?", "build_x", false));
  EXPECT_FALSE(matchesGlob("build_?", "build_", false));
  EXPECT_FALSE(matchesGlob("build_?", "build_xx", false));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestGlobMatchFixture, PathModeStopsAtSeparators)
{
  // A single star stays inside one path component
  EXPECT_TRUE(matchesGlob("src/*", "src/generated", true));
  EXPECT_FALSE(matchesGlob("src/*", "src/deep/generated", true));

  // Outside path mode there are no components to respect
  EXPECT_TRUE(matchesGlob("src/*", "src/deep/generated", false));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestGlobMatchFixture, DoubleStarSpansDirectories)
{
  EXPECT_TRUE(matchesGlob("**/temp_out", "deep/nest/temp_out", true));
  EXPECT_TRUE(matchesGlob("**/temp_out", "one/temp_out", true));

  // A leading "**/" may also match zero directories
  EXPECT_TRUE(matchesGlob("**/temp_out", "temp_out", true));

  // And in the middle of a pattern
  EXPECT_TRUE(matchesGlob("a/**/b", "a/x/y/b", true));
  EXPECT_TRUE(matchesGlob("a/**/b", "a/b", true));
  EXPECT_FALSE(matchesGlob("a/**/b", "a/x/y/c", true));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestGlobMatchFixture, TrailingStarsAcceptAnEmptyRemainder)
{
  EXPECT_TRUE(matchesGlob("build*", "build", false));
  EXPECT_TRUE(matchesGlob("build**", "build", false));
}
