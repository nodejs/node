'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');

// The SEA VFS never shadows the real file system: the assets are mounted at
// a reserved mount point and the main script runs from inside the mount, so
// everything is reachable through __dirname-relative paths and relative
// requires.

// The main script runs from inside the VFS, not from the executable.
assert.notStrictEqual(__filename, process.execPath);
assert.strictEqual(path.basename(__filename), 'sea.js');
assert.strictEqual(require.main, module);
console.log('main script runs from', __filename);

// The main script itself is readable through fs.
assert.strictEqual(fs.existsSync(__filename), true);

// Read the config file through standard fs (via VFS hooks)
const configContent = fs.readFileSync(path.join(__dirname, 'config.json'), 'utf8');
const config = JSON.parse(configContent);
assert.strictEqual(config.name, 'test-app', 'config.name should match');
assert.strictEqual(config.version, '1.0.0', 'config.version should match');
console.log('Read config.json:', config);

// Read a text file
const greetingPath = path.join(__dirname, 'data', 'greeting.txt');
const greeting = fs.readFileSync(greetingPath, 'utf8');
assert.strictEqual(greeting, 'Hello from SEA VFS!', 'greeting should match');
console.log('Read greeting.txt:', greeting);

// Test existsSync
assert.strictEqual(fs.existsSync(path.join(__dirname, 'config.json')), true);
assert.strictEqual(fs.existsSync(greetingPath), true);
assert.strictEqual(fs.existsSync(path.join(__dirname, 'nonexistent.txt')), false);
console.log('existsSync tests passed');

// Test statSync
const configStat = fs.statSync(path.join(__dirname, 'config.json'));
assert.strictEqual(configStat.isFile(), true);
assert.strictEqual(configStat.isDirectory(), false);
const dirStat = fs.statSync(path.join(__dirname, 'data'));
assert.strictEqual(dirStat.isDirectory(), true);
console.log('statSync tests passed');

// Test readdirSync - the mount root lists the assets and the main script
const entries = fs.readdirSync(__dirname);
assert.ok(entries.includes('config.json'), 'Should include config.json');
assert.ok(entries.includes('data'), 'Should include data directory');
assert.ok(entries.includes('sea.js'), 'Should include the main script');
console.log('readdirSync tests passed, entries:', entries);

// The VFS is read-only
assert.throws(() => {
  fs.writeFileSync(path.join(__dirname, 'new-file.txt'), 'nope');
}, { code: 'EROFS' });
console.log('read-only test passed');

// Test relative require from main script - __filename is inside the mount so
// relative paths resolve against the bundled assets via module hooks
const mathModule = require('./modules/math.js');
assert.strictEqual(mathModule.add(2, 3), 5, 'math.add should work');
assert.strictEqual(mathModule.multiply(4, 5), 20, 'math.multiply should work');
console.log('relative require from main script passed');

// Test transitive requires: calculator.js requires ./math.js internally
const calculator = require('./modules/calculator.js');
assert.strictEqual(calculator.sum(10, 20), 30, 'calculator.sum should work');
assert.strictEqual(calculator.product(3, 7), 21, 'calculator.product should work');
console.log('transitive require from VFS tests passed');

// Module lookup paths are confined to the mount
assert.deepStrictEqual(module.paths, [path.join(__dirname, 'node_modules')]);
console.log('module.paths confinement test passed');

// Test that node:sea API and VFS can load the same asset
const sea = require('node:sea');
const seaAsset = sea.getAsset('data/greeting.txt', 'utf8');
const vfsAsset = fs.readFileSync(greetingPath, 'utf8');
assert.strictEqual(seaAsset, vfsAsset, 'node:sea and VFS should return the same content');
console.log('node:sea API and VFS coexistence test passed');

// Test buffer independence: multiple reads return independent copies
const buf1 = fs.readFileSync(greetingPath);
const buf2 = fs.readFileSync(greetingPath);
const original = buf1[0];
buf1[0] = 0xFF;
assert.strictEqual(buf2[0], original, 'buf2 should be unaffected by buf1 mutation');
assert.strictEqual(buf1[0], 0xFF, 'buf1 mutation should persist');
console.log('buffer independence test passed');

// Test node_modules package lookup via VFS (resolved through "exports" field)
const testPkg = require('test-pkg');
assert.strictEqual(testPkg.name, 'test-pkg', 'package name should match');
assert.strictEqual(testPkg.greet('World'), 'Hello, World!', 'package function should work');
console.log('node_modules package lookup test passed');

// Test exports-only package (no "main" field, entry in subdirectory)
// This proves the package.json reader is VFS-aware - without it,
// "exports" would not be consulted and resolution would fail.
const exportsPkg = require('test-exports-pkg');
assert.strictEqual(exportsPkg.fromExports, true, 'exports-only package should resolve');
console.log('exports-only package lookup test passed');

console.log('All SEA VFS tests passed!');
