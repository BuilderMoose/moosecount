/**
 * @brief Test Main for Google Unit Testing of this code
 *
 */

#include <gtest/gtest.h>

std::string g_pathToThisBinary;

int main(int argc, char **argv)
{
  std::string exePath = argv[0];
  g_pathToThisBinary = exePath.substr(0, exePath.find_last_of("/\\"));

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
