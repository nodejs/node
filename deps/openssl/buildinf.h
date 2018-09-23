/* crypto/Makefile usually creates the file at build time. Since we don't care
 * about the build timestamp we fill in placeholder values. */
#ifndef MK1MF_BUILD
#define CFLAGS "-C flags not included-"
#define PLATFORM "google"
#define DATE "Sun Jan 1 00:00:00 GMT 1970"
#endif
