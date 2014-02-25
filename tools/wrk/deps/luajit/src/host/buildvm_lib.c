/*
** LuaJIT VM builder: library definition compiler.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "buildvm.h"
#include "lj_obj.h"
#include "lj_lib.h"

/* Context for library definitions. */
static uint8_t obuf[8192];
static uint8_t *optr;
static char modname[80];
static size_t modnamelen;
static char funcname[80];
static int modstate, regfunc;
static int ffid, recffid, ffasmfunc;

enum {
  REGFUNC_OK,
  REGFUNC_NOREG,
  REGFUNC_NOREGUV
};

static void libdef_name(const char *p, int kind)
{
  size_t n = strlen(p);
  if (kind != LIBINIT_STRING) {
    if (n > modnamelen && p[modnamelen] == '_' &&
	!strncmp(p, modname, modnamelen)) {
      p += modnamelen+1;
      n -= modnamelen+1;
    }
  }
  if (n > LIBINIT_MAXSTR) {
    fprintf(stderr, "Error: string too long: '%s'\n",  p);
    exit(1);
  }
  if (optr+1+n+2 > obuf+sizeof(obuf)) {  /* +2 for caller. */
    fprintf(stderr, "Error: output buffer overflow\n");
    exit(1);
  }
  *optr++ = (uint8_t)(n | kind);
  memcpy(optr, p, n);
  optr += n;
}

static void libdef_endmodule(BuildCtx *ctx)
{
  if (modstate != 0) {
    char line[80];
    const uint8_t *p;
    int n;
    if (modstate == 1)
      fprintf(ctx->fp, "  (lua_CFunction)0");
    fprintf(ctx->fp, "\n};\n");
    fprintf(ctx->fp, "static const uint8_t %s%s[] = {\n",
	    LABEL_PREFIX_LIBINIT, modname);
    line[0] = '\0';
    for (n = 0, p = obuf; p < optr; p++) {
      n += sprintf(line+n, "%d,", *p);
      if (n >= 75) {
	fprintf(ctx->fp, "%s\n", line);
	n = 0;
	line[0] = '\0';
      }
    }
    fprintf(ctx->fp, "%s%d\n};\n#endif\n\n", line, LIBINIT_END);
  }
}

static void libdef_module(BuildCtx *ctx, char *p, int arg)
{
  UNUSED(arg);
  if (ctx->mode == BUILD_libdef) {
    libdef_endmodule(ctx);
    optr = obuf;
    *optr++ = (uint8_t)ffid;
    *optr++ = (uint8_t)ffasmfunc;
    *optr++ = 0;  /* Hash table size. */
    modstate = 1;
    fprintf(ctx->fp, "#ifdef %sMODULE_%s\n", LIBDEF_PREFIX, p);
    fprintf(ctx->fp, "#undef %sMODULE_%s\n", LIBDEF_PREFIX, p);
    fprintf(ctx->fp, "static const lua_CFunction %s%s[] = {\n",
	    LABEL_PREFIX_LIBCF, p);
  }
  modnamelen = strlen(p);
  if (modnamelen > sizeof(modname)-1) {
    fprintf(stderr, "Error: module name too long: '%s'\n", p);
    exit(1);
  }
  strcpy(modname, p);
}

static int find_ffofs(BuildCtx *ctx, const char *name)
{
  int i;
  for (i = 0; i < ctx->nglob; i++) {
    const char *gl = ctx->globnames[i];
    if (gl[0] == 'f' && gl[1] == 'f' && gl[2] == '_' && !strcmp(gl+3, name)) {
      return (int)((uint8_t *)ctx->glob[i] - ctx->code);
    }
  }
  fprintf(stderr, "Error: undefined fast function %s%s\n",
	  LABEL_PREFIX_FF, name);
  exit(1);
}

static void libdef_func(BuildCtx *ctx, char *p, int arg)
{
  if (arg != LIBINIT_CF)
    ffasmfunc++;
  if (ctx->mode == BUILD_libdef) {
    if (modstate == 0) {
      fprintf(stderr, "Error: no module for function definition %s\n", p);
      exit(1);
    }
    if (regfunc == REGFUNC_NOREG) {
      if (optr+1 > obuf+sizeof(obuf)) {
	fprintf(stderr, "Error: output buffer overflow\n");
	exit(1);
      }
      *optr++ = LIBINIT_FFID;
    } else {
      if (arg != LIBINIT_ASM_) {
	if (modstate != 1) fprintf(ctx->fp, ",\n");
	modstate = 2;
	fprintf(ctx->fp, "  %s%s", arg ? LABEL_PREFIX_FFH : LABEL_PREFIX_CF, p);
      }
      if (regfunc != REGFUNC_NOREGUV) obuf[2]++;  /* Bump hash table size. */
      libdef_name(regfunc == REGFUNC_NOREGUV ? "" : p, arg);
    }
  } else if (ctx->mode == BUILD_ffdef) {
    fprintf(ctx->fp, "FFDEF(%s)\n", p);
  } else if (ctx->mode == BUILD_recdef) {
    if (strlen(p) > sizeof(funcname)-1) {
      fprintf(stderr, "Error: function name too long: '%s'\n", p);
      exit(1);
    }
    strcpy(funcname, p);
  } else if (ctx->mode == BUILD_vmdef) {
    int i;
    for (i = 1; p[i] && modname[i-1]; i++)
      if (p[i] == '_') p[i] = '.';
    fprintf(ctx->fp, "\"%s\",\n", p);
  } else if (ctx->mode == BUILD_bcdef) {
    if (arg != LIBINIT_CF)
      fprintf(ctx->fp, ",\n%d", find_ffofs(ctx, p));
  }
  ffid++;
  regfunc = REGFUNC_OK;
}

static uint32_t find_rec(char *name)
{
  char *p = (char *)obuf;
  uint32_t n;
  for (n = 2; *p; n++) {
    if (strcmp(p, name) == 0)
      return n;
    p += strlen(p)+1;
  }
  if (p+strlen(name)+1 >= (char *)obuf+sizeof(obuf)) {
    fprintf(stderr, "Error: output buffer overflow\n");
    exit(1);
  }
  strcpy(p, name);
  return n;
}

static void libdef_rec(BuildCtx *ctx, char *p, int arg)
{
  UNUSED(arg);
  if (ctx->mode == BUILD_recdef) {
    char *q;
    uint32_t n;
    for (; recffid+1 < ffid; recffid++)
      fprintf(ctx->fp, ",\n0");
    recffid = ffid;
    if (*p == '.') p = funcname;
    q = strchr(p, ' ');
    if (q) *q++ = '\0';
    n = find_rec(p);
    if (q)
      fprintf(ctx->fp, ",\n0x%02x00+(%s)", n, q);
    else
      fprintf(ctx->fp, ",\n0x%02x00", n);
  }
}

static void memcpy_endian(void *dst, void *src, size_t n)
{
  union { uint8_t b; uint32_t u; } host_endian;
  host_endian.u = 1;
  if (host_endian.b == LJ_ENDIAN_SELECT(1, 0)) {
    memcpy(dst, src, n);
  } else {
    size_t i;
    for (i = 0; i < n; i++)
      ((uint8_t *)dst)[i] = ((uint8_t *)src)[n-i-1];
  }
}

static void libdef_push(BuildCtx *ctx, char *p, int arg)
{
  UNUSED(arg);
  if (ctx->mode == BUILD_libdef) {
    int len = (int)strlen(p);
    if (*p == '"') {
      if (len > 1 && p[len-1] == '"') {
	p[len-1] = '\0';
	libdef_name(p+1, LIBINIT_STRING);
	return;
      }
    } else if (*p >= '0' && *p <= '9') {
      char *ep;
      double d = strtod(p, &ep);
      if (*ep == '\0') {
	if (optr+1+sizeof(double) > obuf+sizeof(obuf)) {
	  fprintf(stderr, "Error: output buffer overflow\n");
	  exit(1);
	}
	*optr++ = LIBINIT_NUMBER;
	memcpy_endian(optr, &d, sizeof(double));
	optr += sizeof(double);
	return;
      }
    } else if (!strcmp(p, "lastcl")) {
      if (optr+1 > obuf+sizeof(obuf)) {
	fprintf(stderr, "Error: output buffer overflow\n");
	exit(1);
      }
      *optr++ = LIBINIT_LASTCL;
      return;
    } else if (len > 4 && !strncmp(p, "top-", 4)) {
      if (optr+2 > obuf+sizeof(obuf)) {
	fprintf(stderr, "Error: output buffer overflow\n");
	exit(1);
      }
      *optr++ = LIBINIT_COPY;
      *optr++ = (uint8_t)atoi(p+4);
      return;
    }
    fprintf(stderr, "Error: bad value for %sPUSH(%s)\n", LIBDEF_PREFIX, p);
    exit(1);
  }
}

static void libdef_set(BuildCtx *ctx, char *p, int arg)
{
  UNUSED(arg);
  if (ctx->mode == BUILD_libdef) {
    if (p[0] == '!' && p[1] == '\0') p[0] = '\0';  /* Set env. */
    libdef_name(p, LIBINIT_STRING);
    *optr++ = LIBINIT_SET;
    obuf[2]++;  /* Bump hash table size. */
  }
}

static void libdef_regfunc(BuildCtx *ctx, char *p, int arg)
{
  UNUSED(ctx); UNUSED(p);
  regfunc = arg;
}

typedef void (*LibDefFunc)(BuildCtx *ctx, char *p, int arg);

typedef struct LibDefHandler {
  const char *suffix;
  const char *stop;
  const LibDefFunc func;
  const int arg;
} LibDefHandler;

static const LibDefHandler libdef_handlers[] = {
  { "MODULE_",	" \t\r\n",	libdef_module,		0 },
  { "CF(",	")",		libdef_func,		LIBINIT_CF },
  { "ASM(",	")",		libdef_func,		LIBINIT_ASM },
  { "ASM_(",	")",		libdef_func,		LIBINIT_ASM_ },
  { "REC(",	")",		libdef_rec,		0 },
  { "PUSH(",	")",		libdef_push,		0 },
  { "SET(",	")",		libdef_set,		0 },
  { "NOREGUV",	NULL,		libdef_regfunc,		REGFUNC_NOREGUV },
  { "NOREG",	NULL,		libdef_regfunc,		REGFUNC_NOREG },
  { NULL,	NULL,		(LibDefFunc)0,		0 }
};

/* Emit C source code for library function definitions. */
void emit_lib(BuildCtx *ctx)
{
  const char *fname;

  if (ctx->mode == BUILD_ffdef || ctx->mode == BUILD_libdef ||
      ctx->mode == BUILD_recdef)
    fprintf(ctx->fp, "/* This is a generated file. DO NOT EDIT! */\n\n");
  else if (ctx->mode == BUILD_vmdef)
    fprintf(ctx->fp, "ffnames = {\n[0]=\"Lua\",\n\"C\",\n");
  if (ctx->mode == BUILD_recdef)
    fprintf(ctx->fp, "static const uint16_t recff_idmap[] = {\n0,\n0x0100");
  recffid = ffid = FF_C+1;
  ffasmfunc = 0;

  while ((fname = *ctx->args++)) {
    char buf[256];  /* We don't care about analyzing lines longer than that. */
    FILE *fp;
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
    modstate = 0;
    regfunc = REGFUNC_OK;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
      char *p;
      /* Simplistic pre-processor. Only handles top-level #if/#endif. */
      if (buf[0] == '#' && buf[1] == 'i' && buf[2] == 'f') {
	int ok = 1;
	if (!strcmp(buf, "#if LJ_52\n"))
	  ok = LJ_52;
	else if (!strcmp(buf, "#if LJ_HASJIT\n"))
	  ok = LJ_HASJIT;
	else if (!strcmp(buf, "#if LJ_HASFFI\n"))
	  ok = LJ_HASFFI;
	if (!ok) {
	  int lvl = 1;
	  while (fgets(buf, sizeof(buf), fp) != NULL) {
	    if (buf[0] == '#' && buf[1] == 'e' && buf[2] == 'n') {
	      if (--lvl == 0) break;
	    } else if (buf[0] == '#' && buf[1] == 'i' && buf[2] == 'f') {
	      lvl++;
	    }
	  }
	  continue;
	}
      }
      for (p = buf; (p = strstr(p, LIBDEF_PREFIX)) != NULL; ) {
	const LibDefHandler *ldh;
	p += sizeof(LIBDEF_PREFIX)-1;
	for (ldh = libdef_handlers; ldh->suffix != NULL; ldh++) {
	  size_t n, len = strlen(ldh->suffix);
	  if (!strncmp(p, ldh->suffix, len)) {
	    p += len;
	    n = ldh->stop ? strcspn(p, ldh->stop) : 0;
	    if (!p[n]) break;
	    p[n] = '\0';
	    ldh->func(ctx, p, ldh->arg);
	    p += n+1;
	    break;
	  }
	}
	if (ldh->suffix == NULL) {
	  buf[strlen(buf)-1] = '\0';
	  fprintf(stderr, "Error: unknown library definition tag %s%s\n",
		  LIBDEF_PREFIX, p);
	  exit(1);
	}
      }
    }
    fclose(fp);
    if (ctx->mode == BUILD_libdef) {
      libdef_endmodule(ctx);
    }
  }

  if (ctx->mode == BUILD_ffdef) {
    fprintf(ctx->fp, "\n#undef FFDEF\n\n");
    fprintf(ctx->fp,
      "#ifndef FF_NUM_ASMFUNC\n#define FF_NUM_ASMFUNC %d\n#endif\n\n",
      ffasmfunc);
  } else if (ctx->mode == BUILD_vmdef) {
    fprintf(ctx->fp, "}\n\n");
  } else if (ctx->mode == BUILD_bcdef) {
    int i;
    fprintf(ctx->fp, "\n};\n\n");
    fprintf(ctx->fp, "LJ_DATADEF const uint16_t lj_bc_mode[] = {\n");
    fprintf(ctx->fp, "BCDEF(BCMODE)\n");
    for (i = ffasmfunc-1; i > 0; i--)
      fprintf(ctx->fp, "BCMODE_FF,\n");
    fprintf(ctx->fp, "BCMODE_FF\n};\n\n");
  } else if (ctx->mode == BUILD_recdef) {
    char *p = (char *)obuf;
    fprintf(ctx->fp, "\n};\n\n");
    fprintf(ctx->fp, "static const RecordFunc recff_func[] = {\n"
	    "recff_nyi,\n"
	    "recff_c");
    while (*p) {
      fprintf(ctx->fp, ",\nrecff_%s", p);
      p += strlen(p)+1;
    }
    fprintf(ctx->fp, "\n};\n\n");
  }
}

