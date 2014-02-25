/*
** LuaJIT VM builder: IR folding hash table generator.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "buildvm.h"
#include "lj_obj.h"
#include "lj_ir.h"

/* Context for the folding hash table generator. */
static int lineno;
static int funcidx;
static uint32_t foldkeys[BUILD_MAX_FOLD];
static uint32_t nkeys;

/* Try to fill the hash table with keys using the hash parameters. */
static int tryhash(uint32_t *htab, uint32_t sz, uint32_t r, int dorol)
{
  uint32_t i;
  if (dorol && ((r & 31) == 0 || (r>>5) == 0))
    return 0;  /* Avoid zero rotates. */
  memset(htab, 0xff, (sz+1)*sizeof(uint32_t));
  for (i = 0; i < nkeys; i++) {
    uint32_t key = foldkeys[i];
    uint32_t k = key & 0xffffff;
    uint32_t h = (dorol ? lj_rol(lj_rol(k, r>>5) - k, r&31) :
			  (((k << (r>>5)) - k) << (r&31))) % sz;
    if (htab[h] != 0xffffffff) {  /* Collision on primary slot. */
      if (htab[h+1] != 0xffffffff) {  /* Collision on secondary slot. */
	/* Try to move the colliding key, if possible. */
	if (h < sz-1 && htab[h+2] == 0xffffffff) {
	  uint32_t k2 = htab[h+1] & 0xffffff;
	  uint32_t h2 = (dorol ? lj_rol(lj_rol(k2, r>>5) - k2, r&31) :
				 (((k2 << (r>>5)) - k2) << (r&31))) % sz;
	  if (h2 != h+1) return 0;  /* Cannot resolve collision. */
	  htab[h+2] = htab[h+1];  /* Move colliding key to secondary slot. */
	} else {
	  return 0;  /* Collision. */
	}
      }
      htab[h+1] = key;
    } else {
      htab[h] = key;
    }
  }
  return 1;  /* Success, all keys could be stored. */
}

/* Print the generated hash table. */
static void printhash(BuildCtx *ctx, uint32_t *htab, uint32_t sz)
{
  uint32_t i;
  fprintf(ctx->fp, "static const uint32_t fold_hash[%d] = {\n0x%08x",
	  sz+1, htab[0]);
  for (i = 1; i < sz+1; i++)
    fprintf(ctx->fp, ",\n0x%08x", htab[i]);
  fprintf(ctx->fp, "\n};\n\n");
}

/* Exhaustive search for the shortest semi-perfect hash table. */
static void makehash(BuildCtx *ctx)
{
  uint32_t htab[BUILD_MAX_FOLD*2+1];
  uint32_t sz, r;
  /* Search for the smallest hash table with an odd size. */
  for (sz = (nkeys|1); sz < BUILD_MAX_FOLD*2; sz += 2) {
    /* First try all shift hash combinations. */
    for (r = 0; r < 32*32; r++) {
      if (tryhash(htab, sz, r, 0)) {
	printhash(ctx, htab, sz);
	fprintf(ctx->fp,
		"#define fold_hashkey(k)\t(((((k)<<%u)-(k))<<%u)%%%u)\n\n",
		r>>5, r&31, sz);
	return;
      }
    }
    /* Then try all rotate hash combinations. */
    for (r = 0; r < 32*32; r++) {
      if (tryhash(htab, sz, r, 1)) {
	printhash(ctx, htab, sz);
	fprintf(ctx->fp,
	  "#define fold_hashkey(k)\t(lj_rol(lj_rol((k),%u)-(k),%u)%%%u)\n\n",
		r>>5, r&31, sz);
	return;
      }
    }
  }
  fprintf(stderr, "Error: search for perfect hash failed\n");
  exit(1);
}

/* Parse one token of a fold rule. */
static uint32_t nexttoken(char **pp, int allowlit, int allowany)
{
  char *p = *pp;
  if (p) {
    uint32_t i;
    char *q = strchr(p, ' ');
    if (q) *q++ = '\0';
    *pp = q;
    if (allowlit && !strncmp(p, "IRFPM_", 6)) {
      for (i = 0; irfpm_names[i]; i++)
	if (!strcmp(irfpm_names[i], p+6))
	  return i;
    } else if (allowlit && !strncmp(p, "IRFL_", 5)) {
      for (i = 0; irfield_names[i]; i++)
	if (!strcmp(irfield_names[i], p+5))
	  return i;
    } else if (allowlit && !strncmp(p, "IRCALL_", 7)) {
      for (i = 0; ircall_names[i]; i++)
	if (!strcmp(ircall_names[i], p+7))
	  return i;
    } else if (allowlit && !strncmp(p, "IRCONV_", 7)) {
      for (i = 0; irt_names[i]; i++) {
	const char *r = strchr(p+7, '_');
	if (r && !strncmp(irt_names[i], p+7, r-(p+7))) {
	  uint32_t j;
	  for (j = 0; irt_names[j]; j++)
	    if (!strcmp(irt_names[j], r+1))
	      return (i << 5) + j;
	}
      }
    } else if (allowlit && *p >= '0' && *p <= '9') {
      for (i = 0; *p >= '0' && *p <= '9'; p++)
	i = i*10 + (*p - '0');
      if (*p == '\0')
	return i;
    } else if (allowany && !strcmp("any", p)) {
      return allowany;
    } else {
      for (i = 0; ir_names[i]; i++)
	if (!strcmp(ir_names[i], p))
	  return i;
    }
    fprintf(stderr, "Error: bad fold definition token \"%s\" at line %d\n", p, lineno);
    exit(1);
  }
  return 0;
}

/* Parse a fold rule. */
static void foldrule(char *p)
{
  uint32_t op = nexttoken(&p, 0, 0);
  uint32_t left = nexttoken(&p, 0, 0x7f);
  uint32_t right = nexttoken(&p, 1, 0x3ff);
  uint32_t key = (funcidx << 24) | (op << 17) | (left << 10) | right;
  uint32_t i;
  if (nkeys >= BUILD_MAX_FOLD) {
    fprintf(stderr, "Error: too many fold rules, increase BUILD_MAX_FOLD.\n");
    exit(1);
  }
  /* Simple insertion sort to detect duplicates. */
  for (i = nkeys; i > 0; i--) {
    if ((foldkeys[i-1]&0xffffff) < (key & 0xffffff))
      break;
    if ((foldkeys[i-1]&0xffffff) == (key & 0xffffff)) {
      fprintf(stderr, "Error: duplicate fold definition at line %d\n", lineno);
      exit(1);
    }
    foldkeys[i] = foldkeys[i-1];
  }
  foldkeys[i] = key;
  nkeys++;
}

/* Emit C source code for IR folding hash table. */
void emit_fold(BuildCtx *ctx)
{
  char buf[256];  /* We don't care about analyzing lines longer than that. */
  const char *fname = ctx->args[0];
  FILE *fp;

  if (fname == NULL) {
    fprintf(stderr, "Error: missing input filename\n");
    exit(1);
  }

  if (fname[0] == '-' && fname[1] == '\0') {
    fp = stdin;
  } else {
    fp = fopen(fname, "r");
    if (!fp) {
      fprintf(stderr, "Error: cannot open input file '%s': %s\n",
	      fname, strerror(errno));
      exit(1);
    }
  }

  fprintf(ctx->fp, "/* This is a generated file. DO NOT EDIT! */\n\n");
  fprintf(ctx->fp, "static const FoldFunc fold_func[] = {\n");

  lineno = 0;
  funcidx = 0;
  nkeys = 0;
  while (fgets(buf, sizeof(buf), fp) != NULL) {
    lineno++;
    /* The prefix must be at the start of a line, otherwise it's ignored. */
    if (!strncmp(buf, FOLDDEF_PREFIX, sizeof(FOLDDEF_PREFIX)-1)) {
      char *p = buf+sizeof(FOLDDEF_PREFIX)-1;
      char *q = strchr(p, ')');
      if (p[0] == '(' && q) {
	p++;
	*q = '\0';
	foldrule(p);
      } else if ((p[0] == 'F' || p[0] == 'X') && p[1] == '(' && q) {
	p += 2;
	*q = '\0';
	if (funcidx)
	  fprintf(ctx->fp, ",\n");
	if (p[-2] == 'X')
	  fprintf(ctx->fp, "  %s", p);
	else
	  fprintf(ctx->fp, "  fold_%s", p);
	funcidx++;
      } else {
	buf[strlen(buf)-1] = '\0';
	fprintf(stderr, "Error: unknown fold definition tag %s%s at line %d\n",
		FOLDDEF_PREFIX, p, lineno);
	exit(1);
      }
    }
  }
  fclose(fp);
  fprintf(ctx->fp, "\n};\n\n");

  makehash(ctx);
}

