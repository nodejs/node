'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

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

const policyFilepath = path.join(tmpdir.path, 'policy');

const packageFilepath = path.join(tmpdir.path, 'package.json');
const packageURL = pathToFileURL(packageFilepath);
const packageBody = '{"main": "dep.js"}';
const policyToPackageRelativeURLString = `./${
  path.relative(path.dirname(policyFilepath), packageFilepath)
}`;

const parentFilepath = path.join(tmpdir.path, 'parent.js');
const parentURL = pathToFileURL(parentFilepath);
const parentBody = 'require(\'./dep.js\')';

const workerSpawningFilepath = path.join(tmpdir.path, 'worker_spawner.js');
const workerSpawningURL = pathToFileURL(workerSpawningFilepath);
const workerSpawningBody = `
const { Worker } = require('worker_threads');
// make sure this is gone to ensure we don't do another fs read of it
// will error out if we do
require('fs').unlinkSync(${JSON.stringify(policyFilepath)});
const w = new Worker(${JSON.stringify(parentFilepath)});
w.on('exit', process.exit);
`;

const depFilepath = path.join(tmpdir.path, 'dep.js');
const depURL = pathToFileURL(depFilepath);
const depBody = '';
const policyToDepRelativeURLString = `./${
  path.relative(path.dirname(policyFilepath), depFilepath)
}`;

fs.writeFileSync(parentFilepath, parentBody);
fs.writeFileSync(depFilepath, depBody);

const tmpdirURL = pathToFileURL(tmpdir.path);
if (!tmpdirURL.pathname.endsWith('/')) {
  tmpdirURL.pathname += '/';
}
function test({
  shouldFail = false,
  preload = [],
  entry,
  onerror = undefined,
  resources = {}
}) {
  const manifest = {
    onerror,
    resources: {}
  };
  for (const [url, { body, match }] of Object.entries(resources)) {
    manifest.resources[url] = {
      integrity: `sha256-${hash('sha256', match ? body : body + '\n')}`,
      dependencies: true
    };
    fs.writeFileSync(new URL(url, tmpdirURL.href), body);
  }
  fs.writeFileSync(policyFilepath, JSON.stringify(manifest, null, 2));
  const { status } = spawnSync(process.execPath, [
    '--experimental-policy', policyFilepath,
    ...preload.map((m) => ['-r', m]).flat(),
    entry
  ]);
  if (shouldFail) {
    assert.notStrictEqual(status, 0);
  } else {
    assert.strictEqual(status, 0);
  }
}

{
  const { status } = spawnSync(process.execPath, [
    '--experimental-policy', policyFilepath,
    '--experimental-policy', policyFilepath
  ], {
    stdio: 'pipe'
  });
  assert.notStrictEqual(status, 0, 'Should not allow multiple policies');
}
{
  const enoentFilepath = path.join(tmpdir.path, 'enoent');
  try { fs.unlinkSync(enoentFilepath); } catch {}
  const { status } = spawnSync(process.execPath, [
    '--experimental-policy', enoentFilepath, '-e', ''
  ], {
    stdio: 'pipe'
  });
  assert.notStrictEqual(status, 0, 'Should not allow missing policies');
}

test({
  shouldFail: true,
  entry: parentFilepath,
  resources: {
  }
});
test({
  shouldFail: false,
  entry: parentFilepath,
  onerror: 'log',
});
test({
  shouldFail: true,
  entry: parentFilepath,
  onerror: 'exit',
});
test({
  shouldFail: true,
  entry: parentFilepath,
  onerror: 'throw',
});
test({
  shouldFail: true,
  entry: parentFilepath,
  onerror: 'unknown-onerror-value',
});
test({
  shouldFail: true,
  entry: path.dirname(packageFilepath),
  resources: {
  }
});
test({
  shouldFail: true,
  entry: path.dirname(packageFilepath),
  resources: {
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: false,
  entry: path.dirname(packageFilepath),
  onerror: 'log',
  resources: {
    [packageURL]: {
      body: packageBody,
      match: false,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: true,
  entry: path.dirname(packageFilepath),
  resources: {
    [packageURL]: {
      body: packageBody,
      match: false,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: true,
  entry: path.dirname(packageFilepath),
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: false,
    }
  }
});
test({
  shouldFail: false,
  entry: path.dirname(packageFilepath),
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: false,
  entry: parentFilepath,
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [parentURL]: {
      body: parentBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: false,
  preload: [depFilepath],
  entry: parentFilepath,
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [parentURL]: {
      body: parentBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: true,
  entry: parentFilepath,
  resources: {
    [parentURL]: {
      body: parentBody,
      match: false,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: true,
  entry: parentFilepath,
  resources: {
    [parentURL]: {
      body: parentBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: false,
    }
  }
});
test({
  shouldFail: true,
  entry: parentFilepath,
  resources: {
    [parentURL]: {
      body: parentBody,
      match: true,
    }
  }
});
test({
  shouldFail: false,
  entry: depFilepath,
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: false,
  entry: depFilepath,
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [policyToDepRelativeURLString]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: true,
  entry: depFilepath,
  resources: {
    [policyToDepRelativeURLString]: {
      body: depBody,
      match: false,
    }
  }
});
test({
  shouldFail: false,
  entry: depFilepath,
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [policyToDepRelativeURLString]: {
      body: depBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: true,
  entry: depFilepath,
  resources: {
    [policyToPackageRelativeURLString]: {
      body: packageBody,
      match: true,
    },
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: false,
    }
  }
});
test({
  shouldFail: true,
  entry: workerSpawningFilepath,
  resources: {
    [workerSpawningURL]: {
      body: workerSpawningBody,
      match: true,
    },
  }
});
test({
  shouldFail: false,
  entry: workerSpawningFilepath,
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [workerSpawningURL]: {
      body: workerSpawningBody,
      match: true,
    },
    [parentURL]: {
      body: parentBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
test({
  shouldFail: false,
  entry: workerSpawningFilepath,
  preload: [parentFilepath],
  resources: {
    [packageURL]: {
      body: packageBody,
      match: true,
    },
    [workerSpawningURL]: {
      body: workerSpawningBody,
      match: true,
    },
    [parentURL]: {
      body: parentBody,
      match: true,
    },
    [depURL]: {
      body: depBody,
      match: true,
    }
  }
});
