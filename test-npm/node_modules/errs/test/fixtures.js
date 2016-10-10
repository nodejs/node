/*
 * fixtures.js: Test fixtures for the `errs` module.
 *
 * (C) 2012, Charlie Robbins, Nuno Job, and the Contributors.
 * MIT LICENSE
 *
 */

var util = require('util');

var fixtures = exports;

fixtures.NamedError = function NamedError() {
  this.named = true;
};

util.inherits(fixtures.NamedError, Error);

fixtures.AnError = function AnError() {
  this.named = true;
};

util.inherits(fixtures.AnError, Error);
