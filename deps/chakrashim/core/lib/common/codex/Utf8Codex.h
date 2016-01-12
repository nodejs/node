//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#include <windows.h>
#include <wtypes.h>

typedef unsigned __int32 uint32;

// charcount_t represents a count of characters in a String
// It is unsigned and the maximum value is (INT_MAX-1)
typedef uint32 charcount_t;

typedef BYTE utf8char_t;
typedef const utf8char_t CUTF8;
typedef utf8char_t* LPUTF8;
typedef const utf8char_t *LPCUTF8;

// Unicode 4.0, unknown char should be converted to replace mark, U+FFFD.
#define UNICODE_UNKNOWN_CHAR_MARK 0xFFFD
#define UNICODE_TCHAR_UKNOWN_CHAR_MARK _T('\xFFFD')

namespace utf8
{

    // Terminology -
    //   Code point      - A ordinal value mapped to an standard ideograph as defined by ISO/IEC 10646-1. Here
    //                     also referred to as a UCS code point but can also be often be referred to as a UNICODE
    //                     code point.
    //   UTF-8           - An encoding of UCS code points as defined by RFC-3629.
    //   UTF-16          - An encoding of UCS code points as defined by RFC-2781. Use as a synonym for UNICODE or
    //                     UCS-2. This is technically incorrect but usually harmless. This file assumes wchar_t *
    //                     maps to an UTF-16LE (little-endian) encoded sequence of words.
    //   Unit            - The unit of encoding. For UTF-8 it is a byte (octet). For UTF-16 it is a word (two octets).
    //   Valid           - A UTF-8 byte sequence conforming to RFC-3629.
    //   Well-formed     - A sequence of bytes that conform to the encoding pattern of UTF8 but might be too long or
    //                     otherwise invalid. For example C0 80 is a well-formed but invalid encoding of U+0000.
    //   Start byte      - A byte can start a well-formed UTF-8 sequence.
    //   Lead byte       - A byte can start a well-formed multi-unit sequence but not a single byte sequence.
    //   Trail byte      - A byte that can appear after a lead-byte in a well-formed multi-unit sequence.
    //   Surrogate pair  - A UTF-16 word pair to encode characters outside the Unicode base plain as defined by
    //                     RFC-2781. Two wchar_t values are used to encode one UCS code point.
    //   character index - The index into a UTF-16 sequence.
    //   byte index      - The index into a UTF-8 sequence.

    // Return the number of bytes needed to encode the given character (ignoring surrogate pairs)
    inline size_t EncodedSize(wchar_t ch)
    {
        if (ch < 0x0080) return 1;
        if (ch < 0x0800) return 2;
        return 3;
    }

    enum DecodeOptions
    {
        doDefault                   = 0x00,
        doAllowThreeByteSurrogates  = 0x01, // Allow invalid 3 byte encodings as would be encoded by CSEU-8
        doChunkedEncoding           = 0x02, // For sequences at the end of a buffer do not advance into incomplete sequences
                                            //   If incomplete UTF-8 sequence is encountered at the end of a buffer, this
                                            //   option will cause Decode() to not advance the ptr value and DecodeTail to
                                            //   move the pointer back one position so it again points to where c1 was read by
                                            //   Decode(). In effect, incomplete sequences are treated as if end pointed to the
                                            //   beginning incomplete sequence instead of in the middle of it.
        doSecondSurrogatePair       = 0x04, // A previous call to DecodeTail returned the first word of a UTF-16
                                            // surrogate pair. The second call will return the second word and reset
                                            // this 'option'.
        doAllowInvalidWCHARs        = 0x08, // Don't replace invalid wide chars with 0xFFFD
    };
    DEFINE_ENUM_FLAG_OPERATORS(DecodeOptions);

    // Decode the trail bytes after the UTF8 lead byte c1 but returning 0xFFFD if trail bytes are expected after end.
    _At_(ptr, _In_reads_(end - ptr) _Post_satisfies_(ptr >= _Old_(ptr) - 1 && ptr <= end))
    wchar_t DecodeTail(wchar_t c1, LPCUTF8& ptr, LPCUTF8 end, DecodeOptions& options);

    // Decode the UTF8 sequence into a UTF16 encoding. Code points outside the Unicode base plain will generate
    // surrogate pairs, using the 'doSecondSurrogatePair' option to remember the first word has already been returned.
    // If ptr == end 0x0000 is emitted. If ptr < end but the lead byte of the UTF8 sequence
    // expects trail bytes past end then 0xFFFD are emitted until ptr == end.
    _At_(ptr, _In_reads_(end - ptr) _Post_satisfies_(ptr >= _Old_(ptr) && ptr <= end))
    inline wchar_t Decode(LPCUTF8& ptr, LPCUTF8 end, DecodeOptions& options)
    {
        if (ptr >= end) return 0;
        utf8char_t c1 = *ptr++;
        if (c1 < 0x80) return static_cast<wchar_t>(c1);
        return DecodeTail(c1, ptr, end, options);
    }

    // Encode ch into a UTF8 sequence ignoring surrogate pairs (which are encoded as two
    // separate code points). Use Encode() instead of EncodeFull() directly because it
    // special cases ASCII to avoid a call the most common characters.
    LPUTF8 EncodeFull(wchar_t ch, __out_ecount(3) LPUTF8 ptr);

    // Encode ch into a UTF8 sequence ignoring surrogate pairs (which are encoded as two
    // separate code points).
    inline LPUTF8 Encode(wchar_t ch, __out_ecount(3) LPUTF8 ptr)
    {
        if (ch < 0x80)
        {
            *ptr = static_cast<utf8char_t>(ch);
            return ptr + 1;
        }
        return EncodeFull(ch, ptr);
    }

    // Return true if ch is a lead byte of a UTF8 multi-unit sequence.
    inline bool IsLeadByte(utf8char_t ch)
    {
        return ch >= 0xC0;
    }

    // Return true if ch is a byte that starts a well-formed UTF8 sequence (i.e. is a ASCII character or a valid UTF8 lead byte)
    inline bool IsStartByte(utf8char_t ch)
    {
        return ch < 0x80 || ch >= 0xC0;
    }

    // Returns true if ch is a UTF8 multi-unit sequence trail byte.
    inline bool IsTrailByte(utf8char_t ch)
    {
        return (ch & 0xC0) == 0x80;
    }

    // Returns true if ptr points to a well-formed UTF8
    inline bool IsCharStart(LPCUTF8 ptr)
    {
        return IsStartByte(*ptr);
    }

    // Return the start of the next well-formed UTF-8 sequence. Use NextChar() instead of
    // NextCharFull() since NextChar() avoid a call if ptr references a single byte sequence.
    LPCUTF8 NextCharFull(LPCUTF8 ptr);

    // Return the start of the next well-formed UTF-8 sequence.
    inline LPCUTF8 NextChar(LPCUTF8 ptr)
    {
        if (*ptr < 0x80) return ptr + 1;
        return NextCharFull(ptr);
    }

    // Return the start of the previous well-formed UTF-8 sequence prior to start or start if
    // if ptr is already start or no well-formed sequence starts a start. Use PrevChar() instead of
    // PrevCharFull() since PrevChar() avoids a call if the previous sequence is a single byte
    // sequence.
    LPCUTF8 PrevCharFull(LPCUTF8 ptr, LPCUTF8 start);

    // Return the start of the previous well-formed UTF-8 sequence prior to start or start if
    // if ptr is already start or no well-formed sequence starts a start.
    inline LPCUTF8 PrevChar(LPCUTF8 ptr, LPCUTF8 start)
    {
        if (ptr > start && *(ptr - 1) < 0x80) return ptr - 1;
        return PrevCharFull(ptr, start);
    }

    // Decode a UTF-8 sequence of cch UTF-16 characters into buffer. ptr could advance up to 3 times
    // longer than cch so DecodeInto should only be used when it is already known that
    // ptr refers to at least cch number of UTF-8 sequences.
    void DecodeInto(__out_ecount_full(cch) wchar_t *buffer, LPCUTF8 ptr, size_t cch, DecodeOptions options = doDefault);

    // Provided for dual-mode templates
    inline void DecodeInto(__out_ecount_full(cch) wchar_t *buffer, const wchar_t *ptr, size_t cch, DecodeOptions /* options */ = doDefault)
    {
        memcpy_s(buffer, cch * sizeof(wchar_t), ptr, cch * sizeof(wchar_t));
    }

    // Like DecodeInto but ensures buffer ends with a NULL at buffer[cch].
    void DecodeIntoAndNullTerminate(__out_ecount(cch+1) __nullterminated wchar_t *buffer, LPCUTF8 ptr, size_t cch, DecodeOptions options = doDefault);

    // Decode cb bytes from ptr to into buffer returning the number of characters converted and written to buffer
    _Ret_range_(0, pbEnd - _Old_(pbUtf8))
    size_t DecodeUnitsInto(_Out_writes_(pbEnd - pbUtf8) wchar_t *buffer, LPCUTF8& pbUtf8, LPCUTF8 pbEnd, DecodeOptions options = doDefault);

    // Decode cb bytes from ptr to into buffer returning the number of characters converted and written to buffer (excluding the null terminator)
    size_t DecodeUnitsIntoAndNullTerminate(__out_ecount(pbEnd - pbUtf8 + 1) __nullterminated wchar_t *buffer, LPCUTF8& pbUtf8, LPCUTF8 pbEnd, DecodeOptions options = doDefault);

    // Encode a UTF-8 sequence into a UTF-8 sequence (which is just a memcpy). This is included for convenience in templates
    // when the character encoding is a template parameter.
    __range(cch, cch)
    inline size_t EncodeInto(__out_ecount(cch) utf8char_t *buffer, const utf8char_t *source, size_t cch)
    {
       memcpy_s(buffer, cch * sizeof(utf8char_t), source, cch * sizeof(utf8char_t));
       return cch;
    }

    // Encode a UTF16-LE sequence of cch words into a UTF-8 sequence returning the number of bytes needed.
    // Since a UTF16 encoding can take up to 3 bytes buffer must refer to a buffer at least 3 times larger
    // than cch.
    // Returns the number of bytes copied into the buffer.
    __range(0, cch * 3)
    size_t EncodeInto(__out_ecount(cch * 3) LPUTF8 buffer, __in_ecount(cch) const wchar_t *source, charcount_t cch);

    // Like EncodeInto but ensures that buffer[return value] == 0.
    __range(0, cch * 3)
    size_t EncodeIntoAndNullTerminate(__out_ecount(cch * 3 + 1) utf8char_t *buffer, __in_ecount(cch) const wchar_t *source, charcount_t cch);

    // Returns true if the pch refers to a UTF-16LE encoding of the given UTF-8 encoding bch.
    bool CharsAreEqual(__in_ecount(cch) LPCOLESTR pch, LPCUTF8 bch, size_t cch, DecodeOptions options = doDefault);

    // Convert the character index into a byte index.
    size_t CharacterIndexToByteIndex(__in_ecount(cbLength) LPCUTF8 pch, size_t cbLength, const charcount_t cchIndex, size_t cbStartIndex, charcount_t cchStartIndex, DecodeOptions options = doDefault);
    size_t CharacterIndexToByteIndex(__in_ecount(cbLength) LPCUTF8 pch, size_t cbLength, const charcount_t cchIndex, DecodeOptions options = doDefault);

    // Convert byte index into character index
    charcount_t ByteIndexIntoCharacterIndex(__in_ecount(cbIndex) LPCUTF8 pch, size_t cbIndex, DecodeOptions options = doDefault);
}
