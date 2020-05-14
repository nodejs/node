'use strict';

class FastTransform extends Readable {
  constructor(options) {
    super(options);
    this._writableState = {
      length: 0,
      needDrain: false,
      ended: false,
      finished: false,
      errored: false
    };
  }
  get writable () {
    const wState = this._writableState
    return (
      !wState.ended &&
      !wState.errored &&
      !wState.destroyed
    )
  }
  get writableEnded () {
    return this._writableState.ended;
  }
  get writableFinished () {
    return this._writableState.finished;
  }
  get writableLength () {
    return wState.length + rState.length
  }
  get writableCorked () {
    // TODO
  }
  get writableHighWaterMark () {
    return this._readableState.highWaterMark;
  }
  get writableBuffer () {
    // TODO
  }
  get writableObjectMode () {
    return this._readableState.objectMode;
  }
  _read () {
    const rState = this._readableState;
    const wState = this._writableState;

    if (!wState.needDrain) {
      return
    }

    if (wState.length + rState.length > rState.highWaterMark) {
      return
    }

    wState.needDrain = false;
    this.emit('drain');
  }
  write (chunk) {
    const rState = this._readableState;
    const wState = this._writableState;

    const len = chunk.length;

    wState.length += len;
    this._transform(chunk, null, (err, data) => {
      wState.length -= len;
      if (err) {
        wState.errored = true;
        this.destroy(err);
      } else if (data != null) {
        this.push(data);
      }
      this._read();
    });

    wState.needDrain = wState.length + rState.length > rState.highWaterMark;

    return wState.needDrain;
  }
  end () {
    const wState = this._writableState;

    wState.ended = true;
    if (this._flush) {
      this._flush(chunk, (err, data) => {
        if (err) {
          this.destroy(err);
        } else {
          if (data != null) {
            this.push(data);
          }
          this.push(null);
          wState.finished = true;
          this.emit('finish');
        }
      })
    } else {
      this.push(null);
      wState.finished = true;
      this.emit('finish');
    }
  }
}

module.exports = FastTransform;
