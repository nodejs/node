#include "debug_utils-inl.h"
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_options.h"
#include "node_test_fixture.h"
#include "path.h"
#include "util.h"
#include "v8.h"

using node::BufferValue;
using node::NormalizeString;
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
  EXPECT_EQ(PathResolve(*env, {"\\\\.\\PHYSICALDRIVE0"}),
            "\\\\.\\PHYSICALDRIVE0");
  EXPECT_EQ(PathResolve(*env, {"\\\\?\\PHYSICALDRIVE0"}),
            "\\\\?\\PHYSICALDRIVE0");
  // Backtracking past the drive root stays clamped at the drive root.
  EXPECT_EQ(PathResolve(*env, {"c:/a/b/c", "..\\..\\..\\.."}), "c:\\");
  // UNC root is preserved when backtracking past it. The UNC share
  // \\server\share is the root, so "..","..","x" cannot escape it and the
  // remaining segment "x" is appended to the share root.
  EXPECT_EQ(PathResolve(*env, {"//server/share", "..", "..", "x"}),
            "\\\\server\\share\\x");
#else
  EXPECT_EQ(PathResolve(*env, {"/var/lib", "../", "file/"}), "/var/file");
  EXPECT_EQ(PathResolve(*env, {"/var/lib", "/../", "file/"}), "/file");
  EXPECT_EQ(PathResolve(*env, {"a/b/c/", "../../.."}), cwd);
  EXPECT_EQ(PathResolve(*env, {"."}), cwd);
  EXPECT_EQ(PathResolve(*env, {"/some/dir", ".", "/absolute/"}), "/absolute");
  EXPECT_EQ(PathResolve(*env, {"/foo/tmp.3/", "../tmp.3/cycles/root.js"}),
            "/foo/tmp.3/cycles/root.js");
  // Backtracking past the root stays clamped at the root.
  EXPECT_EQ(PathResolve(*env, {"/a/b/c/d/e", "../../../../.."}), "/");
  EXPECT_EQ(PathResolve(*env, {"/a/b/c", "../../../../.."}), "/");
  // Mixed current-dir and parent-dir segments.
  EXPECT_EQ(PathResolve(*env, {"/a/./b/../c/./d"}), "/a/c/d");
  // Collapsing of repeated separators.
  EXPECT_EQ(PathResolve(*env, {"/a//b///c"}), "/a/b/c");
  // Single parent-dir traversal.
  EXPECT_EQ(PathResolve(*env, {"/a/../b"}), "/b");
  EXPECT_EQ(PathResolve(*env, {"/a/b/../../c"}), "/c");
  // Trailing separator is stripped.
  EXPECT_EQ(PathResolve(*env, {"/a/b/c/"}), "/a/b/c");
  // Single absolute segment.
  EXPECT_EQ(PathResolve(*env, {"/single"}), "/single");
#endif
}

TEST_F(PathTest, NormalizeString) {
  // allowAboveRoot = false (absolute context): ".." that cannot be resolved is
  // dropped, "." segments and repeated/trailing separators are collapsed.
  EXPECT_EQ(NormalizeString("a/b/../../../c", false, "/"), "c");
  EXPECT_EQ(NormalizeString("a/b/c/d/e/../../../../..", false, "/"), "");
  EXPECT_EQ(NormalizeString("a/./b//c/", false, "/"), "a/b/c");
  EXPECT_EQ(NormalizeString("./foo/./bar/", false, "/"), "foo/bar");
  // allowAboveRoot = true (relative context): leading ".." is preserved.
  EXPECT_EQ(NormalizeString("a/b/../../../c", true, "/"), "../c");
  EXPECT_EQ(NormalizeString("../../a", true, "/"), "../../a");
  EXPECT_EQ(NormalizeString("foo/..", true, "/"), "");
  EXPECT_EQ(NormalizeString("foo/../..", true, "/"), "..");
#ifdef _WIN32
  // The Windows separator is handled the same way.
  EXPECT_EQ(NormalizeString("a\\b\\..\\..\\..\\c", false, "\\"), "c");
  EXPECT_EQ(NormalizeString("..\\..\\a", true, "\\"), "..\\..\\a");
#endif  // _WIN32
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
  BufferValue data_2(
      isolate_, v8::String::NewFromUtf8(isolate_, "C://").ToLocalChecked());
  ToNamespacedPath(*env, &data_2);
  EXPECT_EQ(data_2.ToStringView(), "\\\\?\\C:\\");
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
  BufferValue data_4(
      isolate_,
      v8::String::NewFromUtf8(isolate_, "\\\\?\\c:\\Windows/System")
          .ToLocalChecked());
  ToNamespacedPath(*env, &data_4);
  EXPECT_EQ(data_4.ToStringView(), "\\\\?\\c:\\Windows\\System");
#else
  BufferValue data(
      isolate_,
      v8::String::NewFromUtf8(isolate_, "hello world").ToLocalChecked());
  ToNamespacedPath(*env, &data);
  EXPECT_EQ(data.ToStringView(), "hello world");  // Input should not be mutated
#endif
}
