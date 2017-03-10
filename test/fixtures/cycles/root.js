const foo = exports.foo = require('./folder/foo');

exports.hello = 'hello';
exports.sayHello = () => foo.hello();
exports.calledFromFoo = () => exports.hello;
