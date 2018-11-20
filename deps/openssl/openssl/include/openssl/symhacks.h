/*
 * Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_SYMHACKS_H
# define HEADER_SYMHACKS_H

# include <openssl/e_os2.h>

/* Case insensitive linking causes problems.... */
# if defined(OPENSSL_SYS_VMS)
#  undef ERR_load_CRYPTO_strings
#  define ERR_load_CRYPTO_strings                 ERR_load_CRYPTOlib_strings
#  undef OCSP_crlID_new
#  define OCSP_crlID_new                          OCSP_crlID2_new

#  undef d2i_ECPARAMETERS
#  define d2i_ECPARAMETERS                        d2i_UC_ECPARAMETERS
#  undef i2d_ECPARAMETERS
#  define i2d_ECPARAMETERS                        i2d_UC_ECPARAMETERS
#  undef d2i_ECPKPARAMETERS
#  define d2i_ECPKPARAMETERS                      d2i_UC_ECPKPARAMETERS
#  undef i2d_ECPKPARAMETERS
#  define i2d_ECPKPARAMETERS                      i2d_UC_ECPKPARAMETERS

/*
 * These functions do not seem to exist! However, I'm paranoid... Original
 * command in x509v3.h: These functions are being redefined in another
 * directory, and clash when the linker is case-insensitive, so let's hide
 * them a little, by giving them an extra 'o' at the beginning of the name...
 */
#  undef X509v3_cleanup_extensions
#  define X509v3_cleanup_extensions               oX509v3_cleanup_extensions
#  undef X509v3_add_extension
#  define X509v3_add_extension                    oX509v3_add_extension
#  undef X509v3_add_netscape_extensions
#  define X509v3_add_netscape_extensions          oX509v3_add_netscape_extensions
#  undef X509v3_add_standard_extensions
#  define X509v3_add_standard_extensions          oX509v3_add_standard_extensions

/* This one clashes with CMS_data_create */
#  undef cms_Data_create
#  define cms_Data_create                         priv_cms_Data_create

# endif

#endif                          /* ! defined HEADER_VMS_IDHACKS_H */
