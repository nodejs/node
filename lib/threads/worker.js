'use strict';

const {
  getEnvironmentData,
  getPublicWorkerContext,
  isMainThread,
  resourceLimits,
  setEnvironmentData,
  SHARE_ENV,
  threadId,
  threadName,
  Worker,
} = require('internal/worker');

const {
  postMessageToThread,
} = require('internal/worker/messaging');

const { parentPort, workerData } = getPublicWorkerContext();

module.exports = {
  getEnvironmentData,
  isInternalThread: false,
  isMainThread,
  parentPort,
  postMessageToThread,
  resourceLimits,
  setEnvironmentData,
  SHARE_ENV,
  threadId,
  threadName,
  Worker,
  workerData,
};
