require('../common');
const assert = require('assert');

var once = 0;

process.on('beforeExit', () => {
  setTimeout(() => {}, 1).unref();
  once++;
});

process.on('exit', (code) => {
  if (code !== 0) return;

  assert.strictEqual(once, 1);
});
