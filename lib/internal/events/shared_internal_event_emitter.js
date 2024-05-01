'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  Error,
  ErrorCaptureStackTrace,
  FunctionPrototypeBind,
  ObjectDefineProperty,
  StringPrototypeSplit,
} = primordials;

const { kCapture, kRejection } = require('internal/events/symbols');
const { inspect, identicalSequenceRange } = require('internal/util/inspect');

const {
  codes: {
    ERR_UNHANDLED_ERROR,
  },
  kEnhanceStackBeforeInspector,
} = require('internal/errors');

let EventEmitter;

function arrayClone(arr) {
  // At least since V8 8.3, this implementation is faster than the previous
  // which always used a simple for-loop
  switch (arr.length) {
    case 2: return [arr[0], arr[1]];
    case 3: return [arr[0], arr[1], arr[2]];
    case 4: return [arr[0], arr[1], arr[2], arr[3]];
    case 5: return [arr[0], arr[1], arr[2], arr[3], arr[4]];
    case 6: return [arr[0], arr[1], arr[2], arr[3], arr[4], arr[5]];
  }
  return ArrayPrototypeSlice(arr);
}


function addCatch(that, promise, type, args) {
  if (!that[kCapture]) {
    return;
  }

  // Handle Promises/A+ spec, then could be a getter
  // that throws on second use.
  try {
    const then = promise.then;

    if (typeof then === 'function') {
      then.call(promise, undefined, function(err) {
        // The callback is called with nextTick to avoid a follow-up
        // rejection from this promise.
        process.nextTick(emitUnhandledRejectionOrErr, that, err, type, args);
      });
    }
  } catch (err) {
    that.emit('error', err);
  }
}

function emitUnhandledRejectionOrErr(ee, err, type, args) {
  if (typeof ee.eventEmitterTranslationLayer[kRejection] === 'function') {
    ee.eventEmitterTranslationLayer[kRejection](err, type, ...args);
  } else {
    // We have to disable the capture rejections mechanism, otherwise
    // we might end up in an infinite loop.
    const prev = ee.eventEmitterTranslationLayer[kCapture];

    // If the error handler throws, it is not catchable and it
    // will end up in 'uncaughtException'. We restore the previous
    // value of kCapture in case the uncaughtException is present
    // and the exception is handled.
    try {
      ee.eventEmitterTranslationLayer[kCapture] = false;
      ee.emit('error', err);
    } finally {
      ee.eventEmitterTranslationLayer[kCapture] = prev;
    }
  }
}


function throwErrorOnMissingErrorHandler(...args) {
  let er;
  if (args.length > 0)
    er = args[0];

  if (er instanceof Error) {
    try {
      const capture = {};
      // TODO
      EventEmitter ??= require('internal/events/event_emitter').EventEmitter;
      ErrorCaptureStackTrace(capture, EventEmitter.prototype.emit);
      ObjectDefineProperty(er, kEnhanceStackBeforeInspector, {
        __proto__: null,
        value: FunctionPrototypeBind(enhanceStackTrace, this, er, capture),
        configurable: true,
      });
    } catch {
      // Continue regardless of error.
    }

    // Note: The comments on the `throw` lines are intentional, they show
    // up in Node's output if this results in an unhandled exception.
    throw er; // Unhandled 'error' event
  }

  let stringifiedEr;
  try {
    stringifiedEr = inspect(er);
  } catch {
    stringifiedEr = er;
  }

  // At least give some kind of context to the user
  const err = new ERR_UNHANDLED_ERROR(stringifiedEr);
  err.context = er;
  throw err; // Unhandled 'error' event
}


function enhanceStackTrace(err, own) {
  let ctorInfo = '';
  try {
    const { name } = this.constructor;
    if (name !== 'EventEmitter')
      ctorInfo = ` on ${name} instance`;
  } catch {
    // Continue regardless of error.
  }
  const sep = `\nEmitted 'error' event${ctorInfo} at:\n`;

  const errStack = ArrayPrototypeSlice(
    StringPrototypeSplit(err.stack, '\n'), 1);
  const ownStack = ArrayPrototypeSlice(
    StringPrototypeSplit(own.stack, '\n'), 1);

  const { len, offset } = identicalSequenceRange(ownStack, errStack);
  if (len > 0) {
    ArrayPrototypeSplice(ownStack, offset + 1, len - 2,
                         '    [... lines matching original stack trace ...]');
  }

  return err.stack + sep + ArrayPrototypeJoin(ownStack, '\n');
}

module.exports = {
  arrayClone,
  addCatch,
  throwErrorOnMissingErrorHandler,
};
