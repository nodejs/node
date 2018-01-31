// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int main(int argc, char* argv[]) {
  if (LLVMFuzzerInitialize(&argc, &argv)) {
    fprintf(stderr, "Failed to initialize fuzzer target\n");
    return 1;
  }

  if (argc < 2) {
    fprintf(stderr, "USAGE: %s <input>\n", argv[0]);
    return 1;
  }

  FILE* input = fopen(argv[1], "rb");

  if (!input) {
    fprintf(stderr, "Failed to open '%s'\n", argv[1]);
    return 1;
  }

  fseek(input, 0, SEEK_END);
  size_t size = ftell(input);
  fseek(input, 0, SEEK_SET);

  uint8_t* data = reinterpret_cast<uint8_t*>(malloc(size));
  if (!data) {
    fclose(input);
    fprintf(stderr, "Failed to allocate %zu bytes\n", size);
    return 1;
  }

  size_t bytes_read = fread(data, 1, size, input);
  fclose(input);

  if (bytes_read != static_cast<size_t>(size)) {
    free(data);
    fprintf(stderr, "Failed to read %s\n", argv[1]);
    return 1;
  }

  int result = LLVMFuzzerTestOneInput(data, size);

  free(data);

  return result;
}
