// From https://npmjs.com/package/@exodus/bytes
// Copyright Exodus Movement. Licensed under MIT License.

'use strict';

const {
  Uint8Array,
} = primordials;


/**
 * Get a number of last bytes in an Uint8Array `data` ending at `len` that don't
 * form a codepoint yet, but can be a part of a single codepoint on more data.
 * @param {Uint8Array} data Uint8Array of potentially UTF-8 bytes
 * @param {number} len Position to look behind from
 * @returns {number} Number of unfinished potentially valid UTF-8 bytes ending at position `len`
 */
function unfinishedBytesUtf8(data, len) {
  // 0-3
  let pos = 0;
  while (pos < 2 && pos < len && (data[len - pos - 1] & 0xc0) === 0x80) pos++; // Go back 0-2 trailing bytes
  if (pos === len) return 0; // no space for lead
  const lead = data[len - pos - 1];
  if (lead < 0xc2 || lead > 0xf4) return 0; // not a lead
  if (pos === 0) return 1; // Nothing to recheck, we have only lead, return it. 2-byte must return here
  if (lead < 0xe0 || (lead < 0xf0 && pos >= 2)) return 0; // 2-byte, or 3-byte or less and we already have 2 trailing
  const lower = lead === 0xf0 ? 0x90 : lead === 0xe0 ? 0xa0 : 0x80;
  const upper = lead === 0xf4 ? 0x8f : lead === 0xed ? 0x9f : 0xbf;
  const next = data[len - pos];
  return next >= lower && next <= upper ? pos + 1 : 0;
}

/**
 * Merge prefix `chunk` with `data` and return new combined prefix.
 * For data.length < 3, fully consumes data and can return unfinished data,
 * otherwise returns a prefix with no unfinished bytes
 * @param {Uint8Array} data Uint8Array of potentially UTF-8 bytes
 * @param {Uint8Array} chunk Prefix to prepend before `data`
 * @returns {Uint8Array} If data.length >= 3: an Uint8Array containing `chunk` and a slice of `data`
 *   so that the result has no unfinished UTF-8 codepoints. If data.length < 3: concat(chunk, data).
 */
function mergePrefixUtf8(data, chunk) {
  if (data.length === 0) return chunk;
  if (data.length < 3) {
    // No reason to bruteforce offsets, also it's possible this doesn't yet end the sequence
    const res = new Uint8Array(data.length + chunk.length);
    res.set(chunk);
    res.set(data, chunk.length);
    return res;
  }

  // Slice off a small portion of data into prefix chunk so we can decode them separately without extending array size
  const temp = new Uint8Array(chunk.length + 3); // We have 1-3 bytes and need 1-3 more bytes
  temp.set(chunk);
  temp.set(data.subarray(0, 3), chunk.length);

  // Stop at the first offset where unfinished bytes reaches 0 or fits into data
  // If that doesn't happen (data too short), just concat chunk and data completely (above)
  for (let i = 1; i <= 3; i++) {
    const unfinished = unfinishedBytesUtf8(temp, chunk.length + i); // 0-3
    if (unfinished <= i) {
      // Always reachable at 3, but we still need 'unfinished' value for it
      const add = i - unfinished; // 0-3
      return add > 0 ? temp.subarray(0, chunk.length + add) : chunk;
    }
  }

  // Unreachable
  return null;
}

module.exports = { unfinishedBytesUtf8, mergePrefixUtf8 };
