'use strict';

// The Web IDL and specification prose quoted below is reproduced verbatim
// from the specification, including its original (lowercase) casing.
// Refs: https://w3c.github.io/FileAPI/
/* eslint-disable capitalized-comments */

const {
  ArrayPrototypePush,
  DateNow,
  ObjectDefineProperties,
  ObjectDefineProperty,
  PromisePrototypeThen,
  Symbol,
  SymbolToStringTag,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeSet,
  TypedArrayPrototypeSubarray,
  Uint8Array,
} = primordials;

const { Buffer } = require('buffer');

const { setImmediate } = require('timers');

const {
  EventTarget,
  defineEventHandler,
  kTrustEvent,
} = require('internal/event_target');

const {
  createBlobReaderStream,
  getBlobDataSync,
  isBlob,
  kHandle,
  kType,
} = require('internal/blob');

const { TextDecoder, getEncodingFromLabel } = require('internal/encoding');

const { MIMEType } = require('internal/mime');

const { ProgressEvent } = require('internal/progress_event');

const {
  kEnumerableProperty,
  lazyDOMException,
} = require('internal/util');

const { isUint8Array } = require('internal/util/types');

const {
  validateThisInternalField,
} = require('internal/validators');

const {
  converters,
  createInterfaceConverter,
  requiredArguments,
} = require('internal/webidl');

const kError = Symbol('error');
const kGeneration = Symbol('generation');
const kReader = Symbol('reader');
const kResult = Symbol('result');
const kState = Symbol('state');
const kSync = Symbol('sync');

const kProgressInterval = 50;

const convertBlob = createInterfaceConverter('Blob', isBlob);

// "When this specification says to fire a progress event called e (for some
// ProgressEvent e at a given FileReader reader), the following are normative:
//
// * The progress event e does not bubble. e.bubbles must be false
// * The progress event e is NOT cancelable. e.cancelable must be false"
const kTrustedProgressEventInit = {
  __proto__: null,
  [kTrustEvent]: true,
};

function fireAProgressEvent(name, reader) {
  reader.dispatchEvent(new ProgressEvent(name, kTrustedProgressEventInit));
}

// "Use the file reading task source for all these tasks."
function queueFileReadingTask(fr, generation, steps) {
  setImmediate(() => {
    if (fr[kGeneration] !== generation) {
      return;
    }
    steps();
  });
}

function combineByteSequences(chunks, totalBytes) {
  const bytes = new Uint8Array(totalBytes);
  let offset = 0;
  for (let i = 0; i < chunks.length; i++) {
    const chunk = chunks[i];
    TypedArrayPrototypeSet(bytes, chunk, offset);
    offset += TypedArrayPrototypeGetByteLength(chunk);
  }
  return bytes;
}

// https://encoding.spec.whatwg.org/#bom-sniff
//
// "To BOM sniff an I/O queue of bytes ioQueue:"
function bomSniff(bytes) {
  // "Let BOM be the result of peeking 3 bytes from ioQueue, converted to a
  // byte sequence."
  const length = TypedArrayPrototypeGetByteLength(bytes);
  // "For each of the rows in the table below, starting with the first one
  // and going down, if BOM starts with the bytes given in the first column,
  // then return the encoding given in the cell in the second column of that
  // row. Otherwise, return null."
  //
  // 0xEF 0xBB 0xBF  UTF-8
  // 0xFE 0xFF       UTF-16BE
  // 0xFF 0xFE       UTF-16LE
  if (length >= 3 &&
      bytes[0] === 0xEF && bytes[1] === 0xBB && bytes[2] === 0xBF) {
    return 'utf-8';
  }
  if (length >= 2) {
    const a = bytes[0];
    const b = bytes[1];
    if (a === 0xFE && b === 0xFF) {
      return 'utf-16be';
    }
    if (a === 0xFF && b === 0xFE) {
      return 'utf-16le';
    }
  }
  return null;
}

// https://encoding.spec.whatwg.org/#decode
//
// "To decode an I/O queue of bytes ioQueue given a fallback encoding
// encoding:"
function decode(bytes, encoding) {
  // "Let BOMEncoding be the result of BOM sniffing ioQueue."
  const bomEncoding = bomSniff(bytes);
  let offset = 0;
  // "If BOMEncoding is non-null:
  //  1. Set encoding to BOMEncoding.
  //  2. Read three bytes from ioQueue, if BOMEncoding is UTF-8; otherwise
  //     read two bytes. (Do nothing with those bytes.)"
  if (bomEncoding !== null) {
    encoding = bomEncoding;
    offset = bomEncoding === 'utf-8' ? 3 : 2;
  }
  const view = offset === 0 ? bytes :
    TypedArrayPrototypeSubarray(bytes, offset);
  // "replacement's decoder's handler, given unused and byte, runs these
  // steps:
  //  1. If byte is end-of-queue, then return finished.
  //  2. If replacement error returned is false, then set replacement error
  //     returned to true and return error.
  //  3. Return finished."
  // Error mode "replacement" turns that error into U+FFFD.
  if (encoding === 'replacement') {
    return TypedArrayPrototypeGetByteLength(view) === 0 ? '' : '\uFFFD';
  }
  // "Process a queue with an instance of encoding's decoder, ioQueue,
  // output, and "replacement"."
  // "Return output."
  return new TextDecoder(encoding, { ignoreBOM: true }).decode(view);
}

// https://w3c.github.io/FileAPI/#packaging-data
//
// "A Blob has an associated package data algorithm, given bytes, a type,
// a mimeType, and an optional encodingLabel, which switches on type and
// runs the associated steps:"
function packageData(bytes, type, mimeType, encodingLabel) {
  switch (type) {
    case 'DataURL': {
      // "Return bytes as a DataURL subject to the considerations below:
      //  * Use mimeType as part of the Data URL if it is not the empty
      //    string in keeping with the Data URL specification.
      //  * Otherwise, return a Data URL without a media-type."
      const encoded = Buffer.from(
        bytes.buffer,
        bytes.byteOffset,
        bytes.byteLength,
      ).toString('base64');
      if (mimeType === '') {
        return `data:;base64,${encoded}`;
      }
      return `data:${mimeType};base64,${encoded}`;
    }
    case 'Text': {
      // "1. Let encoding be failure."
      let encoding;
      // "2. If the encodingLabel is present, set encoding to the result of
      // getting an encoding from encodingLabel."
      if (encodingLabel !== undefined) {
        encoding = getEncodingFromLabel(encodingLabel);
      }
      // "3. If encoding is failure:"
      if (encoding === undefined) {
        // "1. Let type be the result of parse a MIME type given mimeType."
        const parsed = mimeType === '' ? null : MIMEType.parse(mimeType);
        // "2. If type is not failure, set encoding to the result of getting
        // an encoding from type's parameters["charset"]."
        if (parsed !== null) {
          const charset = parsed.params.get('charset');
          if (charset !== null) {
            encoding = getEncodingFromLabel(charset);
          }
        }
      }
      // "4. If encoding is failure, then set encoding to UTF-8."
      if (encoding === undefined) {
        encoding = 'utf-8';
      }
      // "5. Decode bytes using fallback encoding encoding, and return the
      // result."
      return decode(bytes, encoding);
    }
    case 'ArrayBuffer':
      // "Return a new ArrayBuffer whose contents are bytes."
      return combineByteSequences([bytes], bytes.byteLength).buffer;
    case 'BinaryString':
      // "Return bytes as a binary string, in which every byte is
      // represented by a code unit of equal value [0..255]."
      return Buffer.from(
        bytes.buffer,
        bytes.byteOffset,
        bytes.byteLength,
      ).toString('latin1');
  }
}

function settleWithError(fr, generation, error) {
  // "Otherwise, if chunkPromise is rejected with an error error, queue a
  // task to run the following steps and abort this algorithm:"
  queueFileReadingTask(fr, generation, () => {
    // "1. Set fr's state to "done"."
    fr[kState] = 'done';
    // "2. Set fr's error to error."
    fr[kError] = error;
    // "3. Fire a progress event called error at fr."
    fireAProgressEvent('error', fr);
    queueLoadEnd(fr, generation);
  });
}

function queueLoadEnd(fr, generation) {
  // "If fr's state is not "loading", fire a progress event called loadend
  // at the fr."
  //
  // "Event handler for the load or error events could have started another
  // load, if that happens the loadend event for this load is not fired."
  queueFileReadingTask(fr, generation, () => {
    if (fr[kState] !== 'loading') {
      fireAProgressEvent('loadend', fr);
    }
  });
}

// "A FileReader fr has an associated read operation algorithm, which given
// blob, a type and an optional encodingLabel, runs the following steps:"
function readOperation(fr, blob, type, encodingLabel) {
  // "1. If fr's state is "loading", throw an InvalidStateError DOMException."
  if (fr[kState] === 'loading') {
    throw lazyDOMException(
      'The object is in an invalid state.',
      'InvalidStateError');
  }
  const previousState = fr[kState];
  const previousResult = fr[kResult];
  const previousError = fr[kError];
  // "2. Set fr's state to "loading"."
  fr[kState] = 'loading';
  // "3. Set fr's result to null."
  fr[kResult] = null;
  // "4. Set fr's error to null."
  fr[kError] = null;

  const generation = ++fr[kGeneration];
  // "5. Let stream be the result of calling get stream on blob."
  let stream;
  try {
    stream = createBlobReaderStream(blob[kHandle].getReader());
  } catch (error) {
    fr[kState] = previousState;
    fr[kResult] = previousResult;
    fr[kError] = previousError;
    throw error;
  }
  // "6. Let reader be the result of getting a reader from stream."
  const reader = stream.getReader();
  fr[kReader] = reader;
  // "7. Let bytes be an empty byte sequence."
  const chunks = [];
  let totalBytes = 0;
  // "8. Let chunkPromise be the result of reading a chunk from stream with
  // reader."
  let chunkPromise = reader.read();
  // "9. Let isFirstChunk be true."
  let isFirstChunk = true;
  let lastStepsAt;

  // "10. In parallel, while true:"
  async function readInParallel() {
    while (true) {
      if (fr[kGeneration] !== generation) {
        return;
      }

      // "1. Wait for chunkPromise to be fulfilled or rejected."
      let chunk;
      try {
        chunk = await chunkPromise;
      } catch (error) {
        if (fr[kGeneration] !== generation) {
          return;
        }
        settleWithError(fr, generation, error);
        return;
      }

      if (fr[kGeneration] !== generation) {
        return;
      }

      // "2. If chunkPromise is fulfilled, and isFirstChunk is true, queue a
      // task to fire a progress event called loadstart at fr."
      if (isFirstChunk) {
        queueFileReadingTask(fr, generation, () => {
          fireAProgressEvent('loadstart', fr);
        });
      }
      // "3. Set isFirstChunk to false."
      isFirstChunk = false;

      // "4. If chunkPromise is fulfilled with an object whose done property
      // is false and whose value property is a Uint8Array object, run these
      // steps:"
      if (chunk.done !== true && isUint8Array(chunk.value)) {
        // "1. Let bs be the byte sequence represented by the Uint8Array
        // object."
        const bs = chunk.value;
        // "2. Append bs to bytes."
        ArrayPrototypePush(chunks, bs);
        totalBytes += bs.byteLength;
        // "3. If roughly 50ms have passed since these steps were last
        // invoked, queue a task to fire a progress event called progress
        // at fr."
        const now = DateNow();
        if (lastStepsAt !== undefined && now - lastStepsAt >= kProgressInterval) {
          queueFileReadingTask(fr, generation, () => {
            fireAProgressEvent('progress', fr);
          });
        }
        lastStepsAt = now;
        // "4. Set chunkPromise to the result of reading a chunk from stream
        // with reader."
        chunkPromise = reader.read();
        continue;
      }

      // "5. Otherwise, if chunkPromise is fulfilled with an object whose
      // done property is true, queue a task to run the following steps and
      // abort this algorithm:"
      if (chunk.done === true) {
        const bytes = combineByteSequences(chunks, totalBytes);
        const mimeType = blob[kType];
        // "One progress event will fire when blob has been completely read
        // into memory."
        if (totalBytes > 0) {
          queueFileReadingTask(fr, generation, () => {
            fireAProgressEvent('progress', fr);
          });
        }
        queueFileReadingTask(fr, generation, () => {
          // "1. Set fr's state to "done"."
          fr[kState] = 'done';
          fr[kReader] = undefined;
          try {
            // "2. Let result be the result of package data given bytes,
            // type, blob's type, and encodingLabel."
            const result = packageData(bytes, type, mimeType, encodingLabel);
            // "4. Else:
            //  1. Set fr's result to result.
            //  2. Fire a progress event called load at the fr."
            fr[kResult] = result;
            fireAProgressEvent('load', fr);
          } catch (error) {
            // "3. If package data threw an exception error:
            //  1. Set fr's error to error.
            //  2. Fire a progress event called error at fr."
            fr[kError] = error;
            fireAProgressEvent('error', fr);
          }
          queueLoadEnd(fr, generation);
        });
        return;
      }

      return;
    }
  }

  readInParallel().catch((error) => {
    if (fr[kGeneration] !== generation) {
      return;
    }
    settleWithError(fr, generation, error);
  });
}

function defineReadStateConstants(target) {
  // const unsigned short EMPTY = 0;
  ObjectDefineProperty(target, 'EMPTY', {
    __proto__: null,
    writable: false,
    enumerable: true,
    configurable: false,
    value: 0,
  });
  // const unsigned short LOADING = 1;
  ObjectDefineProperty(target, 'LOADING', {
    __proto__: null,
    writable: false,
    enumerable: true,
    configurable: false,
    value: 1,
  });
  // const unsigned short DONE = 2;
  ObjectDefineProperty(target, 'DONE', {
    __proto__: null,
    writable: false,
    enumerable: true,
    configurable: false,
    value: 2,
  });
}

// https://w3c.github.io/FileAPI/#APIASynch
//
// [Exposed=(Window,Worker)]
// interface FileReader: EventTarget {
class FileReader extends EventTarget {
  // constructor();
  constructor() {
    super();
    // "The FileReader() constructor, when invoked, must return a new
    // FileReader object."
    //
    // "A FileReader has an associated state, that is "empty", "loading",
    // or "done". It is initially "empty"."
    this[kState] = 'empty';
    // "A FileReader has an associated result (null, a DOMString or an
    // ArrayBuffer). It is initially null."
    this[kResult] = null;
    // "A FileReader has an associated error (null or a DOMException).
    // It is initially null."
    this[kError] = null;
    this[kGeneration] = 0;
  }

  // undefined readAsArrayBuffer(Blob blob);
  readAsArrayBuffer(blob) {
    const prefix = "Failed to execute 'readAsArrayBuffer' on 'FileReader'";
    validateThisInternalField(this, kState, 'FileReader');
    requiredArguments(arguments.length, 1, { prefix });
    blob = convertBlob(blob, { prefix, context: 'Argument 1' });
    // "The readAsArrayBuffer(blob) method, when invoked, must initiate a
    // read operation for blob with ArrayBuffer."
    readOperation(this, blob, 'ArrayBuffer');
  }

  // undefined readAsBinaryString(Blob blob);
  readAsBinaryString(blob) {
    const prefix = "Failed to execute 'readAsBinaryString' on 'FileReader'";
    validateThisInternalField(this, kState, 'FileReader');
    requiredArguments(arguments.length, 1, { prefix });
    blob = convertBlob(blob, { prefix, context: 'Argument 1' });
    // "The readAsBinaryString(blob) method, when invoked, must initiate a
    // read operation for blob with BinaryString."
    readOperation(this, blob, 'BinaryString');
  }

  // undefined readAsText(Blob blob, optional DOMString encoding);
  readAsText(blob, encoding = undefined) {
    const prefix = "Failed to execute 'readAsText' on 'FileReader'";
    validateThisInternalField(this, kState, 'FileReader');
    requiredArguments(arguments.length, 1, { prefix });
    blob = convertBlob(blob, { prefix, context: 'Argument 1' });
    let encodingLabel;
    if (arguments.length > 1) {
      encodingLabel = converters.DOMString(encoding, {
        prefix,
        context: 'Argument 2',
      });
    }
    // "The readAsText(blob, encoding) method, when invoked, must initiate a
    // read operation for blob with Text and encoding."
    readOperation(this, blob, 'Text', encodingLabel);
  }

  // undefined readAsDataURL(Blob blob);
  readAsDataURL(blob) {
    const prefix = "Failed to execute 'readAsDataURL' on 'FileReader'";
    validateThisInternalField(this, kState, 'FileReader');
    requiredArguments(arguments.length, 1, { prefix });
    blob = convertBlob(blob, { prefix, context: 'Argument 1' });
    // "The readAsDataURL(blob) method, when invoked, must initiate a read
    // operation for blob with DataURL."
    readOperation(this, blob, 'DataURL');
  }

  // undefined abort();
  abort() {
    validateThisInternalField(this, kState, 'FileReader');
    // "When the abort() method is called, the user agent must run the steps
    // below:"
    // "1. If this's state is "empty" or if this's state is "done" set
    // this's result to null and terminate this algorithm."
    if (this[kState] === 'empty' || this[kState] === 'done') {
      this[kResult] = null;
      return;
    }
    // "2. If this's state is "loading" set this's state to "done" and set
    // this's result to null."
    if (this[kState] === 'loading') {
      this[kState] = 'done';
      this[kResult] = null;
    }
    // "3. If there are any tasks from this on the file reading task source
    // in an affiliated task queue, then remove those tasks from that task
    // queue."
    // "4. Terminate the algorithm for the read method being processed."
    this[kGeneration]++;
    const reader = this[kReader];
    this[kReader] = undefined;
    if (reader !== undefined) {
      PromisePrototypeThen(reader.cancel(), undefined, () => {});
    }
    // "5. Fire a progress event called abort at this."
    fireAProgressEvent('abort', this);
    // "6. If this's state is not "loading", fire a progress event called
    // loadend at this."
    if (this[kState] !== 'loading') {
      fireAProgressEvent('loadend', this);
    }
  }

  // readonly attribute unsigned short readyState;
  get readyState() {
    validateThisInternalField(this, kState, 'FileReader');
    // "The readyState attribute's getter, when invoked, switches on this's
    // state and runs the associated step:"
    switch (this[kState]) {
      case 'empty':
        // `"empty"`
        // Return EMPTY
        return 0;
      case 'loading':
        // `"loading"`
        // Return LOADING
        return 1;
      case 'done':
        // `"done"`
        // Return DONE
        return 2;
      default:
        return 0;
    }
  }

  // readonly attribute (DOMString or ArrayBuffer)? result;
  get result() {
    validateThisInternalField(this, kState, 'FileReader');
    // "The result attribute's getter, when invoked, must return this's
    // result."
    return this[kResult];
  }

  // readonly attribute DOMException? error;
  get error() {
    validateThisInternalField(this, kState, 'FileReader');
    // "The error attribute's getter, when invoked, must return this's
    // error."
    return this[kError];
  }
}
// };

defineReadStateConstants(FileReader);
defineReadStateConstants(FileReader.prototype);

ObjectDefineProperties(FileReader.prototype, {
  readAsArrayBuffer: kEnumerableProperty,
  readAsBinaryString: kEnumerableProperty,
  readAsText: kEnumerableProperty,
  readAsDataURL: kEnumerableProperty,
  abort: kEnumerableProperty,
  readyState: kEnumerableProperty,
  result: kEnumerableProperty,
  error: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'FileReader',
  },
});

// "The following are the event handler content attributes (and their
// corresponding event handler event types) that user agents must support
// on FileReader as DOM attributes:"
// onloadstart / loadstart
defineEventHandler(FileReader.prototype, 'loadstart');
// onprogress / progress
defineEventHandler(FileReader.prototype, 'progress');
// onload / load
defineEventHandler(FileReader.prototype, 'load');
// onabort / abort
defineEventHandler(FileReader.prototype, 'abort');
// onerror / error
defineEventHandler(FileReader.prototype, 'error');
// onloadend / loadend
defineEventHandler(FileReader.prototype, 'loadend');

function readSync(blob, type, encodingLabel) {
  // "Let stream be the result of calling get stream on blob."
  // "Let reader be the result of getting a reader from stream."
  // "Let promise be the result of reading all bytes from stream with
  // reader."
  // "Wait for promise to be fulfilled or rejected."
  const buffer = getBlobDataSync(blob);
  // "If promise fulfilled with a byte sequence bytes:"
  if (buffer !== undefined) {
    return packageData(new Uint8Array(buffer), type, blob[kType], encodingLabel);
  }
  // "Throw promise's rejection reason."
  throw lazyDOMException(
    'The blob could not be read',
    'NotReadableError');
}

// https://w3c.github.io/FileAPI/#FileReaderSync
//
// [Exposed=(DedicatedWorker,SharedWorker)]
// interface FileReaderSync {
class FileReaderSync {
  // constructor();
  constructor() {
    // "When the FileReaderSync() constructor is invoked, the user agent
    // must return a new FileReaderSync object."
    this[kSync] = true;
  }

  // ArrayBuffer readAsArrayBuffer(Blob blob);
  readAsArrayBuffer(blob) {
    const prefix = "Failed to execute 'readAsArrayBuffer' on 'FileReaderSync'";
    validateThisInternalField(this, kSync, 'FileReaderSync');
    requiredArguments(arguments.length, 1, { prefix });
    blob = convertBlob(blob, { prefix, context: 'Argument 1' });
    // "The readAsArrayBuffer(blob) method, when invoked, must run these
    // steps:"
    return readSync(blob, 'ArrayBuffer');
  }

  // DOMString readAsBinaryString(Blob blob);
  readAsBinaryString(blob) {
    const prefix = "Failed to execute 'readAsBinaryString' on 'FileReaderSync'";
    validateThisInternalField(this, kSync, 'FileReaderSync');
    requiredArguments(arguments.length, 1, { prefix });
    blob = convertBlob(blob, { prefix, context: 'Argument 1' });
    // "The readAsBinaryString(blob) method, when invoked, must run these
    // steps:"
    return readSync(blob, 'BinaryString');
  }

  // DOMString readAsText(Blob blob, optional DOMString encoding);
  readAsText(blob, encoding = undefined) {
    const prefix = "Failed to execute 'readAsText' on 'FileReaderSync'";
    validateThisInternalField(this, kSync, 'FileReaderSync');
    requiredArguments(arguments.length, 1, { prefix });
    blob = convertBlob(blob, { prefix, context: 'Argument 1' });
    let encodingLabel;
    if (arguments.length > 1) {
      encodingLabel = converters.DOMString(encoding, {
        prefix,
        context: 'Argument 2',
      });
    }
    // "The readAsText(blob, encoding) method, when invoked, must run these
    // steps:"
    return readSync(blob, 'Text', encodingLabel);
  }

  // DOMString readAsDataURL(Blob blob);
  readAsDataURL(blob) {
    const prefix = "Failed to execute 'readAsDataURL' on 'FileReaderSync'";
    validateThisInternalField(this, kSync, 'FileReaderSync');
    requiredArguments(arguments.length, 1, { prefix });
    blob = convertBlob(blob, { prefix, context: 'Argument 1' });
    // "The readAsDataURL(blob) method, when invoked, must run these steps:"
    return readSync(blob, 'DataURL');
  }
}
// };

ObjectDefineProperties(FileReaderSync.prototype, {
  readAsArrayBuffer: kEnumerableProperty,
  readAsBinaryString: kEnumerableProperty,
  readAsText: kEnumerableProperty,
  readAsDataURL: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'FileReaderSync',
  },
});

module.exports = {
  FileReader,
  FileReaderSync,
};
