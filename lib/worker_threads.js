'use strict';

const {
  isMainThread,
  MessagePort,
  MessageChannel,
  threadId,
  Worker,
  LockManager,
} = require('internal/worker');

module.exports = {
  isMainThread,
  MessagePort,
  MessageChannel,
  threadId,
  Worker,
  parentPort: null,
  locks: Object.create(LockManager.prototype),
};
