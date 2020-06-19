'use strict';
// Flags: --no-warnings --expose-internals

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const path = require('path');

const { FSReqCallback, readdir } = internalBinding('fs');

{
  // Return value of cancel() is undefined
  const req = new FSReqCallback();
  req.oncomplete = () => {};
  assert.strictEqual(req.cancel(), undefined);
}


{
  // Callback is called with AbortError if request is canceled
  const req = new FSReqCallback();
  req.oncomplete = common.mustCall((err, files) => {
    assert.strictEqual(files, undefined);
    assert.strictEqual(err.name, 'AbortError');
  }, 1);
  req.cancel();
  readdir(path.toNamespacedPath('../'), 'utf8', false, req);
}


{
  // Request is canceled if cancel() called before control returns to main loop
  const req = new FSReqCallback();
  req.oncomplete = common.mustCall((err, files) => {
    assert.strictEqual(files, undefined);
    assert.strictEqual(err.name, 'AbortError');
  }, 1);
  readdir(path.toNamespacedPath('../'), 'utf8', false, req);
  req.cancel();
}


{
  // Request is still canceled on next tick
  const req = new FSReqCallback();
  req.oncomplete = common.mustCall((err, files) => {
    assert.strictEqual(files, undefined);
    assert.strictEqual(err.name, 'AbortError');
  }, 1);
  readdir(path.toNamespacedPath('../'), 'utf8', false, req);
  process.nextTick(common.mustCall(() => req.cancel()));
}


{
  // Callback is not called a second time if request canceled after it has
  // already completed
  const req = new FSReqCallback();
  req.oncomplete = common.mustCall((err, files) => {
    assert.strictEqual(err, null);
    assert.notStrictEqual(files, undefined);
    req.cancel();
  }, 1);
  readdir(path.toNamespacedPath('../'), 'utf8', false, req);
}
