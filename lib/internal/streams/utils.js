'use strict';

const {
  SymbolAsyncIterator,
  SymbolIterator,
} = primordials;

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

function isIterable(obj, isAsync) {
  if (obj == null) return false;
  if (isAsync === true) return typeof obj[SymbolAsyncIterator] === 'function';
  if (isAsync === false) return typeof obj[SymbolIterator] === 'function';
  return typeof obj[SymbolAsyncIterator] === 'function' ||
    typeof obj[SymbolIterator] === 'function';
}

module.exports = {
  isIterable,
  isReadable,
  isStream,
};
