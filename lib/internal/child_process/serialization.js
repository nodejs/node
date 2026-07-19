'use strict';

const {
  ArrayPrototypePush,
  JSONParse,
  JSONStringify,
  StringPrototypeSplit,
  Symbol,
  TypedArrayPrototypeSubarray,
} = primordials;
const { Buffer } = require('buffer');
const { StringDecoder } = require('string_decoder');
const { serialize, deserialize } = internalBinding('ipc_serdes');
const { streamBaseState, kLastWriteWasAsync } = internalBinding('stream_wrap');

const kMessageBuffer = Symbol('kMessageBuffer');
const kMessageBufferSize = Symbol('kMessageBufferSize');
const kJSONBuffer = Symbol('kJSONBuffer');
const kStringDecoder = Symbol('kStringDecoder');

// Messages are parsed in either of the following formats:
// - Newline-delimited JSON, or
// - V8-serialized buffers, prefixed with their length as a big endian uint32
//   (aka 'advanced')
const advanced = {
  initMessageChannel(channel) {
    channel[kMessageBuffer] = [];
    channel[kMessageBufferSize] = 0;
    channel.buffering = false;
  },

  *parseChannelMessages(channel, readData) {
    if (readData.length === 0) return;

    if (channel[kMessageBufferSize] && channel[kMessageBuffer][0].length < 4) {
      // Message length split into two buffers, so let's concatenate it.
      channel[kMessageBuffer][0] = Buffer.concat([channel[kMessageBuffer][0], readData]);
    } else {
      ArrayPrototypePush(channel[kMessageBuffer], readData);
    }
    channel[kMessageBufferSize] += readData.length;

    // Index 0 should always be present because we just pushed data into it.
    let messageBufferHead = channel[kMessageBuffer][0];
    while (messageBufferHead.length >= 4) {
      // We call `readUInt32BE` manually here, because this is faster than first converting
      // it to a buffer and using `readUInt32BE` on that.
      const fullMessageSize = ((
        messageBufferHead[0] << 24 |
        messageBufferHead[1] << 16 |
        messageBufferHead[2] << 8 |
        messageBufferHead[3]
      ) >>> 0) + 4;

      if (channel[kMessageBufferSize] < fullMessageSize) break;

      const concatenatedBuffer = channel[kMessageBuffer].length === 1 ?
        channel[kMessageBuffer][0] :
        Buffer.concat(
          channel[kMessageBuffer],
          channel[kMessageBufferSize],
        );

      const serializedMessage =
        TypedArrayPrototypeSubarray(concatenatedBuffer, 4, fullMessageSize);

      messageBufferHead = TypedArrayPrototypeSubarray(concatenatedBuffer, fullMessageSize);
      channel[kMessageBufferSize] = messageBufferHead.length;
      channel[kMessageBuffer] =
        channel[kMessageBufferSize] !== 0 ? [messageBufferHead] : [];

      yield deserialize(serializedMessage);
    }

    channel.buffering = channel[kMessageBufferSize] > 0;
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
