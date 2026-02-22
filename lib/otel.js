'use strict';

const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('node:otel');

const {
  start,
  stop,
  isActive,
} = require('internal/otel/core');

module.exports = {
  start,
  stop,
  get active() { return isActive(); },
};
