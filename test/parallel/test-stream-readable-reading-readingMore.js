'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

const readable = new Readable({
	read(size) {}
});

const state = readable._readableState;

// Starting off with false initially.
assert.strictEqual(state.reading, false);
assert.strictEqual(state.readingMore, false);

readable.on('data', common.mustCall((data) => {
	// while in a flowing state, should try to read more.
	if (state.flowing)
		assert.strictEqual(state.readingMore, true);

	// reading as long as we've not ended
	assert.strictEqual(state.reading, !state.ended);
}, 2));

readable.on('end', common.mustCall(() => {
	// End of stream; _readableState.reading is false
	// And so should be readingMore.
	assert.strictEqual(state.readingMore, false);
	assert.strictEqual(state.reading, false);
}));

readable.push('pushed');

// stop emitting 'data' events
readable.pause();

// read() should only be called while operating in paused mode
readable.read(6);

// reading
assert.strictEqual(state.reading, true);

// resume emitting 'data' events
readable.resume();

// add chunk to front
readable.unshift('unshifted');
readable.push(null); // end

process.nextTick(() => {
	// Finished reading. _readableState.reading is false.
	// readingMore then should be false as well.
	assert.strictEqual(state.readingMore, false);
});
