'use strict';
const {
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypeMap,
  ObjectFromEntries,
  ObjectEntries,
  SafeArrayIterator,
} = primordials;
const types = require('internal/util/types');

const factories = {
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

module.exports = ObjectFromEntries(
  new SafeArrayIterator(
    ArrayPrototypeMap(ObjectEntries(factories), ({ 0: key, 1: factory }) => {
      let result;
      return [key, () => {
        if (!result) {
          result = factory();
        }
        return result;
      }];
    }),
  ),
);
