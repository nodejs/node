/* compress.c -- compress a memory buffer
 * Copyright (C) 1995-2026 Jean-loup Gailly, Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#define ZLIB_INTERNAL
#include "zlib.h"

/* ===========================================================================
     Compresses the source buffer into the destination buffer. The level
   parameter has the same meaning as in deflateInit.  sourceLen is the byte
   length of the source buffer. Upon entry, destLen is the total size of the
   destination buffer, which must be at least 0.1% larger than sourceLen plus
   12 bytes. Upon exit, destLen is the actual size of the compressed buffer.

     compress2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_BUF_ERROR if there was not enough room in the output buffer,
   Z_STREAM_ERROR if the level parameter is invalid.

     The _z versions of the functions take size_t length arguments.
*/
int ZEXPORT compress2_z(Bytef *dest, z_size_t *destLen, const Bytef *source,
                        z_size_t sourceLen, int level) {
    z_stream stream;
    int err;
    const uInt max = (uInt)-1;
    z_size_t left;

    if ((sourceLen > 0 && source == NULL) ||
        destLen == NULL || (*destLen > 0 && dest == NULL))
        return Z_STREAM_ERROR;

    left = *destLen;
    *destLen = 0;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    err = deflateInit(&stream, level);
    if (err != Z_OK) return err;

    stream.next_out = dest;
    stream.avail_out = 0;
    stream.next_in = (z_const Bytef *)source;
    stream.avail_in = 0;

    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (z_size_t)max ? max : (uInt)left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = sourceLen > (z_size_t)max ? max :
                                                          (uInt)sourceLen;
            sourceLen -= stream.avail_in;
        }
        err = deflate(&stream, sourceLen ? Z_NO_FLUSH : Z_FINISH);
    } while (err == Z_OK);

    *destLen = (z_size_t)(stream.next_out - dest);
    deflateEnd(&stream);
    return err == Z_STREAM_END ? Z_OK : err;
}
int ZEXPORT compress2(Bytef *dest, uLongf *destLen, const Bytef *source,
                      uLong sourceLen, int level) {
    int ret;
    z_size_t got = *destLen;
    ret = compress2_z(dest, &got, source, sourceLen, level);
    *destLen = (uLong)got;
    return ret;
}
/* ===========================================================================
 */
int ZEXPORT compress_z(Bytef *dest, z_size_t *destLen, const Bytef *source,
                       z_size_t sourceLen) {
    return compress2_z(dest, destLen, source, sourceLen,
                       Z_DEFAULT_COMPRESSION);
}
int ZEXPORT compress(Bytef *dest, uLongf *destLen, const Bytef *source,
                     uLong sourceLen) {
    return compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}

/* ===========================================================================
     If the default memLevel or windowBits for deflateInit() is changed, then
   this function needs to be updated.
 */
z_size_t ZEXPORT compressBound_z(z_size_t sourceLen) {
    z_size_t bound = sourceLen + (sourceLen >> 12) + (sourceLen >> 14) +
                     (sourceLen >> 25) + 13;

    /* FIXME(cavalcantii): usage of CRC32 Castagnoli as a hash function
     * for the hash table of symbols used for compression has a side effect
     * where for compression level [4, 5] it will increase the output buffer size
     * by 0.1% (i.e. less than 1%) for a high entropy input (i.e. random data).
     * To avoid a scenario where client code would fail, for safety we increase
     * the expected output size by 0.8% (i.e. 8x more than the worst scenario).
     * See: http://crbug.com/990489
     */
    bound += bound >> 7; // Equivalent to 1.0078125

    return bound < sourceLen ? (z_size_t)-1 : bound;
}
uLong ZEXPORT compressBound(uLong sourceLen) {
    z_size_t bound = compressBound_z(sourceLen);
    return (uLong)bound != bound ? (uLong)-1 : (uLong)bound;
}
