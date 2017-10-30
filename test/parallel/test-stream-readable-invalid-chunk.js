'use strict';

const common = require('../common');
const stream = require('stream');

const readable = new stream.Readable({
  read: () => {}
});

function checkError(fn) {
  common.expectsError(fn, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });
}

checkError(() => readable.push([]));
checkError(() => readable.push({}));
checkError(() => readable.push(0));
