// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

// vm.runInNewContext
{
  const sandbox = {};
  const result = vm.runInNewContext(
    'foo = "bar"; this.typeofProcess = typeof process; typeof Object;',
    sandbox
  );
  assert.deepStrictEqual(sandbox, {
    foo: 'bar',
    typeofProcess: 'undefined',
  });
  assert.strictEqual(result, 'function');
}

// vm.runInContext
{
  const sandbox = { foo: 'bar' };
  const context = vm.createContext(sandbox);
  const result = vm.runInContext(
    'baz = foo; this.typeofProcess = typeof process; typeof Object;',
    context
  );
  assert.deepStrictEqual(sandbox, {
    foo: 'bar',
    baz: 'bar',
    typeofProcess: 'undefined'
  });
  assert.strictEqual(result, 'function');
}

// vm.runInThisContext
{
  const result = vm.runInThisContext(
    'vmResult = "foo"; Object.prototype.toString.call(process);'
  );
  assert.strictEqual(global.vmResult, 'foo');
  assert.strictEqual(result, '[object process]');
  delete global.vmResult;
}

// vm.runInNewContext
{
  const result = vm.runInNewContext(
    'vmResult = "foo"; typeof process;'
  );
  assert.strictEqual(global.vmResult, undefined);
  assert.strictEqual(result, 'undefined');
}

// vm.createContext
{
  const sandbox = {};
  const context = vm.createContext(sandbox);
  assert.strictEqual(sandbox, context);
}

// Run script with filename
{
  const script = 'throw new Error("boom")';
  const filename = 'test-boom-error';
  const context = vm.createContext();

  function checkErr(err) {
    return err.stack.startsWith('test-boom-error:1');
  }

  assert.throws(() => vm.runInContext(script, context, filename), checkErr);
  assert.throws(() => vm.runInNewContext(script, context, filename), checkErr);
  assert.throws(() => vm.runInThisContext(script, filename), checkErr);
}

// Invalid arguments
[null, 'string'].forEach((input) => {
  common.expectsError(() => {
    vm.createContext({}, input);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options" argument must be of type Object. ' +
             `Received type ${typeof input}`
  });
});

['name', 'origin'].forEach((propertyName) => {
  common.expectsError(() => {
    vm.createContext({}, { [propertyName]: null });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: `The "options.${propertyName}" property must be of type string. ` +
             'Received type object'
  });
});

['contextName', 'contextOrigin'].forEach((propertyName) => {
  common.expectsError(() => {
    vm.runInNewContext('', {}, { [propertyName]: null });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: `The "options.${propertyName}" property must be of type string. ` +
             'Received type object'
  });
});
