'use strict';

module.exports = require('stream').PassThrough;

process.emitWarning('The _stream_passthrough module is deprecated. Use `node:stream` instead.',
                    'DeprecationWarning', 'DEP0193');
