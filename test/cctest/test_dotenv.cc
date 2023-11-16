#include <string>
#include <sstream>
#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "cppnv/EnvReader.h"

using cppnv::env_reader;
using cppnv::env_pair;
using std::string;
using std::istringstream;



class DotEnvTest : public EnvironmentTestFixture {
private:
  void TearDown() override {
  }
};

TEST_F(DotEnvTest, ReadDotEnvFile) {
  // std::vector<env_pair*> env_pairs;
  //
  string basic("a=bc\n"
      "b=cdd\n"
      "l=asff\n"
      "d=e\n"
      "b\n");
  istringstream basic_stream(basic);

  std::vector<env_pair*> env_pairs;
  env_reader::read_pairs(basic_stream, &env_pairs);
  for (const auto value : env_pairs)
  {
    std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
  }

  EXPECT_TRUE(env_pairs.size() == 4);
  EXPECT_TRUE(*env_pairs.at(0)->key->key == "a");
  EXPECT_TRUE(*env_pairs.at(0)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(1)->key->key == "b");
  EXPECT_TRUE(*env_pairs.at(1)->value->value == "cdd");
  EXPECT_TRUE(*env_pairs.at(2)->key->key == "l");
  EXPECT_TRUE(*env_pairs.at(2)->value->value == "asff");
  EXPECT_TRUE(*env_pairs.at(3)->key->key == "d");
  EXPECT_TRUE(*env_pairs.at(3)->value->value == "e");
}
