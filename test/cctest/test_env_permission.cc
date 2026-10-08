#include "gtest/gtest.h"
#include "node.h"
#include "node_test_fixture.h"
#include "permission/env_permission.h"

#include <string>
#include <vector>

using node::permission::EnvNameMatchesPattern;
using node::permission::GetRuntimeEnvironmentDefaults;
using node::permission::IntersectEnvAllowLists;
using node::permission::IsRuntimeEnvironmentDefault;
using node::permission::IsValidEnvAllowPattern;
using node::permission::ParseEnvAllowList;

TEST(EnvPermissionTest, ParseEnvAllowList) {
  const std::vector<std::string> values = {"A,B", "C", "", ",D,,E,"};
  EXPECT_EQ(ParseEnvAllowList(values),
            (std::vector<std::string>{"A", "B", "C", "D", "E"}));
}

TEST(EnvPermissionTest, IsValidEnvAllowPattern) {
  EXPECT_TRUE(IsValidEnvAllowPattern("*"));
  EXPECT_TRUE(IsValidEnvAllowPattern("PATH"));
  EXPECT_TRUE(IsValidEnvAllowPattern("APP_*"));
  EXPECT_FALSE(IsValidEnvAllowPattern(""));
  EXPECT_FALSE(IsValidEnvAllowPattern("A=B"));
  EXPECT_FALSE(IsValidEnvAllowPattern("*A"));
  EXPECT_FALSE(IsValidEnvAllowPattern("A*B"));
  EXPECT_FALSE(IsValidEnvAllowPattern("A**"));
}

TEST(EnvPermissionTest, EnvNameMatchesPattern) {
  EXPECT_TRUE(EnvNameMatchesPattern("*", "ANYTHING"));
  EXPECT_TRUE(EnvNameMatchesPattern("PATH", "PATH"));
  EXPECT_FALSE(EnvNameMatchesPattern("PATH", "PATHEXT"));
  EXPECT_FALSE(EnvNameMatchesPattern("PATHEXT", "PATH"));
  EXPECT_TRUE(EnvNameMatchesPattern("APP_*", "APP_"));
  EXPECT_TRUE(EnvNameMatchesPattern("APP_*", "APP_DB_URL"));
  EXPECT_FALSE(EnvNameMatchesPattern("APP_*", "APP"));
  EXPECT_FALSE(EnvNameMatchesPattern("APP_*", "OTHER_APP_DB"));
#ifdef _WIN32
  EXPECT_TRUE(EnvNameMatchesPattern("path", "PATH"));
  EXPECT_TRUE(EnvNameMatchesPattern("app_*", "APP_DB_URL"));
#else
  EXPECT_FALSE(EnvNameMatchesPattern("path", "PATH"));
  EXPECT_FALSE(EnvNameMatchesPattern("app_*", "APP_DB_URL"));
#endif
}

TEST(EnvPermissionTest, IntersectEnvAllowLists) {
  using List = std::vector<std::string>;
  EXPECT_EQ(IntersectEnvAllowLists(List{"*"}, List{"A", "B_*"}),
            (List{"A", "B_*"}));
  EXPECT_EQ(IntersectEnvAllowLists(List{"A", "B"}, List{"*"}),
            (List{"A", "B"}));
  EXPECT_EQ(IntersectEnvAllowLists(List{"A", "B"}, List{"B", "C"}),
            (List{"B"}));
  EXPECT_EQ(IntersectEnvAllowLists(List{"APP_*"}, List{"APP_DB*", "OTHER"}),
            (List{"APP_DB*"}));
  EXPECT_EQ(IntersectEnvAllowLists(List{"APP_DB_URL"}, List{"APP_*"}),
            (List{"APP_DB_URL"}));
  EXPECT_EQ(IntersectEnvAllowLists(List{"APP_*"}, List{"OTHER_*"}), List{});
  EXPECT_EQ(IntersectEnvAllowLists(List{}, List{"*"}), List{});
}

TEST(EnvPermissionTest, RuntimeEnvironmentDefaults) {
  EXPECT_TRUE(IsRuntimeEnvironmentDefault("NODE_OPTIONS"));
  EXPECT_TRUE(IsRuntimeEnvironmentDefault("TZ"));
  EXPECT_TRUE(IsRuntimeEnvironmentDefault("LC_ALL"));
  EXPECT_FALSE(IsRuntimeEnvironmentDefault("NODE_AUTH_TOKEN"));
  EXPECT_FALSE(IsRuntimeEnvironmentDefault("HTTP_PROXY"));
  EXPECT_EQ(node::GetRuntimeEnvironmentDefaults().size(),
            GetRuntimeEnvironmentDefaults().size());
}

class EnvPermissionScrubTest : public NodeZeroIsolateTestFixture {};

TEST_F(EnvPermissionScrubTest, ScrubProcessEnvironmentAfterInitialization) {
  // The fixture has already initialized Node.js, so modifying the process
  // environment is no longer safe.
  EXPECT_TRUE(node::ScrubProcessEnvironment({}).IsNothing());
}
