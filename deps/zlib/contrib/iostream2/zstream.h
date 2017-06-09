/*
 *
 * Copyright (c) 1997
 * Christian Michelsen Research AS
 * Advanced Computing
 * Fantoftvegen 38, 5036 BERGEN, Norway
 * http://www.cmr.no
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Christian Michelsen Research AS makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef ZSTREAM__H
#define ZSTREAM__H

/*
 * zstream.h - C++ interface to the 'zlib' general purpose compression library
 * $Id: zstream.h 1.1 1997-06-25 12:00:56+02 tyge Exp tyge $
 */

#include <strstream.h>
#include <string.h>
#include <stdio.h>
#include "zlib.h"

#if defined(_WIN32)
#   include <fcntl.h>
#   include <io.h>
#   define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#   define SET_BINARY_MODE(file)
#endif

class zstringlen {
public:
    zstringlen(class izstream&);
    zstringlen(class ozstream&, const char*);
    size_t value() const { return val.word; }
private:
    struct Val { unsigned char byte; size_t word; } val;
};

//  ----------------------------- izstream -----------------------------

class izstream
{
    public:
        izstream() : m_fp(0) {}
        izstream(FILE* fp) : m_fp(0) { open(fp); }
        izstream(const char* name) : m_fp(0) { open(name); }
        ~izstream() { close(); }

        /* Opens a gzip (.gz) file for reading.
         * open() can be used to read a file which is not in gzip format;
         * in this case read() will directly read from the file without
         * decompression. errno can be checked to distinguish two error
         * cases (if errno is zero, the zlib error is Z_MEM_ERROR).
         */
        void open(const char* name) {
            if (m_fp) close();
            m_fp = ::gzopen(name, "rb");
        }

        void open(FILE* fp) {
            SET_BINARY_MODE(fp);
            if (m_fp) close();
            m_fp = ::gzdopen(fileno(fp), "rb");
        }

        /* Flushes all pending input if necessary, closes the compressed file
         * and deallocates all the (de)compression state. The return value is
         * the zlib error number (see function error() below).
         */
        int close() {
            int r = ::gzclose(m_fp);
            m_fp = 0; return r;
        }

        /* Binary read the given number of bytes from the compressed file.
         */
        int read(void* buf, size_t len) {
            return ::gzread(m_fp, buf, len);
        }

        /* Returns the error message for the last error which occurred on the
         * given compressed file. errnum is set to zlib error number. If an
         * error occurred in the file system and not in the compression library,
         * errnum is set to Z_ERRNO and the application may consult errno
         * to get the exact error code.
         */
        const char* error(int* errnum) {
            return ::gzerror(m_fp, errnum);
        }

        gzFile fp() { return m_fp; }

    private:
        gzFile m_fp;
};

/*
 * Binary read the given (array of) object(s) from the compressed file.
 * If the input file was not in gzip format, read() copies the objects number
 * of bytes into the buffer.
 * returns the number of uncompressed bytes actually read
 * (0 for end of file, -1 for error).
 */
template <class T, class Items>
inline int read(izstream& zs, T* x, Items items) {
    return ::gzread(zs.fp(), x, items*sizeof(T));
}

/*
 * Binary input with the '>' operator.
 */
template <class T>
inline izstream& operator>(izstream& zs, T& x) {
    ::gzread(zs.fp(), &x, sizeof(T));
    return zs;
}


inline zstringlen::zstringlen(izstream& zs) {
    zs > val.byte;
    if (val.byte == 255) zs > val.word;
    else val.word = val.byte;
}

/*
 * Read length of string + the string with the '>' operator.
 */
inline izstream& operator>(izstream& zs, char* x) {
    zstringlen len(zs);
    ::gzread(zs.fp(), x, len.value());
    x[len.value()] = '\0';
    return zs;
}

inline char* read_string(izstream& zs) {
    zstringlen len(zs);
    char* x = new char[len.value()+1];
    ::gzread(zs.fp(), x, len.value());
    x[len.value()] = '\0';
    return x;
}

// ----------------------------- ozstream -----------------------------

class ozstream
{
    public:
        ozstream() : m_fp(0), m_os(0) {
        }
        ozstream(FILE* fp, int level = Z_DEFAULT_COMPRESSION)
            : m_fp(0), m_os(0) {
            open(fp, level);
        }
        ozstream(const char* name, int level = Z_DEFAULT_COMPRESSION)
            : m_fp(0), m_os(0) {
            open(name, level);
        }
        ~ozstream() {
            close();
        }

        /* Opens a gzip (.gz) file for writing.
         * The compression level parameter should be in 0..9
         * errno can be checked to distinguish two error cases
         * (if errno is zero, the zlib error is Z_MEM_ERROR).
         */
        void open(const char* name, int level = Z_DEFAULT_COMPRESSION) {
            char mode[4] = "wb\0";
            if (level != Z_DEFAULT_COMPRESSION) mode[2] = '0'+level;
            if (m_fp) close();
            m_fp = ::gzopen(name, mode);
        }

        /* open from a FILE pointer.
         */
        void open(FILE* fp, int level = Z_DEFAULT_COMPRESSION) {
            SET_BINARY_MODE(fp);
            char mode[4] = "wb\0";
            if (level != Z_DEFAULT_COMPRESSION) mode[2] = '0'+level;
            if (m_fp) close();
            m_fp = ::gzdopen(fileno(fp), mode);
        }

        /* Flushes all pending output if necessary, closes the compressed file
         * and deallocates all the (de)compression state. The return value is
         * the zlib error number (see function error() below).
         */
        int close() {
            if (m_os) {
                ::gzwrite(m_fp, m_os->str(), m_os->pcount());
                delete[] m_os->str(); delete m_os; m_os = 0;
            }
            int r = ::gzclose(m_fp); m_fp = 0; return r;
        }

        /* Binary write the given number of bytes into the compressed file.
         */
        int write(const void* buf, size_t len) {
            return ::gzwrite(m_fp, (voidp) buf, len);
        }

        /* Flushes all pending output into the compressed file. The parameter
         * _flush is as in the deflate() function. The return value is the zlib
         * error number (see function gzerror below). flush() returns Z_OK if
         * the flush_ parameter is Z_FINISH and all output could be flushed.
         * flush() should be called only when strictly necessary because it can
         * degrade compression.
         */
        int flush(int _flush) {
            os_flush();
            return ::gzflush(m_fp, _flush);
        }

        /* Returns the error message for the last error which occurred on the
         * given compressed file. errnum is set to zlib error number. If an
         * error occurred in the file system and not in the compression library,
         * errnum is set to Z_ERRNO and the application may consult errno
         * to get the exact error code.
         */
        const char* error(int* errnum) {
            return ::gzerror(m_fp, errnum);
        }

        gzFile fp() { return m_fp; }

        ostream& os() {
            if (m_os == 0) m_os = new ostrstream;
            return *m_os;
        }

        void os_flush() {
            if (m_os && m_os->pcount()>0) {
                ostrstream* oss = new ostrstream;
                oss->fill(m_os->fill());
                oss->flags(m_os->flags());
                oss->precision(m_os->precision());
                oss->width(m_os->width());
                ::gzwrite(m_fp, m_os->str(), m_os->pcount());
                delete[] m_os->str(); delete m_os; m_os = oss;
            }
        }

    private:
        gzFile m_fp;
        ostrstream* m_os;
};

/*
 * Binary write the given (array of) object(s) into the compressed file.
 * returns the number of uncompressed bytes actually written
 * (0 in case of error).
 */
template <class T, class Items>
inline int write(ozstream& zs, const T* x, Items items) {
    return ::gzwrite(zs.fp(), (voidp) x, items*sizeof(T));
}

/*
 * Binary output with the '<' operator.
 */
template <class T>
inline ozstream& operator<(ozstream& zs, const T& x) {
    ::gzwrite(zs.fp(), (voidp) &x, sizeof(T));
    return zs;
}

inline zstringlen::zstringlen(ozstream& zs, const char* x) {
    val.byte = 255;  val.word = ::strlen(x);
    if (val.word < 255) zs < (val.byte = val.word);
    else zs < val;
}

/*
 * Write length of string + the string with the '<' operator.
 */
inline ozstream& operator<(ozstream& zs, const char* x) {
    zstringlen len(zs, x);
    ::gzwrite(zs.fp(), (voidp) x, len.value());
    return zs;
}

#ifdef _MSC_VER
inline ozstream& operator<(ozstream& zs, char* const& x) {
    return zs < (const char*) x;
}
#endif

/*
 * Ascii write with the << operator;
 */
template <class T>
inline ostream& operator<<(ozstream& zs, const T& x) {
    zs.os_flush();
    return zs.os() << x;
}

#endif
