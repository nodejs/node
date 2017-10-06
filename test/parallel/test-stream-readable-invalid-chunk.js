'use strict';

require('../common');
const stream = require('stream');
const assert = require('assert');

const readable = new stream.Readable({
  read: () => {}
});

const errMessage = /Invalid non-string\/buffer chunk/;
assert.throws(() => readable.push([]), errMessage);
assert.throws(() => readable.push({}), errMessage);
assert.throws(() => readable.push(0), errMessage);
