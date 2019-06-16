'use strict';

const { cjsContext } = require('internal/bootstrap/loaders');
const { getStackSourceName } = internalBinding('contextify');
const { deprecate } = require('internal/util');
const { Object } = primordials;

module.exports = () => {
  // Deprecate global.process and global.Buffer access in ES Modules.
  const processGetter = deprecate(
    () => cjsContext.process,
    'global.process is deprecated in ECMAScript Modules. ' +
        'Please use `import process from \'process\'` instead.', 'DEP0XXX');
  Object.defineProperty(global, 'process', {
    get() {
      const sourceName = getStackSourceName(1);
      if (sourceName && sourceName.startsWith('file://'))
        return processGetter();
      return cjsContext.process;
    },
    set(value) {
      cjsContext.process = value;
    },
    enumerable: false,
    configurable: true
  });

  const bufferGetter = deprecate(
    () => cjsContext.Buffer,
    'global.Buffer is deprecated in ECMAScript Modules. ' +
        'Please use `import { Buffer } from \'buffer\'` instead.', 'DEP0YYY');
  Object.defineProperty(global, 'Buffer', {
    get() {
      const sourceName = getStackSourceName(1);
      if (sourceName && sourceName.startsWith('file://'))
        return bufferGetter();
      return cjsContext.Buffer;
    },
    set(value) {
      cjsContext.Buffer = value;
    },
    enumerable: false,
    configurable: true
  });
};
