// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { AbortError } = require('internal/errors');

// Purpose: pass an AbortError instance, which isn't the DOMException, as an
// abort reason.

for (const message of [undefined, 'abc']) {
  const rs = new ReadableStream();
  const ws = new WritableStream({ write(chunk) { } });
  const ac = new AbortController();
  const reason = new AbortError(message);
  ac.abort(reason);

  assert.rejects(rs.pipeTo(ws, { signal: ac.signal }), (e) => {
    assert(e instanceof DOMException);
    assert.strictEqual(e.name, 'AbortError');
    assert.strictEqual(e.message, reason.message);
    return true;
  }).then(common.mustCall());
}
