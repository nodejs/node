'use strict';
const fs = require('fs');
const assert = require('assert');

// Test that the VFS is automatically mounted at /sea
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

// Test relative require from main script - __filename is /sea/sea.js so
// relative paths resolve under /sea/ via module hooks
const mathModule = require('./modules/math.js');
assert.strictEqual(mathModule.add(2, 3), 5, 'math.add should work');
assert.strictEqual(mathModule.multiply(4, 5), 20, 'math.multiply should work');
console.log('relative require from main script passed');

// Test transitive requires: calculator.js requires ./math.js internally
const calculator = require('./modules/calculator.js');
assert.strictEqual(calculator.sum(10, 20), 30, 'calculator.sum should work');
assert.strictEqual(calculator.product(3, 7), 21, 'calculator.product should work');
console.log('transitive require from VFS tests passed');

// Test that node:sea API and VFS can load the same asset
const sea = require('node:sea');
const seaAsset = sea.getAsset('data/greeting.txt', 'utf8');
const vfsAsset = fs.readFileSync('/sea/data/greeting.txt', 'utf8');
assert.strictEqual(seaAsset, vfsAsset, 'node:sea and VFS should return the same content');
console.log('node:sea API and VFS coexistence test passed');

// Test buffer independence: multiple reads return independent copies
const buf1 = fs.readFileSync('/sea/data/greeting.txt');
const buf2 = fs.readFileSync('/sea/data/greeting.txt');
const original = buf1[0];
buf1[0] = 0xFF;
assert.strictEqual(buf2[0], original, 'buf2 should be unaffected by buf1 mutation');
assert.strictEqual(buf1[0], 0xFF, 'buf1 mutation should persist');
console.log('buffer independence test passed');

// TODO(mcollina): The CJS module loader reads package.json via C++ binding
// (internalBinding('modules').readPackageJSON), which doesn't go through VFS.
// This means "exports" conditions in package.json won't work for VFS packages.
// The test below works because "main": "index.js" matches the default fallback.
// A follow-up PR should make the C++ package.json reader VFS-aware.

// Test node_modules package lookup via VFS
const testPkg = require('test-pkg');
assert.strictEqual(testPkg.name, 'test-pkg', 'package name should match');
assert.strictEqual(testPkg.greet('World'), 'Hello, World!', 'package function should work');
console.log('node_modules package lookup test passed');

console.log('All SEA VFS tests passed!');
