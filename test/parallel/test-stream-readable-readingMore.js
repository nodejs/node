'use strict';
const Readable = require('stream').Readable,
	common = require('../common'),
	assert = require('assert');

const readable = new Readable({
	highWaterMark: 4,
	read(size) {}
});

// false initially
assert.strictEqual(readable._readableState.readingMore, false);

readable.on('data', data => {
	const state = readable._readableState;

	// still in a flowing state, try to read more
	assert.strictEqual(state.readingMore, true);

	const lenSurpassesWMark = state.length + data.length > state.highWaterMark,
		emptyData = !data;

	process.nextTick(() => {
		assert.strictEqual(state.readingMore, !(lenSurpassesWMark || emptyData));
	});
});

readable.on('end', () => {
	// end of stream
	// reading is false, and so should be readingMore
	assert.strictEqual(readable._readableState.readingMore, false);
});

readable.push("abc");
readable.push("abcdef");
readable.push(null);
