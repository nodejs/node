// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const {
  getSystemErrorMap,
  _errnoException
} = require('util');

const { internalBinding } = require('internal/test/binding');
const uv = internalBinding('uv');
const uvKeys = Object.keys(uv);

const errMap = getSystemErrorMap();

uvKeys.forEach((key) => {
  if (!key.startsWith('UV_'))
    return;

  const err = _errnoException(uv[key]);
  const name = uv.errname(uv[key]);
  assert.strictEqual(errMap.get(err.errno)[0], name);
});
