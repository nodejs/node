'use strict';

const {
  JSONParse,
  JSONStringify,
  StringPrototypeSplit,
  Symbol,
} = primordials;
const { Buffer } = require('buffer');
const { StringDecoder } = require('string_decoder');
const { serialize, IPCChannelFramer } = internalBinding('ipc_serdes');
const { streamBaseState, kLastWriteWasAsync } = internalBinding('stream_wrap');

const kFramer = Symbol('kFramer');
const kJSONBuffer = Symbol('kJSONBuffer');
const kStringDecoder = Symbol('kStringDecoder');

// Messages are parsed in either of the following formats:
// - V8-serialized buffers, prefixed with their length as a big endian uint32
//   (aka 'advanced'), or
// - newline-delimited JSON.
//
// The 'advanced' read path is framed natively: IPCChannelFramer accumulates
// partial reads in C++ and hands back an array of complete, deserialized
// messages, so JavaScript never reframes the byte stream. The 'json' read path
// stays in JavaScript: its StringDecoder + split('\n') pipeline already avoids
// copies via V8 rope concatenation and O(1) substrings, and is measurably
// faster than reframing the bytes in C++.
const advanced = {
  initMessageChannel(channel) {
    channel[kFramer] = new IPCChannelFramer();
    channel.buffering = false;
  },

  parseChannelMessages(channel, readData) {
    if (readData.length === 0) return [];
    const messages = channel[kFramer].read(readData);
    channel.buffering = channel[kFramer].buffering();
    return messages;
  },

  writeChannelMessage(channel, req, message, handle) {
    // Pass the stable Buffer constructor so the native codec can classify Node
    // Buffers via `value.constructor === Buffer` without reading the tamperable
    // Buffer.prototype.constructor.
    const serializedMessage = serialize(message, Buffer);

    const result = channel.writeBuffer(req, serializedMessage, handle);

    // Mirror what stream_base_commons.js does for Buffer retention.
    if (streamBaseState[kLastWriteWasAsync])
      req.buffer = serializedMessage;

    return result;
  },
};

const json = {
  initMessageChannel(channel) {
    channel[kJSONBuffer] = '';
    channel[kStringDecoder] = undefined;
  },

  *parseChannelMessages(channel, readData) {
    if (readData.length === 0) return;

    if (channel[kStringDecoder] === undefined)
      channel[kStringDecoder] = new StringDecoder('utf8');
    const chunks =
      StringPrototypeSplit(channel[kStringDecoder].write(readData), '\n');
    const numCompleteChunks = chunks.length - 1;
    // Last line does not have trailing linebreak
    const incompleteChunk = chunks[numCompleteChunks];
    if (numCompleteChunks === 0) {
      channel[kJSONBuffer] += incompleteChunk;
    } else {
      chunks[0] = channel[kJSONBuffer] + chunks[0];
      for (let i = 0; i < numCompleteChunks; i++)
        yield JSONParse(chunks[i]);
      channel[kJSONBuffer] = incompleteChunk;
    }
    channel.buffering = channel[kJSONBuffer].length !== 0;
  },

  writeChannelMessage(channel, req, message, handle) {
    const string = JSONStringify(message) + '\n';
    return channel.writeUtf8String(req, string, handle);
  },
};

module.exports = { advanced, json };
