'use strict';

const {
  markAsUncloneable,
  moveMessagePortToContext,
  receiveMessageOnPort,
} = require('internal/worker/io');

const {
  markAsUntransferable,
  isMarkedAsUntransferable,
} = require('internal/buffer');

module.exports = {
  isMarkedAsUntransferable,
  markAsUncloneable,
  markAsUntransferable,
  moveMessagePortToContext,
  receiveMessageOnPort,
};
