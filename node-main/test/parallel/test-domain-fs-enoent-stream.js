'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const fs = require('fs');

const d = new domain.Domain();

const fst = fs.createReadStream('stream for nonexistent file');

d.on('error', common.mustCall((err) => {
  assert.ok(err.message.match(/^ENOENT: no such file or directory, open '/));
  assert.strictEqual(err.domain, d);
  assert.strictEqual(err.domainEmitter, fst);
  assert.strictEqual(err.domainBound, undefined);
  assert.strictEqual(err.domainThrown, false);
}));

d.add(fst);
