'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');

emitExperimentalWarning('validator');

const { Schema } = require('internal/validator/schema');
const { codes } = require('internal/validator/errors');

module.exports = {
  Schema,
  codes,
};
