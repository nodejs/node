'use strict';

// Undocumented cb() API, needed for core, not for public API.
// The cb() will be invoked synchronously if _destroy is synchronous.
// If cb is passed no 'error' event will be emitted.
function destroy(err, cb) {
  const r = this._readableState;
  const w = this._writableState;

  // TODO(ronag): readable & writable = false?

  if (err) {
    if (w) {
      w.errored = true;
    }
    if (r) {
      r.errored = true;
    }
  }

  if ((w && w.destroyed) || (r && r.destroyed)) {
    if (cb) {
      cb(err);
    } else if (err) {
      process.nextTick(emitErrorNT, this, err);
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
      if (w) {
        w.errored = true;
      }
      if (r) {
        r.errored = true;
      }
    }

    if (cb) {
      // Invoke callback before scheduling emitClose so that callback
      // can schedule before.
      cb(err);
      // Don't emit 'error' if passed a callback.
      process.nextTick(emitCloseNT, this);
    } else if (err) {
      process.nextTick(emitErrorCloseNT, this, err);
    } else {
      process.nextTick(emitCloseNT, this);
    }
  });

  return this;
}

function emitErrorCloseNT(self, err) {
  emitErrorNT(self, err);
  emitCloseNT(self);
}

function emitCloseNT(self) {
  const r = self._readableState;
  const w = self._writableState;

  if ((w && w.emitClose) || (r && r.emitClose)) {
    self.emit('close');
  }
}

function emitErrorNT(self, err) {
  const r = self._readableState;
  const w = self._writableState;

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
    r.errored = false;
    r.reading = false;
    r.ended = false;
    r.endEmitted = false;
    r.errorEmitted = false;
  }

  if (w) {
    w.destroyed = false;
    w.errored = false;
    w.ended = false;
    w.ending = false;
    w.finalCalled = false;
    w.prefinished = false;
    w.finished = false;
    w.errorEmitted = false;
  }
}

function errorOrDestroy(stream, err, sync) {
  // We have tests that rely on errors being emitted
  // in the same tick, so changing this is semver major.
  // For now when you opt-in to autoDestroy we allow
  // the error to be emitted nextTick. In a future
  // semver major update we should change the default to this.

  const r = stream._readableState;
  const w = stream._writableState;

  // TODO(ronag): readable & writable = false?

  if ((r && r.autoDestroy) || (w && w.autoDestroy))
    stream.destroy(err);
  else if (err) {
    if (w) {
      w.errored = true;
    }
    if (r) {
      r.errored = true;
    }

    if (sync) {
      process.nextTick(emitErrorNT, stream, err);
    } else {
      emitErrorNT(stream, err);
    }
  }
}


module.exports = {
  destroy,
  undestroy,
  errorOrDestroy
};
