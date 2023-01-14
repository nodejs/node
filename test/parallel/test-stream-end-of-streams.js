'use strict';
require('../common');
const assert = require('assert');

const { Duplex, finished } = require('stream');

assert.throws(
  () => {
    // Passing empty object to mock invalid stream
    // should throw error
    finished({}, () => {});
  },
  { code: 'ERR_INVALID_ARG_TYPE' }
);

const streamObj = new Duplex();
streamObj.end();
// Below code should not throw any errors as the
// streamObj is `Stream`
finished(streamObj, () => {});
