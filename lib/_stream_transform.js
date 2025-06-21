'use strict';

module.exports = require('stream').Transform;

process.emitWarning('The _stream_transform module is deprecated. Use `node:stream` instead.',
                    'DeprecationWarning', 'DEP0193');
