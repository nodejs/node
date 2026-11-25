/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#ifndef ZSTD_DDICT_H
#define ZSTD_DDICT_H

/*-*******************************************************
 *  Dependencies
 *********************************************************/
#include "../common/zstd_deps.h"   /* size_t */
#include "../zstd.h"     /* ZSTD_DDict, and several public functions */


/*-*******************************************************
 *  Interface
 *********************************************************/

/* note: several prototypes are already published in `zstd.h` :
 * ZSTD_createDDict()
 * ZSTD_createDDict_byReference()
 * ZSTD_createDDict_advanced()
 * ZSTD_freeDDict()
 * ZSTD_initStaticDDict()
 * ZSTD_sizeof_DDict()
 * ZSTD_estimateDDictSize()
 * ZSTD_getDictID_fromDict()
 */

/* The complete dictionary blob, including its header. Legacy decoders need
 * the original bytes; ZSTD_copyDDictParameters() excludes the header from
 * modern match history. */
const void* ZSTD_DDict_dictContent(const ZSTD_DDict* ddict);
size_t ZSTD_DDict_dictSize(const ZSTD_DDict* ddict);

void ZSTD_copyDDictParameters(ZSTD_DCtx* dctx, const ZSTD_DDict* ddict);

/* Same as ZSTD_createDDict_advanced(), but returns the error code on failure,
 * in which case *ddictPtr is set to NULL. */
size_t ZSTD_createDDict_advanced_internal(ZSTD_DDict** ddictPtr,
                                          const void* dict, size_t dictSize,
                                          ZSTD_dictLoadMethod_e dictLoadMethod,
                                          ZSTD_dictContentType_e dictContentType,
                                          ZSTD_customMem customMem);



#endif /* ZSTD_DDICT_H */
