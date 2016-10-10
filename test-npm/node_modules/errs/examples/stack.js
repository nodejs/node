var errs = require('../lib/errs');

console.log(
  errs.create('This is an error. There are many like it. It has a transparent stack trace.')
    .stack
    .split('\n')
);