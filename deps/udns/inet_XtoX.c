/* $Id: inet_XtoX.c,v 1.1 2006/12/04 01:55:39 mjt Exp $
 * Simple implementation of the following functions:
 *  inet_ntop(), inet_ntoa(), inet_pton(), inet_aton().
 *
 * Differences from traditional implementaitons:
 *  o modifies destination buffers even on error return.
 *  o no fancy (hex, or 1.2) input support in inet_aton()
 *  o inet_aton() does not accept junk after an IP address.
 *  o inet_ntop(AF_INET) requires at least 16 bytes in dest,
 *    and inet_ntop(AF_INET6) at least 40 bytes
 *    (traditional inet_ntop() will try to fit anyway)
 *
 * Compile with -Dinet_XtoX_prefix=pfx_ to have pfx_*() instead of inet_*()
 * Compile with -Dinet_XtoX_no_ntop or -Dinet_XtoX_no_pton
 *  to disable net2str or str2net conversions.
 *
 * #define inet_XtoX_prototypes and #include "this_file.c"
 * to get function prototypes only (but not for inet_ntoa()).
 * #define inet_XtoX_decl to be `static' for static visibility,
 * or use __declspec(dllexport) or somesuch...
 *
 * Compile with -DTEST to test against stock implementation.
 *
 * Written by Michael Tokarev.  Public domain.
 */

#ifdef inet_XtoX_prototypes

struct in_addr;

#else

#include <errno.h>

#ifdef TEST

# include <netinet/in.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# undef inet_XtoX_prefix
# define inet_XtoX_prefix mjt_inet_
# undef inet_XtoX_no_ntop
# undef inet_XtoX_no_pton

#else /* !TEST */

struct in_addr {	/* declare it here to avoid messing with headers */
  unsigned char x[4];
};

#endif /* TEST */

#endif /* inet_XtoX_prototypes */

#ifndef inet_XtoX_prefix
# define inet_XtoX_prefix inet_
#endif
#ifndef inet_XtoX_decl
# define inet_XtoX_decl /*empty*/
#endif

#define cc2_(x,y) cc2__(x,y)
#define cc2__(x,y) x##y
#define fn(x) cc2_(inet_XtoX_prefix,x)

#ifndef inet_XtoX_no_ntop

inet_XtoX_decl const char *
fn(ntop)(int af, const void *src, char *dst, unsigned size);

#ifndef inet_XtoX_prototypes

static int mjt_ntop4(const void *_src, char *dst, int size) {
  unsigned i, x, r;
  char *p;
  const unsigned char *s = _src;
  if (size < 4*4)	/* for simplicity, disallow non-max-size buffer */
    return 0;
  for (i = 0, p = dst; i < 4; ++i) {
    if (i) *p++ = '.';
    x = r = s[i];
    if (x > 99) { *p++ = (char)(r / 100 + '0'); r %= 100; }
    if (x > 9) { *p++ = (char)(r / 10 + '0'); r %= 10; }
    *p++ = (char)(r + '0');
  }
  *p = '\0';
  return 1;
}

static char *hexc(char *p, unsigned x) {
  static char hex[16] = "0123456789abcdef";
  if (x > 0x0fff) *p++ = hex[(x >>12) & 15];
  if (x > 0x00ff) *p++ = hex[(x >> 8) & 15];
  if (x > 0x000f) *p++ = hex[(x >> 4) & 15];
  *p++ = hex[x & 15];
  return p;
}

static int mjt_ntop6(const void *_src, char *dst, int size) {
  unsigned i;
  unsigned short w[8];
  unsigned bs = 0, cs = 0;
  unsigned bl = 0, cl = 0;
  char *p;
  const unsigned char *s = _src;

  if (size < 40)	/* for simplicity, disallow non-max-size buffer */
    return 0;

  for(i = 0; i < 8; ++i, s += 2) {
    w[i] = (((unsigned short)(s[0])) << 8) | s[1];
    if (!w[i]) {
      if (!cl++) cs = i;
    }
    else {
      if (cl > bl) bl = cl, bs = cs;
    }
  }
  if (cl > bl) bl = cl, bs = cs;
  p = dst;
  if (bl == 1)
    bl = 0;
  if (bl) {
    for(i = 0; i < bs; ++i) {
      if (i) *p++ = ':';
      p = hexc(p, w[i]);
    }
    *p++ = ':';
    i += bl;
    if (i == 8)
      *p++ = ':';
  }
  else
    i = 0;
  for(; i < 8; ++i) {
    if (i) *p++ = ':';
    if (i == 6 && !bs && (bl == 6 || (bl == 5 && w[5] == 0xffff)))
      return mjt_ntop4(s - 4, p, size - (p - dst));
    p = hexc(p, w[i]);
  }
  *p = '\0';
  return 1;
}

inet_XtoX_decl const char *
fn(ntop)(int af, const void *src, char *dst, unsigned size) {
  switch(af) {
  /* don't use AF_*: don't mess with headers */
  case 2:  /* AF_INET */  if (mjt_ntop4(src, dst, size)) return dst; break;
  case 10: /* AF_INET6 */ if (mjt_ntop6(src, dst, size)) return dst; break;
  default: errno = EAFNOSUPPORT; return (char*)0;
  }
  errno = ENOSPC;
  return (char*)0;
}

inet_XtoX_decl const char *
fn(ntoa)(struct in_addr addr) {
  static char buf[4*4];
  mjt_ntop4(&addr, buf, sizeof(buf));
  return buf;
}

#endif /* inet_XtoX_prototypes */
#endif /* inet_XtoX_no_ntop */

#ifndef inet_XtoX_no_pton

inet_XtoX_decl int fn(pton)(int af, const char *src, void *dst);
inet_XtoX_decl int fn(aton)(const char *src, struct in_addr *addr);

#ifndef inet_XtoX_prototypes

static int mjt_pton4(const char *c, void *dst) {
  unsigned char *a = dst;
  unsigned n, o;
  for (n = 0; n < 4; ++n) {
    if (*c < '0' || *c > '9')
      return 0;
    o = *c++ - '0';
    while(*c >= '0' && *c <= '9')
      if ((o = o * 10 + (*c++ - '0')) > 255)
        return 0;
    if (*c++ != (n == 3 ? '\0' : '.'))
      return 0;
    *a++ = (unsigned char)o;
  }
  return 1;
}

static int mjt_pton6(const char *c, void *dst) {
  unsigned short w[8], *a = w, *z, *i;
  unsigned v, o;
  const char *sc;
  unsigned char *d = dst;
  if (*c != ':') z = (unsigned short*)0;
  else if (*++c != ':') return 0;
  else ++c, z = a;
  i = 0;
  for(;;) {
    v = 0;
    sc = c;
    for(;;) {
      if (*c >= '0' && *c <= '9') o = *c - '0';
      else if (*c >= 'a' && *c <= 'f') o = *c - 'a' + 10;
      else if (*c >= 'A' && *c <= 'F') o = *c - 'A' + 10;
      else break;
      v = (v << 4) | o;
      if (v > 0xffff) return 0;
      ++c;
    }
    if (sc == c) {
      if (z == a && !*c)
        break;
      else
        return 0;
    }
    if (*c == ':') {
      if (a >= w + 8)
        return 0;
      *a++ = v;
      if (*++c == ':') {
        if (z)
          return 0;
        z = a;
        if (!*++c)
          break;
      }
    }
    else if (!*c) {
      if (a >= w + 8)
        return 0;
      *a++ = v;
      break;
    }
    else if (*c == '.') {
      if (a > w + 6)
        return 0;
      if (!mjt_pton4(sc, d))
        return 0;
      *a++ = ((unsigned)(d[0]) << 8) | d[1];
      *a++ = ((unsigned)(d[2]) << 8) | d[3];
      break;
    }
    else
      return 0;
  }
  v = w + 8 - a;
  if ((v && !z) || (!v && z))
    return 0;
  for(i = w; ; ++i) {
    if (i == z)
      while(v--) { *d++ = '\0'; *d++ = '\0'; }
    if (i >= a)
      break;
    *d++ = (unsigned char)((*i >> 8) & 255);
    *d++ = (unsigned char)(*i & 255);
  }
  return 1;
}

inet_XtoX_decl int fn(pton)(int af, const char *src, void *dst) {
  switch(af) {
  /* don't use AF_*: don't mess with headers */
  case 2  /* AF_INET  */: return mjt_pton4(src, dst);
  case 10 /* AF_INET6 */: return mjt_pton6(src, dst);
  default: errno = EAFNOSUPPORT; return -1;
  }
}

inet_XtoX_decl int fn(aton)(const char *src, struct in_addr *addr) {
  return mjt_pton4(src, addr);
}

#endif /* inet_XtoX_prototypes */

#endif /* inet_XtoX_no_pton */

#ifdef TEST

int main(int argc, char **argv) {
  int i;
  char n0[16], n1[16];
  char p0[64], p1[64];
  int af = AF_INET;
  int pl = sizeof(p0);
  int r0, r1;
  const char *s0, *s1;

  while((i = getopt(argc, argv, "46a:p:")) != EOF) switch(i) {
  case '4': af = AF_INET;  break;
  case '6': af = AF_INET6; break;
  case 'a': case 'p': pl = atoi(optarg); break;
  default: return 1;
  }
  for(i = optind; i < argc; ++i) {
    char *a = argv[i];

    printf("%s:\n", a);
    r0 = inet_pton(af, a, n0);
    printf(" p2n stock: %s\n",
     (r0 < 0 ? "(notsupp)" : !r0 ? "(inval)" : fn(ntop)(af,n0,p0,sizeof(p0))));
    r1 = fn(pton)(af, a, n1);
    printf(" p2n this : %s\n",
     (r1 < 0 ? "(notsupp)" : !r1 ? "(inval)" : fn(ntop)(af,n1,p1,sizeof(p1))));

    if ((r0 > 0) != (r1 > 0) ||
        (r0 > 0 && r1 > 0 && memcmp(n0, n1, af == AF_INET ? 4 : 16) != 0))
      printf(" DIFFER!\n");

    s0 = inet_ntop(af, n1, p0, pl);
    printf(" n2p stock: %s\n", s0 ? s0 : "(inval)");
    s1 = fn(ntop)(af, n1, p1, pl);
    printf(" n2p this : %s\n", s1 ? s1 : "(inval)");
    if ((s0 != 0) != (s1 != 0) ||
        (s0 && s1 && strcmp(s0, s1) != 0))
      printf(" DIFFER!\n");

  }
  return 0;
}

#endif /* TEST */
