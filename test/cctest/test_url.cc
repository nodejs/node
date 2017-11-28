#include "node_url.h"
#include "node_i18n.h"

#include "gtest/gtest.h"

using node::url::URL;
using node::url::URL_FLAGS_FAILED;

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

  EXPECT_FALSE(simple.flags() & URL_FLAGS_FAILED);
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

  EXPECT_FALSE(simple.flags() & URL_FLAGS_FAILED);
  EXPECT_EQ(simple.protocol(), "https:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.port(), 81);
  EXPECT_EQ(simple.path(), "/a/b/c");
  EXPECT_EQ(simple.query(), "query");
  EXPECT_EQ(simple.fragment(), "fragment");
}

TEST_F(URLTest, NoBase1) {
  URL error("123noscheme");
  EXPECT_TRUE(error.flags() & URL_FLAGS_FAILED);
}

TEST_F(URLTest, Base1) {
  URL base("http://example.org/foo/bar");
  ASSERT_FALSE(base.flags() & URL_FLAGS_FAILED);

  URL simple("../baz", &base);
  EXPECT_FALSE(simple.flags() & URL_FLAGS_FAILED);
  EXPECT_EQ(simple.protocol(), "http:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.path(), "/baz");
}

TEST_F(URLTest, Base2) {
  URL simple("../baz", "http://example.org/foo/bar");

  EXPECT_FALSE(simple.flags() & URL_FLAGS_FAILED);
  EXPECT_EQ(simple.protocol(), "http:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.path(), "/baz");
}

TEST_F(URLTest, Base3) {
  const char* input = "../baz";
  const char* base = "http://example.org/foo/bar";

  URL simple(input, strlen(input), base, strlen(base));

  EXPECT_FALSE(simple.flags() & URL_FLAGS_FAILED);
  EXPECT_EQ(simple.protocol(), "http:");
  EXPECT_EQ(simple.host(), "example.org");
  EXPECT_EQ(simple.path(), "/baz");
}

TEST_F(URLTest, ToFilePath) {
#define T(url, path) EXPECT_EQ(path, URL(url).ToFilePath())
  T("http://example.org/foo/bar", "");

#ifdef _WIN32
  T("file:///C:/Program%20Files/", "C:\\Program Files\\");
  T("file:///C:/a/b/c?query#fragment", "C:\\a\\b\\c");
  T("file://host/path/a/b/c?query#fragment", "\\\\host\\path\\a\\b\\c");
  T("file://xn--weird-prdj8vva.com/host/a", "\\\\wͪ͊eiͬ͋rd.com\\host\\a");
  T("file:///C:/a%2Fb", "");
  T("file:///", "");
  T("file:///home", "");
#else
  T("file:///", "/");
  T("file:///home/user?query#fragment", "/home/user");
  T("file:///home/user/?query#fragment", "/home/user/");
  T("file:///home/user/%20space", "/home/user/ space");
  T("file:///home/us%5Cer", "/home/us\\er");
  T("file:///home/us%2Fer", "");
  T("file://host/path", "");
#endif

#undef T
}
