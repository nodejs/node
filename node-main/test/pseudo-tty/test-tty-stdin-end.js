'use strict';
require('../common');

// This test ensures that Node.js doesn't crash on `process.stdin.emit("end")`.
// https://github.com/nodejs/node/issues/1068

process.stdin.emit('end');
