'use strict';
const { ObjectAssign } = primordials;
const { Suite } = require('internal/benchmark/runner');
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('The benchmark module');

ObjectAssign(module.exports, {
  Suite,
});
