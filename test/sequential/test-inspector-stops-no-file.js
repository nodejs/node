'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');

async function runTests() {
  {
    const child = new NodeInstance(['--inspect'], '', 'no-such-script.js');
    let match = false;
    while (true) {
      const stderr = await child.nextStderrString();
      if (/MODULE_NOT_FOUND/.test(stderr)) {
        match = true;
        break;
      }
      if (/Debugger listening on/.test(stderr)) {
        break;
      }
    }
    assert.ok(match);
    child.kill();
  }
  {
    const child = new NodeInstance(['--inspect-brk'], '', 'no-such-script.js');
    let match = false;
    while (true) {
      const stderr = await child.nextStderrString();
      if (/MODULE_NOT_FOUND/.test(stderr)) {
        match = true;
        break;
      }
      if (/Debugger listening on/.test(stderr)) {
        break;
      }
    }
    assert.ok(match);
    child.kill();
  }
}

runTests().then(common.mustCall());
