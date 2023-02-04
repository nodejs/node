// Flags: --expose-gc
'use strict';

const common = require('../common');
const { aborted } = require('util');
const assert = require('assert');
const { getEventListeners } = require('events');

{
  // Test aborted works
  const ac = new AbortController();
  aborted(ac.signal).then(common.mustCall());
  ac.abort();
  assert.strictEqual(ac.signal.aborted, true);
  assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);
}

{
  // Test aborted works when provided a resource
  const ac = new AbortController();
  aborted(ac.signal, {}).then(common.mustCall());
  ac.abort();
  assert.strictEqual(ac.signal.aborted, true);
  assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);
}

{
  // Test aborted with gc cleanup
  const ac = new AbortController();
  aborted(ac.signal, {}).then(common.mustNotCall());
  setImmediate(() => {
    global.gc();
    ac.abort();
    assert.strictEqual(ac.signal.aborted, true);
    assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);
  });
}
