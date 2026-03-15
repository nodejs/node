// Fixture CJS file that calls require.resolve with the paths option.
'use strict';
const path = require('path');
const fixturesDir = path.resolve(__dirname, '..', '..');
const nodeModules = path.join(fixturesDir, 'node_modules');

// Use the paths option to resolve 'bar' from the fixtures node_modules.
const resolved = require.resolve('bar', { paths: [fixturesDir] });
const expected = path.join(nodeModules, 'bar.js');
if (resolved !== expected) {
  throw new Error(`Expected ${expected}, got ${resolved}`);
}
module.exports = { resolved };
