'use strict';

require('../common');
const assert = require('node:assert');

// Without --experimental-fileapi, the File API globals are not exposed.
assert.strictEqual(typeof FileReader, 'undefined');
assert.strictEqual(typeof FileReaderSync, 'undefined');
assert.strictEqual(typeof ProgressEvent, 'undefined');
