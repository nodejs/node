'use strict';

function needError(stream, err) {
  if (!err) {
    return false;
  }

  const r = stream._readableState;
  const w = stream._writableState;

  if ((w && w.errorEmitted) || (r && r.errorEmitted)) {
    return false;
  }

  if (w) {
    w.errorEmitted = true;
  }
  if (r) {
    r.errorEmitted = true;
  }

  return true;
}

// Undocumented cb() API, needed for core, not for public API
function destroy(err, cb) {
  const r = this._readableState;
  const w = this._writableState;

  if ((w && w.destroyed) || (r && r.destroyed)) {
    if (cb) {
      cb(err || null);
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

  this._destroy(err || null, (er) => {
    if (cb) {
      cb(er || null);
    }
    if (err && needError(this, er || null)) {
      process.nextTick(emitErrorAndCloseNT, this, err);
    } else {
      process.nextTick(emitCloseNT, this);
    }
  });

  return this;
}

function emitErrorAndCloseNT(self, err) {
  emitErrorNT(self, err);
  emitCloseNT(self);
}

function emitCloseNT(self) {
  const r = self._readableState;
  const w = self._writableState;

  if (w && !w.emitClose)
    return;
  if (r && !r.emitClose)
    return;
  self.emit('close');
}

function undestroy() {
  const r = this._readableState;
  const w = this._writableState;

  if (r) {
    r.destroyed = false;
    r.reading = false;
    r.ended = false;
    r.endEmitted = false;
    r.errorEmitted = false;
  }

  if (w) {
    w.destroyed = false;
    w.ended = false;
    w.ending = false;
    w.finalCalled = false;
    w.prefinished = false;
    w.finished = false;
    w.errorEmitted = false;
  }
}

function emitErrorNT(self, err) {
  self.emit('error', err);
}

function errorOrDestroy(stream, err) {
  // We have tests that rely on errors being emitted
  // in the same tick, so changing this is semver major.
  // For now when you opt-in to autoDestroy we allow
  // the error to be emitted nextTick. In a future
  // semver major update we should change the default to this.

  const r = stream._readableState;
  const w = stream._writableState;

  // Don't emit errors if already destroyed.
  if ((w && w.destroyed) || (r && r.destroyed)) {
    return;
  }

  if ((r && r.autoDestroy) || (w && w.autoDestroy))
    stream.destroy(err);
  else if (needError(stream, err))
    stream.emit('error', err);
}


module.exports = {
  destroy,
  undestroy,
  errorOrDestroy
};
