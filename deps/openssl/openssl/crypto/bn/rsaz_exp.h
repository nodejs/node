/*****************************************************************************
*                                                                            *
*  Copyright (c) 2012, Intel Corporation                                     *
*                                                                            *
*  All rights reserved.                                                      *
*                                                                            *
*  Redistribution and use in source and binary forms, with or without        *
*  modification, are permitted provided that the following conditions are    *
*  met:                                                                      *
*                                                                            *
*  *  Redistributions of source code must retain the above copyright         *
*     notice, this list of conditions and the following disclaimer.          *
*                                                                            *
*  *  Redistributions in binary form must reproduce the above copyright      *
*     notice, this list of conditions and the following disclaimer in the    *
*     documentation and/or other materials provided with the                 *
*     distribution.                                                          *
*                                                                            *
*  *  Neither the name of the Intel Corporation nor the names of its         *
*     contributors may be used to endorse or promote products derived from   *
*     this software without specific prior written permission.               *
*                                                                            *
*                                                                            *
*  THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION ""AS IS"" AND ANY          *
*  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE         *
*  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR        *
*  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL CORPORATION OR            *
*  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,     *
*  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,       *
*  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR        *
*  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    *
*  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      *
*  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        *
*  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              *
*                                                                            *
******************************************************************************
* Developers and authors:                                                    *
* Shay Gueron (1, 2), and Vlad Krasnov (1)                                   *
* (1) Intel Corporation, Israel Development Center, Haifa, Israel            *
* (2) University of Haifa, Israel                                            *
*****************************************************************************/

#ifndef RSAZ_EXP_H
# define RSAZ_EXP_H

# undef RSAZ_ENABLED
# if defined(OPENSSL_BN_ASM_MONT) && \
        (defined(__x86_64) || defined(__x86_64__) || \
         defined(_M_AMD64) || defined(_M_X64))
#  define RSAZ_ENABLED

#  include <openssl/bn.h>

void RSAZ_1024_mod_exp_avx2(BN_ULONG result[16],
                            const BN_ULONG base_norm[16],
                            const BN_ULONG exponent[16],
                            const BN_ULONG m_norm[16], const BN_ULONG RR[16],
                            BN_ULONG k0);
int rsaz_avx2_eligible();

void RSAZ_512_mod_exp(BN_ULONG result[8],
                      const BN_ULONG base_norm[8], const BN_ULONG exponent[8],
                      const BN_ULONG m_norm[8], BN_ULONG k0,
                      const BN_ULONG RR[8]);

# endif

#endif
