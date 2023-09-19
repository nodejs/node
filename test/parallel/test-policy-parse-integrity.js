'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');
common.requireNoPackageJSONAbove();

const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');

tmpdir.refresh();

function hash(algo, body) {
  const h = crypto.createHash(algo);
  h.update(body);
  return h.digest('base64');
}

const tmpdirPath = tmpdir.resolve('test-policy-parse-integrity');
fs.rmSync(tmpdirPath, { maxRetries: 3, recursive: true, force: true });
fs.mkdirSync(tmpdirPath, { recursive: true });

const policyFilepath = path.join(tmpdirPath, 'policy');

const parentFilepath = path.join(tmpdirPath, 'parent.js');
const parentBody = "require('./dep.js')";

const depFilepath = path.join(tmpdirPath, 'dep.js');
const depURL = pathToFileURL(depFilepath);
const depBody = '';

fs.writeFileSync(parentFilepath, parentBody);
fs.writeFileSync(depFilepath, depBody);

const tmpdirURL = pathToFileURL(tmpdirPath);
if (!tmpdirURL.pathname.endsWith('/')) {
  tmpdirURL.pathname += '/';
}

const packageFilepath = path.join(tmpdirPath, 'package.json');
const packageURL = pathToFileURL(packageFilepath);
const packageBody = '{"main": "dep.js"}';

function test({ shouldFail, integrity, manifest = {} }) {
  manifest.resources = {};
  const resources = {
    [packageURL]: {
      body: packageBody,
      integrity: `sha256-${hash('sha256', packageBody)}`
    },
    [depURL]: {
      body: depBody,
      integrity
    }
  };
  for (const [url, { body, integrity }] of Object.entries(resources)) {
    manifest.resources[url] = {
      integrity,
    };
    fs.writeFileSync(new URL(url, tmpdirURL.href), body);
  }
  fs.writeFileSync(policyFilepath, JSON.stringify(manifest, null, 2));
  const { status } = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    depFilepath,
  ]);
  if (shouldFail) {
    assert.notStrictEqual(status, 0);
  } else {
    assert.strictEqual(status, 0);
  }
}

test({
  shouldFail: false,
  integrity: `sha256-${hash('sha256', depBody)}`,
});
test({
  shouldFail: true,
  integrity: `1sha256-${hash('sha256', depBody)}`,
});
test({
  shouldFail: true,
  integrity: 'hoge',
});
test({
  shouldFail: true,
  integrity: `sha256-${hash('sha256', depBody)}sha256-${hash(
    'sha256',
    depBody
  )}`,
});
test({
  shouldFail: true,
  integrity: `sha256-${hash('sha256', 'file:///')}`,
  manifest: {
    onerror: 'exit'
  }
});
test({
  shouldFail: false,
  integrity: `sha256-${hash('sha256', 'file:///')}`,
  manifest: {
    onerror: 'log'
  }
});
