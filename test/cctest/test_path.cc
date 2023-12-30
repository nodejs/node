#include "debug_utils-inl.h"
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_options.h"
#include "node_test_fixture.h"
#include "path.h"

using node::PathResolve;

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
