/*
** Client for the GDB JIT API.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_gdbjit_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_frame.h"
#include "lj_jit.h"
#include "lj_dispatch.h"

/* This is not compiled in by default.
** Enable with -DLUAJIT_USE_GDBJIT in the Makefile and recompile everything.
*/
#ifdef LUAJIT_USE_GDBJIT

/* The GDB JIT API allows JIT compilers to pass debug information about
** JIT-compiled code back to GDB. You need at least GDB 7.0 or higher
** to see it in action.
**
** This is a passive API, so it works even when not running under GDB
** or when attaching to an already running process. Alas, this implies
** enabling it always has a non-negligible overhead -- do not use in
** release mode!
**
** The LuaJIT GDB JIT client is rather minimal at the moment. It gives
** each trace a symbol name and adds a source location and frame unwind
** information. Obviously LuaJIT itself and any embedding C application
** should be compiled with debug symbols, too (see the Makefile).
**
** Traces are named TRACE_1, TRACE_2, ... these correspond to the trace
** numbers from -jv or -jdump. Use "break TRACE_1" or "tbreak TRACE_1" etc.
** to set breakpoints on specific traces (even ahead of their creation).
**
** The source location for each trace allows listing the corresponding
** source lines with the GDB command "list" (but only if the Lua source
** has been loaded from a file). Currently this is always set to the
** location where the trace has been started.
**
** Frame unwind information can be inspected with the GDB command
** "info frame". This also allows proper backtraces across JIT-compiled
** code with the GDB command "bt".
**
** You probably want to add the following settings to a .gdbinit file
** (or add them to ~/.gdbinit):
**   set disassembly-flavor intel
**   set breakpoint pending on
**
** Here's a sample GDB session:
** ------------------------------------------------------------------------

$ cat >x.lua
for outer=1,100 do
  for inner=1,100 do end
end
^D

$ luajit -jv x.lua
[TRACE   1 x.lua:2]
[TRACE   2 (1/3) x.lua:1 -> 1]

$ gdb --quiet --args luajit x.lua
(gdb) tbreak TRACE_1
Function "TRACE_1" not defined.
Temporary breakpoint 1 (TRACE_1) pending.
(gdb) run
Starting program: luajit x.lua

Temporary breakpoint 1, TRACE_1 () at x.lua:2
2	  for inner=1,100 do end
(gdb) list
1	for outer=1,100 do
2	  for inner=1,100 do end
3	end
(gdb) bt
#0  TRACE_1 () at x.lua:2
#1  0x08053690 in lua_pcall [...]
[...]
#7  0x0806ff90 in main [...]
(gdb) disass TRACE_1
Dump of assembler code for function TRACE_1:
0xf7fd9fba <TRACE_1+0>:	mov    DWORD PTR ds:0xf7e0e2a0,0x1
0xf7fd9fc4 <TRACE_1+10>:	movsd  xmm7,QWORD PTR [edx+0x20]
[...]
0xf7fd9ff8 <TRACE_1+62>:	jmp    0xf7fd2014
End of assembler dump.
(gdb) tbreak TRACE_2
Function "TRACE_2" not defined.
Temporary breakpoint 2 (TRACE_2) pending.
(gdb) cont
Continuing.

Temporary breakpoint 2, TRACE_2 () at x.lua:1
1	for outer=1,100 do
(gdb) info frame
Stack level 0, frame at 0xffffd7c0:
 eip = 0xf7fd9f60 in TRACE_2 (x.lua:1); saved eip 0x8053690
 called by frame at 0xffffd7e0
 source language unknown.
 Arglist at 0xffffd78c, args:
 Locals at 0xffffd78c, Previous frame's sp is 0xffffd7c0
 Saved registers:
  ebx at 0xffffd7ac, ebp at 0xffffd7b8, esi at 0xffffd7b0, edi at 0xffffd7b4,
  eip at 0xffffd7bc
(gdb)

** ------------------------------------------------------------------------
*/

/* -- GDB JIT API --------------------------------------------------------- */

/* GDB JIT actions. */
enum {
  GDBJIT_NOACTION = 0,
  GDBJIT_REGISTER,
  GDBJIT_UNREGISTER
};

/* GDB JIT entry. */
typedef struct GDBJITentry {
  struct GDBJITentry *next_entry;
  struct GDBJITentry *prev_entry;
  const char *symfile_addr;
  uint64_t symfile_size;
} GDBJITentry;

/* GDB JIT descriptor. */
typedef struct GDBJITdesc {
  uint32_t version;
  uint32_t action_flag;
  GDBJITentry *relevant_entry;
  GDBJITentry *first_entry;
} GDBJITdesc;

GDBJITdesc __jit_debug_descriptor = {
  1, GDBJIT_NOACTION, NULL, NULL
};

/* GDB sets a breakpoint at this function. */
void LJ_NOINLINE __jit_debug_register_code()
{
  __asm__ __volatile__("");
};

/* -- In-memory ELF object definitions ------------------------------------ */

/* ELF definitions. */
typedef struct ELFheader {
  uint8_t emagic[4];
  uint8_t eclass;
  uint8_t eendian;
  uint8_t eversion;
  uint8_t eosabi;
  uint8_t eabiversion;
  uint8_t epad[7];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uintptr_t entry;
  uintptr_t phofs;
  uintptr_t shofs;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstridx;
} ELFheader;

typedef struct ELFsectheader {
  uint32_t name;
  uint32_t type;
  uintptr_t flags;
  uintptr_t addr;
  uintptr_t ofs;
  uintptr_t size;
  uint32_t link;
  uint32_t info;
  uintptr_t align;
  uintptr_t entsize;
} ELFsectheader;

#define ELFSECT_IDX_ABS		0xfff1

enum {
  ELFSECT_TYPE_PROGBITS = 1,
  ELFSECT_TYPE_SYMTAB = 2,
  ELFSECT_TYPE_STRTAB = 3,
  ELFSECT_TYPE_NOBITS = 8
};

#define ELFSECT_FLAGS_WRITE	1
#define ELFSECT_FLAGS_ALLOC	2
#define ELFSECT_FLAGS_EXEC	4

typedef struct ELFsymbol {
#if LJ_64
  uint32_t name;
  uint8_t info;
  uint8_t other;
  uint16_t sectidx;
  uintptr_t value;
  uint64_t size;
#else
  uint32_t name;
  uintptr_t value;
  uint32_t size;
  uint8_t info;
  uint8_t other;
  uint16_t sectidx;
#endif
} ELFsymbol;

enum {
  ELFSYM_TYPE_FUNC = 2,
  ELFSYM_TYPE_FILE = 4,
  ELFSYM_BIND_LOCAL = 0 << 4,
  ELFSYM_BIND_GLOBAL = 1 << 4,
};

/* DWARF definitions. */
#define DW_CIE_VERSION	1

enum {
  DW_CFA_nop = 0x0,
  DW_CFA_offset_extended = 0x5,
  DW_CFA_def_cfa = 0xc,
  DW_CFA_def_cfa_offset = 0xe,
  DW_CFA_offset_extended_sf = 0x11,
  DW_CFA_advance_loc = 0x40,
  DW_CFA_offset = 0x80
};

enum {
  DW_EH_PE_udata4 = 3,
  DW_EH_PE_textrel = 0x20
};

enum {
  DW_TAG_compile_unit = 0x11
};

enum {
  DW_children_no = 0,
  DW_children_yes = 1
};

enum {
  DW_AT_name = 0x03,
  DW_AT_stmt_list = 0x10,
  DW_AT_low_pc = 0x11,
  DW_AT_high_pc = 0x12
};

enum {
  DW_FORM_addr = 0x01,
  DW_FORM_data4 = 0x06,
  DW_FORM_string = 0x08
};

enum {
  DW_LNS_extended_op = 0,
  DW_LNS_copy = 1,
  DW_LNS_advance_pc = 2,
  DW_LNS_advance_line = 3
};

enum {
  DW_LNE_end_sequence = 1,
  DW_LNE_set_address = 2
};

enum {
#if LJ_TARGET_X86
  DW_REG_AX, DW_REG_CX, DW_REG_DX, DW_REG_BX,
  DW_REG_SP, DW_REG_BP, DW_REG_SI, DW_REG_DI,
  DW_REG_RA,
#elif LJ_TARGET_X64
  /* Yes, the order is strange, but correct. */
  DW_REG_AX, DW_REG_DX, DW_REG_CX, DW_REG_BX,
  DW_REG_SI, DW_REG_DI, DW_REG_BP, DW_REG_SP,
  DW_REG_8, DW_REG_9, DW_REG_10, DW_REG_11,
  DW_REG_12, DW_REG_13, DW_REG_14, DW_REG_15,
  DW_REG_RA,
#elif LJ_TARGET_ARM
  DW_REG_SP = 13,
  DW_REG_RA = 14,
#elif LJ_TARGET_PPC
  DW_REG_SP = 1,
  DW_REG_RA = 65,
  DW_REG_CR = 70,
#elif LJ_TARGET_MIPS
  DW_REG_SP = 29,
  DW_REG_RA = 31,
#else
#error "Unsupported target architecture"
#endif
};

/* Minimal list of sections for the in-memory ELF object. */
enum {
  GDBJIT_SECT_NULL,
  GDBJIT_SECT_text,
  GDBJIT_SECT_eh_frame,
  GDBJIT_SECT_shstrtab,
  GDBJIT_SECT_strtab,
  GDBJIT_SECT_symtab,
  GDBJIT_SECT_debug_info,
  GDBJIT_SECT_debug_abbrev,
  GDBJIT_SECT_debug_line,
  GDBJIT_SECT__MAX
};

enum {
  GDBJIT_SYM_UNDEF,
  GDBJIT_SYM_FILE,
  GDBJIT_SYM_FUNC,
  GDBJIT_SYM__MAX
};

/* In-memory ELF object. */
typedef struct GDBJITobj {
  ELFheader hdr;			/* ELF header. */
  ELFsectheader sect[GDBJIT_SECT__MAX];	/* ELF sections. */
  ELFsymbol sym[GDBJIT_SYM__MAX];	/* ELF symbol table. */
  uint8_t space[4096];			/* Space for various section data. */
} GDBJITobj;

/* Combined structure for GDB JIT entry and ELF object. */
typedef struct GDBJITentryobj {
  GDBJITentry entry;
  size_t sz;
  GDBJITobj obj;
} GDBJITentryobj;

/* Template for in-memory ELF header. */
static const ELFheader elfhdr_template = {
  .emagic = { 0x7f, 'E', 'L', 'F' },
  .eclass = LJ_64 ? 2 : 1,
  .eendian = LJ_ENDIAN_SELECT(1, 2),
  .eversion = 1,
#if LJ_TARGET_LINUX
  .eosabi = 0,  /* Nope, it's not 3. */
#elif defined(__FreeBSD__)
  .eosabi = 9,
#elif defined(__NetBSD__)
  .eosabi = 2,
#elif defined(__OpenBSD__)
  .eosabi = 12,
#elif (defined(__sun__) && defined(__svr4__))
  .eosabi = 6,
#else
  .eosabi = 0,
#endif
  .eabiversion = 0,
  .epad = { 0, 0, 0, 0, 0, 0, 0 },
  .type = 1,
#if LJ_TARGET_X86
  .machine = 3,
#elif LJ_TARGET_X64
  .machine = 62,
#elif LJ_TARGET_ARM
  .machine = 40,
#elif LJ_TARGET_PPC
  .machine = 20,
#elif LJ_TARGET_MIPS
  .machine = 8,
#else
#error "Unsupported target architecture"
#endif
  .version = 1,
  .entry = 0,
  .phofs = 0,
  .shofs = offsetof(GDBJITobj, sect),
  .flags = 0,
  .ehsize = sizeof(ELFheader),
  .phentsize = 0,
  .phnum = 0,
  .shentsize = sizeof(ELFsectheader),
  .shnum = GDBJIT_SECT__MAX,
  .shstridx = GDBJIT_SECT_shstrtab
};

/* -- In-memory ELF object generation ------------------------------------- */

/* Context for generating the ELF object for the GDB JIT API. */
typedef struct GDBJITctx {
  uint8_t *p;		/* Pointer to next address in obj.space. */
  uint8_t *startp;	/* Pointer to start address in obj.space. */
  GCtrace *T;		/* Generate symbols for this trace. */
  uintptr_t mcaddr;	/* Machine code address. */
  MSize szmcode;	/* Size of machine code. */
  MSize spadjp;		/* Stack adjustment for parent trace or interpreter. */
  MSize spadj;		/* Stack adjustment for trace itself. */
  BCLine lineno;	/* Starting line number. */
  const char *filename;	/* Starting file name. */
  size_t objsize;	/* Final size of ELF object. */
  GDBJITobj obj;	/* In-memory ELF object. */
} GDBJITctx;

/* Add a zero-terminated string. */
static uint32_t gdbjit_strz(GDBJITctx *ctx, const char *str)
{
  uint8_t *p = ctx->p;
  uint32_t ofs = (uint32_t)(p - ctx->startp);
  do {
    *p++ = (uint8_t)*str;
  } while (*str++);
  ctx->p = p;
  return ofs;
}

/* Append a decimal number. */
static void gdbjit_catnum(GDBJITctx *ctx, uint32_t n)
{
  if (n >= 10) { uint32_t m = n / 10; n = n % 10; gdbjit_catnum(ctx, m); }
  *ctx->p++ = '0' + n;
}

/* Add a ULEB128 value. */
static void gdbjit_uleb128(GDBJITctx *ctx, uint32_t v)
{
  uint8_t *p = ctx->p;
  for (; v >= 0x80; v >>= 7)
    *p++ = (uint8_t)((v & 0x7f) | 0x80);
  *p++ = (uint8_t)v;
  ctx->p = p;
}

/* Add a SLEB128 value. */
static void gdbjit_sleb128(GDBJITctx *ctx, int32_t v)
{
  uint8_t *p = ctx->p;
  for (; (uint32_t)(v+0x40) >= 0x80; v >>= 7)
    *p++ = (uint8_t)((v & 0x7f) | 0x80);
  *p++ = (uint8_t)(v & 0x7f);
  ctx->p = p;
}

/* Shortcuts to generate DWARF structures. */
#define DB(x)		(*p++ = (x))
#define DI8(x)		(*(int8_t *)p = (x), p++)
#define DU16(x)		(*(uint16_t *)p = (x), p += 2)
#define DU32(x)		(*(uint32_t *)p = (x), p += 4)
#define DADDR(x)	(*(uintptr_t *)p = (x), p += sizeof(uintptr_t))
#define DUV(x)		(ctx->p = p, gdbjit_uleb128(ctx, (x)), p = ctx->p)
#define DSV(x)		(ctx->p = p, gdbjit_sleb128(ctx, (x)), p = ctx->p)
#define DSTR(str)	(ctx->p = p, gdbjit_strz(ctx, (str)), p = ctx->p)
#define DALIGNNOP(s)	while ((uintptr_t)p & ((s)-1)) *p++ = DW_CFA_nop
#define DSECT(name, stmt) \
  { uint32_t *szp_##name = (uint32_t *)p; p += 4; stmt \
    *szp_##name = (uint32_t)((p-(uint8_t *)szp_##name)-4); } \

/* Initialize ELF section headers. */
static void LJ_FASTCALL gdbjit_secthdr(GDBJITctx *ctx)
{
  ELFsectheader *sect;

  *ctx->p++ = '\0';  /* Empty string at start of string table. */

#define SECTDEF(id, tp, al) \
  sect = &ctx->obj.sect[GDBJIT_SECT_##id]; \
  sect->name = gdbjit_strz(ctx, "." #id); \
  sect->type = ELFSECT_TYPE_##tp; \
  sect->align = (al)

  SECTDEF(text, NOBITS, 16);
  sect->flags = ELFSECT_FLAGS_ALLOC|ELFSECT_FLAGS_EXEC;
  sect->addr = ctx->mcaddr;
  sect->ofs = 0;
  sect->size = ctx->szmcode;

  SECTDEF(eh_frame, PROGBITS, sizeof(uintptr_t));
  sect->flags = ELFSECT_FLAGS_ALLOC;

  SECTDEF(shstrtab, STRTAB, 1);
  SECTDEF(strtab, STRTAB, 1);

  SECTDEF(symtab, SYMTAB, sizeof(uintptr_t));
  sect->ofs = offsetof(GDBJITobj, sym);
  sect->size = sizeof(ctx->obj.sym);
  sect->link = GDBJIT_SECT_strtab;
  sect->entsize = sizeof(ELFsymbol);
  sect->info = GDBJIT_SYM_FUNC;

  SECTDEF(debug_info, PROGBITS, 1);
  SECTDEF(debug_abbrev, PROGBITS, 1);
  SECTDEF(debug_line, PROGBITS, 1);

#undef SECTDEF
}

/* Initialize symbol table. */
static void LJ_FASTCALL gdbjit_symtab(GDBJITctx *ctx)
{
  ELFsymbol *sym;

  *ctx->p++ = '\0';  /* Empty string at start of string table. */

  sym = &ctx->obj.sym[GDBJIT_SYM_FILE];
  sym->name = gdbjit_strz(ctx, "JIT mcode");
  sym->sectidx = ELFSECT_IDX_ABS;
  sym->info = ELFSYM_TYPE_FILE|ELFSYM_BIND_LOCAL;

  sym = &ctx->obj.sym[GDBJIT_SYM_FUNC];
  sym->name = gdbjit_strz(ctx, "TRACE_"); ctx->p--;
  gdbjit_catnum(ctx, ctx->T->traceno); *ctx->p++ = '\0';
  sym->sectidx = GDBJIT_SECT_text;
  sym->value = 0;
  sym->size = ctx->szmcode;
  sym->info = ELFSYM_TYPE_FUNC|ELFSYM_BIND_GLOBAL;
}

/* Initialize .eh_frame section. */
static void LJ_FASTCALL gdbjit_ehframe(GDBJITctx *ctx)
{
  uint8_t *p = ctx->p;
  uint8_t *framep = p;

  /* Emit DWARF EH CIE. */
  DSECT(CIE,
    DU32(0);			/* Offset to CIE itself. */
    DB(DW_CIE_VERSION);
    DSTR("zR");			/* Augmentation. */
    DUV(1);			/* Code alignment factor. */
    DSV(-(int32_t)sizeof(uintptr_t));  /* Data alignment factor. */
    DB(DW_REG_RA);		/* Return address register. */
    DB(1); DB(DW_EH_PE_textrel|DW_EH_PE_udata4);  /* Augmentation data. */
    DB(DW_CFA_def_cfa); DUV(DW_REG_SP); DUV(sizeof(uintptr_t));
#if LJ_TARGET_PPC
    DB(DW_CFA_offset_extended_sf); DB(DW_REG_RA); DSV(-1);
#else
    DB(DW_CFA_offset|DW_REG_RA); DUV(1);
#endif
    DALIGNNOP(sizeof(uintptr_t));
  )

  /* Emit DWARF EH FDE. */
  DSECT(FDE,
    DU32((uint32_t)(p-framep));	/* Offset to CIE. */
    DU32(0);			/* Machine code offset relative to .text. */
    DU32(ctx->szmcode);		/* Machine code length. */
    DB(0);			/* Augmentation data. */
    /* Registers saved in CFRAME. */
#if LJ_TARGET_X86
    DB(DW_CFA_offset|DW_REG_BP); DUV(2);
    DB(DW_CFA_offset|DW_REG_DI); DUV(3);
    DB(DW_CFA_offset|DW_REG_SI); DUV(4);
    DB(DW_CFA_offset|DW_REG_BX); DUV(5);
#elif LJ_TARGET_X64
    DB(DW_CFA_offset|DW_REG_BP); DUV(2);
    DB(DW_CFA_offset|DW_REG_BX); DUV(3);
    DB(DW_CFA_offset|DW_REG_15); DUV(4);
    DB(DW_CFA_offset|DW_REG_14); DUV(5);
    /* Extra registers saved for JIT-compiled code. */
    DB(DW_CFA_offset|DW_REG_13); DUV(9);
    DB(DW_CFA_offset|DW_REG_12); DUV(10);
#elif LJ_TARGET_ARM
    {
      int i;
      for (i = 11; i >= 4; i--) { DB(DW_CFA_offset|i); DUV(2+(11-i)); }
    }
#elif LJ_TARGET_PPC
    {
      int i;
      DB(DW_CFA_offset_extended); DB(DW_REG_CR); DUV(55);
      for (i = 14; i <= 31; i++) {
	DB(DW_CFA_offset|i); DUV(37+(31-i));
	DB(DW_CFA_offset|32|i); DUV(2+2*(31-i));
      }
    }
#elif LJ_TARGET_MIPS
    {
      int i;
      DB(DW_CFA_offset|30); DUV(2);
      for (i = 23; i >= 16; i--) { DB(DW_CFA_offset|i); DUV(26-i); }
      for (i = 30; i >= 20; i -= 2) { DB(DW_CFA_offset|32|i); DUV(42-i); }
    }
#else
#error "Unsupported target architecture"
#endif
    if (ctx->spadjp != ctx->spadj) {  /* Parent/interpreter stack frame size. */
      DB(DW_CFA_def_cfa_offset); DUV(ctx->spadjp);
      DB(DW_CFA_advance_loc|1);  /* Only an approximation. */
    }
    DB(DW_CFA_def_cfa_offset); DUV(ctx->spadj);  /* Trace stack frame size. */
    DALIGNNOP(sizeof(uintptr_t));
  )

  ctx->p = p;
}

/* Initialize .debug_info section. */
static void LJ_FASTCALL gdbjit_debuginfo(GDBJITctx *ctx)
{
  uint8_t *p = ctx->p;

  DSECT(info,
    DU16(2);			/* DWARF version. */
    DU32(0);			/* Abbrev offset. */
    DB(sizeof(uintptr_t));	/* Pointer size. */

    DUV(1);			/* Abbrev #1: DW_TAG_compile_unit. */
    DSTR(ctx->filename);	/* DW_AT_name. */
    DADDR(ctx->mcaddr);		/* DW_AT_low_pc. */
    DADDR(ctx->mcaddr + ctx->szmcode);  /* DW_AT_high_pc. */
    DU32(0);			/* DW_AT_stmt_list. */
  )

  ctx->p = p;
}

/* Initialize .debug_abbrev section. */
static void LJ_FASTCALL gdbjit_debugabbrev(GDBJITctx *ctx)
{
  uint8_t *p = ctx->p;

  /* Abbrev #1: DW_TAG_compile_unit. */
  DUV(1); DUV(DW_TAG_compile_unit);
  DB(DW_children_no);
  DUV(DW_AT_name);	DUV(DW_FORM_string);
  DUV(DW_AT_low_pc);	DUV(DW_FORM_addr);
  DUV(DW_AT_high_pc);	DUV(DW_FORM_addr);
  DUV(DW_AT_stmt_list);	DUV(DW_FORM_data4);
  DB(0); DB(0);

  ctx->p = p;
}

#define DLNE(op, s)	(DB(DW_LNS_extended_op), DUV(1+(s)), DB((op)))

/* Initialize .debug_line section. */
static void LJ_FASTCALL gdbjit_debugline(GDBJITctx *ctx)
{
  uint8_t *p = ctx->p;

  DSECT(line,
    DU16(2);			/* DWARF version. */
    DSECT(header,
      DB(1);			/* Minimum instruction length. */
      DB(1);			/* is_stmt. */
      DI8(0);			/* Line base for special opcodes. */
      DB(2);			/* Line range for special opcodes. */
      DB(3+1);			/* Opcode base at DW_LNS_advance_line+1. */
      DB(0); DB(1); DB(1);	/* Standard opcode lengths. */
      /* Directory table. */
      DB(0);
      /* File name table. */
      DSTR(ctx->filename); DUV(0); DUV(0); DUV(0);
      DB(0);
    )

    DLNE(DW_LNE_set_address, sizeof(uintptr_t)); DADDR(ctx->mcaddr);
    if (ctx->lineno) {
      DB(DW_LNS_advance_line); DSV(ctx->lineno-1);
    }
    DB(DW_LNS_copy);
    DB(DW_LNS_advance_pc); DUV(ctx->szmcode);
    DLNE(DW_LNE_end_sequence, 0);
  )

  ctx->p = p;
}

#undef DLNE

/* Undef shortcuts. */
#undef DB
#undef DI8
#undef DU16
#undef DU32
#undef DADDR
#undef DUV
#undef DSV
#undef DSTR
#undef DALIGNNOP
#undef DSECT

/* Type of a section initializer callback. */
typedef void (LJ_FASTCALL *GDBJITinitf)(GDBJITctx *ctx);

/* Call section initializer and set the section offset and size. */
static void gdbjit_initsect(GDBJITctx *ctx, int sect, GDBJITinitf initf)
{
  ctx->startp = ctx->p;
  ctx->obj.sect[sect].ofs = (uintptr_t)((char *)ctx->p - (char *)&ctx->obj);
  initf(ctx);
  ctx->obj.sect[sect].size = (uintptr_t)(ctx->p - ctx->startp);
}

#define SECTALIGN(p, a) \
  ((p) = (uint8_t *)(((uintptr_t)(p) + ((a)-1)) & ~(uintptr_t)((a)-1)))

/* Build in-memory ELF object. */
static void gdbjit_buildobj(GDBJITctx *ctx)
{
  GDBJITobj *obj = &ctx->obj;
  /* Fill in ELF header and clear structures. */
  memcpy(&obj->hdr, &elfhdr_template, sizeof(ELFheader));
  memset(&obj->sect, 0, sizeof(ELFsectheader)*GDBJIT_SECT__MAX);
  memset(&obj->sym, 0, sizeof(ELFsymbol)*GDBJIT_SYM__MAX);
  /* Initialize sections. */
  ctx->p = obj->space;
  gdbjit_initsect(ctx, GDBJIT_SECT_shstrtab, gdbjit_secthdr);
  gdbjit_initsect(ctx, GDBJIT_SECT_strtab, gdbjit_symtab);
  gdbjit_initsect(ctx, GDBJIT_SECT_debug_info, gdbjit_debuginfo);
  gdbjit_initsect(ctx, GDBJIT_SECT_debug_abbrev, gdbjit_debugabbrev);
  gdbjit_initsect(ctx, GDBJIT_SECT_debug_line, gdbjit_debugline);
  SECTALIGN(ctx->p, sizeof(uintptr_t));
  gdbjit_initsect(ctx, GDBJIT_SECT_eh_frame, gdbjit_ehframe);
  ctx->objsize = (size_t)((char *)ctx->p - (char *)obj);
  lua_assert(ctx->objsize < sizeof(GDBJITobj));
}

#undef SECTALIGN

/* -- Interface to GDB JIT API -------------------------------------------- */

/* Add new entry to GDB JIT symbol chain. */
static void gdbjit_newentry(lua_State *L, GDBJITctx *ctx)
{
  /* Allocate memory for GDB JIT entry and ELF object. */
  MSize sz = (MSize)(sizeof(GDBJITentryobj) - sizeof(GDBJITobj) + ctx->objsize);
  GDBJITentryobj *eo = lj_mem_newt(L, sz, GDBJITentryobj);
  memcpy(&eo->obj, &ctx->obj, ctx->objsize);  /* Copy ELF object. */
  eo->sz = sz;
  ctx->T->gdbjit_entry = (void *)eo;
  /* Link new entry to chain and register it. */
  eo->entry.prev_entry = NULL;
  eo->entry.next_entry = __jit_debug_descriptor.first_entry;
  if (eo->entry.next_entry)
    eo->entry.next_entry->prev_entry = &eo->entry;
  eo->entry.symfile_addr = (const char *)&eo->obj;
  eo->entry.symfile_size = ctx->objsize;
  __jit_debug_descriptor.first_entry = &eo->entry;
  __jit_debug_descriptor.relevant_entry = &eo->entry;
  __jit_debug_descriptor.action_flag = GDBJIT_REGISTER;
  __jit_debug_register_code();
}

/* Add debug info for newly compiled trace and notify GDB. */
void lj_gdbjit_addtrace(jit_State *J, GCtrace *T)
{
  GDBJITctx ctx;
  GCproto *pt = &gcref(T->startpt)->pt;
  TraceNo parent = T->ir[REF_BASE].op1;
  const BCIns *startpc = mref(T->startpc, const BCIns);
  ctx.T = T;
  ctx.mcaddr = (uintptr_t)T->mcode;
  ctx.szmcode = T->szmcode;
  ctx.spadjp = CFRAME_SIZE_JIT +
	       (MSize)(parent ? traceref(J, parent)->spadjust : 0);
  ctx.spadj = CFRAME_SIZE_JIT + T->spadjust;
  lua_assert(startpc >= proto_bc(pt) && startpc < proto_bc(pt) + pt->sizebc);
  ctx.lineno = lj_debug_line(pt, proto_bcpos(pt, startpc));
  ctx.filename = proto_chunknamestr(pt);
  if (*ctx.filename == '@' || *ctx.filename == '=')
    ctx.filename++;
  else
    ctx.filename = "(string)";
  gdbjit_buildobj(&ctx);
  gdbjit_newentry(J->L, &ctx);
}

/* Delete debug info for trace and notify GDB. */
void lj_gdbjit_deltrace(jit_State *J, GCtrace *T)
{
  GDBJITentryobj *eo = (GDBJITentryobj *)T->gdbjit_entry;
  if (eo) {
    if (eo->entry.prev_entry)
      eo->entry.prev_entry->next_entry = eo->entry.next_entry;
    else
      __jit_debug_descriptor.first_entry = eo->entry.next_entry;
    if (eo->entry.next_entry)
      eo->entry.next_entry->prev_entry = eo->entry.prev_entry;
    __jit_debug_descriptor.relevant_entry = &eo->entry;
    __jit_debug_descriptor.action_flag = GDBJIT_UNREGISTER;
    __jit_debug_register_code();
    lj_mem_free(J2G(J), eo, eo->sz);
  }
}

#endif
#endif
