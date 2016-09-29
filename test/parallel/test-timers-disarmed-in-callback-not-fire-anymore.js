'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

const testPath = './test/fixtures/test-timer-disarm.js';
const testCommand = 'NODE_DEBUG=timer ' + process.execPath + ' ' + testPath;

setTimeout(() => {
  exec(testCommand, (err, stdout, stderr) => {
    const o = stderr.split('\n');
    let timer1Fired = 0;
    let timer2Fired = 0;
    let timer3Fired = 0;
    for (var i = 0; i < o.length; i++) {
      var line = o[i];
      if (line.indexOf('timeout callback 11') >= 0) {
        timer1Fired++;
      }
      if (line.indexOf('timeout callback 12') >= 0) {
        timer2Fired++;
      }
      if (line.indexOf('timeout callback 13') >= 0) {
        timer3Fired++;
      }
    }
    assert.strictEqual(timer1Fired, 1);
    assert.strictEqual(timer2Fired, 1);
    assert.strictEqual(timer3Fired, 1);
  });
}, 100);
