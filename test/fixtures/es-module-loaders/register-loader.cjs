const { register } = require('node:module');
const fixtures = require('../../common/fixtures.js');

register(fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'));
