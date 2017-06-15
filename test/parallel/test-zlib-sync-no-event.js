'use strict';
require('../common');
const zlib = require('zlib');
const assert = require('assert');

const shouldNotBeCalled = () => { throw new Error('unexpected event'); };

const message = 'Come on, Fhqwhgads.';

const zipper = new zlib.Gzip();
zipper.on('close', shouldNotBeCalled);

const buffer = Buffer.from(message);
const zipped = zipper._processChunk(buffer, zlib.constants.Z_FINISH);

const unzipper = new zlib.Gunzip();
unzipper.on('close', shouldNotBeCalled);

const unzipped = unzipper._processChunk(zipped, zlib.constants.Z_FINISH);
assert.notStrictEqual(zipped.toString(), message);
assert.strictEqual(unzipped.toString(), message);
