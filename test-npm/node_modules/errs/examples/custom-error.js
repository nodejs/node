var util = require('util'),
    errs = require('../lib/errs');

function MyError () {
  this.message = 'This is my error; I made it myself. It has a transparent stack trace.';
}

//
// Alternatively `MyError.prototype.__proto__ = Error;`
//
util.inherits(MyError, Error);

//
// Register the error type
//
errs.register('myerror', MyError);

console.log(
  errs.create('myerror')
    .stack
    .split('\n')
);