/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib_int.h"

#if defined(_WIN32) || defined(__CYGWIN__)
# ifdef __CYGWIN__
/* pick DLL_[PROCESS|THREAD]_[ATTACH|DETACH] definitions */
#  include <windows.h>
/*
 * this has side-effect of _WIN32 getting defined, which otherwise is
 * mutually exclusive with __CYGWIN__...
 */
# endif

/*
 * All we really need to do is remove the 'error' state when a thread
 * detaches
 */

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OPENSSL_cpuid_setup();
# if defined(_WIN32_WINNT)
        {
            IMAGE_DOS_HEADER *dos_header = (IMAGE_DOS_HEADER *) hinstDLL;
            IMAGE_NT_HEADERS *nt_headers;

            if (dos_header->e_magic == IMAGE_DOS_SIGNATURE) {
                nt_headers = (IMAGE_NT_HEADERS *) ((char *)dos_header
                                                   + dos_header->e_lfanew);
                if (nt_headers->Signature == IMAGE_NT_SIGNATURE &&
                    hinstDLL !=
                    (HINSTANCE) (nt_headers->OptionalHeader.ImageBase))
                    OPENSSL_NONPIC_relocated = 1;
            }
        }
# endif
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        OPENSSL_thread_stop();
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return (TRUE);
}
#endif

