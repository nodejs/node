// Fixture CJS file that calls require.resolve and exports the result.
'use strict';
const resolved = require.resolve('test-require-resolve-hook-target');
module.exports = { resolved };
