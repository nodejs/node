var c = require('./b/c');

console.error('load fixtures/a.js');

var string = 'A';

exports.SomeClass = c.SomeClass;

exports.A = function() {
  return string;
};

exports.C = function() {
  return c.C();
};

exports.D = function() {
  return c.D();
};

exports.number = 42;

process.on('exit', function() {
  string = 'A done';
});
