// Flags: --allow-natives-syntax --expose-gc --no-incremental-marking
'use strict';

// A value-returning fast API callback that allocates (e.g. by flattening a
// string) can run a GC that marks its optimized caller for lazy deopt, which
// V8 does not support and turns into an abort ("Lazy deopt after a fast API
// call with return value is unsupported").

require('../common');
const assert = require('assert');
const zlib = require('zlib');

const cases = {
  'Buffer.byteLength': (s) => Buffer.byteLength(s, 'utf8'),
  'zlib.crc32': (s) => zlib.crc32(s),
  'zlib.crc32c': (s) => zlib.crc32c(s),
  'URL.canParse': (s) => URL.canParse(s),
  'URL.canParse with base': (s) => URL.canParse(s, 'http://example.com/'),
};

for (const [name, call] of Object.entries(cases)) {
  for (const unit of ['abcdefgh', 'aé中']) {
    const state = { current: undefined };
    // Only object with map "dying": once dropped, the next full GC clears the
    // map that the optimized caller embeds weakly.
    let dying = { x: 1, dying: true };
    const keep = { x: 1, keep: true };

    // `eval` keeps each case's caller distinct for the optimizer.
    const caller = eval(`(function caller(s) {
      const result = call(s);
      return [result, state.current.x];  // Polymorphic map check after the call.
    })`);

    eval('%PrepareFunctionForOptimization(caller)');
    for (let i = 0; i < 10; i++) {
      state.current = i % 2 ? keep : dying;
      caller('http://example.com/' + i);
    }
    eval('%OptimizeFunctionOnNextCall(caller)');
    caller('http://example.com/');
    state.current = keep;
    dying = null;

    // Non-flat old-space strings: flattening them inside the call eventually
    // triggers a full GC there.
    const half = unit.repeat(256 * 1024);
    const strings = [];
    for (let i = 0; i < 16; i++) {
      strings.push('http://example.com/' + half + String(i) + half);
    }
    globalThis.gc();
    globalThis.gc();

    for (let i = 0; i < strings.length; i++) {
      const [result] = caller(strings[i]);
      assert.strictEqual(result, call(strings[i]), `${name} (${i})`);
      strings[i] = null;
    }
  }
}
