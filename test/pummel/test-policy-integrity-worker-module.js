'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

if (common.isPi) {
  common.skip('Too slow for Raspberry Pi devices');
}

common.requireNoPackageJSONAbove();

const { debuglog } = require('util');
const debug = debuglog('test');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync, spawn } = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');

const cpus = require('os').availableParallelism();

function hash(algo, body) {
  const values = [];
  {
    const h = crypto.createHash(algo);
    h.update(body);
    values.push(`${algo}-${h.digest('base64')}`);
  }
  {
    const h = crypto.createHash(algo);
    h.update(body.replace('\n', '\r\n'));
    values.push(`${algo}-${h.digest('base64')}`);
  }
  return values;
}

const policyPath = './policy.json';
const parentBody = {
  commonjs: `
    if (!process.env.DEP_FILE) {
      console.error(
        'missing required DEP_FILE env to determine dependency'
      );
      process.exit(33);
    }
    require(process.env.DEP_FILE)
  `,
  module: `
    if (!process.env.DEP_FILE) {
      console.error(
        'missing required DEP_FILE env to determine dependency'
      );
      process.exit(33);
    }
    import(process.env.DEP_FILE)
  `,
};
const workerSpawningBody = `
  const path = require('path');
  const { Worker } = require('worker_threads');
  if (!process.env.PARENT_FILE) {
    console.error(
      'missing required PARENT_FILE env to determine worker entry point'
    );
    process.exit(33);
  }
  if (!process.env.DELETABLE_POLICY_FILE) {
    console.error(
      'missing required DELETABLE_POLICY_FILE env to check reloading'
    );
    process.exit(33);
  }
  const w = new Worker(path.resolve(process.env.PARENT_FILE));
  w.on('exit', (status) => process.exit(status === 0 ? 0 : 1));
`;

let nextTestId = 1;
function newTestId() {
  return nextTestId++;
}
tmpdir.refresh();
common.requireNoPackageJSONAbove(tmpdir.path);

let spawned = 0;
const toSpawn = [];
function queueSpawn(opts) {
  toSpawn.push(opts);
  drainQueue();
}

function drainQueue() {
  if (spawned > cpus) {
    return;
  }
  if (toSpawn.length) {
    const config = toSpawn.shift();
    const {
      shouldSucceed,
      preloads,
      entryPath,
      onError,
      resources,
      parentPath,
      depPath,
    } = config;
    const testId = newTestId();
    const configDirPath = path.join(
      tmpdir.path,
      `test-policy-integrity-permutation-${testId}`,
    );
    const tmpPolicyPath = path.join(
      tmpdir.path,
      `deletable-policy-${testId}.json`,
    );

    fs.rmSync(configDirPath, { maxRetries: 3, recursive: true, force: true });
    fs.mkdirSync(configDirPath, { recursive: true });
    const manifest = {
      onerror: onError,
      resources: {},
    };
    const manifestPath = path.join(configDirPath, policyPath);
    for (const [resourcePath, { body, integrities }] of Object.entries(
      resources,
    )) {
      const filePath = path.join(configDirPath, resourcePath);
      if (integrities !== null) {
        manifest.resources[pathToFileURL(filePath).href] = {
          integrity: integrities.join(' '),
          dependencies: true,
        };
      }
      fs.writeFileSync(filePath, body, 'utf8');
    }
    const manifestBody = JSON.stringify(manifest);
    fs.writeFileSync(manifestPath, manifestBody);

    fs.writeFileSync(tmpPolicyPath, manifestBody);

    const spawnArgs = [
      process.execPath,
      [
        '--unhandled-rejections=strict',
        '--experimental-policy',
        tmpPolicyPath,
        ...preloads.flatMap((m) => ['-r', m]),
        entryPath,
        '--',
        testId,
        configDirPath,
      ],
      {
        env: {
          ...process.env,
          DELETABLE_POLICY_FILE: tmpPolicyPath,
          PARENT_FILE: parentPath,
          DEP_FILE: depPath,
        },
        cwd: configDirPath,
        stdio: 'pipe',
      },
    ];
    spawned++;
    const stdout = [];
    const stderr = [];
    const child = spawn(...spawnArgs);
    child.stdout.on('data', (d) => stdout.push(d));
    child.stderr.on('data', (d) => stderr.push(d));
    child.on('exit', (status, signal) => {
      spawned--;
      try {
        if (shouldSucceed) {
          assert.strictEqual(status, 0);
        } else {
          assert.notStrictEqual(status, 0);
        }
      } catch (e) {
        console.log(
          'permutation',
          testId,
          'failed',
        );
        console.dir(
          { config, manifest },
          { depth: null },
        );
        console.log('exit code:', status, 'signal:', signal);
        console.log(`stdout: ${Buffer.concat(stdout)}`);
        console.log(`stderr: ${Buffer.concat(stderr)}`);
        throw e;
      }
      fs.rmSync(configDirPath, { maxRetries: 3, recursive: true, force: true });
      drainQueue();
    });
  }
}

{
  const { status } = spawnSync(
    process.execPath,
    ['--experimental-policy', policyPath, '--experimental-policy', policyPath],
    {
      stdio: 'pipe',
    },
  );
  assert.notStrictEqual(status, 0, 'Should not allow multiple policies');
}
{
  const enoentFilepath = tmpdir.resolve('enoent');
  try {
    fs.unlinkSync(enoentFilepath);
  } catch {
    // Continue regardless of error.
  }
  const { status } = spawnSync(
    process.execPath,
    ['--experimental-policy', enoentFilepath, '-e', ''],
    {
      stdio: 'pipe',
    },
  );
  assert.notStrictEqual(status, 0, 'Should not allow missing policies');
}

/**
 * @template {Record<string, Array<string | string[] | boolean>>} T
 * @param {T} configurations
 * @param {object} path
 * @returns {Array<{[key: keyof T]: T[keyof configurations]}>}
 */
function permutations(configurations, path = {}) {
  const keys = Object.keys(configurations);
  if (keys.length === 0) {
    return path;
  }
  const config = keys[0];
  const { [config]: values, ...otherConfigs } = configurations;
  return values.flatMap((value) => {
    return permutations(otherConfigs, { ...path, [config]: value });
  });
}
const tests = new Set();
function fileExtensionFormat(extension) {
  if (extension === '.js') {
    return 'module';
  } else if (extension === '.mjs') {
    return 'module';
  } else if (extension === '.cjs') {
    return 'commonjs';
  }
  throw new Error('unknown format ' + extension);
}
for (const permutation of permutations({
  preloads: [[], ['parent'], ['dep']],
  onError: ['log', 'exit'],
  parentExtension: ['.js', '.mjs', '.cjs'],
  parentIntegrity: ['match', 'invalid', 'missing'],
  depExtension: ['.js', '.mjs', '.cjs'],
  depIntegrity: ['match', 'invalid', 'missing'],
  packageIntegrity: ['match', 'invalid', 'missing'],
})) {
  let shouldSucceed = true;
  const parentPath = `./parent${permutation.parentExtension}`;
  const parentFormat = fileExtensionFormat(permutation.parentExtension);
  const depFormat = fileExtensionFormat(permutation.depExtension);

  // non-sensical attempt to require ESM
  if (depFormat === 'module' && parentFormat === 'commonjs') {
    continue;
  }
  const depPath = `./dep${permutation.depExtension}`;
  const workerSpawnerPath = './worker-spawner.cjs';
  const packageJSON = {
    main: workerSpawnerPath,
    type: 'module',
  };

  const resources = {
    [depPath]: {
      body: '',
      integrities: hash('sha256', ''),
    },
  };
  if (permutation.depIntegrity === 'invalid') {
    resources[depPath].body += '\n// INVALID INTEGRITY';
    shouldSucceed = false;
  } else if (permutation.depIntegrity === 'missing') {
    resources[depPath].integrities = null;
    shouldSucceed = false;
  } else if (permutation.depIntegrity !== 'match') {
    throw new Error('unreachable');
  }
  if (parentFormat !== 'commonjs') {
    permutation.preloads = permutation.preloads.filter((_) => _ !== 'parent');
  }

  resources[parentPath] = {
    body: parentBody[parentFormat],
    integrities: hash('sha256', parentBody[parentFormat]),
  };
  if (permutation.parentIntegrity === 'invalid') {
    resources[parentPath].body += '\n// INVALID INTEGRITY';
    shouldSucceed = false;
  } else if (permutation.parentIntegrity === 'missing') {
    resources[parentPath].integrities = null;
    shouldSucceed = false;
  } else if (permutation.parentIntegrity !== 'match') {
    throw new Error('unreachable');
  }

  resources[workerSpawnerPath] = {
    body: workerSpawningBody,
    integrities: hash('sha256', workerSpawningBody),
  };

  let packageBody = JSON.stringify(packageJSON, null, 2);
  let packageIntegrities = hash('sha256', packageBody);
  if (
    permutation.parentExtension !== '.js' ||
    permutation.depExtension !== '.js'
  ) {
    // NO PACKAGE LOOKUP
    continue;
  }
  if (permutation.packageIntegrity === 'invalid') {
    packageJSON['//'] = 'INVALID INTEGRITY';
    packageBody = JSON.stringify(packageJSON, null, 2);
    shouldSucceed = false;
  } else if (permutation.packageIntegrity === 'missing') {
    packageIntegrities = [];
    shouldSucceed = false;
  } else if (permutation.packageIntegrity !== 'match') {
    throw new Error('unreachable');
  }
  resources['./package.json'] = {
    body: packageBody,
    integrities: packageIntegrities,
  };

  if (permutation.onError === 'log') {
    shouldSucceed = true;
  }
  tests.add(
    JSON.stringify({
      onError: permutation.onError,
      shouldSucceed,
      entryPath: workerSpawnerPath,
      preloads: permutation.preloads
        .map((_) => {
          return {
            '': '',
            'parent': parentFormat === 'commonjs' ? parentPath : '',
            'dep': depFormat === 'commonjs' ? depPath : '',
          }[_];
        })
        .filter(Boolean),
      parentPath,
      depPath,
      resources,
    }),
  );
}
debug(`spawning ${tests.size} policy integrity permutations`);

for (const config of tests) {
  const parsed = JSON.parse(config);
  queueSpawn(parsed);
}
