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

#include <memory>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

constexpr auto kIdleSize = 10000 * 4096;
constexpr auto kNoIdleSize = 1000 * 4096;

static bool interrupted = false;

volatile unsigned char* __attribute__((noinline)) AllocIdle(size_t bytes);
volatile unsigned char* __attribute__((noinline)) AllocIdle(size_t bytes) {
  // This volatile is needed to prevent the compiler from trying to be
  // helpful and compiling a "useless" malloc + free into a noop.
  volatile unsigned char* x = static_cast<unsigned char*>(malloc(bytes));
  if (x) {
    x[1] = 'x';
  }
  return x;
}

volatile unsigned char* __attribute__((noinline)) AllocNoIdle(size_t bytes);
volatile unsigned char* __attribute__((noinline)) AllocNoIdle(size_t bytes) {
  // This volatile is needed to prevent the compiler from trying to be
  // helpful and compiling a "useless" malloc + free into a noop.
  volatile unsigned char* x = static_cast<unsigned char*>(malloc(bytes));
  if (x) {
    x[0] = 'x';
  }
  return x;
}

class MemoryToucher {
 public:
  virtual void Touch(volatile unsigned char* nonidle) = 0;
  virtual ~MemoryToucher() = default;
};

class ReadDevZeroChunks : public MemoryToucher {
 public:
  ReadDevZeroChunks(size_t chunk_size)
      : chunk_size_(chunk_size), fd_(open("/dev/zero", O_RDONLY)) {
    if (fd_ == -1) {
      fprintf(stderr, "Failed to open: %s", strerror(errno));
      abort();
    }
  }

  ~ReadDevZeroChunks() override = default;

  void Touch(volatile unsigned char* nonidle) override {
    size_t total_rd = 0;
    while (total_rd < kNoIdleSize) {
      size_t chunk = chunk_size_;
      if (chunk > kNoIdleSize - total_rd)
        chunk = kNoIdleSize - total_rd;

      ssize_t rd =
          read(fd_, const_cast<unsigned char*>(nonidle) + total_rd, chunk);
      if (rd == -1) {
        fprintf(stderr, "Failed to write: %s.", strerror(errno));
        abort();
      }
      total_rd += static_cast<size_t>(rd);
    }
  }

 private:
  size_t chunk_size_;
  int fd_;
};

class ReadDevZeroChunksAndSleep : public ReadDevZeroChunks {
 public:
  ReadDevZeroChunksAndSleep(size_t chunk_size)
      : ReadDevZeroChunks(chunk_size) {}
  void Touch(volatile unsigned char* nonidle) override {
    ReadDevZeroChunks::Touch(nonidle);
    sleep(1);
  }
};

class SumUp : public MemoryToucher {
 public:
  SumUp()
      : sum_(const_cast<volatile uint64_t*>(
            static_cast<uint64_t*>(malloc(sizeof(uint64_t))))) {}
  ~SumUp() override = default;

  void Touch(volatile unsigned char* nonidle) override {
    for (size_t i = 0; i < kNoIdleSize; ++i)
      *sum_ += nonidle[i];
  }

 private:
  volatile uint64_t* sum_;
};

class ReadDevZeroChunksAndSum : public ReadDevZeroChunks {
 public:
  ReadDevZeroChunksAndSum(size_t chunk_size) : ReadDevZeroChunks(chunk_size) {}
  void Touch(volatile unsigned char* nonidle) override {
    ReadDevZeroChunks::Touch(nonidle);
    sum_up_.Touch(nonidle);
  }

 private:
  SumUp sum_up_;
};

class AssignValues : public MemoryToucher {
 public:
  ~AssignValues() override = default;

  void Touch(volatile unsigned char* nonidle) override {
    for (size_t i = 0; i < kNoIdleSize; ++i)
      nonidle[i] = static_cast<unsigned char>(i % 256);
  }
};

}  // namespace

int main(int argc, char** argv) {
  volatile auto* idle = AllocIdle(kIdleSize);
  volatile auto* nonidle = AllocNoIdle(kNoIdleSize);

  printf("Own PID: %" PRIdMAX "\n", static_cast<intmax_t>(getpid()));
  printf("Idle: %p\n", static_cast<void*>(const_cast<unsigned char*>(idle)));
  printf("Nonidle: %p\n",
         static_cast<void*>(const_cast<unsigned char*>(nonidle)));

  for (size_t i = 0; i < kIdleSize; ++i)
    idle[i] = static_cast<unsigned char>(i % 256);
  for (size_t i = 0; i < kNoIdleSize; ++i)
    nonidle[i] = static_cast<unsigned char>(i % 256);

  printf("Allocated everything.\n");

  struct sigaction action = {};
  action.sa_handler = [](int) { interrupted = true; };
  if (sigaction(SIGUSR1, &action, nullptr) != 0) {
    fprintf(stderr, "Failed to register signal handler.\n");
    abort();
  }

  if (argc < 2) {
    fprintf(stderr,
            "Specifiy one of AssignValues / SumUp / ReadDevZeroChunks\n");
    abort();
  }

  std::unique_ptr<MemoryToucher> toucher;
  if (strcmp(argv[1], "AssignValues") == 0) {
    toucher.reset(new AssignValues());
    printf("Using AssignValues.\n");
  } else if (strcmp(argv[1], "SumUp") == 0) {
    toucher.reset(new SumUp());
    printf("Using SumUp.\n");
  } else if (strcmp(argv[1], "ReadDevZeroChunks") == 0 ||
             strcmp(argv[1], "ReadDevZeroChunksAndSleep") == 0 ||
             strcmp(argv[1], "ReadDevZeroChunksAndSum") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Specify chunk size.\n");
      abort();
    }
    char* end;
    long long chunk_arg = strtoll(argv[2], &end, 10);
    if (*end != '\0' || *argv[2] == '\0') {
      fprintf(stderr, "Invalid chunk size: %s\n", argv[2]);
      abort();
    }
    if (strcmp(argv[1], "ReadDevZeroChunksAndSleep") == 0) {
      printf("Using ReadDevZeroChunksAndSleep.\n");
      toucher.reset(
          new ReadDevZeroChunksAndSleep(static_cast<size_t>(chunk_arg)));
    } else if (strcmp(argv[1], "ReadDevZeroChunksAndSum") == 0) {
      printf("Using ReadDevZeroChunksAndSum.\n");
      toucher.reset(
          new ReadDevZeroChunksAndSum(static_cast<size_t>(chunk_arg)));
    } else {
      printf("Using ReadDevZeroChunks.\n");
      toucher.reset(new ReadDevZeroChunks(static_cast<size_t>(chunk_arg)));
    }
  } else {
    fprintf(stderr, "Invalid input.\n");
    abort();
  }

  while (true) {
    bool report = interrupted;
    if (report) {
      printf("Waiting to finish touching everything.\n");
      interrupted = false;
      sleep(2);
    }

    toucher->Touch(nonidle);

    if (report)
      printf("Touched everything.\n");
  }
}
