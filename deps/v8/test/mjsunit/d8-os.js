// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Test the OS module of d8.  This test only makes sense with d8.  It
// only does non-trivial work on Unix since os.system() is not currently
// implemented on Windows, and even if it were then many of the things
// we are calling would not be available.

function arg_error(str) {
  try {
    eval(str);
  } catch (e) {
    assertTrue(/rgument/.test(e), str);
  }
}


function str_error(str) {
  var e = new Object();
  e.toString = function() { throw new Error("foo bar"); }
  try {
    eval(str);
  } catch (exception) {
    assertTrue(/tring conversion/.test(exception), str);
  }
}


if (this.os && os.system) {
  try {
    // Delete the dir if it is lying around from last time.
    os.system("ls", ["d8-os-test-directory"]);
    os.system("rm", ["-r", "d8-os-test-directory"]);
  } catch (e) {
  }
  os.mkdirp("d8-os-test-directory");
  os.chdir("d8-os-test-directory");
  // Check the chdir worked.
  os.system('ls', ['../d8-os-test-directory']);
  // Simple create dir.
  os.mkdirp("dir");
  // Create dir in dir.
  os.mkdirp("dir/foo");
  // Check that they are there.
  os.system('ls', ['dir/foo']);
  // Check that we can detect when something is not there.
  assertThrows("os.system('ls', ['dir/bar']);", "dir not there");
  // Check that mkdirp makes intermediate directories.
  os.mkdirp("dir2/foo");
  os.system("ls", ["dir2/foo"]);
  // Check that mkdirp doesn't mind if the dir is already there.
  os.mkdirp("dir2/foo");
  os.mkdirp("dir2/foo/");
  // Check that mkdirp can cope with trailing /
  os.mkdirp("dir3/");
  os.system("ls", ["dir3"]);
  // Check that we get an error if the name is taken by a file.
  os.system("sh", ["-c", "echo foo > file1"]);
  os.system("ls", ["file1"]);
  assertThrows("os.mkdirp('file1');", "mkdir over file1");
  assertThrows("os.mkdirp('file1/foo');", "mkdir over file2");
  assertThrows("os.mkdirp('file1/');", "mkdir over file3");
  assertThrows("os.mkdirp('file1/foo/');", "mkdir over file4");
  // Create a dir we cannot read.
  os.mkdirp("dir4", 0);
  // This test fails if you are root since root can read any dir.
  assertThrows("os.chdir('dir4');", "chdir dir4 I");
  os.rmdir("dir4");
  assertThrows("os.chdir('dir4');", "chdir dir4 II");
  // Set umask.
  var old_umask = os.umask(0777);
  // Create a dir we cannot read.
  os.mkdirp("dir5");
  // This test fails if you are root since root can read any dir.
  assertThrows("os.chdir('dir5');", "cd dir5 I");
  os.rmdir("dir5");
  assertThrows("os.chdir('dir5');", "chdir dir5 II");
  os.umask(old_umask);

  os.mkdirp("hest/fisk/../fisk/ged");
  os.system("ls", ["hest/fisk/ged"]);

  os.setenv("FOO", "bar");
  var environment = os.system("printenv");
  assertTrue(/FOO=bar/.test(environment));

  // Check we time out.
  var have_sleep = true;
  var have_echo = true;
  try {
    os.system("ls", ["/bin/sleep"]);
  } catch (e) {
    have_sleep = false;
  }
  try {
    os.system("ls", ["/bin/echo"]);
  } catch (e) {
    have_echo = false;
  }
  if (have_sleep) {
    assertThrows("os.system('sleep', ['2000'], 200);", "sleep 1");

    // Check we time out with total time.
    assertThrows("os.system('sleep', ['2000'], -1, 200);", "sleep 2");

    // Check that -1 means no timeout.
    os.system('sleep', ['1'], -1, -1);

  }

  // Check that we don't fill up the process table with zombies.
  // Disabled because it's too slow.
  if (have_echo) {
    //for (var i = 0; i < 65536; i++) {
      assertEquals("baz\n", os.system("echo", ["baz"]));
    //}
  }

  os.chdir("..");
  os.system("rm", ["-r", "d8-os-test-directory"]);

  // Too few args.
  arg_error("os.umask();");
  arg_error("os.system();");
  arg_error("os.mkdirp();");
  arg_error("os.chdir();");
  arg_error("os.setenv();");
  arg_error("os.rmdir();");

  // Too many args.
  arg_error("os.setenv('FOO=bar');");
  arg_error("os.umask(0, 0);");
  arg_error("os.system('ls', [], -1, -1, -1);");
  arg_error("os.mkdirp('foo', 0, 0)");
  arg_error("os.chdir('foo', 'bar')");
  arg_error("os.rmdir('foo', 'bar');");

  // Wrong kind of args.
  arg_error("os.umask([]);");
  arg_error("os.system('ls', 'foo');");
  arg_error("os.system('ls', 123);");
  arg_error("os.system('ls', [], 'foo');");
  arg_error("os.system('ls', [], -1, 'foo');");
  arg_error("os.mkdirp('foo', 'bar');");

  // Test broken toString().
  str_error("os.system(e);");
  str_error("os.system('ls', [e]);");
  str_error("os.system('ls', ['.', e]);");
  str_error("os.system('ls', [e, '.']);");
  str_error("os.mkdirp(e);");
  str_error("os.setenv(e, 'goo');");
  str_error("os.setenv('goo', e);");
  str_error("os.chdir(e);");
  str_error("os.rmdir(e);");
}
