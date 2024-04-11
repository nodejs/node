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
        return (
            key === 'isArrayBuffer' ||
            key === 'isArrayBufferView' ||
            key === 'isAsyncFunction' ||
            key === 'isDataView' ||
            key === 'isDate' ||
            key === 'isExternal' ||
            key === 'isMap' ||
            key === 'isMapIterator' ||
            key === 'isNativeError' ||
            key === 'isPromise' ||
            key === 'isRegExp' ||
            key === 'isSet' ||
            key === 'isSetIterator' ||
            key === 'isTypedArray' ||
            key === 'isUint8Array' ||
            key === 'isAnyArrayBuffer'
        )
      })));
  },
  natives() {
    const { natives: result, configs } = internalBinding('builtins');
    // Legacy feature: process.binding('natives').config contains stringified
    // config.gypi. We do not use this object internally so it's fine to mutate
    // it.
    result.configs = configs;
    return result;
  },
};
