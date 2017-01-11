const c = require('./b/c');

console.error('load fixtures/a.js');

let string = 'A';

exports.SomeClass = c.SomeClass;

exports.A = () => string;

exports.C = () => c.C();

exports.D = () => c.D();

exports.number = 42;

process.on('exit', () => {
  string = 'A done';
});
