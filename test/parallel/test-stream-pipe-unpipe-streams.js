'use strict';
const common = require('../common');
const assert = require('assert');

const { Readable, Writable } = require('stream');

const source = Readable({read: () => {}});
const dest1 = Writable({write: () => {}});
const dest2 = Writable({write: () => {}});

source.pipe(dest1);
source.pipe(dest2);

dest1.on('unpipe', common.mustCall(() => {}));
dest2.on('unpipe', common.mustCall(() => {}));

// Should be able to unpipe them in the reverse order that they were piped.

source.unpipe(dest2);

assert.strictEqual(source._readableState.pipes, dest1);
assert.notStrictEqual(source._readableState.pipes, dest2);

source.unpipe(dest1);

assert.strictEqual(source._readableState.pipes, null);
