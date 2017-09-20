'use strict';
const common = require('../common');
const zlib = require('zlib');
const assert = require('assert');

const message = 'Come on, Fhqwhgads.';
const buffer = Buffer.from(message);

const zipper = new zlib.Gzip();
zipper.on('close', common.mustNotCall);

const zipped = zipper._processChunk(buffer, zlib.constants.Z_FINISH);

const unzipper = new zlib.Gunzip();
unzipper.on('close', common.mustNotCall);

const unzipped = unzipper._processChunk(zipped, zlib.constants.Z_FINISH);
assert.notStrictEqual(zipped.toString(), message);
assert.strictEqual(unzipped.toString(), message);
