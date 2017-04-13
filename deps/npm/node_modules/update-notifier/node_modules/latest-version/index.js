'use strict';
const packageJson = require('package-json');

module.exports = name => packageJson(name.toLowerCase(), 'latest').then(data => data.version);
