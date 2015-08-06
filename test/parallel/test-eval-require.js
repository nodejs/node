'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const path = require('path');
const fs = require('fs');

var options = {
  cwd: common.fixturesDir
};
var child = spawn(process.execPath, ['-e', 'require("foo")'], options);
child.on('exit', function(code) {
  assert.equal(code, 0);
});

