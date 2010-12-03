var c = require('./b/c');

common.debug('load fixtures/a.js');

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

process.addListener('exit', function() {
  string = 'A done';
});
