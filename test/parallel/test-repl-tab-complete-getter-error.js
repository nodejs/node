'use strict';

const common = require('../common');
const repl = require('repl');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');

(async function() {
  await runTest();
})().then(common.mustCall());

async function runTest() {
  const input = new ArrayStream();
  const output = new ArrayStream();

  const replServer = repl.start({
    prompt: '',
    input,
    output: output,
    allowBlockingCompletions: true,
    terminal: true
  });

  replServer._domain.on('error', (e) => {
    assert.fail(`Error in REPL domain: ${e}`);
  });
  await new Promise((resolve, reject) => {
    replServer.eval(`
class Person1 {
  constructor(name) { this.name = name; }
  get name() { throw new Error(); } set name(value) { this._name = value; }
};
const foo = new Person1("Alice")
      `, replServer.context, '', (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
  replServer.complete(
    'foo.name.',
    common.mustCall((error, data) => {
      assert.strictEqual(error, null);
      assert.strictEqual(data.length, 2);
      assert.strictEqual(data[1], 'foo.name.');
    })
  );

  replServer.complete(
    'foo["name"].',
    common.mustCall((error, data) => {
      assert.strictEqual(error, null);
      assert.strictEqual(data.length, 2);
      assert.strictEqual(data[1], 'foo["name"].');
    })
  );
  await new Promise((resolve, reject) => {
    replServer.eval(`
function getNameText() {
  return "name";
}
      `, replServer.context, '', (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
  replServer.complete(
    'foo[getNameText()].',
    common.mustCall((error, data) => {
      assert.strictEqual(error, null);
      assert.strictEqual(data.length, 2);
      assert.strictEqual(data[1], 'foo[getNameText()].');
    })
  );
}
