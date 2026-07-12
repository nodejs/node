'use strict';

// DOS/IBM legacy decoding: MS-DOS date/time fields (sec. 4.4.6) and the
// historical Code Page 437 name/comment encoding, plus the modern
// UTF-8/Unicode-Path handling layered on top of them.

const {
  Date,
  NumberIsNaN,
  StringFromCharCode,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { crc32: crc32Native } = internalBinding('zlib');
const { isUtf8 } = internalBinding('buffer');
const {
  FLAG_UTF8,
  EXTRA_ID_UNICODE_PATH,
} = require('internal/zip/constants');
const { forEachExtraField } = require('internal/zip/extra-fields');

// DOS date/time (sec. 4.4.6): local time by convention.
// time: bits 0-4 seconds/2, 5-10 minutes, 11-15 hours
// date: bits 0-4 day, 5-8 month, 9-15 years since 1980
function decodeDosDateTime(time, date) {
  // A zeroed/absent date field has month 0 and day 0, both invalid; the DOS
  // epoch is 1980-01-01. Month/day 0 are treated as 1 so a zero field decodes
  // to 1980-01-01 (and re-encodes to the same value).
  return new Date(
    ((date >>> 9) & 0x7f) + 1980,
    ((date >>> 5) & 0x0f || 1) - 1,
    (date & 0x1f) || 1,
    (time >>> 11) & 0x1f,
    (time >>> 5) & 0x3f,
    (time & 0x1f) * 2,
  );
}

// Encode a Date into packed MS-DOS time/date fields (sec. 4.4.6), clamping to
// the representable range 1980-01-01 .. 2107-12-31.
function encodeDosDateTime(value) {
  const year = value.getFullYear();
  if (NumberIsNaN(year)) {
    throw new ERR_INVALID_ARG_VALUE('modified', value, 'must be a valid Date');
  }
  if (year < 1980) return { time: 0, date: (1 << 5) | 1 }; // Clamp to 1980-01-01 00:00:00
  if (year > 2107) {
    // Clamp to 2107-12-31 23:59:58
    return {
      time: (23 << 11) | (59 << 5) | 29,
      date: (127 << 9) | (12 << 5) | 31,
    };
  }
  const date =
    ((year - 1980) << 9) | ((value.getMonth() + 1) << 5) | value.getDate();
  const time =
    (value.getHours() << 11) |
    (value.getMinutes() << 5) |
    (value.getSeconds() >>> 1);
  return { time, date };
}

// Code Page 437 high half (0x80-0xFF) -> Unicode. Names without the UTF-8
// language-encoding flag (bit 11) are historically CP437, which every real
// tool (Info-ZIP, Windows Explorer) assumes; bytes 0x00-0x7F are ASCII.
const CP437_HIGH = [
  0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
  0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
  0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
  0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
  0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
  0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
  0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
  0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
  0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
  0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
  0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
  0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
  0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
  0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
  0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
  0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0,
];

// Decode a CP437-encoded name/comment to a JS string via the table above.
function decodeCp437(buffer) {
  let out = '';
  for (let i = 0; i < buffer.length; i++) {
    const b = buffer[i];
    out += StringFromCharCode(b < 0x80 ? b : CP437_HIGH[b - 0x80]);
  }
  return out;
}

// The UTF-8 name from an Info-ZIP Unicode Path extra field (sec. 4.6.9), but
// only when its version is 1 and its CRC-32 matches the standard-field name
// bytes (so a stale extra left over from a rename is ignored). Otherwise null.
function unicodePathName(extra, standardNameBuffer) {
  let result = null;
  forEachExtraField(extra, (id, body) => {
    if (result !== null || id !== EXTRA_ID_UNICODE_PATH || body.length < 5) return;
    if (body[0] !== 1) return;
    // The crc32 binding returns an unsigned uint32, directly comparable.
    if (body.readUInt32LE(1) !== crc32Native(standardNameBuffer, 0)) return;
    result = body.toString('utf8', 5);
  });
  return result;
}

// Decode an entry name: prefer a valid Unicode Path extra field, else the
// UTF-8/CP437 heuristic below.
function decodeZipName(nameBuffer, flags, extra) {
  if (extra?.length) {
    const unicode = unicodePathName(extra, nameBuffer);
    if (unicode !== null) return unicode;
  }
  return decodeZipText(nameBuffer, flags);
}

// Decode a name/comment: UTF-8 when bit 11 says so, or when the bytes are
// valid UTF-8 anyway - plenty of real tools (pre-JDK7 java.util.zip among
// them) wrote UTF-8 names without ever setting the flag. Only genuinely
// non-UTF-8 bytes take the historical CP437 default.
function decodeZipText(buffer, flags) {
  if ((flags & FLAG_UTF8) || isUtf8(buffer)) return buffer.toString('utf8');
  return decodeCp437(buffer);
}

module.exports = {
  decodeDosDateTime,
  encodeDosDateTime,
  decodeZipName,
  decodeZipText,
};
