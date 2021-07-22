'use strict';

const {
  isReadable,
  isWritable,
} = require('internal/streams/utils');
const eos = require('internal/streams/end-of-stream');
const {
  AbortError,
} = require('internal/errors');
const { destroyer } = require('internal/streams/destroy');
const Duplex = require('internal/streams/duplex');

// This is needed for pre node 17.
class DuplexifyDuplex extends Duplex {
  constructor(options) {
    super(options);

    // https://github.com/nodejs/node/pull/34385

    if (options?.readable === false) {
      this._readableState.readable = false;
      this._readableState.ended = true;
      this._readableState.endEmitted = true;
    }

    if (options?.writable === false) {
      this._writableState.writable = false;
      this._writableState.ending = true;
      this._writableState.ended = true;
      this._writableState.finished = true;
    }
  }
}

module.exports = function duplexify(pair) {
  const r = pair.readable;
  const w = pair.writable;

  let readable = !!isReadable(r);
  let writable = !!isWritable(w);

  let ondrain;
  let onfinish;
  let onreadable;
  let onclose;
  let d;

  function onfinished(err) {
    const cb = onclose;
    onclose = null;

    if (cb) {
      cb(err);
    } else if (err) {
      d.destroy(err);
    } else if (!readable && !writable) {
      d.destroy();
    }
  }

  eos(r, (err) => {
    readable = false;
    if (err) {
      destroyer(w, err);
    }
    onfinished(err);
  });

  eos(w, (err) => {
    writable = false;
    if (err) {
      destroyer(r, err);
    }
    onfinished(err);
  });

  d = new DuplexifyDuplex({
    // TODO (ronag): highWaterMark?
    readableObjectMode: !!r?.readableObjectMode,
    writableObjectMode: !!w?.writableObjectMode,
    readable,
    writable,
  });

  if (writable) {
    d._write = function(chunk, encoding, callback) {
      if (w.write(chunk, encoding)) {
        callback();
      } else {
        ondrain = callback;
      }
    };

    d._final = function(callback) {
      w.end();
      onfinish = callback;
    };

    w.on('drain', function() {
      if (ondrain) {
        const cb = ondrain;
        ondrain = null;
        cb();
      }
    });

    w.on('finish', function() {
      if (onfinish) {
        const cb = onfinish;
        onfinish = null;
        cb();
      }
    });
  }

  if (readable) {
    r.on('readable', function() {
      if (onreadable) {
        const cb = onreadable;
        onreadable = null;
        cb();
      }
    });

    r.on('end', function() {
      d.push(null);
    });

    d._read = function() {
      while (true) {
        const buf = r.read();

        if (buf === null) {
          onreadable = d._read;
          return;
        }

        if (!d.push(buf)) {
          return;
        }
      }
    };
  }

  d._destroy = function(err, callback) {
    if (!err && onclose !== null) {
      err = new AbortError();
    }

    onreadable = null;
    ondrain = null;
    onfinish = null;

    if (onclose === null) {
      callback(err);
    } else {
      onclose = callback;
      destroyer(w, err);
      destroyer(r, err);
    }
  };

  return d;
};
