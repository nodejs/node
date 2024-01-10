import '../common/index.mjs';
import assert from 'assert';

{
  // Not providing url argument should throw
  assert.throws(() => new EventSource(), TypeError);
}

{
  // Stringify url argument
  // eventsource-constructor-stringify.window.js
  // assert.strictEqual(new EventSource(1).url, '1');
  // assert.strictEqual(new EventSource(undefined).url, 'undefined');
  // assert.strictEqual(new EventSource(null).url, 'null');
}
