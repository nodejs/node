'use strict';
require('../common');

// This test ensures that the escapeCodeTimeout option set correctly

const assert = require('assert');
const readline = require('readline');
const EventEmitter = require('events').EventEmitter;
const ESCAPE_CODE_TIMEOUT = 500;

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

{
  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    escapeCodeTimeout: null
  });
  assert.strictEqual(rli.escapeCodeTimeout, ESCAPE_CODE_TIMEOUT);
  rli.close();
}

{
  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    escapeCodeTimeout: {}
  });
  assert.strictEqual(rli.escapeCodeTimeout, ESCAPE_CODE_TIMEOUT);
  rli.close();
}

{
  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    escapeCodeTimeout: NaN
  });
  assert.strictEqual(rli.escapeCodeTimeout, ESCAPE_CODE_TIMEOUT);
  rli.close();
}

{
  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    escapeCodeTimeout: '50'
  });
  assert.strictEqual(rli.escapeCodeTimeout, ESCAPE_CODE_TIMEOUT);
  rli.close();
}
