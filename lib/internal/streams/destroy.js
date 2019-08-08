'use strict';

// Undocumented cb() API, needed for core, not for public API
function destroy(err, cb) {
  const r = this._readableState;
  const w = this._writableState;

  const readableDestroyed = r && r.destroyed;
  const writableDestroyed = w && w.destroyed;

  if (readableDestroyed || writableDestroyed) {
    if (cb) {
      cb(err);
    } else if (err) {
      if (!w) {
        process.nextTick(emitErrorNT, this, err);
      } else if (!w.errorEmitted) {
        w.errorEmitted = true;
        process.nextTick(emitErrorNT, this, err);
      }
    }

    return this;
  }

  // We set destroyed to true before firing error callbacks in order
  // to make it re-entrance safe in case destroy() is called within callbacks

  if (r) {
    r.destroyed = true;
  }

  // If this is a duplex stream mark the writable part as destroyed as well
  if (w) {
    w.destroyed = true;
  }

  this._destroy(err || null, (err) => {
    if (!cb && err) {
      if (!w) {
        process.nextTick(emitErrorAndCloseNT, this, err);
      } else if (!w.errorEmitted) {
        w.errorEmitted = true;
        process.nextTick(emitErrorAndCloseNT, this, err);
      } else {
        process.nextTick(emitCloseNT, this);
      }
    } else if (cb) {
      process.nextTick(emitCloseNT, this);
      cb(err);
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

  if ((r && r.autoDestroy) || (w && w.autoDestroy))
    stream.destroy(err);
  else
    stream.emit('error', err);
}


module.exports = {
  destroy,
  undestroy,
  errorOrDestroy
};
