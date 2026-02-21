'use strict';

require('../common');

const { inspect } = require('util');

class FoobarError extends Error {
  status = 'Feeling good';
}

const cause1 = new TypeError('Inner error');
const cause2 = new FoobarError('Individual message', { cause: cause1 });
cause2.extraProperties = 'Yes!';
const cause3 = new Error('Stack causes', { cause: cause2 });

const cause4 = new Error('Number error cause', { cause: 42 });
const cause5 = new Error('Object cause', {
  cause: {
    message: 'Unique',
    name: 'Error',
    stack: 'Error: Unique\n' +
           '    at Module._compile (node:internal/modules/cjs/loader:827:30)',
  },
});
const cause6 = new Error('undefined cause', {
  cause: undefined,
});

console.log(cause4);
console.log(cause5);
console.log(cause6);

process.nextTick(() => {
  const error = new RangeError('New Stack Frames', { cause: cause2 });
  const error2 = new RangeError('New Stack Frames', { cause: cause3 });

  inspect.defaultOptions.colors = true;

  console.log(inspect(error));
  console.log(inspect(cause3));
  console.log(inspect(error2));

  inspect.defaultOptions.colors = false;

  console.log(inspect(error));
  console.log(inspect(cause3));
  console.log(inspect(error2));
});

{
  const error = new Error('cause that throws');
  Reflect.defineProperty(error, 'cause', { get() { throw new Error(); } });
  console.log(inspect(error));
}
