'use strict';
require('../../../common');
const { describe, it } = require('node:test');

describe('describe timeout signal', { signal: AbortSignal.timeout(1) }, (t) => {
  it('ok 1', async () => {});
  it('ok 2', () => {});
  it('ok 3', { signal: t.signal }, async () => {});
  it('ok 4', { signal: t.signal }, () => {});
  it('not ok 1', () => new Promise(() => {}));
  it('not ok 2', (done) => {});
  it('not ok 3', { signal: t.signal }, () => new Promise(() => {}));
  it('not ok 4', { signal: t.signal }, (done) => {});
  it('not ok 5', { signal: t.signal }, function(done) {
    this.signal.addEventListener('abort', done);
  });
});

describe('describe abort signal', { signal: AbortSignal.abort() }, () => {
  it('should not appear', () => {});
});

// AbortSignal.timeout(1) doesn't prevent process from closing
// thus we have to keep the process open to prevent cancelation
// of the entire test tree
setTimeout(() => {}, 1000);
