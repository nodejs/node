'use strict'

const { addAbortListener } = require('../core/util')
const { RequestAbortedError } = require('../core/errors')
const { kQueuedAbort } = require('../core/symbols')

const kListener = Symbol('kListener')
const kSignal = Symbol('kSignal')

function abort (self) {
  // Keep the reason even when abort is set: a queued request that cannot be
  // taken out of its queue is aborted once it starts.
  self.reason = self[kSignal]?.reason ?? new RequestAbortedError()
  if (self.abort) {
    self.abort(self.reason)
  }
  removeSignal(self)
}

function addSignal (self, signal) {
  self.reason = null

  self[kSignal] = null
  self[kListener] = null

  if (!signal) {
    return
  }

  if (signal.aborted) {
    self.reason = signal.reason ?? new RequestAbortedError()
    return
  }

  self[kSignal] = signal
  self[kListener] = () => {
    abort(self)
  }

  addAbortListener(self[kSignal], self[kListener])
}

function removeSignal (self) {
  if (!self[kSignal]) {
    return
  }

  if ('removeEventListener' in self[kSignal]) {
    self[kSignal].removeEventListener('abort', self[kListener])
  } else {
    self[kSignal].removeListener('abort', self[kListener])
  }

  self[kSignal] = null
  self[kListener] = null
}

// Dispatchers call the kQueuedAbort hook while a request waits for a
// connection, with a function that takes it out of their queue. Until the
// request starts, the handler's abort calls that function.
//
// A retry or a redirect dispatches the same handler again after an earlier
// attempt started. The handler's abort then also calls the controller of that
// attempt: it does nothing once the attempt is over, and still reaches a
// response that a retry resumes.
function queuedAbortHook (handler) {
  // The abort set by onRequestStart, and the one this hook installed last.
  let started = null
  let installed = null
  return (cancel) => {
    if (handler.reason) {
      cancel(handler.reason)
      return
    }

    if (handler.abort !== installed) {
      started = handler.abort
    }

    if (started === null) {
      installed = cancel
    } else {
      const abortStarted = started
      installed = (reason) => {
        abortStarted(reason)
        cancel(reason)
      }
    }
    handler.abort = installed
  }
}

// Dispatches a request whose options carry a signal. Requests without a signal
// are dispatched directly and never reach this function.
function dispatchWithSignal (dispatcher, opts, handler) {
  if (handler.reason) {
    // Already aborted: fail without dispatching, so no connection is opened.
    handler.onResponseError(null, handler.reason)
    return
  }

  dispatcher.dispatch({ ...opts, [kQueuedAbort]: queuedAbortHook(handler) }, handler)
}

module.exports = {
  addSignal,
  removeSignal,
  dispatchWithSignal
}
