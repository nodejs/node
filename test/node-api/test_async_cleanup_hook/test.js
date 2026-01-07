'use strict';
// Addons: binding, binding_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const path = require('path');
const { Worker } = require('worker_threads');
const binding = path.resolve(__dirname, addonPath);

const w = new Worker(`require(${JSON.stringify(binding)})`, { eval: true });
w.on('exit', common.mustCall(() => require(binding)));
