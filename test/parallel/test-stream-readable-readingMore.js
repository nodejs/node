'use strict';
const Readable = require('stream').Readable,
	assert = require('assert');

const readable = new Readable({
	read(size) {}
});

// false initially
assert.strictEqual(readable._readableState.readingMore, false);

readable.on('data', data => {
	// still in a flowing state, try to read more
	assert.strictEqual(readable._readableState.readingMore, true);
});

readable.on('end', () => {
	// end of stream
	// reading is false, and so should be readingMore
	assert.strictEqual(readable._readableState.readingMore, false);
});


readable.push("abc");
readable.push(null);

process.nextTick(() => {
	// finished reading
	// reading is false, and so should be readingMore
	assert.strictEqual(readable._readableState.readingMore, false);
});
