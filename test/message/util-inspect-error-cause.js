'use strict';

require('../common');

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

  console.log(error);
  console.log(cause3);
  console.log(error2);
});
