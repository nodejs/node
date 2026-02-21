'use strict';

module.exports = require('stream').Writable;

process.emitWarning('The _stream_writable module is deprecated. Use `node:stream` instead.',
                    'DeprecationWarning', 'DEP0193');
