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

  let expected = {};
  eval(/(expected\s*=.*?);/s.exec(fs.readFileSync(file, 'utf8'))[1]);

  const links = JSON.parse(execFileSync(
    process.execPath,
    [script, file],
    { encoding: 'utf-8' }
  ));

  for (const [k, v] of Object.entries(expected)) {
    assert.ok(k in links, `link not found: ${k}`);
    assert.ok(links[k].endsWith('/' + v),
              `link ${links[k]} expected to end with ${v}`);
    delete links[k];
  }

  assert.strictEqual(
    Object.keys(links).length, 0,
    `unexpected links returned ${JSON.stringify(Object.keys(links))}`
  );
});
