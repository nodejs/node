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

// vm.compileFunction
{
  assert.strictEqual(
    vm.compileFunction('console.log("Hello, World!")').toString(),
    'function () {\nconsole.log("Hello, World!")\n}'
  );

  assert.strictEqual(
    vm.compileFunction(
      'return p + q + r + s + t',
      ['p', 'q', 'r', 's', 't']
    )('ab', 'cd', 'ef', 'gh', 'ij'),
    'abcdefghij'
  );

  vm.compileFunction('return'); // Should not throw on 'return'

  common.expectsError(() => {
    vm.compileFunction(
      '});\n\n(function() {\nconsole.log(1);\n})();\n\n(function() {'
    );
  }, {
    type: SyntaxError,
    message: 'Unexpected token }'
  });

  // Tests for failed argument validation
  common.expectsError(() => vm.compileFunction(), {
    type: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "code" argument must be of type string. ' +
      'Received type undefined'
  });

  vm.compileFunction(''); // Should pass without params or options

  common.expectsError(() => vm.compileFunction('', null), {
    type: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "params" argument must be of type Array. ' +
      'Received type object'
  });

  // vm.compileFunction('', undefined, null);

  const optionTypes = {
    'filename': 'string',
    'columnOffset': 'number',
    'lineOffset': 'number',
    'cachedData': 'Buffer, TypedArray, or DataView',
    'produceCachedData': 'boolean',
  };

  for (const option in optionTypes) {
    const typeErrorMessage = `The "options.${option}" property must be ` +
       `${option === 'cachedData' ? 'one of' : 'of'} type`;
    common.expectsError(() => {
      vm.compileFunction('', undefined, { [option]: null });
    }, {
      type: TypeError,
      code: 'ERR_INVALID_ARG_TYPE',
      message: typeErrorMessage +
        ` ${optionTypes[option]}. Received type object`
    });
  }

  // Testing for context-based failures
  [Boolean(), Number(), null, String(), Symbol(), {}].forEach(
    (value) => {
      common.expectsError(() => {
        vm.compileFunction('', undefined, { parsingContext: value });
      }, {
        type: TypeError,
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "options.parsingContext" property must be of type ' +
          `Context. Received type ${typeof value}`
      });
    }
  );

  // Testing for non Array type-based failures
  [Boolean(), Number(), null, Object(), Symbol(), {}].forEach(
    (value) => {
      common.expectsError(() => {
        vm.compileFunction('', value);
      }, {
        type: TypeError,
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "params" argument must be of type Array. ' +
          `Received type ${typeof value}`
      });
    }
  );

  assert.strictEqual(
    vm.compileFunction(
      'return a;',
      undefined,
      { contextExtensions: [{ a: 5 }] }
    )(),
    5
  );

  common.expectsError(() => {
    vm.compileFunction('', undefined, { contextExtensions: null });
  }, {
    type: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options.contextExtensions" property must be of type Array' +
       '. Received type object'
  });

  common.expectsError(() => {
    vm.compileFunction('', undefined, { contextExtensions: [0] });
  }, {
    type: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options.contextExtensions[0]" property must be of type ' +
       'object. Received type number'
  });

  const oldLimit = Error.stackTraceLimit;
  // Setting value to run the last three tests
  Error.stackTraceLimit = 1;

  common.expectsError(() => {
    vm.compileFunction('throw new Error("Sample Error")')();
  }, {
    message: 'Sample Error',
    stack: 'Error: Sample Error\n    at <anonymous>:1:7'
  });

  common.expectsError(() => {
    vm.compileFunction(
      'throw new Error("Sample Error")',
      [],
      { lineOffset: 3 }
    )();
  }, {
    message: 'Sample Error',
    stack: 'Error: Sample Error\n    at <anonymous>:4:7'
  });

  common.expectsError(() => {
    vm.compileFunction(
      'throw new Error("Sample Error")',
      [],
      { columnOffset: 3 }
    )();
  }, {
    message: 'Sample Error',
    stack: 'Error: Sample Error\n    at <anonymous>:1:10'
  });

  assert.strictEqual(
    vm.compileFunction(
      'return varInContext',
      [],
      {
        parsingContext: vm.createContext({ varInContext: 'abc' })
      }
    )(),
    'abc'
  );

  common.expectsError(() => {
    vm.compileFunction(
      'return varInContext',
      []
    )();
  }, {
    message: 'varInContext is not defined',
    stack: 'ReferenceError: varInContext is not defined\n    at <anonymous>:1:1'
  });

  assert.notDeepStrictEqual(
    vm.compileFunction(
      'return global',
      [],
      {
        parsingContext: vm.createContext({ global: {} })
      }
    )(),
    global
  );

  assert.deepStrictEqual(
    vm.compileFunction(
      'return global',
      []
    )(),
    global
  );

  // Resetting value
  Error.stackTraceLimit = oldLimit;
}
