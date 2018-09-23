'use strict';

const {
  isMainThread,
  MessagePort,
  MessageChannel,
  threadId,
  Worker
} = require('internal/worker');

module.exports = {
  isMainThread,
  MessagePort,
  MessageChannel,
  threadId,
  Worker,
  parentPort: null
};
