'use strict';

const {
  JSONParse,
  JSONStringify,
  StringPrototypeSplit,
  Symbol,
  TypedArrayPrototypeSubarray,
} = primordials;
const { Buffer } = require('buffer');
const { StringDecoder } = require('string_decoder');
const v8 = require('v8');
const { isArrayBufferView } = require('internal/util/types');
const assert = require('internal/assert');
const { streamBaseState, kLastWriteWasAsync } = internalBinding('stream_wrap');

const kMessageBuffer = Symbol('kMessageBuffer');
const kJSONBuffer = Symbol('kJSONBuffer');
const kStringDecoder = Symbol('kStringDecoder');

// Extend V8's serializer APIs to give more JSON-like behaviour in
// some cases; in particular, for native objects this serializes them the same
// way that JSON does rather than throwing an exception.
const kArrayBufferViewTag = 0;
const kNotArrayBufferViewTag = 1;
class ChildProcessSerializer extends v8.DefaultSerializer {
  _writeHostObject(object) {
    if (isArrayBufferView(object)) {
      this.writeUint32(kArrayBufferViewTag);
      return super._writeHostObject(object);
    }
    this.writeUint32(kNotArrayBufferViewTag);
    this.writeValue({ ...object });
  }
}

class ChildProcessDeserializer extends v8.DefaultDeserializer {
  _readHostObject() {
    const tag = this.readUint32();
    if (tag === kArrayBufferViewTag)
      return super._readHostObject();

    assert(tag === kNotArrayBufferViewTag);
    return this.readValue();
  }
}

// Messages are parsed in either of the following formats:
// - Newline-delimited JSON, or
// - V8-serialized buffers, prefixed with their length as a big endian uint32
//   (aka 'advanced')
const advanced = {
  initMessageChannel(channel) {
    channel[kMessageBuffer] = Buffer.alloc(0);
    channel.buffering = false;
  },

  *parseChannelMessages(channel, readData) {
    if (readData.length === 0) return;

    let messageBuffer = Buffer.concat([channel[kMessageBuffer], readData]);
    while (messageBuffer.length > 4) {
      const size = messageBuffer.readUInt32BE();
      if (messageBuffer.length < 4 + size) {
        break;
      }

      const deserializer = new ChildProcessDeserializer(
        TypedArrayPrototypeSubarray(messageBuffer, 4, 4 + size));
      messageBuffer = TypedArrayPrototypeSubarray(messageBuffer, 4 + size);

      deserializer.readHeader();
      yield deserializer.readValue();
    }
    channel[kMessageBuffer] = messageBuffer;
    channel.buffering = messageBuffer.length > 0;
  },

  writeChannelMessage(channel, req, message, handle) {
    const ser = new ChildProcessSerializer();
    ser.writeHeader();
    ser.writeValue(message);
    const serializedMessage = ser.releaseBuffer();
    const sizeBuffer = Buffer.allocUnsafe(4);
    sizeBuffer.writeUInt32BE(serializedMessage.length);

    const buffer = Buffer.concat([
      sizeBuffer,
      serializedMessage,
    ]);
    const result = channel.writeBuffer(req, buffer, handle);
    // Mirror what stream_base_commons.js does for Buffer retention.
    if (streamBaseState[kLastWriteWasAsync])
      req.buffer = buffer;
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
