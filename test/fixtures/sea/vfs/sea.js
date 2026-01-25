'use strict';
const fs = require('fs');
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

// Test requiring a module from SEA VFS using direct require()
// (SEA's require now supports VFS paths automatically)
const mathModule = require('/sea/modules/math.js');
assert.strictEqual(mathModule.add(2, 3), 5, 'math.add should work');
assert.strictEqual(mathModule.multiply(4, 5), 20, 'math.multiply should work');
console.log('direct require from VFS tests passed');

// Test getSeaVfs with custom prefix
const customVfs = sea.getVfs({ prefix: '/custom' });
// Note: getSeaVfs is a singleton, so it returns the same instance
// with the same mount point (/sea) regardless of options passed after first call
assert.strictEqual(customVfs, vfs, 'Should return the same cached instance');
console.log('Cached VFS instance test passed');

console.log('All SEA VFS tests passed!');
