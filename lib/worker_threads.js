'use strict';

const {
  isMainThread,
  SHARE_ENV,
  resourceLimits,
  setEnvironmentData,
  getEnvironmentData,
  threadId,
  Worker
} = require('internal/worker');

const {
  MessagePort,
  MessageChannel,
  moveMessagePortToContext,
  receiveMessageOnPort,
  BroadcastChannel,
} = require('internal/worker/io');

const {
  markAsUntransferable,
} = require('internal/buffer');

const { getOptionValue } = require('internal/options');
const experimentalSynchronousWorker =
   getOptionValue('--experimental-synchronousworker');

const SynchronousWorker = experimentalSynchronousWorker ?
  require('internal/worker/synchronous') : undefined;

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
  SynchronousWorker,
};
