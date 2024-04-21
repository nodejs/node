const EE = require('events');
const {_onceWrap, addCatch, arrayClone, unwrapListeners} = EE;



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


StreamEventEmitter.prototype.closeListener = undefined;
StreamEventEmitter.prototype.closeListeners = [];
StreamEventEmitter.prototype.dataListener = undefined;
StreamEventEmitter.prototype.dataListeners = [];
StreamEventEmitter.prototype.endListener = undefined;
StreamEventEmitter.prototype.endListeners = [];
StreamEventEmitter.prototype.readableListener = undefined;
StreamEventEmitter.prototype.readableListeners = [];
StreamEventEmitter.prototype.pauseListener = undefined;
StreamEventEmitter.prototype.pauseListeners = [];
StreamEventEmitter.prototype.resumeListener = undefined;
StreamEventEmitter.prototype.resumeListeners = [];
StreamEventEmitter.prototype.pipeListener = undefined;
StreamEventEmitter.prototype.pipeListeners = [];
StreamEventEmitter.prototype.unpipeListener = undefined;
StreamEventEmitter.prototype.unpipeListeners = [];
StreamEventEmitter.prototype.prefinishListener = undefined;
StreamEventEmitter.prototype.prefinishListeners = [];
StreamEventEmitter.prototype.finishListener = undefined;
StreamEventEmitter.prototype.finishListeners = [];
StreamEventEmitter.prototype.drainListener = undefined;
StreamEventEmitter.prototype.drainListeners = [];





// ---- close ----

StreamEventEmitter.prototype.prependClose = function(listener) {
  // TODO - emit listener added
  if(this.closeListener) {
    this.closeListeners.push(this.closeListener);
    this.closeListener = undefined;
  } else if(this.closeListeners.length === 0){
    this.closeListener = listener;
    return this;
  }
  this.closeListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onClose = function(listener) {
  // TODO - emit listener added
  if(this.closeListener) {
    this.closeListeners.push(this.closeListener);
    this.closeListener = undefined;
  } else if(this.closeListeners.length === 0){
    this.closeListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.closeListener === listener) {
    this.closeListener = undefined;
    return this;
  }
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
  if(this.closeListener) {
    const result = this.closeListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "close", args);
    }

    return true;
  }

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
  if(this.dataListener) {
    this.dataListeners.push(this.dataListener);
    this.dataListener = undefined;
  } else if(this.dataListeners.length === 0){
    this.dataListener = listener;
    return this;
  }
  this.dataListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onData = function(listener) {
  // TODO - emit listener added
  if(this.dataListener) {
    this.dataListeners.push(this.dataListener);
    this.dataListener = undefined;
  } else if(this.dataListeners.length === 0){
    this.dataListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.dataListener === listener) {
    this.dataListener = undefined;
    return this;
  }
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
  if(this.dataListener) {
    const result = this.dataListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "data", args);
    }

    return true;
  }

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
  if(this.endListener) {
    this.endListeners.push(this.endListener);
    this.endListener = undefined;
  } else if(this.endListeners.length === 0){
    this.endListener = listener;
    return this;
  }
  this.endListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onEnd = function(listener) {
  // TODO - emit listener added
  if(this.endListener) {
    this.endListeners.push(this.endListener);
    this.endListener = undefined;
  } else if(this.endListeners.length === 0){
    this.endListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.endListener === listener) {
    this.endListener = undefined;
    return this;
  }
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
  if(this.endListener) {
    const result = this.endListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "end", args);
    }

    return true;
  }

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
  if(this.readableListener) {
    this.readableListeners.push(this.readableListener);
    this.readableListener = undefined;
  } else if(this.readableListeners.length === 0){
    this.readableListener = listener;
    return this;
  }
  this.readableListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onReadable = function(listener) {
  // TODO - emit listener added
  if(this.readableListener) {
    this.readableListeners.push(this.readableListener);
    this.readableListener = undefined;
  } else if(this.readableListeners.length === 0){
    this.readableListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.readableListener === listener) {
    this.readableListener = undefined;
    return this;
  }
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
  if(this.readableListener) {
    const result = this.readableListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "readable", args);
    }

    return true;
  }

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
  if(this.pauseListener) {
    this.pauseListeners.push(this.pauseListener);
    this.pauseListener = undefined;
  } else if(this.pauseListeners.length === 0){
    this.pauseListener = listener;
    return this;
  }
  this.pauseListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onPause = function(listener) {
  // TODO - emit listener added
  if(this.pauseListener) {
    this.pauseListeners.push(this.pauseListener);
    this.pauseListener = undefined;
  } else if(this.pauseListeners.length === 0){
    this.pauseListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.pauseListener === listener) {
    this.pauseListener = undefined;
    return this;
  }
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
  if(this.pauseListener) {
    const result = this.pauseListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "pause", args);
    }

    return true;
  }

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
  if(this.resumeListener) {
    this.resumeListeners.push(this.resumeListener);
    this.resumeListener = undefined;
  } else if(this.resumeListeners.length === 0){
    this.resumeListener = listener;
    return this;
  }
  this.resumeListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onResume = function(listener) {
  // TODO - emit listener added
  if(this.resumeListener) {
    this.resumeListeners.push(this.resumeListener);
    this.resumeListener = undefined;
  } else if(this.resumeListeners.length === 0){
    this.resumeListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.resumeListener === listener) {
    this.resumeListener = undefined;
    return this;
  }
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
  if(this.resumeListener) {
    const result = this.resumeListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "resume", args);
    }

    return true;
  }

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
  if(this.pipeListener) {
    this.pipeListeners.push(this.pipeListener);
    this.pipeListener = undefined;
  } else if(this.pipeListeners.length === 0){
    this.pipeListener = listener;
    return this;
  }
  this.pipeListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onPipe = function(listener) {
  // TODO - emit listener added
  if(this.pipeListener) {
    this.pipeListeners.push(this.pipeListener);
    this.pipeListener = undefined;
  } else if(this.pipeListeners.length === 0){
    this.pipeListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.pipeListener === listener) {
    this.pipeListener = undefined;
    return this;
  }
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
  if(this.pipeListener) {
    const result = this.pipeListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "pipe", args);
    }

    return true;
  }

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
  if(this.unpipeListener) {
    this.unpipeListeners.push(this.unpipeListener);
    this.unpipeListener = undefined;
  } else if(this.unpipeListeners.length === 0){
    this.unpipeListener = listener;
    return this;
  }
  this.unpipeListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onUnpipe = function(listener) {
  // TODO - emit listener added
  if(this.unpipeListener) {
    this.unpipeListeners.push(this.unpipeListener);
    this.unpipeListener = undefined;
  } else if(this.unpipeListeners.length === 0){
    this.unpipeListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.unpipeListener === listener) {
    this.unpipeListener = undefined;
    return this;
  }
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
  if(this.unpipeListener) {
    const result = this.unpipeListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "unpipe", args);
    }

    return true;
  }

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




// ---- prefinish ----

StreamEventEmitter.prototype.prependPrefinish = function(listener) {
  // TODO - emit listener added
  if(this.prefinishListener) {
    this.prefinishListeners.push(this.prefinishListener);
    this.prefinishListener = undefined;
  } else if(this.prefinishListeners.length === 0){
    this.prefinishListener = listener;
    return this;
  }
  this.prefinishListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onPrefinish = function(listener) {
  // TODO - emit listener added
  if(this.prefinishListener) {
    this.prefinishListeners.push(this.prefinishListener);
    this.prefinishListener = undefined;
  } else if(this.prefinishListeners.length === 0){
    this.prefinishListener = listener;
    return this;
  }
  this.prefinishListeners.push(listener);
  return this;
};

StreamEventEmitter.prototype.oncePrefinish = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.onPrefinish(_onceWrap(this, "prefinish", listener));
  return this;
};

StreamEventEmitter.prototype.prependPrefinishOnce = function(listener) {
  // TODO - emit listener added (and removed when finished?)
  this.prependPrefinish(_onceWrap(this, "prefinish", listener));
  return this;
}

StreamEventEmitter.prototype.offPrefinish = function(listener) {
  // TODO - add validation
  if(this.prefinishListener === listener) {
    this.prefinishListener = undefined;
    return this;
  }
  // TODO - emit listener removed
  const list = this.prefinishListeners;
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

StreamEventEmitter.prototype.emitPrefinish = function(...args) {
  if(this.prefinishListener) {
    const result = this.prefinishListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "prefinish", args);
    }

    return true;
  }

  // TODO - emit listeners that added to the event emitter as well for backwards compatibility
  // TODO - emit the listeners in the same order as they were added to the event emitter and here
  if (!this.prefinishListeners.length) return false;

  const len = this.prefinishListeners.length;
  const listeners = arrayClone(this.prefinishListeners);
  for (let i = 0; i < len; ++i) {
    const result = listeners[i].apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "prefinish", args);
    }
  }

  return true;
}

// ---- prefinish ----




// ---- finish ----

StreamEventEmitter.prototype.prependFinish = function(listener) {
  // TODO - emit listener added
  if(this.finishListener) {
    this.finishListeners.push(this.finishListener);
    this.finishListener = undefined;
  } else if(this.finishListeners.length === 0){
    this.finishListener = listener;
    return this;
  }
  this.finishListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onFinish = function(listener) {
  // TODO - emit listener added
  if(this.finishListener) {
    this.finishListeners.push(this.finishListener);
    this.finishListener = undefined;
  } else if(this.finishListeners.length === 0){
    this.finishListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.finishListener === listener) {
    this.finishListener = undefined;
    return this;
  }
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
  if(this.finishListener) {
    const result = this.finishListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "finish", args);
    }

    return true;
  }

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
  if(this.drainListener) {
    this.drainListeners.push(this.drainListener);
    this.drainListener = undefined;
  } else if(this.drainListeners.length === 0){
    this.drainListener = listener;
    return this;
  }
  this.drainListeners.unshift(listener);
  return this;
};

StreamEventEmitter.prototype.onDrain = function(listener) {
  // TODO - emit listener added
  if(this.drainListener) {
    this.drainListeners.push(this.drainListener);
    this.drainListener = undefined;
  } else if(this.drainListeners.length === 0){
    this.drainListener = listener;
    return this;
  }
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
  // TODO - add validation
  if(this.drainListener === listener) {
    this.drainListener = undefined;
    return this;
  }
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
  if(this.drainListener) {
    const result = this.drainListener.apply(this, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty.
    // This code is duplicated because extracting it away
    // would make it non-inlineable.
    if (result !== undefined && result !== null) {
      addCatch(this, result, "drain", args);
    }

    return true;
  }

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




// ----------------------
// Compatibility methods
// ----------------------


// TODO - reuse addListener and on
StreamEventEmitter.prototype.on = function(event, listener) {
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

    case "prefinish":
      return this.onPrefinish(listener);
    case "finish":
      return this.onFinish(listener);
    case "drain":
      return this.onDrain(listener);

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

    case "prefinish":
      return this.onPrefinish(listener);
    case "finish":
      return this.onFinish(listener);
    case "drain":
      return this.onDrain(listener);

    default:
      return EE.prototype.addListener.call(this, event, listener);

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

    case "prefinish":
      return this.prependPrefinish(listener);
    case "finish":
      return this.prependFinish(listener);
    case "drain":
      return this.prependDrain(listener);

    default:
      return EE.prototype.prependListener.call(this, event, listener);

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

    case "prefinish":
      return this.prependPrefinishOnce(listener);
    case "finish":
      return this.prependFinishOnce(listener);
    case "drain":
      return this.prependDrainOnce(listener);

    default:
      return EE.prototype.prependOnceListener.call(this, event, listener);

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


    case "prefinish":
      return this.oncePrefinish(listener);
    case "finish":
      return this.onceFinish(listener);
    case "drain":
      return this.onceDrain(listener);

    default:
      return EE.prototype.once.call(this, event, listener);
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

    case "prefinish":
      return this.offPrefinish(listener);
    case "finish":
      return this.offFinish(listener);
    case "drain":
      return this.offDrain(listener);

    default:
      return EE.prototype.off.call(this, event, listener);
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


    case "prefinish":
      return this.offPrefinish(listener);
    case "finish":
      return this.offFinish(listener);
    case "drain":
      return this.offDrain(listener);

    default:
      return EE.prototype.removeListener.call(this, event, listener);
  }

  return this;
}

StreamEventEmitter.prototype.removeAllListeners = function(event) {
  // TODO - check which event emitted here
  switch (event) {
    case "close":
      this.closeListeners = [];
      this.closeListener = undefined;
      return this;

    case "data":
      this.dataListeners = [];
      this.dataListener = undefined;
      return this;
    case "end":
      this.endListeners = [];
      this.endListener = undefined;
      return this;
    case "readable":
      this.readableListeners = [];
      this.readableListener = undefined;
      return this;
    case "pause":
      this.pauseListeners = [];
      this.pauseListener = undefined;
      return this;
    case "resume":
      this.resumeListeners = [];
      this.resumeListener = undefined;
      return this;
    case "pipe":
      this.pipeListeners = [];
      this.pipeListener = undefined;
      return this;

    case "unpipe":
      this.unpipeListeners = [];
      this.unpipeListener = undefined;
      return this;

    case "prefinish":
      this.prefinishListeners = [];
      this.prefinishListener = undefined;
      return this;
    case "finish":
      this.finishListeners = [];
      this.finishListener = undefined;
      return this;
    case "drain":
      this.drainListeners = [];
      this.drainListener = undefined;
      return this;


    default:
      return EE.prototype.removeAllListeners.call(this, event);

  }
}

StreamEventEmitter.prototype.listenerCount = function(event) {
  // TODO - check which event emitted here
  switch (event) {
    case "close":
      return this.closeListeners.length + (this.closeListener ? 1 : 0);

    case "data":
      return this.dataListeners.length + (this.dataListener ? 1 : 0);
    case "end":
      return this.endListeners.length + (this.endListener ? 1 : 0);
    case "readable":
      return this.readableListeners.length + (this.readableListener ? 1 : 0);
    case "pause":
      return this.pauseListeners.length + (this.pauseListener ? 1 : 0);
    case "resume":
      return this.resumeListeners.length + (this.resumeListener ? 1 : 0);
    case "pipe":
      return this.pipeListeners.length + (this.pipeListener ? 1 : 0);

    case "unpipe":
      return this.unpipeListeners.length + (this.unpipeListener ? 1 : 0);

    case "prefinish":
      return this.prefinishListeners.length + (this.prefinishListener ? 1 : 0);
    case "finish":
      return this.finishListeners.length + (this.finishListener ? 1 : 0);
    case "drain":
      return this.drainListeners.length + (this.drainListener ? 1 : 0);

    default:
      return EE.prototype.listenerCount.call(this, event);

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
      return this._listeners(this.closeListener ? [this.closeListener] : this.closeListeners);

    case "data":
      return this._listeners(this.dataListener ? [this.dataListener] : this.dataListeners);
    case "end":
      return this._listeners(this.endListener ? [this.endListener] : this.endListeners);
    case "readable":
      return this._listeners(this.readableListener ? [this.readableListener] : this.readableListeners);
    case "pause":
      return this._listeners(this.pauseListener ? [this.pauseListener] : this.pauseListeners);
    case "resume":
      return this._listeners(this.resumeListener ? [this.resumeListener] : this.resumeListeners);
    case "pipe":
      return this._listeners(this.pipeListener ? [this.pipeListener] : this.pipeListeners);

    case "unpipe":
      return this._listeners(this.unpipeListener ? [this.unpipeListener] : this.unpipeListeners);
    case "prefinish":
      return this._listeners(this.prefinishListener ? [this.prefinishListener] : this.prefinishListeners);
    case "finish":
      return this._listeners(this.finishListener ? [this.finishListener] : this.finishListeners);
    case "drain":
      return this._listeners(this.drainListener ? [this.drainListener] : this.drainListeners);

    default:
      return EE.prototype.listeners.call(this, event);
  }
}

StreamEventEmitter.prototype.eventNames = function() {
  const array = EE.prototype.eventNames.call(this);

  if (this.closeListener || this.closeListeners.length) array.push("close");
  if (this.dataListener || this.dataListeners.length) array.push("data");
  if (this.endListener || this.endListeners.length) array.push("end");
  if (this.readableListener || this.readableListeners.length) array.push("readable");
  if (this.pauseListener || this.pauseListeners.length) array.push("pause");
  if (this.resumeListener || this.resumeListeners.length) array.push("resume");
  if (this.pipeListener || this.pipeListeners.length) array.push("pipe");
  if (this.unpipeListener || this.unpipeListeners.length) array.push("unpipe");
  if (this.prefinishListener || this.prefinishListeners.length) array.push("prefinish");
  if (this.finishListener || this.finishListeners.length) array.push("finish");
  if (this.drainListener || this.drainListeners.length) array.push("drain");
}

// TODO - remove this function
//        This function is used to generate the event emitter methods
function createBatch(eventName, capitalizeEventName) {
  // const events = ['close', 'data', 'end', 'readable', 'pause', 'resume', 'pipe', 'unpipe', 'prefinish', 'finish', 'drain'];
  // const eventsReady = events.map(event => [event, event.charAt(0).toUpperCase() + event.slice(1)])
  // copy(eventsReady.map(args => createBatch(...args)).join('\n'))

  return `

  // ---- ${eventName} ----

  StreamEventEmitter.prototype.prepend${capitalizeEventName} = function(listener) {
    // TODO - emit listener added
    if(this.${eventName}Listener) {
      this.${eventName}Listeners.push(this.${eventName}Listener);
      this.${eventName}Listener = undefined;
    } else if(this.${eventName}Listeners.length === 0){
      this.${eventName}Listener = listener;
      return this;
    }
    this.${eventName}Listeners.unshift(listener);
    return this;
  };

  StreamEventEmitter.prototype.on${capitalizeEventName} = function(listener) {
    // TODO - emit listener added
    if(this.${eventName}Listener) {
      this.${eventName}Listeners.push(this.${eventName}Listener);
      this.${eventName}Listener = undefined;
    } else if(this.${eventName}Listeners.length === 0){
      this.${eventName}Listener = listener;
      return this;
    }
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
    // TODO - add validation
    if(this.${eventName}Listener === listener) {
      this.${eventName}Listener = undefined;
      return this;
    }
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
    if(this.${eventName}Listener) {
      const result = this.${eventName}Listener.apply(this, args);

      // We check if result is undefined first because that
      // is the most common case so we do not pay any perf
      // penalty.
      // This code is duplicated because extracting it away
      // would make it non-inlineable.
      if (result !== undefined && result !== null) {
        addCatch(this, result, "${eventName}", args);
      }

      return true;
    }

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
