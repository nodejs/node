'use strict';
const { before, beforeEach, after, afterEach, test, suite } = require('node:test');

globalThis.GLOBAL_ORDER = [];

before(function() {
  GLOBAL_ORDER.push(`before one: ${this.name}`);
});

beforeEach(function() {
  GLOBAL_ORDER.push(`beforeEach one: ${this.name}`);
});

after(function() {
  GLOBAL_ORDER.push(`after one: ${this.name}`);
});

afterEach(function() {
  GLOBAL_ORDER.push(`afterEach one: ${this.name}`);
});

suite('suite one', function() {
  GLOBAL_ORDER.push(this.name);

  test('suite one - test', { only: true }, function() {
    GLOBAL_ORDER.push(this.name);
  });
});

test('test one', function() {
  GLOBAL_ORDER.push(this.name);
});
