'use strict';

const { suite } = require('node:bench');
const declareChildA = require('./identity-child-a.cjs');
const declareChildB = require('./identity-child-b.cjs');

suite('cross-module suite', () => {
  declareChildA();
  declareChildB();
});
