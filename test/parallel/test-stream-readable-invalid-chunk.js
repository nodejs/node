'use strict';

const common = require('../common');
const stream = require('stream');
const assert = require('assert');

const readable = new stream.Readable({
  read: common.noop
});

assert.throws(() => readable.push([]), /Invalid non-string\/buffer chunk/);
assert.throws(() => readable.push({}), /Invalid non-string\/buffer chunk/);
assert.throws(() => readable.push(0), /Invalid non-string\/buffer chunk/);
