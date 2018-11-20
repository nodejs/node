#ifndef JITDUMP_H
#define JITDUMP_H

#include <sys/time.h>
#include <time.h>
#include <stdint.h>

/* JiTD */
#define JITHEADER_MAGIC 0x4A695444
#define JITHEADER_MAGIC_SW 0x4454694A

#define PADDING_8ALIGNED(x) ((((x) + 7) & 7) ^ 7)

#define JITHEADER_VERSION 1

struct jitheader {
  uint32_t magic;      /* characters "jItD" */
  uint32_t version;    /* header version */
  uint32_t total_size; /* total size of header */
  uint32_t elf_mach;   /* elf mach target */
  uint32_t pad1;       /* reserved */
  uint32_t pid;        /* JIT process id */
  uint64_t timestamp;  /* timestamp */
};

enum jit_record_type {
  JIT_CODE_LOAD = 0,
  JIT_CODE_MOVE = 1,
  JIT_CODE_DEBUG_INFO = 2,
  JIT_CODE_CLOSE = 3,
  JIT_CODE_MAX
};

/* record prefix (mandatory in each record) */
struct jr_prefix {
  uint32_t id;
  uint32_t total_size;
  uint64_t timestamp;
};

struct jr_code_load {
  struct jr_prefix p;

  uint32_t pid;
  uint32_t tid;
  uint64_t vma;
  uint64_t code_addr;
  uint64_t code_size;
  uint64_t code_index;
};

struct jr_code_close {
  struct jr_prefix p;
};

struct jr_code_move {
  struct jr_prefix p;

  uint32_t pid;
  uint32_t tid;
  uint64_t vma;
  uint64_t old_code_addr;
  uint64_t new_code_addr;
  uint64_t code_size;
  uint64_t code_index;
};

struct jr_code_debug_info {
  struct jr_prefix p;

  uint64_t code_addr;
  uint64_t nr_entry;
};

union jr_entry {
  struct jr_code_debug_info info;
  struct jr_code_close close;
  struct jr_code_load load;
  struct jr_code_move move;
  struct jr_prefix prefix;
};

#endif /* !JITDUMP_H */
