'use strict';
require('../common');
const stream = require('stream');
const assert = require('assert');

const readable = new stream.Readable();

assert.throws(() => readable.read(), /not implemented/);
