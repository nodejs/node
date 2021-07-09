'use strict';

const {
  Symbol,
  SymbolAsyncIterator,
  SymbolIterator,
} = primordials;

const kDestroyed = Symbol('kDestroyed');

function isReadableNodeStream(obj) {
  return !!(
    obj &&
    typeof obj.pipe === 'function' &&
    typeof obj.on === 'function' &&
    (!obj._writableState || obj._readableState?.readable !== false) && // Duplex
    (!obj._writableState || obj._readableState) // Writable has .pipe.
  );
}

function isWritableNodeStream(obj) {
  return !!(
    obj &&
    typeof obj.write === 'function' &&
    typeof obj.on === 'function' &&
    (!obj._readableState || obj._writableState?.writable !== false) // Duplex
  );
}

function isNodeStream(obj) {
  return isReadableNodeStream(obj) || isWritableNodeStream(obj);
}

function isIterable(obj, isAsync) {
  if (obj == null) return false;
  if (isAsync === true) return typeof obj[SymbolAsyncIterator] === 'function';
  if (isAsync === false) return typeof obj[SymbolIterator] === 'function';
  return typeof obj[SymbolAsyncIterator] === 'function' ||
    typeof obj[SymbolIterator] === 'function';
}

function isDestroyed(stream) {
  if (!isNodeStream(stream)) return null;
  const wState = stream._writableState;
  const rState = stream._readableState;
  const state = wState || rState;
  return !!(stream.destroyed || stream[kDestroyed] || state?.destroyed);
}

// Have been end():d.
function isWritableEnded(stream) {
  if (!isWritableNodeStream(stream)) return null;
  if (stream.writableEnded === true) return true;
  const wState = stream._writableState;
  if (wState?.errored) return false;
  if (typeof wState?.ended !== 'boolean') return null;
  return wState.ended;
}

// Have emitted 'finish'.
function isWritableFinished(stream, strict) {
  if (!isWritableNodeStream(stream)) return null;
  if (stream.writableFinished === true) return true;
  const wState = stream._writableState;
  if (wState?.errored) return false;
  if (typeof wState?.finished !== 'boolean') return null;
  return !!(
    wState.finished ||
    (strict === false && wState.ended === true && wState.length === 0)
  );
}

// Have been push(null):d.
function isReadableEnded(stream) {
  if (!isReadableNodeStream(stream)) return null;
  if (stream.readableEnded === true) return true;
  const rState = stream._readableState;
  if (!rState || rState.errored) return false;
  if (typeof rState?.ended !== 'boolean') return null;
  return rState.ended;
}

// Have emitted 'end'.
function isReadableFinished(stream, strict) {
  if (!isReadableNodeStream(stream)) return null;
  const rState = stream._readableState;
  if (rState?.errored) return false;
  if (typeof rState?.endEmitted !== 'boolean') return null;
  return !!(
    rState.endEmitted ||
    (strict === false && rState.ended === true && rState.length === 0)
  );
}

function isReadable(stream) {
  const r = isReadableNodeStream(stream);
  if (r === null || typeof stream.readable !== 'boolean') return null;
  if (isDestroyed(stream)) return false;
  return r && stream.readable && !isReadableFinished(stream);
}

function isWritable(stream) {
  const r = isWritableNodeStream(stream);
  if (r === null || typeof stream.writable !== 'boolean') return null;
  if (isDestroyed(stream)) return false;
  return r && stream.writable && !isWritableEnded(stream);
}

function isFinished(stream, opts) {
  if (!isNodeStream(stream)) {
    return null;
  }

  if (isDestroyed(stream)) {
    return true;
  }

  if (opts?.readable !== false && isReadable(stream)) {
    return false;
  }

  if (opts?.writable !== false && isWritable(stream)) {
    return false;
  }

  return true;
}

function isClosed(stream) {
  if (!isNodeStream(stream)) {
    return null;
  }

  const wState = stream._writableState;
  const rState = stream._readableState;

  if (
    typeof wState?.closed === 'boolean' ||
    typeof rState?.closed === 'boolean'
  ) {
    return wState?.closed || rState?.closed;
  }

  if (typeof stream._closed === 'boolean' && isOutgoingMessage(stream)) {
    return stream._closed;
  }

  return null;
}

function isOutgoingMessage(stream) {
  return (
    typeof stream._closed === 'boolean' &&
    typeof stream._defaultKeepAlive === 'boolean' &&
    typeof stream._removedConnection === 'boolean' &&
    typeof stream._removedContLen === 'boolean'
  );
}

function isServerResponse(stream) {
  return (
    typeof stream._sent100 === 'boolean' &&
    isOutgoingMessage(stream)
  );
}

function isServerRequest(stream) {
  return (
    typeof stream._consuming === 'boolean' &&
    typeof stream._dumped === 'boolean' &&
    stream.req?.upgradeOrConnect === undefined
  );
}

function willEmitClose(stream) {
  if (!isNodeStream(stream)) return null;

  const wState = stream._writableState;
  const rState = stream._readableState;
  const state = wState || rState;

  return (!state && isServerResponse(stream)) || !!(
    state &&
    state.autoDestroy &&
    state.emitClose &&
    state.closed === false
  );
}

module.exports = {
  kDestroyed,
  isClosed,
  isDestroyed,
  isFinished,
  isIterable,
  isReadable,
  isReadableNodeStream,
  isReadableEnded,
  isReadableFinished,
  isNodeStream,
  isWritable,
  isWritableNodeStream,
  isWritableEnded,
  isWritableFinished,
  isServerRequest,
  isServerResponse,
  willEmitClose,
};
