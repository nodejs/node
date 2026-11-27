'use strict';

const common = require('../common');
const assert = require('assert');
const {
  compose,
  Duplex,
  Readable,
} = require('stream');

// Returns a Duplex whose writable side finishes (emitting 'finish') but whose
// readable side then errors instead of ending. This mimics a zlib decompressor
// that only detects truncated input during its final flush, which runs after the
// writable side has already finished, so 'end' is never emitted.
function createTail() {
  return new Duplex({
    write(chunk, encoding, callback) {
      callback();
    },
    final(callback) {
      callback();
      setImmediate(() => this.destroy(new Error('readable-side failure after finish')));
    },
    read() {},
  });
}

// Regression test: an error emitted by the last stream of compose() after its
// writable side has finished must be forwarded to the composed Duplex, rather
// than becoming an unhandled 'error' event on the tail.
{
  const composed = compose(Readable.from([Buffer.from('x')]), createTail());

  composed.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'readable-side failure after finish');
  }));
  composed.resume();
}

// The same error must reject a `for await` consumer of the composed stream.
{
  const composed = compose(Readable.from([Buffer.from('x')]), createTail());

  (async () => {
    try {
      for await (const chunk of composed) {
        assert.ok(chunk);
      }
      assert.fail('should have thrown');
    } catch (err) {
      assert.strictEqual(err.message, 'readable-side failure after finish');
    }
  })().then(common.mustCall());
}
