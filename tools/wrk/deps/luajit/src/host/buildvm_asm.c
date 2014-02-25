/*
** LuaJIT VM builder: Assembler source code emitter.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "buildvm.h"
#include "lj_bc.h"

/* ------------------------------------------------------------------------ */

#if LJ_TARGET_X86ORX64
/* Emit bytes piecewise as assembler text. */
static void emit_asm_bytes(BuildCtx *ctx, uint8_t *p, int n)
{
  int i;
  for (i = 0; i < n; i++) {
    if ((i & 15) == 0)
      fprintf(ctx->fp, "\t.byte %d", p[i]);
    else
      fprintf(ctx->fp, ",%d", p[i]);
    if ((i & 15) == 15) putc('\n', ctx->fp);
  }
  if ((n & 15) != 0) putc('\n', ctx->fp);
}

/* Emit relocation */
static void emit_asm_reloc(BuildCtx *ctx, int type, const char *sym)
{
  switch (ctx->mode) {
  case BUILD_elfasm:
    if (type)
      fprintf(ctx->fp, "\t.long %s-.-4\n", sym);
    else
      fprintf(ctx->fp, "\t.long %s\n", sym);
    break;
  case BUILD_coffasm:
    fprintf(ctx->fp, "\t.def %s; .scl 3; .type 32; .endef\n", sym);
    if (type)
      fprintf(ctx->fp, "\t.long %s-.-4\n", sym);
    else
      fprintf(ctx->fp, "\t.long %s\n", sym);
    break;
  default:  /* BUILD_machasm for relative relocations handled below. */
    fprintf(ctx->fp, "\t.long %s\n", sym);
    break;
  }
}

static const char *const jccnames[] = {
  "jo", "jno", "jb", "jnb", "jz", "jnz", "jbe", "ja",
  "js", "jns", "jpe", "jpo", "jl", "jge", "jle", "jg"
};

/* Emit relocation for the incredibly stupid OSX assembler. */
static void emit_asm_reloc_mach(BuildCtx *ctx, uint8_t *cp, int n,
				const char *sym)
{
  const char *opname = NULL;
  if (--n < 0) goto err;
  if (cp[n] == 0xe8) {
    opname = "call";
  } else if (cp[n] == 0xe9) {
    opname = "jmp";
  } else if (cp[n] >= 0x80 && cp[n] <= 0x8f && n > 0 && cp[n-1] == 0x0f) {
    opname = jccnames[cp[n]-0x80];
    n--;
  } else {
err:
    fprintf(stderr, "Error: unsupported opcode for %s symbol relocation.\n",
	    sym);
    exit(1);
  }
  emit_asm_bytes(ctx, cp, n);
  fprintf(ctx->fp, "\t%s %s\n", opname, sym);
}
#else
/* Emit words piecewise as assembler text. */
static void emit_asm_words(BuildCtx *ctx, uint8_t *p, int n)
{
  int i;
  for (i = 0; i < n; i += 4) {
    if ((i & 15) == 0)
      fprintf(ctx->fp, "\t.long 0x%08x", *(uint32_t *)(p+i));
    else
      fprintf(ctx->fp, ",0x%08x", *(uint32_t *)(p+i));
    if ((i & 15) == 12) putc('\n', ctx->fp);
  }
  if ((n & 15) != 0) putc('\n', ctx->fp);
}

/* Emit relocation as part of an instruction. */
static void emit_asm_wordreloc(BuildCtx *ctx, uint8_t *p, int n,
			       const char *sym)
{
  uint32_t ins;
  emit_asm_words(ctx, p, n-4);
  ins = *(uint32_t *)(p+n-4);
#if LJ_TARGET_ARM
  if ((ins & 0xff000000u) == 0xfa000000u) {
    fprintf(ctx->fp, "\tblx %s\n", sym);
  } else if ((ins & 0x0e000000u) == 0x0a000000u) {
    fprintf(ctx->fp, "\t%s%.2s %s\n", (ins & 0x01000000u) ? "bl" : "b",
	    "eqnecsccmiplvsvchilsgeltgtle" + 2*(ins >> 28), sym);
  } else {
    fprintf(stderr,
	    "Error: unsupported opcode %08x for %s symbol relocation.\n",
	    ins, sym);
    exit(1);
  }
#elif LJ_TARGET_PPC || LJ_TARGET_PPCSPE
#if LJ_TARGET_PS3
#define TOCPREFIX "."
#else
#define TOCPREFIX ""
#endif
  if ((ins >> 26) == 16) {
    fprintf(ctx->fp, "\t%s %d, %d, " TOCPREFIX "%s\n",
	    (ins & 1) ? "bcl" : "bc", (ins >> 21) & 31, (ins >> 16) & 31, sym);
  } else if ((ins >> 26) == 18) {
    fprintf(ctx->fp, "\t%s " TOCPREFIX "%s\n", (ins & 1) ? "bl" : "b", sym);
  } else {
    fprintf(stderr,
	    "Error: unsupported opcode %08x for %s symbol relocation.\n",
	    ins, sym);
    exit(1);
  }
#elif LJ_TARGET_MIPS
  fprintf(stderr,
	  "Error: unsupported opcode %08x for %s symbol relocation.\n",
	  ins, sym);
  exit(1);
#else
#error "missing relocation support for this architecture"
#endif
}
#endif

#if LJ_TARGET_ARM
#define ELFASM_PX	"%%"
#else
#define ELFASM_PX	"@"
#endif

/* Emit an assembler label. */
static void emit_asm_label(BuildCtx *ctx, const char *name, int size, int isfunc)
{
  switch (ctx->mode) {
  case BUILD_elfasm:
#if LJ_TARGET_PS3
    if (!strncmp(name, "lj_vm_", 6) &&
	strcmp(name, ctx->beginsym) &&
	!strstr(name, "hook")) {
      fprintf(ctx->fp,
	"\n\t.globl %s\n"
	"\t.section \".opd\",\"aw\"\n"
	"%s:\n"
	"\t.long .%s,.TOC.@tocbase32\n"
	"\t.size %s,8\n"
	"\t.previous\n"
	"\t.globl .%s\n"
	"\t.hidden .%s\n"
	"\t.type .%s, " ELFASM_PX "function\n"
	"\t.size .%s, %d\n"
	".%s:\n",
	name, name, name, name, name, name, name, name, size, name);
      break;
    }
#endif
    fprintf(ctx->fp,
      "\n\t.globl %s\n"
      "\t.hidden %s\n"
      "\t.type %s, " ELFASM_PX "%s\n"
      "\t.size %s, %d\n"
      "%s:\n",
      name, name, name, isfunc ? "function" : "object", name, size, name);
    break;
  case BUILD_coffasm:
    fprintf(ctx->fp, "\n\t.globl %s\n", name);
    if (isfunc)
      fprintf(ctx->fp, "\t.def %s; .scl 3; .type 32; .endef\n", name);
    fprintf(ctx->fp, "%s:\n", name);
    break;
  case BUILD_machasm:
    fprintf(ctx->fp,
      "\n\t.private_extern %s\n"
      "%s:\n", name, name);
    break;
  default:
    break;
  }
}

/* Emit alignment. */
static void emit_asm_align(BuildCtx *ctx, int bits)
{
  switch (ctx->mode) {
  case BUILD_elfasm:
  case BUILD_coffasm:
    fprintf(ctx->fp, "\t.p2align %d\n", bits);
    break;
  case BUILD_machasm:
    fprintf(ctx->fp, "\t.align %d\n", bits);
    break;
  default:
    break;
  }
}

/* ------------------------------------------------------------------------ */

/* Emit assembler source code. */
void emit_asm(BuildCtx *ctx)
{
  int i, rel;

  fprintf(ctx->fp, "\t.file \"buildvm_%s.dasc\"\n", ctx->dasm_arch);
  fprintf(ctx->fp, "\t.text\n");
  emit_asm_align(ctx, 4);

#if LJ_TARGET_PS3
  emit_asm_label(ctx, ctx->beginsym, ctx->codesz, 0);
#else
  emit_asm_label(ctx, ctx->beginsym, 0, 0);
#endif
  if (ctx->mode != BUILD_machasm)
    fprintf(ctx->fp, ".Lbegin:\n");

#if LJ_TARGET_ARM && defined(__GNUC__) && !LJ_NO_UNWIND
  /* This should really be moved into buildvm_arm.dasc. */
  fprintf(ctx->fp,
	  ".fnstart\n"
	  ".save {r4, r5, r6, r7, r8, r9, r10, r11, lr}\n"
	  ".pad #28\n");
#endif
#if LJ_TARGET_MIPS
  fprintf(ctx->fp, ".set nomips16\n.abicalls\n.set noreorder\n.set nomacro\n");
#endif

  for (i = rel = 0; i < ctx->nsym; i++) {
    int32_t ofs = ctx->sym[i].ofs;
    int32_t next = ctx->sym[i+1].ofs;
#if LJ_TARGET_ARM && defined(__GNUC__) && !LJ_NO_UNWIND && LJ_HASFFI
    if (!strcmp(ctx->sym[i].name, "lj_vm_ffi_call"))
      fprintf(ctx->fp,
	      ".globl lj_err_unwind_arm\n"
	      ".personality lj_err_unwind_arm\n"
	      ".fnend\n"
	      ".fnstart\n"
	      ".save {r4, r5, r11, lr}\n"
	      ".setfp r11, sp\n");
#endif
    emit_asm_label(ctx, ctx->sym[i].name, next - ofs, 1);
    while (rel < ctx->nreloc && ctx->reloc[rel].ofs <= next) {
      BuildReloc *r = &ctx->reloc[rel];
      int n = r->ofs - ofs;
#if LJ_TARGET_X86ORX64
      if (ctx->mode == BUILD_machasm && r->type != 0) {
	emit_asm_reloc_mach(ctx, ctx->code+ofs, n, ctx->relocsym[r->sym]);
      } else {
	emit_asm_bytes(ctx, ctx->code+ofs, n);
	emit_asm_reloc(ctx, r->type, ctx->relocsym[r->sym]);
      }
      ofs += n+4;
#else
      emit_asm_wordreloc(ctx, ctx->code+ofs, n, ctx->relocsym[r->sym]);
      ofs += n;
#endif
      rel++;
    }
#if LJ_TARGET_X86ORX64
    emit_asm_bytes(ctx, ctx->code+ofs, next-ofs);
#else
    emit_asm_words(ctx, ctx->code+ofs, next-ofs);
#endif
  }

#if LJ_TARGET_ARM && defined(__GNUC__) && !LJ_NO_UNWIND
  fprintf(ctx->fp,
#if !LJ_HASFFI
	  ".globl lj_err_unwind_arm\n"
	  ".personality lj_err_unwind_arm\n"
#endif
	  ".fnend\n");
#endif

  fprintf(ctx->fp, "\n");
  switch (ctx->mode) {
  case BUILD_elfasm:
#if !LJ_TARGET_PS3
    fprintf(ctx->fp, "\t.section .note.GNU-stack,\"\"," ELFASM_PX "progbits\n");
#endif
#if LJ_TARGET_PPCSPE
    /* Soft-float ABI + SPE. */
    fprintf(ctx->fp, "\t.gnu_attribute 4, 2\n\t.gnu_attribute 8, 3\n");
#elif LJ_TARGET_PPC && !LJ_TARGET_PS3
    /* Hard-float ABI. */
    fprintf(ctx->fp, "\t.gnu_attribute 4, 1\n");
#endif
    /* fallthrough */
  case BUILD_coffasm:
    fprintf(ctx->fp, "\t.ident \"%s\"\n", ctx->dasm_ident);
    break;
  case BUILD_machasm:
    fprintf(ctx->fp,
      "\t.cstring\n"
      "\t.ascii \"%s\\0\"\n", ctx->dasm_ident);
    break;
  default:
    break;
  }
  fprintf(ctx->fp, "\n");
}

