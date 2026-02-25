'use strict';

const {
    PromisePrototypeThen
} = primordials;

const timers = require('timers');

const keepAliveInterval = 2 ** 31 - 1;

let pendingWaiters = 0;
let keepAliveHandle = null;

function maybeStopKeepAlive() {
    if (--pendingWaiters === 0) {
        timers.clearInterval(keepAliveHandle);
        keepAliveHandle = null;
    }
}

function trackWaitAsyncResult(result) {
    if (!result.async) {
        return result;
    }

    if (++pendingWaiters === 1) {
        keepAliveHandle = timers.setInterval(() => {}, keepAliveInterval);
    }

    PromisePrototypeThen(result.value, maybeStopKeepAlive, maybeStopKeepAlive);
    return result;
}

module.exports = {
    trackWaitAsyncResult,
};
