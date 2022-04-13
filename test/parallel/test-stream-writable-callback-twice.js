'use strict';
const common = require('../common');
const { Writable } = require('stream');
const stream = new Writable({
  write(chunk, enc, cb) { cb(); cb(); }
});

stream.on('error', common.expectsError({
  name: 'Error',
  message: 'Callback called multiple times',
  code: 'ERR_MULTIPLE_CALLBACK'
}));

stream.write('foo');
