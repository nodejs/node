'use strict';
const common = require('../common');

// This test ensures that the escapeCodeTimeout option set correctly

const assert = require('assert');
const readline = require('readline');
const EventEmitter = require('events').EventEmitter;

class FakeInput extends EventEmitter {
  resume() {}
  pause() {}
  write() {}
  end() {}
}

{
  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    escapeCodeTimeout: 50
  });
  assert.strictEqual(rli.escapeCodeTimeout, 50);
  rli.close();
}

[
  null,
  {},
  NaN,
  '50'
].forEach((invalidInput) => {
  common.expectsError(() => {
    const fi = new FakeInput();
    const rli = new readline.Interface({
      input: fi,
      output: fi,
      escapeCodeTimeout: invalidInput
    });
    rli.close();
  }, {
    type: TypeError,
    code: 'ERR_INVALID_OPT_VALUE'
  });
});
