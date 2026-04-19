'use strict';

const fixtures = require('./fixtures');
const path = require('path');

function debuggerFixturePath(name) {
  return path.relative(process.cwd(), fixtures.path('debugger', name));
}

function escapeRegex(string) {
  return string.replace(/[-/\\^$*+?.()|[\]{}]/g, '\\$&');
}

module.exports = {
  escapeRegex,
  missScript: debuggerFixturePath('probe-miss.js'),
  probeScript: debuggerFixturePath('probe.js'),
  throwScript: debuggerFixturePath('probe-throw.js'),
  probeTypesScript: debuggerFixturePath('probe-types.js'),
  timeoutScript: debuggerFixturePath('probe-timeout.js'),
};
