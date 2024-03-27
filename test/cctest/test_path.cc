#include "debug_utils-inl.h"
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_options.h"
#include "node_test_fixture.h"
#include "path.h"
#include "util.h"
#include "v8.h"

using node::BufferValue;
using node::PathResolve;
using node::ToNamespacedPath;

class PathTest : public EnvironmentTestFixture {};

TEST_F(PathTest, PathResolve) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv, node::EnvironmentFlags::kNoBrowserGlobals};
  auto cwd = (*env)->GetCwd((*env)->exec_path());
#ifdef _WIN32
  EXPECT_EQ(PathResolve(*env, {"c:/blah\\blah", "d:/games", "c:../a"}),
            "c:\\blah\\a");
  EXPECT_EQ(PathResolve(*env, {"c:/ignore", "d:\\a/b\\c/d", "\\e.exe"}),
            "d:\\e.exe");
  EXPECT_EQ(PathResolve(*env, {"c:/ignore", "c:/some/file"}), "c:\\some\\file");
  EXPECT_EQ(PathResolve(*env, {"d:/ignore", "d:some/dir//"}),
            "d:\\ignore\\some\\dir");
  EXPECT_EQ(PathResolve(*env, {"."}), cwd);
  EXPECT_EQ(PathResolve(*env, {"//server/share", "..", "relative\\"}),
            "\\\\server\\share\\relative");
  EXPECT_EQ(PathResolve(*env, {"c:/", "//"}), "c:\\");
  EXPECT_EQ(PathResolve(*env, {"c:/", "//dir"}), "c:\\dir");
  EXPECT_EQ(PathResolve(*env, {"c:/", "//server/share"}),
            "\\\\server\\share\\");
  EXPECT_EQ(PathResolve(*env, {"c:/", "//server//share"}),
            "\\\\server\\share\\");
  EXPECT_EQ(PathResolve(*env, {"c:/", "///some//dir"}), "c:\\some\\dir");
  EXPECT_EQ(
      PathResolve(*env, {"C:\\foo\\tmp.3\\", "..\\tmp.3\\cycles\\root.js"}),
      "C:\\foo\\tmp.3\\cycles\\root.js");
#else
  EXPECT_EQ(PathResolve(*env, {"/var/lib", "../", "file/"}), "/var/file");
  EXPECT_EQ(PathResolve(*env, {"/var/lib", "/../", "file/"}), "/file");
  EXPECT_EQ(PathResolve(*env, {"a/b/c/", "../../.."}), cwd);
  EXPECT_EQ(PathResolve(*env, {"."}), cwd);
  EXPECT_EQ(PathResolve(*env, {"/some/dir", ".", "/absolute/"}), "/absolute");
  EXPECT_EQ(PathResolve(*env, {"/foo/tmp.3/", "../tmp.3/cycles/root.js"}),
            "/foo/tmp.3/cycles/root.js");
#endif
}

TEST_F(PathTest, ToNamespacedPath) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv, node::EnvironmentFlags::kNoBrowserGlobals};
#ifdef _WIN32
  BufferValue data(isolate_,
                   v8::String::NewFromUtf8(isolate_, "").ToLocalChecked());
  ToNamespacedPath(*env, &data);
  EXPECT_EQ(data.ToStringView(), "");  // Empty string should not be mutated
  // If the resolved name is no more than 2 characters, then path
  // is unchanged by ToNamespacedPath, but it does not follow that a
  // 2-char string path is unchanged, e.g.,
  //
  // Welcome to Node.js v20.7.0.
  //  Type ".help" for more information.
  //  > path.toNamespacedPath("C:")
  //  '\\\\?\\C:\\msys64\\home\\Daniel\\CVS\\github\\node
  //
  // BufferValue data_2(isolate_,
  //                   v8::String::NewFromUtf8(isolate_,
  //                   "c:").ToLocalChecked());
  // ToNamespacedPath(*env, &data_2);
  // EXPECT_EQ(data_2.ToStringView(),
  //          "c:");  // Input less than of equal to 2 characters
  //                  // should be returned directly
  BufferValue data_3(
      isolate_,
      v8::String::NewFromUtf8(
          isolate_,
          "C:\\workspace\\node-test-binary-windows-js-"
          "suites\\node\\test\\fixtures\\permission\\deny\\protected-file.md")
          .ToLocalChecked());
  ToNamespacedPath(*env, &data_3);
  EXPECT_EQ(
      data_3.ToStringView(),
      "\\\\?\\C:\\workspace\\node-test-binary-windows-js-"
      "suites\\node\\test\\fixtures\\permission\\deny\\protected-file.md");
#else
  BufferValue data(
      isolate_,
      v8::String::NewFromUtf8(isolate_, "hello world").ToLocalChecked());
  ToNamespacedPath(*env, &data);
  EXPECT_EQ(data.ToStringView(), "hello world");  // Input should not be mutated
#endif
}
