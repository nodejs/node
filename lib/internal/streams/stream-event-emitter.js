const EE = require('events');
const {_onceWrap, addCatch, arrayClone, unwrapListeners} = require('internal/util');

const {kEnhanceStackBeforeInspector} = require("internal/errors");
const {inspect} = require("internal/util/inspect");
const {Stream} = require("internal/streams/legacy");


// close: undefined,
// error: undefined,
// data: undefined,
// end: undefined,
// readable: undefined,

// pause: undefined,
// resume: undefined,
// pipe: undefined,
// unpipe: undefined,
// [destroyImpl.kConstruct]: undefined,
// [destroyImpl.kDestroy]: undefined,

// EventEmitter.prototype.emit = function emit(type, ...args) {
//   let doError = (type === 'error');
//
//   const events = this._events;
//   if (events !== undefined) {
//     if (doError && events[kErrorMonitor] !== undefined)
//       this.emit(kErrorMonitor, ...args);
//     doError = (doError && events.error === undefined);
//   } else if (!doError)
//     return false;
//
//   // If there is no 'error' event listener then throw.
//   if (doError) {
//     let er;
//     if (args.length > 0)
//       er = args[0];
//     if (er instanceof Error) {
//       try {
//         const capture = {};
//         ErrorCaptureStackTrace(capture, EventEmitter.prototype.emit);
//         ObjectDefineProperty(er, kEnhanceStackBeforeInspector, {
//           __proto__: null,
//           value: FunctionPrototypeBind(enhanceStackTrace, this, er, capture),
//           configurable: true,
//         });
//       } catch {
//         // Continue regardless of error.
//       }
//
//       // Note: The comments on the `throw` lines are intentional, they show
//       // up in Node's output if this results in an unhandled exception.
//       throw er; // Unhandled 'error' event
//     }
//
//     let stringifiedEr;
//     try {
//       stringifiedEr = inspect(er);
//     } catch {
//       stringifiedEr = er;
//     }
//
//     // At least give some kind of context to the user
//     const err = new ERR_UNHANDLED_ERROR(stringifiedEr);
//     err.context = er;
//     throw err; // Unhandled 'error' event
//   }
//
//   const handler = events[type];
//
//   if (handler === undefined)
//     return false;
//
//   if (typeof handler === 'function') {
//     const result = handler.apply(this, args);
//
//     // We check if result is undefined first because that
//     // is the most common case so we do not pay any perf
//     // penalty
//     if (result !== undefined && result !== null) {
//       addCatch(this, result, type, args);
//     }
//   } else {
//     const len = handler.length;
//     const listeners = arrayClone(handler);
//     for (let i = 0; i < len; ++i) {
//       const result = listeners[i].apply(this, args);
//
//       // We check if result is undefined first because that
//       // is the most common case so we do not pay any perf
//       // penalty.
//       // This code is duplicated because extracting it away
//       // would make it non-inlineable.
//       if (result !== undefined && result !== null) {
//         addCatch(this, result, type, args);
//       }
//     }
//   }
//
//   return true;
// };


function StreamEventEmitter(opts) {
  EE.call(this, opts);
}
Object.setPrototypeOf(StreamEventEmitter.prototype, EE.prototype);
Object.setPrototypeOf(StreamEventEmitter, EE);


StreamEventEmitter.prototype.closeListeners = [];
StreamEventEmitter.prototype.dataListeners = [];
StreamEventEmitter.prototype.endListeners = [];
StreamEventEmitter.prototype.readableListeners = [];
StreamEventEmitter.prototype.pauseListeners = [];
StreamEventEmitter.prototype.resumeListeners = [];
StreamEventEmitter.prototype.pipeListeners = [];
StreamEventEmitter.prototype.unpipeListeners = [];
StreamEventEmitter.prototype.preFinishListeners = [];
StreamEventEmitter.prototype.finishListeners = [];
StreamEventEmitter.prototype.drainListeners = [];
StreamEventEmitter.prototype.destroyImplConstructListeners = [];
StreamEventEmitter.prototype.destroyImplDestroyListeners = [];




// ---- close ----

StreamEventEmitter.prototype.prependClose = function(listener) {
  // TODO - emit listener added
  this.closeListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onClose = function(listener) {
  // TODO - emit listener added
  this.closeListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceClose = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onClose(_onceWrap(this, "close", listener));
  return this;
};

StreamEventEmitter.prototype.prependCloseOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependClose(_onceWrap(this, "close", listener));
  return this;
}

StreamEventEmitter.prototype.offClose = function(listener) {
  // TODO - emit listener removed
  const list = this.closeListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitClose = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.closeListeners.length) return false;

  const len = this.closeListeners.length;
  const listeners = arrayClone(this.closeListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "close", args);
    }
  }

  return true;
}

// ---- close ----



// ---- data ----

StreamEventEmitter.prototype.prependData = function(listener) {
  // TODO - emit listener added
  this.dataListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onData = function(listener) {
  // TODO - emit listener added
  this.dataListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceData = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onData(_onceWrap(this, "data", listener));
  return this;
};

StreamEventEmitter.prototype.prependDataOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependData(_onceWrap(this, "data", listener));
  return this;
}

StreamEventEmitter.prototype.offData = function(listener) {
  // TODO - emit listener removed
  const list = this.dataListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitData = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.dataListeners.length) return false;

  const len = this.dataListeners.length;
  const listeners = arrayClone(this.dataListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "data", args);
    }
  }

  return true;
}

// ---- data ----



// ---- end ----

StreamEventEmitter.prototype.prependEnd = function(listener) {
  // TODO - emit listener added
  this.endListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onEnd = function(listener) {
  // TODO - emit listener added
  this.endListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceEnd = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onEnd(_onceWrap(this, "end", listener));
  return this;
};

StreamEventEmitter.prototype.prependEndOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependEnd(_onceWrap(this, "end", listener));
  return this;
}

StreamEventEmitter.prototype.offEnd = function(listener) {
  // TODO - emit listener removed
  const list = this.endListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitEnd = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.endListeners.length) return false;

  const len = this.endListeners.length;
  const listeners = arrayClone(this.endListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "end", args);
    }
  }

  return true;
}

// ---- end ----



// ---- readable ----

StreamEventEmitter.prototype.prependReadable = function(listener) {
  // TODO - emit listener added
  this.readableListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onReadable = function(listener) {
  // TODO - emit listener added
  this.readableListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceReadable = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onReadable(_onceWrap(this, "readable", listener));
  return this;
};

StreamEventEmitter.prototype.prependReadableOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependReadable(_onceWrap(this, "readable", listener));
  return this;
}

StreamEventEmitter.prototype.offReadable = function(listener) {
  // TODO - emit listener removed
  const list = this.readableListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitReadable = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.readableListeners.length) return false;

  const len = this.readableListeners.length;
  const listeners = arrayClone(this.readableListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "readable", args);
    }
  }

  return true;
}

// ---- readable ----



// ---- pause ----

StreamEventEmitter.prototype.prependPause = function(listener) {
  // TODO - emit listener added
  this.pauseListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onPause = function(listener) {
  // TODO - emit listener added
  this.pauseListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.oncePause = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onPause(_onceWrap(this, "pause", listener));
  return this;
};

StreamEventEmitter.prototype.prependPauseOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependPause(_onceWrap(this, "pause", listener));
  return this;
}

StreamEventEmitter.prototype.offPause = function(listener) {
  // TODO - emit listener removed
  const list = this.pauseListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitPause = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.pauseListeners.length) return false;

  const len = this.pauseListeners.length;
  const listeners = arrayClone(this.pauseListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "pause", args);
    }
  }

  return true;
}

// ---- pause ----



// ---- resume ----

StreamEventEmitter.prototype.prependResume = function(listener) {
  // TODO - emit listener added
  this.resumeListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onResume = function(listener) {
  // TODO - emit listener added
  this.resumeListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceResume = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onResume(_onceWrap(this, "resume", listener));
  return this;
};

StreamEventEmitter.prototype.prependResumeOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependResume(_onceWrap(this, "resume", listener));
  return this;
}

StreamEventEmitter.prototype.offResume = function(listener) {
  // TODO - emit listener removed
  const list = this.resumeListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitResume = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.resumeListeners.length) return false;

  const len = this.resumeListeners.length;
  const listeners = arrayClone(this.resumeListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "resume", args);
    }
  }

  return true;
}

// ---- resume ----




// ---- pipe ----

StreamEventEmitter.prototype.prependPipe = function(listener) {
  // TODO - emit listener added
  this.pipeListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onPipe = function(listener) {
  // TODO - emit listener added
  this.pipeListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.oncePipe = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onPipe(_onceWrap(this, "pipe", listener));
  return this;
};

StreamEventEmitter.prototype.prependPipeOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependPipe(_onceWrap(this, "pipe", listener));
  return this;
}

StreamEventEmitter.prototype.offPipe = function(listener) {
  // TODO - emit listener removed
  const list = this.pipeListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitPipe = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.pipeListeners.length) return false;

  const len = this.pipeListeners.length;
  const listeners = arrayClone(this.pipeListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "pipe", args);
    }
  }

  return true;
}

// ---- pipe ----



// ---- unpipe ----

StreamEventEmitter.prototype.prependUnpipe = function(listener) {
  // TODO - emit listener added
  this.unpipeListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onUnpipe = function(listener) {
  // TODO - emit listener added
  this.unpipeListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceUnpipe = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onUnpipe(_onceWrap(this, "unpipe", listener));
  return this;
};

StreamEventEmitter.prototype.prependUnpipeOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependUnpipe(_onceWrap(this, "unpipe", listener));
  return this;
}

StreamEventEmitter.prototype.offUnpipe = function(listener) {
  // TODO - emit listener removed
  const list = this.unpipeListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitUnpipe = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.unpipeListeners.length) return false;

  const len = this.unpipeListeners.length;
  const listeners = arrayClone(this.unpipeListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "unpipe", args);
    }
  }

  return true;
}

// ---- unpipe ----



// ---- preFinish ----

StreamEventEmitter.prototype.prependPreFinish = function(listener) {
  // TODO - emit listener added
  this.preFinishListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onPreFinish = function(listener) {
  // TODO - emit listener added
  this.preFinishListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.oncePreFinish = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onPreFinish(_onceWrap(this, "preFinish", listener));
  return this;
};

StreamEventEmitter.prototype.prependPreFinishOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependPreFinish(_onceWrap(this, "preFinish", listener));
  return this;
}

StreamEventEmitter.prototype.offPreFinish = function(listener) {
  // TODO - emit listener removed
  const list = this.preFinishListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitPreFinish = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.preFinishListeners.length) return false;

  const len = this.preFinishListeners.length;
  const listeners = arrayClone(this.preFinishListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "preFinish", args);
    }
  }

  return true;
}

// ---- preFinish ----



// ---- finish ----

StreamEventEmitter.prototype.prependFinish = function(listener) {
  // TODO - emit listener added
  this.finishListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onFinish = function(listener) {
  // TODO - emit listener added
  this.finishListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceFinish = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onFinish(_onceWrap(this, "finish", listener));
  return this;
};

StreamEventEmitter.prototype.prependFinishOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependFinish(_onceWrap(this, "finish", listener));
  return this;
}

StreamEventEmitter.prototype.offFinish = function(listener) {
  // TODO - emit listener removed
  const list = this.finishListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitFinish = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.finishListeners.length) return false;

  const len = this.finishListeners.length;
  const listeners = arrayClone(this.finishListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "finish", args);
    }
  }

  return true;
}

// ---- finish ----



// ---- drain ----

StreamEventEmitter.prototype.prependDrain = function(listener) {
  // TODO - emit listener added
  this.drainListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onDrain = function(listener) {
  // TODO - emit listener added
  this.drainListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceDrain = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onDrain(_onceWrap(this, "drain", listener));
  return this;
};

StreamEventEmitter.prototype.prependDrainOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependDrain(_onceWrap(this, "drain", listener));
  return this;
}

StreamEventEmitter.prototype.offDrain = function(listener) {
  // TODO - emit listener removed
  const list = this.drainListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitDrain = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.drainListeners.length) return false;

  const len = this.drainListeners.length;
  const listeners = arrayClone(this.drainListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "drain", args);
    }
  }

  return true;
}

// ---- drain ----



// ---- destroyImplConstruct ----

StreamEventEmitter.prototype.prependDestroyImplConstruct = function(listener) {
  // TODO - emit listener added
  this.destroyImplConstructListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onDestroyImplConstruct = function(listener) {
  // TODO - emit listener added
  this.destroyImplConstructListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceDestroyImplConstruct = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onDestroyImplConstruct(_onceWrap(this, "destroyImplConstruct", listener));
  return this;
};

StreamEventEmitter.prototype.prependDestroyImplConstructOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependDestroyImplConstruct(_onceWrap(this, "destroyImplConstruct", listener));
  return this;
}

StreamEventEmitter.prototype.offDestroyImplConstruct = function(listener) {
  // TODO - emit listener removed
  const list = this.destroyImplConstructListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitDestroyImplConstruct = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.destroyImplConstructListeners.length) return false;

  const len = this.destroyImplConstructListeners.length;
  const listeners = arrayClone(this.destroyImplConstructListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "destroyImplConstruct", args);
    }
  }

  return true;
}

// ---- destroyImplConstruct ----



// ---- destroyImplDestroy ----

StreamEventEmitter.prototype.prependDestroyImplDestroy = function(listener) {
  // TODO - emit listener added
  this.destroyImplDestroyListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onDestroyImplDestroy = function(listener) {
  // TODO - emit listener added
  this.destroyImplDestroyListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.onceDestroyImplDestroy = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onDestroyImplDestroy(_onceWrap(this, "destroyImplDestroy", listener));
  return this;
};

StreamEventEmitter.prototype.prependDestroyImplDestroyOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependDestroyImplDestroy(_onceWrap(this, "destroyImplDestroy", listener));
  return this;
}

StreamEventEmitter.prototype.offDestroyImplDestroy = function(listener) {
  // TODO - emit listener removed
  const list = this.destroyImplDestroyListeners;
  let position = -1;

  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i] === listener || list[i].listener === listener) {
      position = i;
      break;
    }
  }

  if (position < 0) return this;

  list.splice(position, 1);

  return this;
}

StreamEventEmitter.prototype.emitDestroyImplDestroy = function(...args) {
  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.destroyImplDestroyListeners.length) return false;

  const len = this.destroyImplDestroyListeners.length;
  const listeners = arrayClone(this.destroyImplDestroyListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "destroyImplDestroy", args);
    }
  }

  return true;
}

// ---- destroyImplDestroy ----




// ----------------------
// Compatibility methods
// ----------------------


// TODO - reuse addListener and on
StreamEventEmitter.prototype.on = function(event, listener) {
  // TODO - emit event listener added when adding a listener by ourself
  switch (event) {
    case "close":
      return this.onClose(listener);
    case "error":
      return this.onError(listener);
    case "data":
      return this.onData(listener);
    case "end":
      return this.onEnd(listener);
    case "readable":
      return this.onReadable(listener);
    case "pause":
      return this.onPause(listener);
    case "resume":
      return this.onResume(listener);
    case "pipe":
      return this.onPipe(listener);
    case "unpipe":
      return this.onUnpipe(listener);

    case "preFinish":
      return this.onPreFinish(listener);
    case "finish":
      return this.onFinish(listener);
    case "drain":
      return this.onDrain(listener);

    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this.onDestroyImplConstruct(listener);
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this.onDestroyImplDestroy(listener);

    default:
      return EE.prototype.on.call(this, event, listener);
  }
}

StreamEventEmitter.prototype.addListener = function(event, listener) {
  // TODO - emit event listener added when adding a listener by ourself
  switch (event) {
    case "close":
      return this.onClose(listener);
    case "data":
      return this.onData(listener);
    case "end":
      return this.onEnd(listener);
    case "readable":
      return this.onReadable(listener);
    case "pause":
      return this.onPause(listener);
    case "resume":
      return this.onResume(listener);
    case "pipe":
      return this.onPipe(listener);
    case "unpipe":
      return this.onUnpipe(listener);

    case "preFinish":
      return this.onPreFinish(listener);
    case "finish":
      return this.onFinish(listener);
    case "drain":
      return this.onDrain(listener);

    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this.onDestroyImplConstruct(listener);
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this.onDestroyImplDestroy(listener);

    default:
      return EE.prototype.addListener(this, event, listener);

  }
}

StreamEventEmitter.prototype.prependListener = function(event, listener) {
  // TODO - emit event listener added when adding a listener by ourself
  switch (event) {
    case "close":
      return this.prependClose(listener);
    case "data":
      return this.prependData(listener);
    case "end":
      return this.prependEnd(listener);
    case "readable":
      return this.prependReadable(listener);
    case "pause":
      return this.prependPause(listener);
    case "resume":
      return this.prependResume(listener);
    case "pipe":
      return this.prependPipe(listener);
    case "unpipe":
      return this.prependUnpipe(listener);

    case "preFinish":
      return this.prependPreFinish(listener);
    case "finish":
      return this.prependFinish(listener);
    case "drain":
      return this.prependDrain(listener);

    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this.prependDestroyImplConstruct(listener);
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this.prependDestroyImplDestroy(listener);

    default:
      return EE.prototype.prependListener(this, event, listener);

  }
}

StreamEventEmitter.prototype.prependOnceListener = function(event, listener) {
  // TODO - emit event listener added when adding a listener by ourself
  switch (event) {
    case "close":
      return this.prependCloseOnce(listener);
    case "data":
      return this.prependDataOnce(listener);
    case "end":
      return this.prependEndOnce(listener);
    case "readable":
      return this.prependReadableOnce(listener);
    case "pause":
      return this.prependPauseOnce(listener);
    case "resume":
      return this.prependResumeOnce(listener);
    case "pipe":
      return this.prependPipeOnce(listener);
    case "unpipe":
      return this.prependUnpipeOnce(listener);

    case "preFinish":
      return this.prependPreFinishOnce(listener);
    case "finish":
      return this.prependFinishOnce(listener);
    case "drain":
      return this.prependDrainOnce(listener);

    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this.prependDestroyImplConstructOnce(listener);
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this.prependDestroyImplDestroyOnce(listener);

    default:
      return EE.prototype.prependOnceListener(this, event, listener);

  }
}

StreamEventEmitter.prototype.once = function(event, listener) {
  // TODO - emit event listener added (and maybe removed when off?) when adding a listener by ourself
  switch (event) {
    case "close":
      return this.onceClose(listener);
    case "data":
      return this.onceData(listener);
    case "end":
      return this.onceEnd(listener);
    case "readable":
      return this.onceReadable(listener);
    case "pause":
      return this.oncePause(listener);
    case "resume":
      return this.onceResume(listener);
    case "pipe":
      return this.oncePipe(listener);
    case "unpipe":
      return this.onceUnpipe(listener);


    case "preFinish":
      return this.oncePreFinish(listener);
    case "finish":
      return this.onceFinish(listener);
    case "drain":
      return this.onceDrain(listener);

    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this.onceDestroyImplConstruct(listener);
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this.onceDestroyImplDestroy(listener);

    default:
      return EE.prototype.once(this, event, listener);
  }
}

// TODO - reuse off and removeListener
StreamEventEmitter.prototype.off = function(event, listener) {
  // TODO - emit event listener removed when removing a listener by ourself
  switch (event) {
    case "close":
      return this.offClose(listener);
    case "data":
      return this.offData(listener);
    case "end":
      return this.offEnd(listener);
    case "readable":
      return this.offReadable(listener);
    case "pause":
      return this.offPause(listener);
    case "resume":
      return this.offResume(listener);
    case "pipe":
      return this.offPipe(listener);
    case "unpipe":
      return this.offUnpipe(listener);

    case "preFinish":
      return this.offPreFinish(listener);
    case "finish":
      return this.offFinish(listener);
    case "drain":
      return this.offDrain(listener);


    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this.offDestroyImplConstruct(listener);
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this.offDestroyImplDestroy(listener);
    default:
      return EE.prototype.off(this, event, listener);
  }

  return this;
}

StreamEventEmitter.prototype.removeListener = function(event, listener) {
  // TODO - emit event listener removed when removing a listener by ourself
  switch (event) {
    case "close":
      return this.offClose(listener);
    case "data":
      return this.offData(listener);
    case "end":
      return this.offEnd(listener);
    case "readable":
      return this.offReadable(listener);
    case "pause":
      return this.offPause(listener);
    case "resume":
      return this.offResume(listener);
    case "pipe":
      return this.offPipe(listener);
    case "unpipe":
      return this.offUnpipe(listener);


    case "preFinish":
      return this.offPreFinish(listener);
    case "finish":
      return this.offFinish(listener);
    case "drain":
      return this.offDrain(listener);

    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this.offDestroyImplConstruct(listener);
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this.offDestroyImplDestroy(listener);
    default:
      return EE.prototype.removeListener(this, event, listener);
  }

  return this;
}

StreamEventEmitter.prototype.removeAllListeners = function(event) {
  // TODO - check which event emitted here
  switch (event) {
    case "close":
      this.closeListeners = [];
      return this;

    case "data":
      this.dataListeners = [];
      return this;
    case "end":
      this.endListeners = [];
      return this;
    case "readable":
      this.readableListeners = [];
      return this;
    case "pause":
      this.pauseListeners = [];
      return this;
    case "resume":
      this.resumeListeners = [];
      return this;
    case "pipe":
      this.pipeListeners = [];
      return this;

    case "unpipe":
      this.unpipeListeners = [];
      return this;

    case "preFinish":
      this.preFinishListeners = [];
      return this;
    case "finish":
      this.finishListeners = [];
      return this;
    case "drain":
      this.drainListeners = [];
      return this;


    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      this.destroyImplConstructListeners = [];
      return this;
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      this.destroyImplDestroyListeners = [];
      return this;

    default:
      return EE.prototype.removeAllListeners(this, event);

  }

  return this;
}

StreamEventEmitter.prototype.listenerCount = function(event) {
  // TODO - check which event emitted here
  switch (event) {
    case "close":
      return this.closeListeners.length;

    case "data":
      return this.dataListeners.length;
    case "end":
      return this.endListeners.length;
    case "readable":
      return this.readableListeners.length;
    case "pause":
      return this.pauseListeners.length;
    case "resume":
      return this.resumeListeners.length;
    case "pipe":
      return this.pipeListeners.length;

    case "unpipe":
      return this.unpipeListeners.length;

    case "preFinish":
      return this.preFinishListeners.length;
    case "finish":
      return this.finishListeners.length;
    case "drain":
      return this.drainListeners.length;

    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this.destroyImplConstructListeners.length;
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this.destroyImplDestroyListeners.length;

    default:
      return EE.prototype.listenerCount(this, event);

  }
}


StreamEventEmitter.prototype._listeners = function(array, unwrap) {
  if (array.length === 0)
    return [];


  return unwrap ?
    unwrapListeners(array) : arrayClone(array);
}

StreamEventEmitter.prototype.listeners = function(event) {
  // TODO - check which event emitted here
  switch (event) {
    case "close":
      return this._listeners(this.closeListeners);

    case "data":
      return this._listeners(this.dataListeners);
    case "end":
      return this._listeners(this.endListeners);
    case "readable":
      return this._listeners(this.readableListeners);
    case "pause":
      return this._listeners(this.pauseListeners);
    case "resume":
      return this._listeners(this.resumeListeners);
    case "pipe":
      return this._listeners(this.pipeListeners);

    case "unpipe":
      return this._listeners(this.unpipeListeners);
    case "preFinish":
      return this._listeners(this.preFinishListeners);
    case "finish":
      return this._listeners(this.finishListeners);
    case "drain":
      return this._listeners(this.drainListeners);

    // destroyImpl.kConstruct
    case "destroyImplConstruct":
      return this._listeners(this.destroyImplConstructListeners);
    // destroyImpl.kDestroy
    case "destroyImplDestroy":
      return this._listeners(this.destroyImplDestroyListeners);

    default:
      return EE.prototype.listeners(this, event);
  }
}

StreamEventEmitter.prototype.eventNames = function() {
  const array = EE.prototype.eventNames(this);

  if (this.closeListeners.length) array.push("close");
  if (this.dataListeners.length) array.push("data");
  if (this.endListeners.length) array.push("end");
  if (this.readableListeners.length) array.push("readable");
  if (this.pauseListeners.length) array.push("pause");
  if (this.resumeListeners.length) array.push("resume");
  if (this.pipeListeners.length) array.push("pipe");
  if (this.unpipeListeners.length) array.push("unpipe");
  if (this.preFinishListeners.length) array.push("preFinish");
  if (this.finishListeners.length) array.push("finish");
  if (this.drainListeners.length) array.push("drain");
  if (this.destroyImplConstructListeners.length) array.push("destroyImplConstruct");
  if (this.destroyImplDestroyListeners.length) array.push("destroyImplDestroy");
}

// TODO - remove this function
//        This function is used to generate the event emitter methods
function createBatch(eventName, capitalizeEventName) {
  return `

  // ---- ${eventName} ----

  StreamEventEmitter.prototype.prepend${capitalizeEventName} = function(listener) {
    // TODO - emit listener added
    this.${eventName}Listeners.unshift(listener);
    return this;
  };

  StreamEventEmitter.prototype.on${capitalizeEventName} = function(listener) {
    // TODO - emit listener added
    this.${eventName}Listeners.push(listener);
    return this;
  };

  StreamEventEmitter.prototype.once${capitalizeEventName} = function(listener) {
    // TODO - emit listener added (and removed when finished?)
    this.on${capitalizeEventName}(_onceWrap(this, "${eventName}", listener));
    return this;
  };

  StreamEventEmitter.prototype.prepend${capitalizeEventName}Once = function(listener) {
    // TODO - emit listener added (and removed when finished?)
    this.prepend${capitalizeEventName}(_onceWrap(this, "${eventName}", listener));
    return this;
  }

  StreamEventEmitter.prototype.off${capitalizeEventName} = function(listener) {
    // TODO - emit listener removed
    const list = this.${eventName}Listeners;
    let position = -1;

    for (let i = list.length - 1; i >= 0; i--) {
      if (list[i] === listener || list[i].listener === listener) {
        position = i;
        break;
      }
    }

    if (position < 0) return this;

    list.splice(position, 1);

    return this;
  }

  StreamEventEmitter.prototype.emit${capitalizeEventName} = function(...args) {
    // TODO - emit listeners that added to the event emitter as well for backwards compatibility
    // TODO - emit the listeners in the same order as they were added to the event emitter and here
    if (!this.${eventName}Listeners.length) return false;

    const len = this.${eventName}Listeners.length;
    const listeners = arrayClone(this.${eventName}Listeners);
    for (let i = 0; i < len; ++i) {
      const result = listeners[i].apply(this, args);

      // We check if result is undefined first because that
      // is the most common case so we do not pay any perf
      // penalty.
      // This code is duplicated because extracting it away
      // would make it non-inlineable.
      if (result !== undefined && result !== null) {
        addCatch(this, result, "${eventName}", args);
      }
    }

    return true;
  }

  // ---- ${eventName} ----

  `;
}

module.exports = {StreamEventEmitter};
