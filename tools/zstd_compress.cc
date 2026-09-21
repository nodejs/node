// Host tool: compress a file with zstd for embedding in the node binary.
//
//   zstd_compress --icu <in> <out>
//
// `--icu` prefixes a little-endian header:
//   magic "ICUZ" | uint64 raw_size | uint64 compressed_size | zstd frame

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdint>

#include "zstd.h"

namespace {

void WriteU64LE(FILE* out, uint64_t value) {
  uint8_t bytes[8];
  for (int i = 0; i < 8; i++) {
    bytes[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xff);
  }
  fwrite(bytes, 1, 8, out);
}

int Fail(const char* message) {
  fprintf(stderr, "zstd_compress: %s\n", message);
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4 || strcmp(argv[1], "--icu") != 0) {
    fprintf(stderr, "usage: zstd_compress --icu <in> <out>\n");
    return 1;
  }

  FILE* in = fopen(argv[2], "rb");
  if (in == nullptr) {
    return Fail("open input");
  }
  if (fseek(in, 0, SEEK_END) != 0) {
    return Fail("seek");
  }
  long raw_long = ftell(in);
  if (raw_long < 0) {
    return Fail("ftell");
  }
  if (fseek(in, 0, SEEK_SET) != 0) {
    return Fail("rewind");
  }
  size_t raw_size = static_cast<size_t>(raw_long);
  uint8_t* raw = static_cast<uint8_t*>(malloc(raw_size));
  if (raw == nullptr || fread(raw, 1, raw_size, in) != raw_size) {
    return Fail("read");
  }
  fclose(in);

  size_t bound = ZSTD_compressBound(raw_size);
  uint8_t* compressed = static_cast<uint8_t*>(malloc(bound));
  if (compressed == nullptr) {
    return Fail("alloc");
  }
  size_t compressed_size =
      ZSTD_compress(compressed, bound, raw, raw_size, 19);
  if (ZSTD_isError(compressed_size)) {
    return Fail(ZSTD_getErrorName(compressed_size));
  }

  FILE* out = fopen(argv[3], "wb");
  if (out == nullptr) {
    return Fail("open output");
  }
  fwrite("ICUZ", 1, 4, out);
  WriteU64LE(out, raw_size);
  WriteU64LE(out, compressed_size);
  if (fwrite(compressed, 1, compressed_size, out) != compressed_size) {
    return Fail("write");
  }
  // genccode emits the file as 32-bit words and drops a short tail.
  size_t total = 20 + compressed_size;
  while (total % 16 != 0) {
    fputc(0, out);
    total++;
  }
  fclose(out);
  fprintf(stderr,
          "zstd_compress: %zu -> %zu bytes\n",
          raw_size,
          compressed_size);
  free(raw);
  free(compressed);
  return 0;
}
