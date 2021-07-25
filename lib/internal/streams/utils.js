'use strict';

const {
  Symbol,
  SymbolAsyncIterator,
  SymbolIterator,
} = primordials;

const kIsDisturbed = Symbol('kIsDisturbed');

function isReadable(obj) {
  return !!(obj && typeof obj.pipe === 'function' &&
            typeof obj.on === 'function');
}

function isWritable(obj) {
  return !!(obj && typeof obj.write === 'function' &&
            typeof obj.on === 'function');
}

function isStream(obj) {
  return isReadable(obj) || isWritable(obj);
}

function isDuplexNodeStream(obj) {
  return !!(
    obj &&
    (typeof obj.pipe === 'function' && obj._readableState) &&
    typeof obj.on === 'function' &&
    typeof obj.write === 'function'
  );
}

function isReadableNodeStream(obj) {
  return !!(
    obj &&
    typeof obj.pipe === 'function' &&
    typeof obj.on === 'function' &&
    (!obj._writableState || obj._readableState?.readable !== false) && // Duplex
    (!obj._writableState || obj._readableState) // Writable has .pipe.
  );
}

function isWritableNodeStream(obj) {
  return !!(
    obj &&
    typeof obj.write === 'function' &&
    typeof obj.on === 'function' &&
    (!obj._readableState || obj._writableState?.writable !== false) // Duplex
  );
}

function isNodeStream(obj) {
  return (
    obj &&
    (
      obj._readableState ||
      obj._writableState ||
      (typeof obj.write === 'function' && typeof obj.on === 'function') ||
      (typeof obj.pipe === 'function' && typeof obj.on === 'function')
    )
  );
}

function isDestroyed(stream) {
  if (!isNodeStream(stream)) return null;
  const wState = stream._writableState;
  const rState = stream._readableState;
  const state = wState || rState;
  return !!(stream.destroyed || state?.destroyed);
}

function isIterable(obj, isAsync) {
  if (obj == null) return false;
  if (isAsync === true) return typeof obj[SymbolAsyncIterator] === 'function';
  if (isAsync === false) return typeof obj[SymbolIterator] === 'function';
  return typeof obj[SymbolAsyncIterator] === 'function' ||
    typeof obj[SymbolIterator] === 'function';
}

function isDisturbed(stream) {
  return !!(stream && (
    stream.readableDidRead ||
    stream.readableAborted ||
    stream[kIsDisturbed]
  ));
}

module.exports = {
  isDisturbed,
  kIsDisturbed,
  isDestroyed,
  isReadableNodeStream,
  isWritableNodeStream,
  isDuplexNodeStream,
  isNodeStream,
  isIterable,
  isReadable,
  isStream,
};
