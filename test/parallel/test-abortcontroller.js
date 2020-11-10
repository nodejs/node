// Flags: --no-warnings
'use strict';

const common = require('../common');

const { ok, strictEqual } = require('assert');

{
  const ac = new AbortController();
  ok(ac.signal);
  ac.signal.onabort = common.mustCall((event) => {
    ok(event);
    strictEqual(event.type, 'abort');
  });
  ac.signal.addEventListener('abort', common.mustCall((event) => {
    ok(event);
    strictEqual(event.type, 'abort');
  }), { once: true });
  ac.abort();
  ac.abort();
  ok(ac.signal.aborted);
}
