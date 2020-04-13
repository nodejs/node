'use strict';

// Backwards compat. cb() is undocumented and unused in core but
// unfortunately might be used by modules.
function destroy(err, cb) {
  const r = this._readableState;
  const w = this._writableState;

  if ((w && w.destroyed) || (r && r.destroyed)) {
    if (typeof cb === 'function') {
      cb();
    }

    return this;
  }

  if (err) {
    if (w) {
      w.errored = true;
    }
    if (r) {
      r.errored = true;
    }
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

    if (w) {
      w.closed = true;
    }
    if (r) {
      r.closed = true;
    }

    if (typeof cb === 'function') {
      cb(err);
    }

    if (err) {
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
    r.closed = false;
    r.destroyed = false;
    r.errored = false;
    r.reading = false;
    r.ended = false;
    r.endEmitted = false;
    r.errorEmitted = false;
  }

  if (w) {
    w.closed = false;
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

  if ((w && w.destroyed) || (r && r.destroyed)) {
    return this;
  }

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

function isRequest(stream) {
  return stream && stream.setHeader && typeof stream.abort === 'function';
}

// Normalize destroy for legacy.
function destroyer(stream, err) {
  if (isRequest(stream)) return stream.abort();
  if (isRequest(stream.req)) return stream.req.abort();
  if (typeof stream.destroy === 'function') return stream.destroy(err);
  if (typeof stream.close === 'function') return stream.close();
}

module.exports = {
  destroyer,
  destroy,
  undestroy,
  errorOrDestroy
};
