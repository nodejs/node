'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const path = require('path');
const { execFileSync } = require('child_process');

const script = path.join(__dirname, '..', '..', 'tools', 'doc', 'apilinks.js');
const fixture = fixtures.path('apilinks', 'known1.js');
const expected = {
  'Known.classField': 'known1.js#L7',
  'known1.createKnown': 'known1.js#L9'
};
const ret = execFileSync(
  process.execPath, [script, fixture], { encoding: 'utf-8' }
);
const links = JSON.parse(ret);
for (const [k, v] of Object.entries(expected)) {
  assert.ok(k in links);
  assert.ok(links[k].includes(v));
  delete links[k];
}
assert.strictEqual(Object.keys(links).length, 0);
