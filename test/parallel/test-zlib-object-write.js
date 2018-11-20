'use strict';

require('../common');
const assert = require('assert');
const { Gunzip } = require('zlib');

const gunzip = new Gunzip({ objectMode: true });
assert.throws(
  () => gunzip.write({}),
  TypeError
);
