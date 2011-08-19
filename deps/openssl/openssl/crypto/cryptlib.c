/* crypto/cryptlib.c */
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECDH support in OpenSSL originally developed by 
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#include "cryptlib.h"
#include <openssl/safestack.h>

#if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_WIN16)
static double SSLeay_MSVC5_hack=0.0; /* and for VC1.5 */
#endif

static void (MS_FAR *locking_callback)(int mode,int type,
	const char *file,int line)=NULL;
static int (MS_FAR *add_lock_callback)(int *pointer,int amount,
	int type,const char *file,int line)=NULL;
static unsigned long (MS_FAR *id_callback)(void)=NULL;

int CRYPTO_num_locks(void)
	{
	return CRYPTO_NUM_LOCKS;
	}

void (*CRYPTO_get_locking_callback(void))(int mode,int type,const char *file,
		int line)
	{
	return(locking_callback);
	}

int (*CRYPTO_get_add_lock_callback(void))(int *num,int mount,int type,
					  const char *file,int line)
	{
	return(add_lock_callback);
	}

void CRYPTO_set_locking_callback(void (*func)(int mode,int type,
					      const char *file,int line))
	{
	locking_callback=func;
	}

void CRYPTO_set_add_lock_callback(int (*func)(int *num,int mount,int type,
					      const char *file,int line))
	{
	add_lock_callback=func;
	}

unsigned long (*CRYPTO_get_id_callback(void))(void)
	{
	return(id_callback);
	}

void CRYPTO_set_id_callback(unsigned long (*func)(void))
	{
	id_callback=func;
	}

unsigned long CRYPTO_thread_id(void)
	{
	unsigned long ret=0;

	if (id_callback == NULL)
		{
#ifdef OPENSSL_SYS_WIN16
		ret=(unsigned long)GetCurrentTask();
#elif defined(OPENSSL_SYS_WIN32)
		ret=(unsigned long)GetCurrentThreadId();
#elif defined(GETPID_IS_MEANINGLESS)
		ret=1L;
#else
		ret=(unsigned long)getpid();
#endif
		}
	else
		ret=id_callback();
	return(ret);
	}

static void (*do_dynlock_cb)(int mode, int type, const char *file, int line);

void int_CRYPTO_set_do_dynlock_callback(
	void (*dyn_cb)(int mode, int type, const char *file, int line))
	{
	do_dynlock_cb = dyn_cb;
	}

void CRYPTO_lock(int mode, int type, const char *file, int line)
	{
#ifdef LOCK_DEBUG
		{
		char *rw_text,*operation_text;

		if (mode & CRYPTO_LOCK)
			operation_text="lock  ";
		else if (mode & CRYPTO_UNLOCK)
			operation_text="unlock";
		else
			operation_text="ERROR ";

		if (mode & CRYPTO_READ)
			rw_text="r";
		else if (mode & CRYPTO_WRITE)
			rw_text="w";
		else
			rw_text="ERROR";

		fprintf(stderr,"lock:%08lx:(%s)%s %-18s %s:%d\n",
			CRYPTO_thread_id(), rw_text, operation_text,
			CRYPTO_get_lock_name(type), file, line);
		}
#endif
	if (type < 0)
		{
		if (do_dynlock_cb)
			do_dynlock_cb(mode, type, file, line);
		}
	else
		if (locking_callback != NULL)
			locking_callback(mode,type,file,line);
	}

int CRYPTO_add_lock(int *pointer, int amount, int type, const char *file,
	     int line)
	{
	int ret = 0;

	if (add_lock_callback != NULL)
		{
#ifdef LOCK_DEBUG
		int before= *pointer;
#endif

		ret=add_lock_callback(pointer,amount,type,file,line);
#ifdef LOCK_DEBUG
		fprintf(stderr,"ladd:%08lx:%2d+%2d->%2d %-18s %s:%d\n",
			CRYPTO_thread_id(),
			before,amount,ret,
			CRYPTO_get_lock_name(type),
			file,line);
#endif
		}
	else
		{
		CRYPTO_lock(CRYPTO_LOCK|CRYPTO_WRITE,type,file,line);

		ret= *pointer+amount;
#ifdef LOCK_DEBUG
		fprintf(stderr,"ladd:%08lx:%2d+%2d->%2d %-18s %s:%d\n",
			CRYPTO_thread_id(),
			*pointer,amount,ret,
			CRYPTO_get_lock_name(type),
			file,line);
#endif
		*pointer=ret;
		CRYPTO_lock(CRYPTO_UNLOCK|CRYPTO_WRITE,type,file,line);
		}
	return(ret);
	}

#if	defined(__i386)   || defined(__i386__)   || defined(_M_IX86) || \
	defined(__INTEL__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)

unsigned long  OPENSSL_ia32cap_P=0;
unsigned long *OPENSSL_ia32cap_loc(void) { return &OPENSSL_ia32cap_P; }

#if defined(OPENSSL_CPUID_OBJ) && !defined(OPENSSL_NO_ASM) && !defined(I386_ONLY)
#define OPENSSL_CPUID_SETUP
void OPENSSL_cpuid_setup(void)
{ static int trigger=0;
  unsigned long OPENSSL_ia32_cpuid(void);
  char *env;

    if (trigger)	return;

    trigger=1;
    if ((env=getenv("OPENSSL_ia32cap")))
	OPENSSL_ia32cap_P = strtoul(env,NULL,0)|(1<<10);
    else
	OPENSSL_ia32cap_P = OPENSSL_ia32_cpuid()|(1<<10);
    /*
     * |(1<<10) sets a reserved bit to signal that variable
     * was initialized already... This is to avoid interference
     * with cpuid snippets in ELF .init segment.
     */
}
#endif

#else
unsigned long *OPENSSL_ia32cap_loc(void) { return NULL; }
#endif
int OPENSSL_NONPIC_relocated = 0;
#if !defined(OPENSSL_CPUID_SETUP)
void OPENSSL_cpuid_setup(void) {}
#endif

#if (defined(_WIN32) || defined(__CYGWIN__)) && defined(_WINDLL)

#ifdef OPENSSL_FIPS

#include <tlhelp32.h>
#if defined(__GNUC__) && __GNUC__>=2
static int DllInit(void) __attribute__((constructor));
#elif defined(_MSC_VER)
static int DllInit(void);
# ifdef _WIN64
# pragma section(".CRT$XCU",read)
  __declspec(allocate(".CRT$XCU"))
# else
# pragma data_seg(".CRT$XCU")
# endif
  static int (*p)(void) = DllInit;
# pragma data_seg()
#endif

static int DllInit(void)
{
#if defined(_WIN32_WINNT)
	union	{ int(*f)(void); BYTE *p; } t = { DllInit };
        HANDLE	hModuleSnap = INVALID_HANDLE_VALUE;
	IMAGE_DOS_HEADER *dos_header;
	IMAGE_NT_HEADERS *nt_headers;
	MODULEENTRY32 me32 = {sizeof(me32)};

	hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,0);
	if (hModuleSnap != INVALID_HANDLE_VALUE &&
	    Module32First(hModuleSnap,&me32)) do
		{
		if (t.p >= me32.modBaseAddr &&
		    t.p <  me32.modBaseAddr+me32.modBaseSize)
			{
			dos_header=(IMAGE_DOS_HEADER *)me32.modBaseAddr;
			if (dos_header->e_magic==IMAGE_DOS_SIGNATURE)
				{
				nt_headers=(IMAGE_NT_HEADERS *)
					((BYTE *)dos_header+dos_header->e_lfanew);
				if (nt_headers->Signature==IMAGE_NT_SIGNATURE &&
				    me32.modBaseAddr!=(BYTE*)nt_headers->OptionalHeader.ImageBase)
					OPENSSL_NONPIC_relocated=1;
				}
			break;
			}
		} while (Module32Next(hModuleSnap,&me32));

	if (hModuleSnap != INVALID_HANDLE_VALUE)
		CloseHandle(hModuleSnap);
#endif
	OPENSSL_cpuid_setup();
	return 0;
}

#else

#ifdef __CYGWIN__
/* pick DLL_[PROCESS|THREAD]_[ATTACH|DETACH] definitions */
#include <windows.h>
#endif

/* All we really need to do is remove the 'error' state when a thread
 * detaches */

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason,
	     LPVOID lpvReserved)
	{
	switch(fdwReason)
		{
	case DLL_PROCESS_ATTACH:
		OPENSSL_cpuid_setup();
#if defined(_WIN32_WINNT)
		{
		IMAGE_DOS_HEADER *dos_header = (IMAGE_DOS_HEADER *)hinstDLL;
		IMAGE_NT_HEADERS *nt_headers;

		if (dos_header->e_magic==IMAGE_DOS_SIGNATURE)
			{
			nt_headers = (IMAGE_NT_HEADERS *)((char *)dos_header
						+ dos_header->e_lfanew);
			if (nt_headers->Signature==IMAGE_NT_SIGNATURE &&
			    hinstDLL!=(HINSTANCE)(nt_headers->OptionalHeader.ImageBase))
				OPENSSL_NONPIC_relocated=1;
			}
		}
#endif
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		ERR_remove_state(0);
		break;
	case DLL_PROCESS_DETACH:
		break;
		}
	return(TRUE);
	}
#endif

#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <tchar.h>

#if defined(_WIN32_WINNT) && _WIN32_WINNT>=0x0333
int OPENSSL_isservice(void)
{ HWINSTA h;
  DWORD len;
  WCHAR *name;

    (void)GetDesktopWindow(); /* return value is ignored */

    h = GetProcessWindowStation();
    if (h==NULL) return -1;

    if (GetUserObjectInformationW (h,UOI_NAME,NULL,0,&len) ||
	GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	return -1;

    if (len>512) return -1;		/* paranoia */
    len++,len&=~1;			/* paranoia */
#ifdef _MSC_VER
    name=(WCHAR *)_alloca(len+sizeof(WCHAR));
#else
    name=(WCHAR *)alloca(len+sizeof(WCHAR));
#endif
    if (!GetUserObjectInformationW (h,UOI_NAME,name,len,&len))
	return -1;

    len++,len&=~1;			/* paranoia */
    name[len/sizeof(WCHAR)]=L'\0';	/* paranoia */
#if 1
    /* This doesn't cover "interactive" services [working with real
     * WinSta0's] nor programs started non-interactively by Task
     * Scheduler [those are working with SAWinSta]. */
    if (wcsstr(name,L"Service-0x"))	return 1;
#else
    /* This covers all non-interactive programs such as services. */
    if (!wcsstr(name,L"WinSta0"))	return 1;
#endif
    else				return 0;
}
#else
int OPENSSL_isservice(void) { return 0; }
#endif

void OPENSSL_showfatal (const char *fmta,...)
{ va_list ap;
  TCHAR buf[256];
  const TCHAR *fmt;
#ifdef STD_ERROR_HANDLE	/* what a dirty trick! */
  HANDLE h;

    if ((h=GetStdHandle(STD_ERROR_HANDLE)) != NULL &&
	GetFileType(h)!=FILE_TYPE_UNKNOWN)
    {	/* must be console application */
	va_start (ap,fmta);
	vfprintf (stderr,fmta,ap);
	va_end (ap);
	return;
    }
#endif

    if (sizeof(TCHAR)==sizeof(char))
	fmt=(const TCHAR *)fmta;
    else do
    { int    keepgoing;
      size_t len_0=strlen(fmta)+1,i;
      WCHAR *fmtw;

#ifdef _MSC_VER
	fmtw = (WCHAR *)_alloca (len_0*sizeof(WCHAR));
#else
	fmtw = (WCHAR *)alloca (len_0*sizeof(WCHAR));
#endif
	if (fmtw == NULL) { fmt=(const TCHAR *)L"no stack?"; break; }

#ifndef OPENSSL_NO_MULTIBYTE
	if (!MultiByteToWideChar(CP_ACP,0,fmta,len_0,fmtw,len_0))
#endif
	    for (i=0;i<len_0;i++) fmtw[i]=(WCHAR)fmta[i];

	for (i=0;i<len_0;i++)
	{   if (fmtw[i]==L'%') do
	    {	keepgoing=0;
		switch (fmtw[i+1])
		{   case L'0': case L'1': case L'2': case L'3': case L'4':
		    case L'5': case L'6': case L'7': case L'8': case L'9':
		    case L'.': case L'*':
		    case L'-':	i++; keepgoing=1; break;
		    case L's':	fmtw[i+1]=L'S';   break;
		    case L'S':	fmtw[i+1]=L's';   break;
		    case L'c':	fmtw[i+1]=L'C';   break;
		    case L'C':	fmtw[i+1]=L'c';   break;
		}
	    } while (keepgoing);
	}
	fmt = (const TCHAR *)fmtw;
    } while (0);

    va_start (ap,fmta);
    _vsntprintf (buf,sizeof(buf)/sizeof(TCHAR)-1,fmt,ap);
    buf [sizeof(buf)/sizeof(TCHAR)-1] = _T('\0');
    va_end (ap);

#if defined(_WIN32_WINNT) && _WIN32_WINNT>=0x0333
    /* this -------------v--- guards NT-specific calls */
    if (GetVersion() < 0x80000000 && OPENSSL_isservice() > 0)
    {	HANDLE h = RegisterEventSource(0,_T("OPENSSL"));
	const TCHAR *pmsg=buf;
	ReportEvent(h,EVENTLOG_ERROR_TYPE,0,0,0,1,0,&pmsg,0);
	DeregisterEventSource(h);
    }
    else
#endif
	MessageBox (NULL,buf,_T("OpenSSL: FATAL"),MB_OK|MB_ICONSTOP);
}
#else
void OPENSSL_showfatal (const char *fmta,...)
{ va_list ap;

    va_start (ap,fmta);
    vfprintf (stderr,fmta,ap);
    va_end (ap);
}
int OPENSSL_isservice (void) { return 0; }
#endif

void OpenSSLDie(const char *file,int line,const char *assertion)
	{
	OPENSSL_showfatal(
		"%s(%d): OpenSSL internal error, assertion failed: %s\n",
		file,line,assertion);
	abort();
	}

void *OPENSSL_stderr(void)	{ return stderr; }
