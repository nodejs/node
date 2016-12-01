'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

const readable = new Readable({
	read(size) {}
});

// Starting off with false initially.
assert.strictEqual(readable._readableState.readingMore, false);

readable.on('data', common.mustCall((data) => {
	// Still in a flowing state, should try to read more.
	assert.strictEqual(readable._readableState.readingMore, true);
}));

readable.on('end', common.mustCall(() => {
	// End of stream; _readableState.reading is false
	// And so should be readingMore.
	assert.strictEqual(readable._readableState.readingMore, false);
}));

readable.push('abc');
readable.push(null); // end

process.nextTick(() => {
	// Finished reading. _readableState.reading is false.
	// readingMore then should be false as well.
	assert.strictEqual(readable._readableState.readingMore, false);
});
