'use strict';

// This tests the error conditions of generating the compile cache of the
// virtual file system when "useCodeCache" is combined with "useVfs": a module
// that does not compile is left out of the cache, while a broken archive, a
// broken main script or a build environment without a usable temporary
// directory fail the build.

const common = require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');

const kCompileCachePrefix = 'node:sea:compile-cache/';
const kBlob = 'sea-prep.blob';

const hasTypeScript = process.features.typescript === 'strip';

function countCompileCacheEntries(blobPath) {
  const blob = fs.readFileSync(blobPath).toString('latin1');
  return blob.split(kCompileCachePrefix).length - 1;
}

function writeConfig(name, config) {
  fs.writeFileSync(tmpdir.resolve(name), JSON.stringify(config));
}

// Generates the preparation blob of the configuration in the working
// directory and returns the child process result.
function generateBlob(configName, env, expectations) {
  return spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', configName],
    {
      cwd: tmpdir.path,
      env: {
        ...process.env,
        NODE_DEBUG: 'sea',
        NODE_DEBUG_NATIVE: 'SEA',
        ...env,
      },
    },
    expectations);
}

// Test: a module that does not compile is skipped, the rest is cached, and
// the executable runs with the broken module failing when it is loaded.
{
  tmpdir.refresh();
  fs.writeFileSync(tmpdir.resolve('main.js'), `
    const path = require('path');
    try {
      require(path.join(__dirname, 'modules', 'broken.js'));
    } catch (err) {
      console.log('broken:', err.name);
    }
    console.log('good:', require(path.join(__dirname, 'modules', 'good.js')));
    if (process.features.typescript === 'strip') {
      console.log('ts:', require(path.join(__dirname, 'modules', 'good.ts')));
    }
  `);
  fs.writeFileSync(tmpdir.resolve('good.js'), 'module.exports = 42;\n');
  fs.writeFileSync(tmpdir.resolve('broken.js'), 'module.exports = {\n');
  const assets = {
    'modules/good.js': 'good.js',
    'modules/broken.js': 'broken.js',
  };
  // main.js, good.js and, with type stripping, good.ts, which has two
  // entries: the stripped source and the code cache compiled from it.
  let modules = 2;
  let entries = 2;
  if (hasTypeScript) {
    fs.writeFileSync(tmpdir.resolve('good.ts'),
                     'const n: number = 42;\nmodule.exports = n;\n');
    // Enums are not supported by type stripping.
    fs.writeFileSync(tmpdir.resolve('enum.ts'), 'enum E { A }\n');
    assets['modules/good.ts'] = 'good.ts';
    assets['modules/enum.ts'] = 'enum.ts';
    modules++;
    entries += 2;
  }
  const config = {
    main: 'main.js',
    output: kBlob,
    useVfs: true,
    useCodeCache: true,
    assets,
  };
  writeConfig('sea-config-blob.json', config);
  writeConfig('sea-config.json', { ...config, output: 'sea' });

  generateBlob('sea-config-blob.json', {}, {
    stderr: common.mustCall((stderr) => {
      assert.match(stderr, /not caching \S*broken\.js: /);
      if (hasTypeScript) {
        assert.match(stderr, /not caching \S*enum\.ts: /);
      }
      assert.match(stderr, new RegExp(
        `Generated ${entries} compile cache entries for ${modules} modules`));
    }),
  });
  assert.strictEqual(countCompileCacheEntries(tmpdir.resolve(kBlob)), entries);

  const outputFile = buildSEA(tmpdir.path);
  spawnSyncAndAssert(
    outputFile,
    {
      env: {
        ...process.env,
        NODE_DEBUG: undefined,
        NODE_DEBUG_NATIVE: undefined,
      },
    },
    {
      stdout: common.mustCall((stdout) => {
        assert.match(stdout, /^broken: SyntaxError$/m);
        assert.match(stdout, /^good: 42$/m);
        if (hasTypeScript) {
          assert.match(stdout, /^ts: 42$/m);
        }
      }),
      stderr: common.mustCall((stderr) => {
        assert.doesNotMatch(stderr, /Code cache data rejected/);
      }),
    });
}

// Test: an ES module that does not compile is skipped too.
{
  tmpdir.refresh();
  fs.cpSync(fixtures.path('sea', 'vfs-esm'), tmpdir.path, { recursive: true });
  fs.writeFileSync(tmpdir.resolve('broken.mjs'), 'export {\n');
  const config = JSON.parse(
    fs.readFileSync(tmpdir.resolve('sea-config-code-cache.json'), 'utf8'));
  config.assets['modules/broken.mjs'] = 'broken.mjs';
  writeConfig('sea-config-code-cache.json', config);

  generateBlob('sea-config-code-cache.json', {}, {
    stderr: common.mustCall((stderr) => {
      assert.match(stderr, /not caching \S*broken\.mjs: /);
      assert.match(stderr, /Generated 4 compile cache entries for 4 modules/);
    }),
  });
  assert.strictEqual(countCompileCacheEntries(tmpdir.resolve(kBlob)), 4);
}

// Test: a "vfsArchive" that passes the signature check but cannot be parsed
// fails the build when the compile cache is generated from it.
{
  tmpdir.refresh();
  fs.writeFileSync(tmpdir.resolve('main.js'), 'console.log("hello");\n');
  fs.writeFileSync(tmpdir.resolve('assets.zip'),
                   Buffer.concat([Buffer.from('PK\x03\x04'), Buffer.alloc(64)]));
  writeConfig('sea-config.json', {
    main: 'main.js',
    output: kBlob,
    useVfs: true,
    useCodeCache: true,
    vfsArchive: 'assets.zip',
  });

  generateBlob('sea-config.json', {}, {
    status: 1,
    stderr: common.mustCall((stderr) => {
      assert.match(stderr, /ERR_ZIP_INVALID_ARCHIVE/);
      assert.match(
        stderr, /Cannot generate the compile cache of the virtual file system/);
    }),
  });
  assert.strictEqual(fs.existsSync(tmpdir.resolve(kBlob)), false);
}

// Test: a main script that does not compile fails the build.
{
  tmpdir.refresh();
  fs.writeFileSync(tmpdir.resolve('main.js'), 'console.log(\n');
  writeConfig('sea-config.json', {
    main: 'main.js',
    output: kBlob,
    useVfs: true,
    useCodeCache: true,
  });

  generateBlob('sea-config.json', {}, {
    status: 1,
    stderr: /Cannot generate V8 code cache/,
  });
  assert.strictEqual(fs.existsSync(tmpdir.resolve(kBlob)), false);
}

// Test: the compile cache is collected through a temporary directory, so the
// build fails without a usable one.
{
  tmpdir.refresh();
  fs.writeFileSync(tmpdir.resolve('main.js'), 'console.log("hello");\n');
  writeConfig('sea-config.json', {
    main: 'main.js',
    output: kBlob,
    useVfs: true,
    useCodeCache: true,
  });

  const missing = tmpdir.resolve('missing');
  generateBlob('sea-config.json', {
    TMPDIR: missing,
    TMP: missing,
    TEMP: missing,
  }, {
    status: 1,
    stderr: common.mustCall((stderr) => {
      assert.match(stderr, /ENOENT/);
      assert.match(
        stderr, /Cannot generate the compile cache of the virtual file system/);
    }),
  });
  assert.strictEqual(fs.existsSync(tmpdir.resolve(kBlob)), false);
}
