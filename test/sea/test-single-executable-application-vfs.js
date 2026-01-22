'use strict';

require('../common');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the SEA VFS integration - sea.getVfs() and sea.hasAssets()

const tmpdir = require('../common/tmpdir');
const { writeFileSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

const configFile = tmpdir.resolve('sea-config.json');
const seaPrepBlob = tmpdir.resolve('sea-prep.blob');
const outputFile = tmpdir.resolve(process.platform === 'win32' ? 'sea.exe' : 'sea');

// The script that will run inside the SEA
const seaMain = `
'use strict';
const fs = require('fs');
const { createRequire } = require('module');
const sea = require('node:sea');
const assert = require('assert');

// Test hasSeaAssets() returns true when we have assets
const hasAssets = sea.hasAssets();
assert.strictEqual(hasAssets, true, 'hasSeaAssets() should return true');
console.log('hasSeaAssets:', hasAssets);

// Test getSeaVfs() returns a VFS instance
const vfs = sea.getVfs();
assert.ok(vfs !== null, 'getSeaVfs() should not return null');
console.log('getSeaVfs returned VFS instance');

// Test that the VFS is mounted at /sea by default
// and contains our assets

// Read the config file through standard fs (via VFS hooks)
const configContent = fs.readFileSync('/sea/config.json', 'utf8');
const config = JSON.parse(configContent);
assert.strictEqual(config.name, 'test-app', 'config.name should match');
assert.strictEqual(config.version, '1.0.0', 'config.version should match');
console.log('Read config.json:', config);

// Read a text file
const greeting = fs.readFileSync('/sea/data/greeting.txt', 'utf8');
assert.strictEqual(greeting, 'Hello from SEA VFS!', 'greeting should match');
console.log('Read greeting.txt:', greeting);

// Test existsSync
assert.strictEqual(fs.existsSync('/sea/config.json'), true);
assert.strictEqual(fs.existsSync('/sea/data/greeting.txt'), true);
assert.strictEqual(fs.existsSync('/sea/nonexistent.txt'), false);
console.log('existsSync tests passed');

// Test statSync
const configStat = fs.statSync('/sea/config.json');
assert.strictEqual(configStat.isFile(), true);
assert.strictEqual(configStat.isDirectory(), false);
console.log('statSync tests passed');

// Test readdirSync
const entries = fs.readdirSync('/sea');
assert.ok(entries.includes('config.json'), 'Should include config.json');
assert.ok(entries.includes('data'), 'Should include data directory');
console.log('readdirSync tests passed, entries:', entries);

// Test requiring a module from SEA VFS using module.createRequire()
// (SEA's built-in require only supports built-in modules)
const seaRequire = createRequire('/sea/');
const mathModule = seaRequire('/sea/modules/math.js');
assert.strictEqual(mathModule.add(2, 3), 5, 'math.add should work');
assert.strictEqual(mathModule.multiply(4, 5), 20, 'math.multiply should work');
console.log('require from VFS tests passed');

// Test getSeaVfs with custom prefix
const customVfs = sea.getVfs({ prefix: '/custom' });
// Note: getSeaVfs is a singleton, so it returns the same instance
// with the same mount point (/sea) regardless of options passed after first call
assert.strictEqual(customVfs, vfs, 'Should return the same cached instance');
console.log('Cached VFS instance test passed');

console.log('All SEA VFS tests passed!');
`;

// Create the main script file
writeFileSync(tmpdir.resolve('sea-main.js'), seaMain);

// Create asset files
writeFileSync(tmpdir.resolve('config.json'), JSON.stringify({
  name: 'test-app',
  version: '1.0.0',
}));

writeFileSync(tmpdir.resolve('greeting.txt'), 'Hello from SEA VFS!');

writeFileSync(tmpdir.resolve('math.js'), `
module.exports = {
  add: (a, b) => a + b,
  multiply: (a, b) => a * b,
};
`);

// Create SEA config with assets
writeFileSync(configFile, JSON.stringify({
  main: 'sea-main.js',
  output: 'sea-prep.blob',
  assets: {
    'config.json': 'config.json',
    'data/greeting.txt': 'greeting.txt',
    'modules/math.js': 'math.js',
  },
}));

// Generate the SEA prep blob
spawnSyncAndAssert(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  { cwd: tmpdir.path },
  {},
);

// Generate the SEA executable
generateSEA(outputFile, process.execPath, seaPrepBlob);

// Run the SEA and verify output
spawnSyncAndAssert(
  outputFile,
  [],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: undefined,
    },
  },
  {
    stdout: /All SEA VFS tests passed!/,
  },
);
