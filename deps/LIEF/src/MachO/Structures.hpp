/* Copyright 2021 - 2025 R. Thomas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// From llvm/Support/MachO.h - The MachO file format
#ifndef LIEF_MACHO_STRUCTURES_H_
#define LIEF_MACHO_STRUCTURES_H_

#include <cstdint>
#include <string>

#include "LIEF/types.hpp"

#include "LIEF/MachO/enums.hpp"


// Swap 2 byte, 16 bit values:
#define Swap2Bytes(val) \
 ( (((val) >> 8) & 0x00FF) | (((val) << 8) & 0xFF00) )


// Swap 4 byte, 32 bit values:
#define Swap4Bytes(val) \
 ( (((val) >> 24) & 0x000000FF) | (((val) >>  8) & 0x0000FF00) | \
   (((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000) )



// Swap 8 byte, 64 bit values:
#define Swap8Bytes(val) \
 ( (((val) >> 56) & 0x00000000000000FF) | (((val) >> 40) & 0x000000000000FF00) | \
   (((val) >> 24) & 0x0000000000FF0000) | (((val) >>  8) & 0x00000000FF000000) | \
   (((val) <<  8) & 0x000000FF00000000) | (((val) << 24) & 0x0000FF0000000000) | \
   (((val) << 40) & 0x00FF000000000000) | (((val) << 56) & 0xFF00000000000000) )

namespace LIEF {
/// Namespace related to the LIEF's MachO module
namespace MachO {

namespace details {

static constexpr uint32_t INDIRECT_SYMBOL_LOCAL = 0x80000000;
static constexpr uint32_t INDIRECT_SYMBOL_ABS   = 0x40000000;

struct mach_header {
  uint32_t magic;
  uint32_t cputype;
  uint32_t cpusubtype;
  uint32_t filetype;
  uint32_t ncmds;
  uint32_t sizeofcmds;
  uint32_t flags;
  //uint32_t reserved; not for 32 bits
};

struct mach_header_64 {
  uint32_t magic;
  uint32_t cputype;
  uint32_t cpusubtype;
  uint32_t filetype;
  uint32_t ncmds;
  uint32_t sizeofcmds;
  uint32_t flags;
  uint32_t reserved;
};

struct load_command {
  uint32_t cmd;
  uint32_t cmdsize;
};

struct segment_command_32 {
  uint32_t cmd;
  uint32_t cmdsize;
  char     segname[16];
  uint32_t vmaddr;
  uint32_t vmsize;
  uint32_t fileoff;
  uint32_t filesize;
  uint32_t maxprot;
  uint32_t initprot;
  uint32_t nsects;
  uint32_t flags;
};

struct segment_command_64 {
  uint32_t cmd;
  uint32_t cmdsize;
  char     segname[16];
  uint64_t vmaddr;
  uint64_t vmsize;
  uint64_t fileoff;
  uint64_t filesize;
  uint32_t maxprot;
  uint32_t initprot;
  uint32_t nsects;
  uint32_t flags;
};

struct section_32 {
  char sectname[16];
  char segname[16];
  uint32_t addr;
  uint32_t size;
  uint32_t offset;
  uint32_t align;
  uint32_t reloff;
  uint32_t nreloc;
  uint32_t flags;
  uint32_t reserved1;
  uint32_t reserved2;
};

struct section_64 {
  char sectname[16];
  char segname[16];
  uint64_t addr;
  uint64_t size;
  uint32_t offset;
  uint32_t align;
  uint32_t reloff;
  uint32_t nreloc;
  uint32_t flags;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
};

struct dylib_command {
  uint32_t cmd;
  uint32_t cmdsize;

  uint32_t name;
  uint32_t timestamp;
  uint32_t current_version;
  uint32_t compatibility_version;
};

struct sub_framework_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t umbrella;
};

struct sub_client_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t client;
};

struct sub_umbrella_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t sub_umbrella;
};

struct sub_library_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t sub_library;
};

struct prebound_dylib_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t name;
  uint32_t nmodules;
  uint32_t linked_modules;
};

struct dylinker_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t name;
};

struct thread_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t flavor;
  uint32_t count;
};

struct routines_command_32 {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t init_address;
  uint32_t init_module;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
  uint32_t reserved4;
  uint32_t reserved5;
  uint32_t reserved6;
};

struct routines_command_64 {
  uint32_t cmd;
  uint32_t cmdsize;
  uint64_t init_address;
  uint64_t init_module;
  uint64_t reserved1;
  uint64_t reserved2;
  uint64_t reserved3;
  uint64_t reserved4;
  uint64_t reserved5;
  uint64_t reserved6;
};

struct symtab_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t symoff;
  uint32_t nsyms;
  uint32_t stroff;
  uint32_t strsize;
};

struct dysymtab_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t ilocalsym;
  uint32_t nlocalsym;
  uint32_t iextdefsym;
  uint32_t nextdefsym;
  uint32_t iundefsym;
  uint32_t nundefsym;
  uint32_t tocoff;
  uint32_t ntoc;
  uint32_t modtaboff;
  uint32_t nmodtab;
  uint32_t extrefsymoff;
  uint32_t nextrefsyms;
  uint32_t indirectsymoff;
  uint32_t nindirectsyms;
  uint32_t extreloff;
  uint32_t nextrel;
  uint32_t locreloff;
  uint32_t nlocrel;
};

struct dylib_table_of_contents {
  uint32_t symbol_index;
  uint32_t module_index;
};

struct dylib_module_32 {
  uint32_t module_name;
  uint32_t iextdefsym;
  uint32_t nextdefsym;
  uint32_t irefsym;
  uint32_t nrefsym;
  uint32_t ilocalsym;
  uint32_t nlocalsym;
  uint32_t iextrel;
  uint32_t nextrel;
  uint32_t iinit_iterm;
  uint32_t ninit_nterm;
  uint32_t objc_module_info_addr;
  uint32_t objc_module_info_size;
};

struct dylib_module_64 {
  uint32_t module_name;
  uint32_t iextdefsym;
  uint32_t nextdefsym;
  uint32_t irefsym;
  uint32_t nrefsym;
  uint32_t ilocalsym;
  uint32_t nlocalsym;
  uint32_t iextrel;
  uint32_t nextrel;
  uint32_t iinit_iterm;
  uint32_t ninit_nterm;
  uint32_t objc_module_info_size;
  uint64_t objc_module_info_addr;
};

struct dylib_reference {
  uint32_t isym:24,
           flags:8;
};

struct twolevel_hints_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t offset;
  uint32_t nhints;
};

struct twolevel_hint {
  uint32_t isub_image:8,
           itoc:24;
};

static_assert(sizeof(twolevel_hint) == sizeof(uint32_t), "Wrong size");

struct prebind_cksum_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t cksum;
};

struct uuid_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint8_t uuid[16];
};

struct rpath_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t path;
};

struct linkedit_data_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t dataoff;
  uint32_t datasize;
};

struct data_in_code_entry {
  uint32_t offset;
  uint16_t length;
  uint16_t kind;
};

struct source_version_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint64_t version;
};

struct encryption_info_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t cryptoff;
  uint32_t cryptsize;
  uint32_t cryptid;
};

struct version_min_command {
  uint32_t cmd;       // LC_VERSION_MIN_MACOSX or
                      // LC_VERSION_MIN_IPHONEOS
  uint32_t cmdsize;   // sizeof(struct version_min_command)
  uint32_t version;   // X.Y.Z is encoded in nibbles xxxx.yy.zz
  uint32_t sdk;       // X.Y.Z is encoded in nibbles xxxx.yy.zz
};


struct build_version_command {
  uint32_t cmd;
  uint32_t cmdsize;

  uint32_t platform;
  uint32_t minos;
  uint32_t sdk;
  uint32_t ntools;
};

struct build_tool_version {
  uint32_t tool;
  uint32_t version;
};

struct dyld_info_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t rebase_off;
  uint32_t rebase_size;
  uint32_t bind_off;
  uint32_t bind_size;
  uint32_t weak_bind_off;
  uint32_t weak_bind_size;
  uint32_t lazy_bind_off;
  uint32_t lazy_bind_size;
  uint32_t export_off;
  uint32_t export_size;
};

struct linker_option_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t count;
};

struct note_command {
  uint32_t cmd;        /* LC_NOTE */
  uint32_t cmdsize;    /* sizeof(struct note_command) */
  char data_owner[16]; /* owner name for this LC_NOTE */
  uint64_t offset;     /* file offset of this data */
  uint64_t size;       /* length of data region */
};

struct symseg_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t offset;
  uint32_t size;
};

struct ident_command {
  uint32_t cmd;
  uint32_t cmdsize;
};

struct fvmfile_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t name;
  uint32_t header_addr;
};

struct tlv_descriptor_32 {
  uint32_t thunk;
  uint32_t key;
  uint32_t offset;
};

struct tlv_descriptor_64 {
  uint64_t thunk;
  uint64_t key;
  uint64_t offset;
};

struct tlv_descriptor {
  uintptr_t thunk;
  uintptr_t key;
  uintptr_t offset;
};

struct entry_point_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint64_t entryoff;
  uint64_t stacksize;
};

struct fileset_entry_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint64_t vmaddr;
  uint64_t fileoff;
  uint32_t entry_id;
  uint32_t reserved;
};

// Structs from <mach-o/fat.h>
struct fat_header {
  uint32_t magic;
  uint32_t nfat_arch;
};

struct fat_arch {
  uint32_t cputype;
  uint32_t cpusubtype;
  uint32_t offset;
  uint32_t size;
  uint32_t align;
};

// Structs from <mach-o/reloc.h>
struct relocation_info {
  int32_t r_address;
  uint32_t r_symbolnum:24,
           r_pcrel:1,
           r_length:2,
           r_extern:1,
           r_type:4;
};


struct scattered_relocation_info {
  uint32_t r_address:24,
           r_type:4,
           r_length:2,
           r_pcrel:1,
           r_scattered:1;
  int32_t r_value;
};


// Structs NOT from <mach-o/reloc.h>, but that make LLVM's life easier
struct any_relocation_info {
  uint32_t r_word0, r_word1;
};

// Structs from <mach-o/nlist.h>
struct nlist_base {
  uint32_t n_strx;
  uint8_t n_type;
  uint8_t n_sect;
  uint16_t n_desc;
};

struct nlist_32 {
  uint32_t n_strx;
  uint8_t n_type;
  uint8_t n_sect;
  int16_t n_desc;
  uint32_t n_value;
};

struct nlist_64 {
  uint32_t n_strx;
  uint8_t n_type;
  uint8_t n_sect;
  uint16_t n_desc;
  uint64_t n_value;
};


struct x86_thread_state64_t {
  uint64_t rax;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rsp;
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
  uint64_t rip;
  uint64_t rflags;
  uint64_t cs;
  uint64_t fs;
  uint64_t gs;
};

struct x86_thread_state_t {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp;
  uint32_t ss;
  uint32_t eflags;
  uint32_t eip;
  uint32_t cs;
  uint32_t ds;
  uint32_t es;
  uint32_t fs;
  uint32_t gs;
};

struct arm_thread_state_t {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t r12;
  uint32_t r13;
  uint32_t r14;
  uint32_t r15;
  uint32_t r16;   /* Apple's thread_state has this 17th reg, bug?? */
};

struct arm_thread_state64_t {
  uint64_t x[29]; // x0-x28
  uint64_t fp;    // x29
  uint64_t lr;    // x30
  uint64_t sp;    // x31
  uint64_t pc;    // pc
  uint32_t cpsr;  // cpsr
};

struct ppc_thread_state_t {
  uint32_t srr0; /* Instruction address register (PC) */
  uint32_t srr1; /* Machine state register (supervisor) */
  uint32_t r[32];

  uint32_t cr;  /* Condition register */
  uint32_t xer; /* User's integer exception register */
  uint32_t lr;  /* Link register */
  uint32_t ctr; /* Count register */
  uint32_t mq;  /* MQ register (601 only) */

  uint32_t vrsave; /* Vector Save Register */
};

struct ppc_thread_state64_t {
  uint64_t srr0;
  uint64_t srr1;
  uint64_t r[32];
  uint32_t cr;
  uint64_t xer;
  uint64_t lr;
  uint64_t ctr;
  uint32_t vrsave;
};


struct code_directory {
  uint32_t version;
  uint32_t flags;
  uint32_t hash_offset;
  uint32_t ident_offset;
  uint32_t nb_special_slots;
  uint32_t nb_code_slots;
  uint32_t code_limit;
  uint8_t  hash_size;
  uint8_t  hash_type;
  uint8_t  reserved;
  uint8_t  page_size;
  uint32_t reserved2;
  uint32_t scatter_offset;
};


// Unwind structures
// =================
// Taken from libunwind-35.3/include/mach-o/compact_unwind_encoding.h
struct unwind_info_section_header {
  uint32_t version;            // unwind_section_version
  uint32_t common_encodings_array_section_offset;
  uint32_t common_encodings_arraycount;
  uint32_t personality_array_section_offset;
  uint32_t personality_array_count;
  uint32_t index_section_offset;
  uint32_t index_count;
  // compact_unwind_encoding_t[]
  // uintptr_t personalities[]
  // unwind_info_section_header_index_entry[]
  // unwind_info_section_header_lsda_index_entry[]
};


struct unwind_info_section_header_index_entry {
  uint32_t function_offset;
  uint32_t second_level_pages_section_offset;  // section offset to start of regular or compress page
  uint32_t lsda_index_array_section_offset;    // section offset to start of lsda_index array for this range
};

struct unwind_info_section_header_lsda_index_entry {
  uint32_t function_offset;
  uint32_t lsda_offset;
};


struct unwind_info_regular_second_level_entry {
  uint32_t function_offset;
  uint32_t encoding;
};

struct unwind_info_regular_second_level_page_header {
  uint32_t kind;    // UNWIND_SECOND_LEVEL_REGULAR
  uint16_t entry_page_offset;
  uint16_t entry_count;
  // entry array
};


struct unwind_info_compressed_second_level_page_header {
  uint32_t kind;    // unwind_second_level_compressed
  uint16_t entry_page_offset;
  uint16_t entry_count;
  uint16_t encodings_page_offset;
  uint16_t encodings_count;
  // 32-bit entry array
  // encodings array
};


struct rebase_instruction {
  rebase_instruction(uint8_t opcode, uint64_t op1, uint64_t op2 = 0) :
    opcode{opcode},
    op1{op1},
    op2{op2}
  {}

  uint8_t opcode;
  uint64_t op1;
  uint64_t op2;
};

struct binding_instruction {
  binding_instruction(uint8_t opcode, uint64_t op1, uint64_t op2 = 0, std::string name = "") :
    opcode{opcode},
    op1{op1},
    op2{op2},
    name{name}
  {}
  binding_instruction(const binding_instruction&) = default;
  binding_instruction& operator=(const binding_instruction&) = default;


  uint8_t opcode;
  uint64_t op1;
  uint64_t op2;
  std::string name;
};


class MachO32 {
  public:
  using header                  = mach_header;
  using segment_command         = segment_command_32;
  using section                 = section_32;
  using routines_command        = routines_command_32;
  using dylib_module            = dylib_module_32;
  using nlist                   = nlist_32;

  using uint                    = uint32_t;
};

class MachO64 {
  public:
  using header                  = mach_header_64;
  using segment_command         = segment_command_64;
  using section                 = section_64;
  using routines_command        = routines_command_64;
  using dylib_module            = dylib_module_64;
  using nlist                   = nlist_64;

  using uint                    = uint64_t;
};
}
} // end namespace MachO
}
#endif
