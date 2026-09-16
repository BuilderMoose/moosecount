/**
 * @file testIgnoreRules.cpp
 *
 * Rule classification and the order-sensitive verdict logic. The fixture
 * trees in tests/ cover these end to end; this covers them directly.
 */

#include <gtest/gtest.h>
#include <string>

#include "ignore.hpp"

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
class TestIgnoreRulesFixture : public ::testing::Test
{
public:
  TestIgnoreRulesFixture()
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

  // Most rules care only about a bare name
  IgnoreVerdict judgeName(const IgnoreRules &rules, const std::string &name, bool isDirectory = true)
  {
    return rules.evaluate(name, "", "", isDirectory);
  }
};

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, BareNameMatchesAtAnyDepth)
{
  IgnoreRules rules;
  ASSERT_TRUE(rules.add("build"));

  EXPECT_EQ(IgnoreVerdict::Ignore, judgeName(rules, "build"));
  EXPECT_EQ(IgnoreVerdict::None, judgeName(rules, "src"));
  EXPECT_FALSE(rules.hasAnchoredRules());
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, TrailingSlashLimitsToDirectories)
{
  IgnoreRules rules;
  ASSERT_TRUE(rules.add("build/"));

  EXPECT_EQ(IgnoreVerdict::Ignore, judgeName(rules, "build", true));
  EXPECT_EQ(IgnoreVerdict::None, judgeName(rules, "build", false));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, FileRulesApplyToFiles)
{
  IgnoreRules rules;
  ASSERT_TRUE(rules.add("*.gen.cpp"));

  EXPECT_EQ(IgnoreVerdict::Ignore, judgeName(rules, "widget.gen.cpp", false));
  EXPECT_EQ(IgnoreVerdict::None, judgeName(rules, "widget.cpp", false));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, LeadingSlashAnchorsToTheSearchRoot)
{
  IgnoreRules rules;
  ASSERT_TRUE(rules.add("/build"));
  EXPECT_TRUE(rules.hasAnchoredRules());

  EXPECT_EQ(IgnoreVerdict::Ignore, rules.evaluate("build", "build", "", true));
  EXPECT_EQ(IgnoreVerdict::None, rules.evaluate("build", "nested/build", "", true));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, EmbeddedSeparatorAnchorsToo)
{
  IgnoreRules rules;
  ASSERT_TRUE(rules.add("src/generated"));

  EXPECT_EQ(IgnoreVerdict::Ignore, rules.evaluate("generated", "src/generated", "", true));
  EXPECT_EQ(IgnoreVerdict::None, rules.evaluate("generated", "other/generated", "", true));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, LastMatchWins)
{
  IgnoreRules rules;
  ASSERT_TRUE(rules.add("*.gen.cpp"));
  ASSERT_TRUE(rules.add("!keep_me.gen.cpp"));

  EXPECT_EQ(IgnoreVerdict::Ignore, judgeName(rules, "drop_me.gen.cpp", false));
  EXPECT_EQ(IgnoreVerdict::Include, judgeName(rules, "keep_me.gen.cpp", false));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, OrderOfNegationMatters)
{
  // The same two rules the other way round: nothing is put back
  IgnoreRules rules;
  ASSERT_TRUE(rules.add("!keep_me.gen.cpp"));
  ASSERT_TRUE(rules.add("*.gen.cpp"));

  EXPECT_EQ(IgnoreVerdict::Ignore, judgeName(rules, "keep_me.gen.cpp", false));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, RulesThatCarryNothingAreRejected)
{
  IgnoreRules rules;

  EXPECT_FALSE(rules.add("/"));
  EXPECT_FALSE(rules.add(""));
  EXPECT_FALSE(rules.add("!"));
  EXPECT_TRUE(rules.empty());
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, DotSlashIsAPathNotAName)
{
  IgnoreRules rules;
  ASSERT_TRUE(rules.add("./build"));

  // Resolved against the working directory, so it is neither a bare name...
  EXPECT_EQ(IgnoreVerdict::None, judgeName(rules, "build"));

  // ...nor anchored to the search root
  EXPECT_FALSE(rules.hasAnchoredRules());
  EXPECT_TRUE(rules.hasAbsoluteRules());

  const std::string here = normalizedPathString(".") + "/build";
  EXPECT_EQ(IgnoreVerdict::Ignore, rules.evaluate("build", "", here, true));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestIgnoreRulesFixture, IsInsideRootHandlesTheFilesystemRoot)
{
  EXPECT_TRUE(isInsideRoot("/tmp/app", "/tmp"));
  EXPECT_TRUE(isInsideRoot("/tmp", "/tmp"));
  EXPECT_FALSE(isInsideRoot("/tmpfoo", "/tmp"));
  EXPECT_TRUE(isInsideRoot("/tmp", "/"));
}
