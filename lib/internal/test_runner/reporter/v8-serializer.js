'use strict';

const {
  TypedArrayPrototypeGetLength,
} = primordials;
const { DefaultSerializer } = require('v8');
const { Buffer } = require('buffer');
const { serializeError } = require('internal/error_serdes');
const {
  collectAssertionPrototypeMetadata,
  kAssertionPrototypeMetadata,
} = require('internal/test_runner/assertion_error_prototype');


module.exports = async function* v8Reporter(source) {
  const serializer = new DefaultSerializer();
  serializer.writeHeader();
  const headerLength = TypedArrayPrototypeGetLength(serializer.releaseBuffer());

  for await (const item of source) {
    const originalError = item.data.details?.error;
    let assertionPrototypeMetadata;
    if (originalError) {
      assertionPrototypeMetadata = collectAssertionPrototypeMetadata(originalError);
      if (assertionPrototypeMetadata !== undefined) {
        // test_runner-only metadata used by the parent process to restore
        // AssertionError actual/expected constructor names.
        item.data.details[kAssertionPrototypeMetadata] = assertionPrototypeMetadata;
      }

      // Error is overridden with a serialized version, so that it can be
      // deserialized in the parent process.
      // Error is restored after serialization.
      item.data.details.error = serializeError(originalError);
    }
    serializer.writeHeader();
    // Add 4 bytes, to later populate with message length
    serializer.writeRawBytes(Buffer.allocUnsafe(4));
    serializer.writeHeader();
    serializer.writeValue(item);

    if (originalError) {
      item.data.details.error = originalError;
      if (assertionPrototypeMetadata !== undefined) {
        delete item.data.details[kAssertionPrototypeMetadata];
      }
    }

    const serializedMessage = serializer.releaseBuffer();
    const serializedMessageLength = serializedMessage.length - (4 + headerLength);

    serializedMessage.set([
      serializedMessageLength >> 24 & 0xFF,
      serializedMessageLength >> 16 & 0xFF,
      serializedMessageLength >> 8 & 0xFF,
      serializedMessageLength & 0xFF,
    ], headerLength);
    yield serializedMessage;
  }
};
