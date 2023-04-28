'use strict';
const common = require('../../common');
const path = require('path');
const { Worker } = require('worker_threads');
const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

const w = new Worker(`require(${JSON.stringify(binding)})`, { eval: true });
w.on('exit', common.mustCall(() => require(binding)));
