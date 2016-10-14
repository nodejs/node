'use strict';

const common = require('../../common');
const assert = require('assert');

const re = new RegExp(
  'was compiled against a different Node.js version using\n' +
  'NODE_MODULE_VERSION 42. This version of Node.js requires\n' +
  `NODE_MODULE_VERSION ${process.versions.modules}.`);

assert.throws(() => require(`./build/${common.buildType}/binding`), re);
