'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');
const path = require('path');
const { execFileSync } = require('child_process');

const script = path.join(__dirname, '..', '..', 'tools', 'doc', 'apilinks.js');

const apilinks = fixtures.path('apilinks');
fs.readdirSync(apilinks).forEach((fixture) => {
  if (!fixture.endsWith('.js')) return;
  const file = path.join(apilinks, fixture);

  const expectedContent = fs.readFileSync(file + 'on', 'utf8');

  const output = execFileSync(
    process.execPath,
    [script, file],
    { encoding: 'utf-8' }
  );

  const expectedLinks = JSON.parse(expectedContent);
  const actualLinks = JSON.parse(output);

  for (const [k, v] of Object.entries(expectedLinks)) {
    assert.ok(k in actualLinks, `link not found: ${k}`);
    assert.ok(actualLinks[k].endsWith('/' + v),
              `link ${actualLinks[k]} expected to end with ${v}`);
    delete actualLinks[k];
  }

  assert.strictEqual(
    Object.keys(actualLinks).length, 0,
    `unexpected links returned ${JSON.stringify(actualLinks)}`
  );
});
