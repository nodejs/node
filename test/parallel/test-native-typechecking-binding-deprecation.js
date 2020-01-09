'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

[
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
  'isAnyArrayBuffer'
].forEach((method) => {
  const { stderr } = spawnSync(process.execPath, ['-p', `
    process.binding('util')['${method}']('meow');
  `]);
  const expectedWarning = '[DEP0103] DeprecationWarning: ' +
                          'Accessing native typechecking bindings ' +
                          'of Node directly is deprecated. ' +
                          `Please use \`util.types.${method}\` instead.`;
  assert(stderr.includes(expectedWarning), stderr);
});
