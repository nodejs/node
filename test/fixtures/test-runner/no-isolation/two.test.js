'use strict';
const { before, beforeEach, after, afterEach, test, suite } = require('node:test');

before(function() {
  GLOBAL_ORDER.push(`before two: ${this.name}`);
});

beforeEach(function() {
  GLOBAL_ORDER.push(`beforeEach two: ${this.name}`);
});

after(function() {
  GLOBAL_ORDER.push(`after two: ${this.name}`);
});

afterEach(function() {
  GLOBAL_ORDER.push(`afterEach two: ${this.name}`);
});

suite('suite two', function() {
  GLOBAL_ORDER.push(this.name);

  before(function() {
    GLOBAL_ORDER.push(`before suite two: ${this.name}`);
  });

  test('suite two - test', { only: true }, function() {
    GLOBAL_ORDER.push(this.name);
  });
});
