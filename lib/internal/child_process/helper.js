'use strict';

const {
  JSONParse,
  JSONStringify,
  StringPrototypeSplit,
  ArrayPrototypePush,
  Symbol,
  TypedArrayPrototypeSubarray,
} = primordials;
const { Buffer } = require('buffer');
const { StringDecoder } = require('string_decoder');
const v8 = require('v8');
const { isArrayBufferView } = require('internal/util/types');
const assert = require('internal/assert');
const { streamBaseState, kLastWriteWasAsync } = internalBinding('stream_wrap');
const { isUint8Array } = require('internal/util');

const kMessageBuffer = Symbol('kMessageBuffer');
const kMessageBufferSize = Symbol('kMessageBufferSize');
const kJSONBuffer = Symbol('kJSONBuffer');
const kStringDecoder = Symbol('kStringDecoder');

const createChildProcessSerializer = () => new ChildProcessSerializer();

// Extend V8's serializer APIs to give more JSON-like behaviour in
// some cases; in particular, for native objects this serializes them the same
// way that JSON does rather than throwing an exception.
const kArrayBufferViewTag = 0;
const kNotArrayBufferViewTag = 1;
class ChildProcessSerializer extends v8.DefaultSerializer {
  _writeHostObject(object) {
    if (isArrayBufferView(object) || isUint8Array(object)) {
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

module.exports = { createChildProcessSerializer, ChildProcessDeserializer };
