'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');

emitExperimentalWarning('fetch/headers');

const {
  Headers
} = require('internal/fetch/headers');

module.exports = {
  Headers
};
