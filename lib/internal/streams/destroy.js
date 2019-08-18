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
    } else if (err) {
      if (!this._writableState) {
        process.nextTick(emitErrorNT, this, err);
      } else if (!this._writableState.errorEmitted) {
        this._writableState.errorEmitted = true;
        process.nextTick(emitErrorNT, this, err);
      }
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
    const emitClose = (this._writableState && this._writableState.emitClose) ||
      (this._readableState && this._readableState.emitClose);
    if (!cb && err) {
      if (!this._writableState) {
        const emitNT = emitClose ? emitErrorAndCloseNT : emitErrorNT;
        process.nextTick(emitNT, this, err);
      } else if (!this._writableState.errorEmitted) {
        this._writableState.errorEmitted = true;
        const emitNT = emitClose ? emitErrorAndCloseNT : emitErrorNT;
        process.nextTick(emitNT, this, err);
      } else if (emitClose) {
        process.nextTick(emitCloseNT, this);
      }
    } else if (cb) {
      if (emitClose) {
        process.nextTick(emitCloseNT, this);
      }
      cb(err);
    } else if (emitClose) {
      process.nextTick(emitCloseNT, this);
    }
  });

  return this;
}

function emitErrorAndCloseNT(self, err) {
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
