#include "node_url.h"
#include "node_i18n.h"

#include "gtest/gtest.h"

using node::url::URL;

class URLTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if defined(NODE_HAVE_I18N_SUPPORT)
    std::string icu_data_dir;
    node::i18n::InitializeICUDirectory(icu_data_dir);
#endif
  }

  void TearDown() override {}
};

TEST_F(URLTest, Simple) {
  URL simple("https://example.org:81/a/b/c?query#fragment");

  EXPECT_EQ(simple.protocol(), "https:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.port(), 81);
  EXPECT_EQ(simple.path(), "/a/b/c");
  EXPECT_EQ(simple.query(), "query");
  EXPECT_EQ(simple.fragment(), "fragment");
}

TEST_F(URLTest, Simple2) {
  const char* input = "https://example.org:81/a/b/c?query#fragment";
  URL simple(input, strlen(input));

  EXPECT_EQ(simple.protocol(), "https:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.port(), 81);
  EXPECT_EQ(simple.path(), "/a/b/c");
  EXPECT_EQ(simple.query(), "query");
  EXPECT_EQ(simple.fragment(), "fragment");
}

TEST_F(URLTest, Base1) {
  URL base("http://example.org/foo/bar");
  URL simple("../baz", &base);

  EXPECT_EQ(simple.protocol(), "http:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.path(), "/baz");
}

TEST_F(URLTest, Base2) {
  URL simple("../baz", "http://example.org/foo/bar");

  EXPECT_EQ(simple.protocol(), "http:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.path(), "/baz");
}

TEST_F(URLTest, Base3) {
  const char* input = "../baz";
  const char* base = "http://example.org/foo/bar";

  URL simple(input, strlen(input), base, strlen(base));

  EXPECT_EQ(simple.protocol(), "http:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.path(), "/baz");
}
