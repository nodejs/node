'use strict';
const { describe, it, before, after, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const stateFile = process.env.FLAKY_STATE;
const counts = { before: 0, after: 0, beforeEach: 0, afterEach: 0, body: 0 };
function dump() { fs.writeFileSync(stateFile, JSON.stringify(counts)); }
describe('hook suite', () => {
  before(() => { counts.before++; dump(); });
  after(() => { counts.after++; dump(); });
  beforeEach(() => { counts.beforeEach++; dump(); });
  afterEach(() => { counts.afterEach++; dump(); });
  it('passes on third attempt', { flaky: 5 }, () => {
    counts.body++; dump();
    assert.strictEqual(counts.body, 3);
  });
});
