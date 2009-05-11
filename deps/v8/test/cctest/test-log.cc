// Copyright 2006-2009 the V8 project authors. All rights reserved.
//
// Tests of logging functions from log.h

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "v8.h"

#include "log.h"

#include "cctest.h"

using v8::internal::Logger;

static void SetUp() {
  // Log to memory buffer.
  v8::internal::FLAG_logfile = "*";
  v8::internal::FLAG_log = true;
  Logger::Setup();
}

static void TearDown() {
  Logger::TearDown();
}


TEST(EmptyLog) {
  SetUp();
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 0));
  CHECK_EQ(0, Logger::GetLogLines(100, NULL, 0));
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 100));
  CHECK_EQ(0, Logger::GetLogLines(100, NULL, 100));
  TearDown();
}


TEST(GetMessages) {
  SetUp();
  Logger::StringEvent("aaa", "bbb");
  Logger::StringEvent("cccc", "dddd");
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 0));
  char log_lines[100];
  memset(log_lines, 0, sizeof(log_lines));
  // Requesting data size which is smaller than first log message length.
  CHECK_EQ(0, Logger::GetLogLines(0, log_lines, 3));
  // See Logger::StringEvent.
  const char* line_1 = "aaa,\"bbb\"\n";
  const int line_1_len = strlen(line_1);
  // Still smaller than log message length.
  CHECK_EQ(0, Logger::GetLogLines(0, log_lines, line_1_len - 1));
  // The exact size.
  CHECK_EQ(line_1_len, Logger::GetLogLines(0, log_lines, line_1_len));
  CHECK_EQ(line_1, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  // A bit more than the first line length.
  CHECK_EQ(line_1_len, Logger::GetLogLines(0, log_lines, line_1_len + 3));
  CHECK_EQ(line_1, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  const char* line_2 = "cccc,\"dddd\"\n";
  const int line_2_len = strlen(line_2);
  // Now start with line_2 beginning.
  CHECK_EQ(0, Logger::GetLogLines(line_1_len, log_lines, 0));
  CHECK_EQ(0, Logger::GetLogLines(line_1_len, log_lines, 3));
  CHECK_EQ(0, Logger::GetLogLines(line_1_len, log_lines, line_2_len - 1));
  CHECK_EQ(line_2_len, Logger::GetLogLines(line_1_len, log_lines, line_2_len));
  CHECK_EQ(line_2, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  CHECK_EQ(line_2_len,
           Logger::GetLogLines(line_1_len, log_lines, line_2_len + 3));
  CHECK_EQ(line_2, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  // Now get entire buffer contents.
  const char* all_lines = "aaa,\"bbb\"\ncccc,\"dddd\"\n";
  const int all_lines_len = strlen(all_lines);
  CHECK_EQ(all_lines_len, Logger::GetLogLines(0, log_lines, all_lines_len));
  CHECK_EQ(all_lines, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  CHECK_EQ(all_lines_len, Logger::GetLogLines(0, log_lines, all_lines_len + 3));
  CHECK_EQ(all_lines, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  TearDown();
}


TEST(BeyondWritePosition) {
  SetUp();
  Logger::StringEvent("aaa", "bbb");
  Logger::StringEvent("cccc", "dddd");
  // See Logger::StringEvent.
  const char* all_lines = "aaa,\"bbb\"\ncccc,\"dddd\"\n";
  const int all_lines_len = strlen(all_lines);
  CHECK_EQ(0, Logger::GetLogLines(all_lines_len, NULL, 1));
  CHECK_EQ(0, Logger::GetLogLines(all_lines_len, NULL, 100));
  CHECK_EQ(0, Logger::GetLogLines(all_lines_len + 1, NULL, 1));
  CHECK_EQ(0, Logger::GetLogLines(all_lines_len + 1, NULL, 100));
  CHECK_EQ(0, Logger::GetLogLines(all_lines_len + 100, NULL, 1));
  CHECK_EQ(0, Logger::GetLogLines(all_lines_len + 100, NULL, 100));
  CHECK_EQ(0, Logger::GetLogLines(10 * 1024 * 1024, NULL, 1));
  CHECK_EQ(0, Logger::GetLogLines(10 * 1024 * 1024, NULL, 100));
  TearDown();
}


TEST(MemoryLoggingTurnedOff) {
  // Log to stdout
  v8::internal::FLAG_logfile = "-";
  v8::internal::FLAG_log = true;
  Logger::Setup();
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 0));
  CHECK_EQ(0, Logger::GetLogLines(100, NULL, 0));
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 100));
  CHECK_EQ(0, Logger::GetLogLines(100, NULL, 100));
  Logger::TearDown();
}


#endif  // ENABLE_LOGGING_AND_PROFILING
