'use strict';

// Undocumented cb() API, needed for core, not for public API
function destroy(err, cb) {
  const readableDestroyed = this._readableState &&
    this._readableState.destroyed;
  const writableDestroyed = this._writableState &&
    this._writableState.destroyed;

  if (readableDestroyed || writableDestroyed) {
    if (cb) {
      cb(err);
    } else if (err &&
               (!this._writableState || !this._writableState.errorEmitted)) {
      process.nextTick(emitErrorNT, this, err);
    }
    return this;
  }

  // We set destroyed to true before firing error callbacks in order
  // to make it re-entrance safe in case destroy() is called within callbacks

  if (this._readableState) {
    this._readableState.destroyed = true;
  }

  // If this is a duplex stream mark the writable part as destroyed as well
  if (this._writableState) {
    this._writableState.destroyed = true;
  }

  this._destroy(err || null, (err) => {
    if (!cb && err) {
      process.nextTick(emitErrorAndCloseNT, this, err);
      if (this._writableState) {
        this._writableState.errorEmitted = true;
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
  if (self._writableState && !self._writableState.emitClose)
    return;
  if (self._readableState && !self._readableState.emitClose)
    return;
  self.emit('close');
}

function undestroy() {
  if (this._readableState) {
    this._readableState.destroyed = false;
    this._readableState.reading = false;
    this._readableState.ended = false;
    this._readableState.endEmitted = false;
  }

  if (this._writableState) {
    this._writableState.destroyed = false;
    this._writableState.ended = false;
    this._writableState.ending = false;
    this._writableState.finalCalled = false;
    this._writableState.prefinished = false;
    this._writableState.finished = false;
    this._writableState.errorEmitted = false;
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

  const rState = stream._readableState;
  const wState = stream._writableState;

  if ((rState && rState.autoDestroy) || (wState && wState.autoDestroy))
    stream.destroy(err);
  else
    stream.emit('error', err);
}


module.exports = {
  destroy,
  undestroy,
  errorOrDestroy
};
