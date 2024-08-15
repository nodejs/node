#include <iostream>
#include <vector>
#include "gtest/gtest.h"
#include "node_dotenv.h"

using node::Dotenv;

#define CHECK_DOTENV(args, expected)                                           \
  {                                                                            \
    auto check_result =                                                        \
        Dotenv::GetPathFromArgs(std::vector<std::string> args);                \
    ASSERT_EQ(check_result.size(), expected.size());                           \
    for (size_t i = 0; i < check_result.size(); ++i) {                         \
      EXPECT_EQ(check_result[i], expected[i]);                                 \
    }                                                                          \
  }

TEST(Dotenv, GetPathFromArgs) {
  CHECK_DOTENV(({"node", "--env-file=.env"}),
               (std::vector<std::string>{".env"}));
  CHECK_DOTENV(({"node", "--env-file", ".env"}),
               (std::vector<std::string>{".env"}));
  CHECK_DOTENV(({"node", "--env-file=.env", "--env-file", "other.env"}),
               (std::vector<std::string>{".env", "other.env"}));

  CHECK_DOTENV(
      ({"node", "--env-file=before.env", "script.js", "--env-file=after.env"}),
      (std::vector<std::string>{"before.env"}));
  CHECK_DOTENV(
      ({"node", "--env-file=before.env", "--", "--env-file=after.env"}),
      (std::vector<std::string>{"before.env"}));

  CHECK_DOTENV(({"node", "-e", "...", "--env-file=after.env"}),
               (std::vector<std::string>{"after.env"}));

  CHECK_DOTENV(({"node", "-too_long", "--env-file=.env"}),
               (std::vector<std::string>{".env"}));

  CHECK_DOTENV(({"node", "-", "--env-file=.env"}),
               (std::vector<std::string>{".env"}));
}
