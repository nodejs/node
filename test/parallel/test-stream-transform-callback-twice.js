'use strict';
const common = require('../common');
const { Transform } = require('stream');
const stream = new Transform({
  transform(chunk, enc, cb) { cb(); cb(); }
});

stream.on('error', common.expectsError({
  name: 'Error',
  message: 'Callback called multiple times',
  code: 'ERR_MULTIPLE_CALLBACK'
}));

stream.write('foo');
