'use strict';
require('../common');

const { cache } = require;

Object.keys(cache).forEach((key) => {
  delete cache[key];
});
// Require the same module again triggers the crash
require('../common');
