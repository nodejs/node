#include <openssl/bio.h>

#if BIO_FLAGS_UPLINK==0
/* Shortcut UPLINK calls on most platforms... */
#define	UP_stdin	stdin
#define	UP_stdout	stdout
#define	UP_stderr	stderr
#define	UP_fprintf	fprintf
#define	UP_fgets	fgets
#define	UP_fread	fread
#define	UP_fwrite	fwrite
#undef	UP_fsetmod
#define	UP_feof		feof
#define	UP_fclose	fclose

#define	UP_fopen	fopen
#define	UP_fseek	fseek
#define	UP_ftell	ftell
#define	UP_fflush	fflush
#define	UP_ferror	ferror
#ifdef _WIN32
#define	UP_fileno	_fileno
#define	UP_open		_open
#define	UP_read		_read
#define	UP_write	_write
#define	UP_lseek	_lseek
#define	UP_close	_close
#else
#define	UP_fileno	fileno
#define	UP_open		open
#define	UP_read		read
#define	UP_write	write
#define	UP_lseek	lseek
#define	UP_close	close
#endif
#endif
