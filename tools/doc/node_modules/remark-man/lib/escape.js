'use strict';

/* Dependencies. */
var groff = require('groff-escape');

/* Expose. */
module.exports = escape;

var escapes = {
  '\\': 'rs',
  '[': 'lB',
  ']': 'rB'
};

/* Construct. */
var expression = init();

/* Escape a value for roff output. */
function escape(value) {
  return value.replace(expression, function ($0) {
    return '\\[' + escapes[$0] + ']';
  });
}

/* Create a regex from the escapes. */
function init() {
  var keys = ['\\\\', '\\[', '\\]'];
  var key;

  for (key in groff) {
    keys.push(key);
    escapes[key] = groff[key];
  }

  return new RegExp(keys.sort(longest).join('|'), 'g');
}

/** Longest first. */
function longest(a, b) {
  return a.length > b.length ? -1 : 1;
}
