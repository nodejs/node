'use strict';

const data = { type: 'cjs-object' };

function sum(a, b) {
  return a + b;
}

function getData() {
  return data;
}

module.exports = { sum, getData };
