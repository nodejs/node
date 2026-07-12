const path = require('path');

const name = require('./esm.mjs').name;
exports.name = name;

const filename = path.join(__dirname, 'esm.mjs');
const cache = require.cache[filename];
exports.cache = require.cache[filename];

