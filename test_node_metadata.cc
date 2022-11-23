#include "node_metadata.h"
#include <string>
#include "gtest/gtest.h"

TEST(NodeMetadataTest, Versions) {
  std::string ver = node::Metadata::Versions().undici;
  std::stringstream versionstream(ver);
  std::string segment;
  std::vector<std::string> seglist;
  while (std::getline(versionstream, segment, '.')) {
    seglist.push_back(segment);
  }

  EXPECT_EQ(seglist.size(), 3);
  EXPECT_GE(std::stoi(seglist[0]), 5);
  EXPECT_GE(std::stoi(seglist[1]), 0);
  EXPECT_GE(std::stoi(seglist[2]), 0);
  
}
