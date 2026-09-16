/**
 * @file testDocument.cpp
 *
 * Document metrics, transcribed from the moosemetrics script this replaces.
 * The numbers must not move, so each rule gets its own case.
 */

#include <gtest/gtest.h>
#include <string>

#include "document.hpp"

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
class TestDocumentFixture : public ::testing::Test
{
public:
  TestDocumentFixture()
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

  DocumentTotals markdown(const std::string &content)
  {
    return countDocument(content, DocumentKind::Markdown);
  }

  DocumentTotals plantUml(const std::string &content)
  {
    return countDocument(content, DocumentKind::PlantUml);
  }
};

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, ExtensionsResolveToTheirKind)
{
  ASSERT_NE(nullptr, documentSpecForExtension(".md"));
  EXPECT_EQ(DocumentKind::Markdown, documentSpecForExtension(".md")->kind);
  EXPECT_EQ(DocumentKind::Markdown, documentSpecForExtension(".txt")->kind);
  EXPECT_EQ(DocumentKind::PlantUml, documentSpecForExtension(".puml")->kind);
  EXPECT_EQ(DocumentKind::PlantUml, documentSpecForExtension(".wsd")->kind);

  EXPECT_EQ(nullptr, documentSpecForExtension(".cpp"));
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, LinesAndWords)
{
  DocumentTotals totals = markdown("one two three\n\nfour\n");

  EXPECT_EQ(3, totals.lines);
  EXPECT_EQ(4, totals.words);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, HeaderLevels)
{
  DocumentTotals totals = markdown("# one\n## two\n###### six\n####### seven\n");

  // Seven hashes is not a heading
  EXPECT_EQ(3, totals.headers);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, EmptyHeadingCounts)
{
  // CommonMark allows an ATX heading with no content, and the script this
  // replaces counted them, so a bare '#' still counts
  DocumentTotals totals = markdown("#\n##\n");

  EXPECT_EQ(2, totals.headers);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, HashWithoutSpaceIsNotAHeading)
{
  DocumentTotals totals = markdown("#hashtag\n");

  EXPECT_EQ(0, totals.headers);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, TaskStates)
{
  DocumentTotals totals = markdown("- [ ] open\n- [x] done\n- [X] also done\n- plain item\n");

  EXPECT_EQ(1, totals.openTasks);
  EXPECT_EQ(2, totals.completedTasks);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, IndentedTasksCount)
{
  DocumentTotals totals = markdown("  - [ ] nested open\n    - [x] deeper done\n");

  EXPECT_EQ(1, totals.openTasks);
  EXPECT_EQ(1, totals.completedTasks);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, UmlEntities)
{
  DocumentTotals totals = plantUml("class Foo\ncomponent Bar\nactor User\n"
                                   "interface Thing\nnote over Foo\n");

  EXPECT_EQ(4, totals.umlEntities);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, UmlEntitiesAreCaseInsensitive)
{
  DocumentTotals totals = plantUml("CLASS Foo\nClass Bar\n");

  EXPECT_EQ(2, totals.umlEntities);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, ClassNameWithoutSpaceIsNotAnEntity)
{
  DocumentTotals totals = plantUml("classroom Foo\n");

  EXPECT_EQ(0, totals.umlEntities);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, RelationshipArrows)
{
  DocumentTotals totals = plantUml("Foo --> Bar\nBar ..> Baz\nBaz <-- Qux\nA <.. B\n");

  EXPECT_EQ(4, totals.relationships);
}

// -- --- --- ... . ....... -- --- --- ... . ....... -- --- --- ... . .......
TEST_F(TestDocumentFixture, MarkdownRulesDoNotApplyToPlantUml)
{
  DocumentTotals totals = plantUml("# not counted as a header\n- [ ] not a task\n");

  EXPECT_EQ(0, totals.headers);
  EXPECT_EQ(0, totals.openTasks);
}
