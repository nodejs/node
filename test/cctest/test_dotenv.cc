#include <string>
#include <sstream>
#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "cppnv/EnvReader.h"

using cppnv::env_reader;
using cppnv::env_pair;
using cppnv::EnvStream;
using cppnv::variable_position;
using std::string;
using std::istringstream;



class DotEnvTest : public EnvironmentTestFixture {
};

TEST_F(DotEnvTest, ReadDotEnvFile) {
  // std::vector<env_pair*> env_pairs;
  //
  string basic("a=bc\n"
      "b=cdd\n"
      "l=asff\n"
      "d=e\n"
      "b\n");
  EnvStream basic_stream(&basic);

  std::vector<env_pair*> env_pairs;
  env_reader::read_pairs(basic_stream, &env_pairs);
  for (const auto value : env_pairs) {
    std::cout << *(value->key->key) << " = " << *(value->value->value) <<
        std::endl;
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

TEST_F(DotEnvTest, InterpolateValues) {
  string interpolate("a1=bc\n"
      "b2=${a1}\n"
      "b3=$ {a1}\n"
      "b4=$ {a1 }\n"
      "b5=$ { a1 }\n"
      "b6=$ { a1}");
  EnvStream interpolate_stream(&interpolate);


  std::vector<env_pair*> env_pairs;

  env_reader::read_pairs(interpolate_stream, &env_pairs);
  for (const auto value : env_pairs) {
    std::cout << *(value->key->key) << " = " << *(value->value->value) <<
        std::endl;
  }

  for (const auto pair : env_pairs) {
    for (const variable_position* interpolation : *pair->value->
         interpolations) {
      std::cout << pair->value->value->substr(interpolation->variable_start,
                                              (interpolation->variable_end -
                                               interpolation->variable_start) +
                                              1) <<
          std::endl;
    }
    env_reader::finalize_value(pair, &env_pairs);
    std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<
        std::endl;
  }

  EXPECT_TRUE(env_pairs.size() == 6);
  EXPECT_TRUE(*env_pairs.at(0)->key->key == "a1");
  EXPECT_TRUE(*env_pairs.at(0)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(1)->key->key == "b2");
  EXPECT_TRUE(*env_pairs.at(1)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(2)->key->key == "b3");
  EXPECT_TRUE(*env_pairs.at(2)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(3)->key->key == "b4");
  EXPECT_TRUE(*env_pairs.at(3)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(4)->key->key == "b5");
  EXPECT_TRUE(*env_pairs.at(4)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(5)->key->key == "b6");
  EXPECT_TRUE(*env_pairs.at(5)->value->value == "bc");
}

TEST_F(DotEnvTest, InterpolateUnClosed) {
  string interpolate_escaped("a1=bc\n"
      "b2=${a1\\}");

  EnvStream interpolate_escaped_stream(&interpolate_escaped);

  std::vector<env_pair*> env_pairs;

  env_reader::read_pairs(interpolate_escaped_stream, &env_pairs);
  for (const auto value : env_pairs) {
    //  std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
  }

  for (const auto pair : env_pairs) {
    std::cout << "__" << std::endl;
    for (const variable_position* interpolation : *pair->value->
         interpolations) {
      std::cout << "Interpolation: " << interpolation->variable_start << " -> "
          << interpolation->variable_end << " length: " << (
            interpolation->variable_end - interpolation->variable_start) + 1
          << std::endl;

      std::cout << pair->value->value->substr(interpolation->variable_start,
                                              (interpolation->variable_end -
                                               interpolation->variable_start) +
                                              1) <<
          std::endl;
    }
    env_reader::finalize_value(pair, &env_pairs);
    std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<
        std::endl;
  }

  EXPECT_TRUE(env_pairs.size() == 2);
  EXPECT_TRUE(*env_pairs.at(0)->key->key == "a1");
  EXPECT_TRUE(*env_pairs.at(0)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(1)->key->key == "b2");
  EXPECT_TRUE(*env_pairs.at(1)->value->value == "${a1\\}");
}


TEST_F(DotEnvTest, InterpolateValuesEscaped) {
  string interpolate_escaped("a1=bc\n"
      "b2=${a1}\n"
      "b3=$ {a1\\}\n"
      "b4=\\$ {a1 }\n"
      "b5=$ \\{ a1 }\n"
      "b6=\\$ { a1}");
  EnvStream interpolate_escaped_stream(&interpolate_escaped);


  std::vector<env_pair*> env_pairs;

  env_reader::read_pairs(interpolate_escaped_stream, &env_pairs);
  for (const auto value : env_pairs) {
    //  std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
  }

  for (const auto pair : env_pairs) {
    std::cout << "__" << std::endl;
    for (const variable_position* interpolation : *pair->value->
         interpolations) {
      std::cout << "Interpolation: " << interpolation->variable_start << " -> "
          << interpolation->variable_end << " length: " << (
            interpolation->variable_end - interpolation->variable_start) + 1
          << std::endl;

      std::cout << pair->value->value->substr(interpolation->variable_start,
                                              (interpolation->variable_end -
                                               interpolation->variable_start) +
                                              1) <<
          std::endl;
    }
    env_reader::finalize_value(pair, &env_pairs);
    std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<
        std::endl;
  }

  EXPECT_TRUE(env_pairs.size() == 6);
  EXPECT_TRUE(*env_pairs.at(0)->key->key == "a1");
  EXPECT_TRUE(*env_pairs.at(0)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(1)->key->key == "b2");
  EXPECT_TRUE(*env_pairs.at(1)->value->value == "bc");
  EXPECT_TRUE(*env_pairs.at(2)->key->key == "b3");
  EXPECT_TRUE(*env_pairs.at(2)->value->value == "$ {a1\\}");
  EXPECT_TRUE(*env_pairs.at(3)->key->key == "b4");
  EXPECT_TRUE(*env_pairs.at(3)->value->value == "\\$ {a1 }");
  EXPECT_TRUE(*env_pairs.at(4)->key->key == "b5");
  EXPECT_TRUE(*env_pairs.at(4)->value->value == "$ \\{ a1 }");
  EXPECT_TRUE(*env_pairs.at(5)->key->key == "b6");
  EXPECT_TRUE(*env_pairs.at(5)->value->value == "\\$ { a1}");
}
