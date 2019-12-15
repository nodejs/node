'use strict';

// Undocumented cb() API, needed for core, not for public API.
// The cb() will be invoked synchronously if _destroy is synchronous.
// If cb is passed no 'error' event will be emitted.
function destroy(err, cb) {
  const r = this._readableState;
  const w = this._writableState;

  if (err) {
    if (w && !w.errored) {
      w.errored = err;
    }
    if (r && !r.errored) {
      r.errored = err;
    }
  }

  if ((w && w.destroyed) || (r && r.destroyed)) {
    if (cb) {
      // TODO(ronag): Wait until 'close' is emitted (if not already emitted).
      cb((w && w.errored) || (r && r.errored));
    }
    return this;
  }

  // We set destroyed to true before firing error callbacks in order
  // to make it re-entrance safe in case destroy() is called within callbacks

  if (w) {
    w.destroyed = true;
  }
  if (r) {
    r.destroyed = true;
  }

  this._destroy(err || null, (err) => {
    if (err) {
      if (w && !w.errored) {
        w.errored = err;
      }
      if (r && !r.errored) {
        r.errored = err;
      }
    }

    if (cb) {
      // Invoke callback before scheduling emitClose so that callback
      // can schedule before.
      cb((w && w.errored) || (r && r.errored));
      // Don't emit 'error' if passed a callback.
      process.nextTick(emitCloseNT, this);
    } else {
      process.nextTick(emitErrorCloseNT, this);
    }
  });

  return this;
}

function emitErrorCloseNT(self) {
  emitErrorNT(self);
  emitCloseNT(self);
}

function emitCloseNT(self) {
  const r = self._readableState;
  const w = self._writableState;

  if ((w && w.emitClose) || (r && r.emitClose)) {
    self.emit('close');
  }
}

function emitErrorNT(self) {
  const r = self._readableState;
  const w = self._writableState;

  const err = (w && w.errored) || (r && r.errored);

  if (!err) {
    return;
  }

  if ((w && w.errorEmitted) || (r && r.errorEmitted)) {
    return;
  }

  if (w) {
    w.errorEmitted = true;
  }
  if (r) {
    r.errorEmitted = true;
  }

  self.emit('error', err);
}

function undestroy() {
  const r = this._readableState;
  const w = this._writableState;

  if (r) {
    r.destroyed = false;
    r.errored = null;
    r.reading = false;
    r.ended = false;
    r.endEmitted = false;
    r.errorEmitted = false;
  }

  if (w) {
    w.destroyed = false;
    w.errored = null;
    w.ended = false;
    w.ending = false;
    w.finalCalled = false;
    w.prefinished = false;
    w.finished = false;
    w.errorEmitted = false;
  }
}

function errorOrDestroy(stream, err) {
  // We have tests that rely on errors being emitted
  // in the same tick, so changing this is semver major.
  // For now when you opt-in to autoDestroy we allow
  // the error to be emitted nextTick. In a future
  // semver major update we should change the default to this.

  const r = stream._readableState;
  const w = stream._writableState;

  if ((r && r.autoDestroy) || (w && w.autoDestroy))
    stream.destroy(err);
  else if (err) {
    if (w && !w.errored) {
      w.errored = err;
    }
    if (r && !r.errored) {
      r.errored = err;
    }
    emitErrorNT(stream);
  }
}


module.exports = {
  destroy,
  undestroy,
  errorOrDestroy
};
