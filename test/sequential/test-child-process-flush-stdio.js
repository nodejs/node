'use strict';
const cp = require('child_process');
const common = require('../common');

var p = cp.spawn('yes');

p.on('close', common.mustCall(function() {}));

p.stdout.read();

setTimeout(function() {
  p.kill();
}, 100);
