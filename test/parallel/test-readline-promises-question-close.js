'use strict';

// Tests that calling rl.close() while a question() promise is pending rejects
// the promise with AbortError instead of leaving it permanently unresolved.

const common = require('../common');
const assert = require('node:assert');
const readline = require('node:readline/promises');
const { Readable } = require('node:stream');
const { EventEmitter } = require('node:events');
const { Writable } = require('node:stream');

class FakeInput extends EventEmitter {
  resume() {}
  pause() {}
  write() {}
  end() {}
}

// Programmatic close() rejects the pending question
{
  const input = new Readable({ read() {} });
  const rl = readline.createInterface({ input, terminal: false });
  assert.rejects(rl.question('prompt: '), { name: 'AbortError' }).then(common.mustCall());
  rl.close();
}

// close() rejects only once when called multiple times
{
  const input = new Readable({ read() {} });
  const rl = readline.createInterface({ input, terminal: false });
  assert.rejects(rl.question('prompt: '), { name: 'AbortError' }).then(common.mustCall());
  rl.close();
  rl.close(); // Second close() should be a no-op
}

// close() does not throw if there is no pending question
{
  const input = new Readable({ read() {} });
  const rl = readline.createInterface({ input, terminal: false });
  rl.close(); // Should not throw
}

// Terminal interface: close() rejects a pending question
{
  const fi = new FakeInput();
  const output = new Writable({ write(chunk, enc, cb) { cb(); } });
  const rl = readline.createInterface({ terminal: true, input: fi, output });
  assert.rejects(rl.question('prompt: '), { name: 'AbortError' }).then(common.mustCall());
  rl.close();
}

// Answering before close() resolves normally; subsequent close() is no-op
{
  const fi = new FakeInput();
  const output = new Writable({ write(chunk, enc, cb) { cb(); } });
  const rl = readline.createInterface({ terminal: true, input: fi, output });
  rl.question('prompt: ').then(common.mustCall((answer) => {
    assert.strictEqual(answer, 'hello');
  }));
  fi.emit('data', 'hello\n');
  // close() with no pending question should not throw
  setImmediate(common.mustCall(() => rl.close()));
}
