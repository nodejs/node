/* eslint-disable require-buffer */
'use strict';

if (process.useOldBuffer) {
  module.exports = require('internal/buffer_old');
} else {
  module.exports = require('internal/buffer_new');
}
