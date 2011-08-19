/* crypto/o_dir.h -*- mode:C; c-file-style: "eay" -*- */
/* Copied from Richard Levitte's (richard@levitte.org) LP library.  All
 * symbol names have been changed, with permission from the author.
 */

/* $LP: LPlib/source/LPdir.h,v 1.1 2004/06/14 08:56:04 _cvs_levitte Exp $ */
/*
 * Copyright (c) 2004, Richard Levitte <richard@levitte.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifndef O_DIR_H
#define O_DIR_H

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct OPENSSL_dir_context_st OPENSSL_DIR_CTX;

  /* returns NULL on error or end-of-directory.
     If it is end-of-directory, errno will be zero */
  const char *OPENSSL_DIR_read(OPENSSL_DIR_CTX **ctx, const char *directory);
  /* returns 1 on success, 0 on error */
  int OPENSSL_DIR_end(OPENSSL_DIR_CTX **ctx);

#ifdef __cplusplus
}
#endif

#endif /* LPDIR_H */
