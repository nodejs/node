'use strict';
const common = require('../../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tmpdir = require('../../common/tmpdir');
const { spawnSync } = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const { pathToFileURL } = require('url');

tmpdir.refresh();

function hash(algo, body) {
  const h = crypto.createHash(algo);
  h.update(body);
  return h.digest('base64');
}

const policyFilepath = tmpdir.resolve('policy');

const depFilepath = require.resolve(`./build/${common.buildType}/binding.node`);
const depURL = pathToFileURL(depFilepath);

const depBody = fs.readFileSync(depURL);
function writePolicy(...resources) {
  const manifest = { resources: {} };
  for (const { url, integrity } of resources) {
    manifest.resources[url] = { integrity };
  }
  fs.writeFileSync(policyFilepath, JSON.stringify(manifest, null, 2));
}


function test(shouldFail, resources) {
  writePolicy(...resources);
  const { status, stdout, stderr } = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    depFilepath,
  ]);

  console.log(stdout.toString(), stderr.toString());
  if (shouldFail) {
    assert.notStrictEqual(status, 0);
  } else {
    assert.strictEqual(status, 0);
  }
}

test(false, [{
  url: depURL,
  integrity: `sha256-${hash('sha256', depBody)}`,
}]);
test(true, [{
  url: depURL,
  integrity: `sha256akjsalkjdlaskjdk-${hash('sha256', depBody)}`,
}]);
