'use strict';

module.exports = require('stream').Readable;

process.emitWarning('The _stream_readable module is deprecated. Use `node:stream` instead.',
                    'DeprecationWarning', 'DEP0193');
