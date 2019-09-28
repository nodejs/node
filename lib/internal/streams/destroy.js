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
      cb(err);
    } else if (needError(this, err)) {
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
    const emitClose = (w && w.emitClose) || (r && r.emitClose);
    if (cb) {
      if (emitClose) {
        process.nextTick(emitCloseNT, this);
      }
      cb(err);
    } else if (needError(this, err)) {
      process.nextTick(emitClose ? emitErrorCloseNT : emitErrorNT, this, err);
    } else if (emitClose) {
      process.nextTick(emitCloseNT, this);
    }
  });

  return this;
}

function emitErrorCloseNT(self, err) {
  self.emit('error', err);
  self.emit('close');
}

function emitCloseNT(self) {
  self.emit('close');
}

function emitErrorNT(self, err) {
  self.emit('error', err);
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
  else if (needError(stream, err))
    stream.emit('error', err);
}


module.exports = {
  destroy,
  undestroy,
  errorOrDestroy
};
