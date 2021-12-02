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
