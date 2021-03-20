'use strict';
const {
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ObjectFromEntries,
  ObjectEntries,
  SafeArrayIterator,
} = primordials;
const { types } = require('util');

module.exports = {
  util() {
    return ObjectFromEntries(new SafeArrayIterator(ArrayPrototypeFilter(
      ObjectEntries(types),
      ({ 0: key }) => {
        return ArrayPrototypeIncludes([
          'isArrayBuffer',
          'isArrayBufferView',
          'isAsyncFunction',
          'isDataView',
          'isDate',
          'isExternal',
          'isMap',
          'isMapIterator',
          'isNativeError',
          'isPromise',
          'isRegExp',
          'isSet',
          'isSetIterator',
          'isTypedArray',
          'isUint8Array',
          'isAnyArrayBuffer',
        ], key);
      })));
  }
};
