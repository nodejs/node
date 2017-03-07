const foo = exports.foo = require('./folder/foo');

exports.hello = 'hello';
exports.sayHello = function sayHello() {
  return foo.hello();
};
exports.calledFromFoo = function calledFromFoo() {
  return exports.hello;
};
