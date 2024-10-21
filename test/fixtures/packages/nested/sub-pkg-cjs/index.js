const { getPackageJSON } = require('node:module');
const { resolve } = require('node:path');

module.exports = getPackageJSON(resolve(__dirname, '..'));
