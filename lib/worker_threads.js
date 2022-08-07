'use strict';

const {
  SHARE_ENV,
  Worker,
  getEnvironmentData,
  isMainThread,
  resourceLimits,
  setEnvironmentData,
  threadId
} = require('internal/worker');

const {
  BroadcastChannel,
  MessageChannel,
  MessagePort,
  moveMessagePortToContext,
  receiveMessageOnPort,
} = require('internal/worker/io');

const {
  markAsUntransferable,
} = require('internal/buffer');

module.exports = {
  isMainThread,
  MessagePort,
  MessageChannel,
  markAsUntransferable,
  moveMessagePortToContext,
  receiveMessageOnPort,
  resourceLimits,
  threadId,
  SHARE_ENV,
  Worker,
  parentPort: null,
  workerData: null,
  BroadcastChannel,
  setEnvironmentData,
  getEnvironmentData,
};
