// Flags: --disable-warning=ExperimentalWarning
'use strict';

// URLPattern and URLPatternList work in a worker, which has its own isolate
// and its own copy of the URLPattern constructor template.

const common = require('../common');
const assert = require('assert');
const { URLPattern, URLPatternList } = require('url');
const { Worker } = require('worker_threads');

const list = new URLPatternList([new URLPattern({ pathname: '/main' })]);
assert.strictEqual(list.test({ pathname: '/main' }), true);

const worker = new Worker(`
  const assert = require('assert');
  const { parentPort } = require('worker_threads');
  const { URLPattern, URLPatternList } = require('url');

  const users = new URLPattern({ pathname: '/users/:id' });
  const list = new URLPatternList([users]);
  const result = list.exec('https://example.com/users/42');
  assert.strictEqual(result.pattern, users);
  assert.throws(() => new URLPatternList([{ __proto__: URLPattern.prototype }]),
                { code: 'ERR_INVALID_ARG_TYPE' });
  parentPort.postMessage(result.pathname.groups.id);
`, { eval: true, execArgv: ['--disable-warning=ExperimentalWarning'] });

worker.on('message', common.mustCall((id) => {
  assert.strictEqual(id, '42');
}));
worker.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
