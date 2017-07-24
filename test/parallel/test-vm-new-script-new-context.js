'use strict';
require('../common');

const assert = require('assert');

const Script = require('vm').Script;

{
  const script = new Script('\'passed\';');
  const result1 = script.runInNewContext();
  const result2 = script.runInNewContext();
  assert.strictEqual('passed', result1);
  assert.strictEqual('passed', result2);
}

{
  const script = new Script('throw new Error(\'test\');');
  assert.throws(() => {
    script.runInNewContext();
  }, /^Error: test$/);
}

{
  const script = new Script('foo.bar = 5;');
  assert.throws(() => {
    script.runInNewContext();
  }, /^ReferenceError: foo is not defined$/);
}

{
  global.hello = 5;
  const script = new Script('hello = 2');
  script.runInNewContext();
  assert.strictEqual(5, global.hello);

  // cleanup
  delete global.hello;
}

{
  global.code = 'foo = 1;' +
                'bar = 2;' +
                'if (baz !== 3) throw new Error(\'test fail\');';
  global.foo = 2;
  global.obj = { foo: 0, baz: 3 };
  const script = new Script(global.code);
  /* eslint-disable no-unused-vars */
  const baz = script.runInNewContext(global.obj);
  /* eslint-enable no-unused-vars */
  assert.strictEqual(1, global.obj.foo);
  assert.strictEqual(2, global.obj.bar);
  assert.strictEqual(2, global.foo);

  //cleanup
  delete global.code;
  delete global.foo;
  delete global.obj;
}

{
  const script = new Script('f()');
  function changeFoo() { global.foo = 100; }
  script.runInNewContext({ f: changeFoo });
  assert.strictEqual(global.foo, 100);

  // cleanup
  delete global.foo;
}

{
  const script = new Script('f.a = 2');
  const f = { a: 1 };
  script.runInNewContext({ f: f });
  assert.strictEqual(f.a, 2);

  assert.throws(() => {
    script.runInNewContext();
  }, /^ReferenceError: f is not defined$/);
}

{
  const script = new Script('');
  assert.throws(() => {
    script.runInNewContext.call('\'hello\';');
  }, /^TypeError: this\.runInContext is not a function$/);
}
