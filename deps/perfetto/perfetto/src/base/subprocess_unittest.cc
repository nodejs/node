/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/ext/base/subprocess.h"

#if PERFETTO_HAS_SUBPROCESS()
#include <thread>

#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/temp_file.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

std::string GenLargeString() {
  std::string contents;
  for (int i = 0; i < 4096; i++) {
    contents += "very long text " + std::to_string(i) + "\n";
  }
  // Make sure that |contents| is > the default pipe buffer on Linux (4 pages).
  PERFETTO_DCHECK(contents.size() > 4096 * 4);
  return contents;
}

TEST(SubprocessTest, InvalidPath) {
  Subprocess p({"/usr/bin/invalid_1337"});
  EXPECT_FALSE(p.Call());
  EXPECT_EQ(p.status(), Subprocess::kExited);
  EXPECT_EQ(p.returncode(), 128);
  EXPECT_EQ(p.output(), "execve() failed\n");
}

TEST(SubprocessTest, StdoutOnly) {
  Subprocess p({"sh", "-c", "(echo skip_err >&2); echo out_only"});
  p.args.stdout_mode = Subprocess::kBuffer;
  p.args.stderr_mode = Subprocess::kDevNull;
  EXPECT_TRUE(p.Call());
  EXPECT_EQ(p.status(), Subprocess::kExited);
  EXPECT_EQ(p.output(), "out_only\n");
}

TEST(SubprocessTest, StderrOnly) {
  Subprocess p({"sh", "-c", "(echo err_only >&2); echo skip_out"});
  p.args.stdout_mode = Subprocess::kDevNull;
  p.args.stderr_mode = Subprocess::kBuffer;
  EXPECT_TRUE(p.Call());
  EXPECT_EQ(p.output(), "err_only\n");
}

TEST(SubprocessTest, BothStdoutAndStderr) {
  Subprocess p({"sh", "-c", "echo out; (echo err >&2); echo out2"});
  p.args.stdout_mode = Subprocess::kBuffer;
  p.args.stderr_mode = Subprocess::kBuffer;
  EXPECT_TRUE(p.Call());
  EXPECT_EQ(p.output(), "out\nerr\nout2\n");
}

TEST(SubprocessTest, BinTrue) {
  Subprocess p({"true"});
  EXPECT_TRUE(p.Call());
  EXPECT_EQ(p.status(), Subprocess::kExited);
  EXPECT_EQ(p.returncode(), 0);
}

TEST(SubprocessTest, BinFalse) {
  Subprocess p({"false"});
  EXPECT_FALSE(p.Call());
  EXPECT_EQ(p.status(), Subprocess::kExited);
  EXPECT_EQ(p.returncode(), 1);
}

TEST(SubprocessTest, Echo) {
  Subprocess p({"echo", "-n", "foobar"});
  p.args.stdout_mode = Subprocess::kBuffer;
  EXPECT_TRUE(p.Call());
  EXPECT_EQ(p.status(), Subprocess::kExited);
  EXPECT_EQ(p.returncode(), 0);
  EXPECT_EQ(p.output(), "foobar");
}

TEST(SubprocessTest, FeedbackLongInput) {
  std::string contents = GenLargeString();
  Subprocess p({"cat", "-"});
  p.args.stdout_mode = Subprocess::kBuffer;
  p.args.input = contents;
  EXPECT_TRUE(p.Call());
  EXPECT_EQ(p.status(), Subprocess::kExited);
  EXPECT_EQ(p.returncode(), 0);
  EXPECT_EQ(p.output(), contents);
}

TEST(SubprocessTest, CatLargeFile) {
  std::string contents = GenLargeString();
  TempFile tf = TempFile::Create();
  WriteAll(tf.fd(), contents.data(), contents.size());
  FlushFile(tf.fd());
  Subprocess p({"cat", tf.path().c_str()});
  p.args.stdout_mode = Subprocess::kBuffer;
  EXPECT_TRUE(p.Call());
  EXPECT_EQ(p.output(), contents);
}

TEST(SubprocessTest, Timeout) {
  Subprocess p({"sleep", "60"});
  EXPECT_FALSE(p.Call(/*timeout_ms=*/1));
  EXPECT_EQ(p.status(), Subprocess::kKilledBySignal);
}

TEST(SubprocessTest, TimeoutNotHit) {
  Subprocess p({"sleep", "0.01"});
  EXPECT_TRUE(p.Call(/*timeout_ms=*/100000));
}

TEST(SubprocessTest, TimeoutStopOutput) {
  Subprocess p({"sh", "-c", "while true; do echo stuff; done"});
  p.args.stdout_mode = Subprocess::kDevNull;
  EXPECT_FALSE(p.Call(/*timeout_ms=*/10));
  EXPECT_EQ(p.status(), Subprocess::kKilledBySignal);
}

TEST(SubprocessTest, ExitBeforeReadingStdin) {
  // 'sh -c' is to avoid closing stdin (sleep closes it before sleeping).
  Subprocess p({"sh", "-c", "sleep 0.01"});
  p.args.stdout_mode = Subprocess::kDevNull;
  p.args.stderr_mode = Subprocess::kDevNull;
  p.args.input = GenLargeString();
  EXPECT_TRUE(p.Call());
  EXPECT_EQ(p.status(), Subprocess::kExited);
  EXPECT_EQ(p.returncode(), 0);
}

TEST(SubprocessTest, StdinWriteStall) {
  // 'sh -c' is to avoid closing stdin (sleep closes it before sleeping).
  // This causes a situation where the write on the stdin will stall because
  // nobody reads it and the pipe buffer fills up. In this situation we should
  // still handle the timeout properly.
  Subprocess p({"sh", "-c", "sleep 10"});
  p.args.stdout_mode = Subprocess::kDevNull;
  p.args.stderr_mode = Subprocess::kDevNull;
  p.args.input = GenLargeString();
  EXPECT_FALSE(p.Call(/*timeout_ms=*/10));
  EXPECT_EQ(p.status(), Subprocess::kKilledBySignal);
}

TEST(SubprocessTest, StartAndWait) {
  Subprocess p({"sleep", "1000"});
  p.Start();
  EXPECT_EQ(p.Poll(), Subprocess::kRunning);
  p.KillAndWaitForTermination();
  EXPECT_EQ(p.status(), Subprocess::kKilledBySignal);
  EXPECT_EQ(p.Poll(), Subprocess::kKilledBySignal);
  EXPECT_EQ(p.returncode(), 128 + SIGKILL);
}

TEST(SubprocessTest, PollBehavesProperly) {
  Subprocess p({"sh", "-c", "echo foobar"});
  p.args.stdout_mode = Subprocess::kBuffer;
  p.args.input = "ignored";
  p.Start();

  // Here we use kill() as a way to tell if the process is still running.
  // SIGWINCH is ignored by default.
  while (kill(p.pid(), SIGWINCH) == 0) {
    usleep(1000);
  }

  // At this point Poll() must detect the termination.
  EXPECT_EQ(p.Poll(), Subprocess::kExited);
  EXPECT_EQ(p.returncode(), 0);
}

// Test the case of passing a lambda in |entrypoint| but no cmd.c
TEST(SubprocessTest, Entrypoint) {
  Subprocess p;
  p.args.input = "ping\n";
  p.args.stdout_mode = Subprocess::kBuffer;
  p.args.entrypoint_for_testing = [] {
    char buf[32]{};
    PERFETTO_CHECK(fgets(buf, sizeof(buf), stdin));
    PERFETTO_CHECK(strcmp(buf, "ping\n") == 0);
    printf("pong\n");
    fflush(stdout);
    _exit(42);
  };
  EXPECT_FALSE(p.Call());
  EXPECT_EQ(p.returncode(), 42);
  EXPECT_EQ(p.output(), "pong\n");
}

// Test the case of passing both a lambda entrypoint and a process to exec.
TEST(SubprocessTest, EntrypointAndExec) {
  base::Pipe pipe1 = base::Pipe::Create();
  base::Pipe pipe2 = base::Pipe::Create();
  int pipe1_wr = *pipe1.wr;
  int pipe2_wr = *pipe2.wr;

  Subprocess p({"echo", "123"});
  p.args.stdout_mode = Subprocess::kBuffer;
  p.args.preserve_fds.push_back(pipe2_wr);
  p.args.entrypoint_for_testing = [pipe1_wr, pipe2_wr] {
    base::ignore_result(write(pipe1_wr, "fail", 4));
    base::ignore_result(write(pipe2_wr, "pass", 4));
  };

  p.Start();
  pipe1.wr.reset();
  pipe2.wr.reset();

  char buf[8];
  EXPECT_LE(read(*pipe1.rd, buf, sizeof(buf)), 0);
  EXPECT_EQ(read(*pipe2.rd, buf, sizeof(buf)), 4);
  buf[4] = '\0';
  EXPECT_STREQ(buf, "pass");
  EXPECT_TRUE(p.Wait());
  EXPECT_EQ(p.status(), Subprocess::kExited);
  EXPECT_EQ(p.output(), "123\n");
}

TEST(SubprocessTest, Wait) {
  Subprocess p({"sleep", "10000"});
  p.Start();
  for (int i = 0; i < 3; i++) {
    EXPECT_FALSE(p.Wait(1 /*ms*/));
    EXPECT_EQ(p.status(), Subprocess::kRunning);
  }
  kill(p.pid(), SIGBUS);
  EXPECT_TRUE(p.Wait(30000 /*ms*/));
  EXPECT_TRUE(p.Wait());  // Should be a no-op.
  EXPECT_EQ(p.status(), Subprocess::kKilledBySignal);
  EXPECT_EQ(p.returncode(), 128 + SIGBUS);
}

TEST(SubprocessTest, KillOnDtor) {
  // Here we use kill(SIGWINCH) as a way to tell if the process is still alive.
  // SIGWINCH is one of the few signals that has default ignore disposition.
  int pid;
  {
    Subprocess p({"sleep", "10000"});
    p.Start();
    pid = p.pid();
    EXPECT_EQ(kill(pid, SIGWINCH), 0);
  }
  EXPECT_EQ(kill(pid, SIGWINCH), -1);
}

}  // namespace
}  // namespace base
}  // namespace perfetto

#endif  // PERFETTO_HAS_SUBPROCESS()
