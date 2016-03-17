'use strict';
require('../common');
const zlib = require('zlib');

const zipper = new zlib.Gzip();
zipper.on('close', () => { throw new Error('unexpected `close` event'); });

const buffer = new Buffer('hello world');
zipper._processChunk(buffer, zlib.Z_FINISH);
