/*
  liboverride.c -- override function called by zip_open()

  Copyright (C) 2017-2022 Dieter Baron and Thomas Klausner

  This file is part of ckmame, a program to check rom sets for MAME.
  The authors can be contacted at <ckmame@nih.at>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The name of the author may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
int main(int argc, const char *argv[]) {
    /* Symbol override is not supported on Windows. */
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        printf("not supported on Windows\n");
    }
    exit(1);
}
#else

#include <errno.h>
#include <unistd.h>

#include "zip.h"

/*
 Some systems bind functions called and defined within a shared library, so the override doesn't work. This program calls zip_open and checks whether the override worked.
 */

int
main(int argc, const char *argv[]) {
    int verbose = 0;
    int error_code;
    
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        verbose = 1;
    }
    
    if (getenv("LIBOVERRIDE_SET") == NULL) {
        char *cwd = getcwd(NULL, 0);
        size_t so_size = strlen(cwd) + 64;
        char *so = (char *)malloc(so_size);
        if (so == NULL) {
            if (verbose) {
                printf("malloc failed\n");
            }
            exit(2);
        }
        snprintf(so, so_size, "%s/libliboverride.so", cwd);
        setenv("LIBOVERRIDE_SET", "1", 1);
        setenv("LD_PRELOAD", so, 1);
        execv(argv[0], (void *)argv);
        if (verbose) {
            printf("exec failed: %s\n", strerror(errno));
        }
        exit(2);
    }
    
    if (zip_open("nosuchfile", 0, &error_code) != NULL) {
        /* We expect failure. */
        if (verbose) {
            printf("open succeeded\n");
        }
        exit(1);
    }
    if (error_code != 32000) {
        /* Override didn't take, we didn't get its magic error code. */
        if (verbose) {
            printf("got unexpected error %d\n", error_code);
        }
        exit(1);
    }
    
    if (verbose) {
        printf("success\n");
    }
    exit(0);
}

#endif /* not Windows */
