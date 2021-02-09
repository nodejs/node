'use strict';

// This test crashes most of the time unless the v8 patch:
// https://github.com/v8/v8/commit/beebee4f80ff2eb91187ef1e8fa00b8ff82a20b3
// is applied.
const common = require('../../common');
const bindings = require(`./build/${common.buildType}/binding`);

function fn() { setImmediate(fn); }
setInterval(function() {
  for (let i = 0; i < 1000000; i++)
    fn();
  clearInterval(this);
  setTimeout(process.reallyExit, 2000);
}, 0);


setTimeout(() => {
  bindings.startCpuProfiler();
}, 1000);
