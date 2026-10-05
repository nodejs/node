'use strict';

/**
 * concat(sources[, options]) - lazily concatenate readable sources into a
 * single async iterator.
 *
 * `sources` is one iterable or async iterable of sources. Each item may be:
 *   - stream.Readable (including legacy "streams1" objects)
 *   - web ReadableStream
 *   - a sync/async iterable of chunks
 *   - a chunk (string, Buffer, TypedArray) - yielded as-is
 *   - a factory: () => any of the above, possibly async, built on demand
 *
 * Returns a plain async iterator. It deliberately does not return a stream, so
 * it never has to guess an objectMode. Compose to taste:
 *
 *   Readable.from(concat(sources))
 *   await text(concat(sources))
 *
 * -- Signature ----------------------------------------------------------------
 *
 * One collection, one options bag. Not variadic. Sources usually arrive from a
 * calling context already in a collection, so varargs would force a spread at
 * every call site, and - more importantly - it would permanently occupy the
 * second position. `pipeline()` went variadic and has been paying for it ever
 * since: the trailing argument had to be special-cased as the callback, then
 * `{ signal }` had to squeeze in ahead of it. `Buffer.concat(list, ...)` and
 * `Readable.from(iterable, options)` are the shapes to copy.
 *
 * -- Eagerness contract -------------------------------------------------------
 *
 * A Node stream starts working the moment it is constructed, and an 'error' it
 * emits with no listener attached takes down the process. So the invariant is:
 *
 *     never hold a reference to a live Node stream we are not listening to.
 *
 * We cannot ask an iterable whether its items already exist. A Set of open file
 * streams and a generator that opens one per pull have identical interfaces.
 * So the input protocol picks the default:
 *
 *   sync iterable  -> collection semantics. Drained at call time; every live
 *                     Node stream in it is guarded immediately. Sync iteration
 *                     cannot block, and anything already in there is already
 *                     running, so there is nothing to lose by looking.
 *
 *   async iterable -> producer semantics. Pulled one at a time, guarded at the
 *                     instant of acquisition, never read ahead. Choosing an
 *                     async producer is how you declare that pulling costs
 *                     something (or that the sequence may be unbounded).
 *
 *   function        -> laziness, under either default. A factory is inert, so a
 *                      sync collection of factories can be drained safely.
 *
 * ...and `options.lazy` overrides it when the default guesses wrong - a sync
 * generator that opens a stream per pull wants `{ lazy: true }`.
 *
 * The guarding only applies to Node streams. Web streams and async iterables
 * store their failure and hand it over on the next read; they have no
 * unhandled-'error' failure mode and need no babysitting.
 *
 * -- Options ------------------------------------------------------------------
 *
 *   signal           AbortSignal. Aborting fails the iterator with
 *                    `signal.reason` and tears down every source.
 *
 *   lazy             Override the sync/async default above. `true` pulls one
 *                    source at a time; `false` drains `sources` up front and
 *                    guards everything. Draining an unbounded async producer
 *                    will hang, so `false` is only for bounded ones.
 *
 *   destroyOnReturn  Default true. When false, sources are handed back live and
 *                    unguarded on early return or failure - you own them, and
 *                    an unhandled 'error' from one is yours to catch.
 */

const {
  ArrayBuffer,
  ArrayBufferIsView,
  ArrayFrom,
  Boolean,
  Promise,
  PromisePrototypeThen,
  PromiseResolve,
  SafeMap,
  SafePromiseRace,
  SymbolAsyncIterator,
  SymbolIterator,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const {
  validateObject,
  validateAbortSignal,
} = require('internal/validators');

const Readable = require('internal/streams/readable');


const isFn = (v) => typeof v === 'function';

const noop = () => {};

// Release an iterator without waiting on it. A stream async iterator's return()
// does not settle until any outstanding next() does, and the next() abandoned
// when racing the latch never will - so awaiting this deadlocks against a
// source that has stalled. Destroying the stream is what settles that pending
// read, and teardown() has already done it by the time this runs.
function closeQuietly(iterator) {
  if (!iterator || !isFn(iterator.return)) return;
  try {
    const result = iterator.return();
    if (result !== null && typeof result === 'object' && isFn(result.then)) {
      PromisePrototypeThen(PromiseResolve(result), noop, noop);
    }
  } catch {
    // Already gone.
  }
}

const isNodeStream = (o) =>
  o !== null && typeof o === 'object' && isFn(o.pipe) && isFn(o.on);

const isWebReadable = (o) =>
  o !== null && typeof o === 'object' && isFn(o.getReader) && isFn(o.cancel) &&
  !isNodeStream(o);

const isAsyncIterable = (o) =>
  o !== null && o !== undefined && isFn(o[SymbolAsyncIterator]);

const isSyncIterable = (o) =>
  o !== null && o !== undefined && isFn(o[SymbolIterator]);

// Valid as an item, never as the `sources` collection, so concat('abc') is a
// mistake rather than three single-character sources.
const isChunk = (o) =>
  typeof o === 'string' || ArrayBufferIsView(o) || o instanceof ArrayBuffer;

// ---------------------------------------------------------------------------
// source normalization
// ---------------------------------------------------------------------------

async function* fromWebReadable(rs) {
  const reader = rs.getReader();
  let drained = false;
  try {
    for (;;) {
      const { done, value } = await reader.read();
      if (done) {
        drained = true;
        return;
      }
      yield value;
    }
  } finally {
    if (!drained) {
      reader.cancel().catch(() => {}); // Best effort: tell the producer we left
    }
    reader.releaseLock();
  }
}

function toAsyncIterator(source) {
  if (isChunk(source)) {
    return (async function* () { yield source; })();
  }
  if (isNodeStream(source)) {
    // Legacy streams1 have no Symbol.asyncIterator. This is the case ronag
    // flagged on the original PR; pipeline() solves it the same way.
    const modern = isAsyncIterable(source) ? source :
      new Readable({ objectMode: true }).wrap(source);
    return modern[SymbolAsyncIterator]();
  }
  if (isWebReadable(source)) {
    return isAsyncIterable(source) ? source[SymbolAsyncIterator]() :
      fromWebReadable(source);
  }
  if (isAsyncIterable(source)) return source[SymbolAsyncIterator]();
  if (isSyncIterable(source)) {
    return (async function* () { yield* source; })();
  }
  throw new ERR_INVALID_ARG_TYPE('source',
                                 ['Stream', 'Iterable', 'AsyncIterable'],
                                 source);
}

// ---------------------------------------------------------------------------
// error latch
// ---------------------------------------------------------------------------

function createLatch(signal) {
  let reject;
  const trigger = new Promise((_, rej) => { reject = rej; });
  trigger.catch(() => {}); // Never an unhandled rejection; we always race it

  const guards = new SafeMap(); // stream -> remove its 'error' listener
  const state = { error: null, done: false };
  let detachSignal = () => {};

  const fail = (err) => {
    if (state.error === null && !state.done) {
      state.error = err;
      reject(err);
    }
  };

  if (signal) {
    if (signal.aborted) {
      fail(signal.reason);
    } else {
      const onAbort = () => fail(signal.reason);
      signal.addEventListener('abort', onAbort, { once: true });
      detachSignal = () => signal.removeEventListener('abort', onAbort);
    }
  }

  return {
    trigger,
    get error() { return state.error; },

    // One 'error' listener goes on at the instant we take a reference to a
    // stream. It is both the floor - while it is attached the stream cannot
    // throw an unhandled 'error' at the process, including when *we* destroy
    // it during teardown - and the observer that reports a sibling's failure.
    //
    // This deliberately does not use end-of-stream. eos() reports end, close
    // and premature-close as well, none of which we need: we know a source is
    // finished because our own loop drained it. All we want is the error, and
    // a listener cannot hand back the wrong kind of cleanup value.
    track(source) {
      if (!isNodeStream(source) || guards.has(source)) return;
      const onError = (err) => fail(err);
      source.on('error', onError);
      guards.set(source, () => source.removeListener('error', onError));

      // A source that failed before we ever saw it emitted 'error' with no
      // listener attached; `errored` is how that stays observable afterwards.
      if (source.errored) fail(source.errored);
    },

    // Read to completion, no failure. Hand the stream back exactly as we found
    // it. Membership is decided by presence in the map, never by the
    // truthiness of what is stored against it.
    release(source) {
      if (!guards.has(source)) return;
      guards.get(source)();
      guards.delete(source);
    },

    fail,

    // Destroy with the listeners still attached: destroy(err) emits 'error'
    // asynchronously, and by delivery time anything detached first would be
    // gone. state.done already blocks fail(), so the echo is absorbed
    // silently. Only the hand-back path detaches, because there the caller is
    // taking ownership.
    //
    // forEach hands the value first and the key second, so there is no
    // .values()/.keys() call to lose in translation and no iterator protocol
    // involved.
    teardown(err, destroy) {
      state.done = true;
      detachSignal();

      const pairs = [];
      guards.forEach((detach, source) => {
        pairs.push(source, detach);
      });
      guards.clear();

      for (let i = 0; i < pairs.length; i += 2) {
        const source = pairs[i];
        if (!destroy) {
          pairs[i + 1](); // Caller owns it now
        } else if (isFn(source.destroy) && !source.destroyed) {
          source.destroy(err ?? undefined);
        }
      }
    },
  };
}

// ---------------------------------------------------------------------------
// concat
// ---------------------------------------------------------------------------

function concat(sources, options = undefined) {

  if (isChunk(sources)) {
    throw new ERR_INVALID_ARG_TYPE('sources', ['Iterable', 'AsyncIterable'], sources);
  }
  const isAsync = isAsyncIterable(sources);
  if (!isAsync && !isSyncIterable(sources)) {
    throw new ERR_INVALID_ARG_TYPE('sources', ['Iterable', 'AsyncIterable'], sources);
  }

  // Let the validators decide. Pre-screening with `typeof === 'object'` lets an
  // array or a stream through as an options bag, which validateObject rejects.
  if (options !== undefined) validateObject(options, 'options');
  const { signal, lazy, destroyOnReturn = true } = options ?? {};
  if (signal !== undefined && signal !== null) {
    validateAbortSignal(signal, 'options.signal');
  }

  const latch = createLatch(signal);
  const deferred = lazy === undefined ? isAsync : Boolean(lazy);

  // Guard at call time, not on first next(). A collection handed to concat()
  // holds running streams whether or not anyone ever iterates the result.
  // (An async collection cannot be drained synchronously, so an eager pass
  // over one happens on first pull instead - nothing in it is reachable, and
  // therefore nothing in it is at risk, until then.)
  let entries = sources;
  if (!deferred && !isAsync) {
    entries = ArrayFrom(sources);
    for (const entry of entries) {
      if (!isFn(entry)) latch.track(entry);
    }
  }

  return run(entries, deferred, isAsync, latch, destroyOnReturn);
}

async function* run(entries, deferred, isAsync, latch, destroyOnReturn) {
  let outer = null;
  let inner = null;

  try {
    if (!deferred && isAsync) {
      // Bounded async producer, eagerness explicitly requested: drain first,
      // then guard everything before reading a single byte.
      const drained = [];
      for await (const entry of entries) drained.push(entry);
      for (const entry of drained) {
        if (!isFn(entry)) latch.track(entry);
      }
      if (latch.error) throw latch.error;
      entries = drained;
      isAsync = false;
    }

    const outerIsAsync = deferred && isAsync;
    outer = outerIsAsync ? entries[SymbolAsyncIterator]() : entries[SymbolIterator]();

    for (;;) {
      // A sync iterator's next() returns a plain result object, not a promise,
      // and there is nothing to race anyway: a synchronous pull cannot be
      // outrun by the latch. Check the latch directly instead.
      const step = outerIsAsync ?
        await SafePromiseRace([PromiseResolve(outer.next()), latch.trigger]) :
        outer.next();
      if (latch.error) throw latch.error;
      if (step.done) break;

      let source = step.value;

      if (isFn(source)) {
        // Factory. Nothing existed until now, so nothing could have failed.
        source = await SafePromiseRace([
          (async () => source())(),
          latch.trigger,
        ]);
        latch.track(source);
      } else if (deferred) {
        latch.track(source);
      }

      inner = toAsyncIterator(source);

      for (;;) {
        // Race the read against a sibling failing, so we abort immediately
        // rather than waiting on a chunk that may never arrive.
        const chunk = await SafePromiseRace([PromiseResolve(inner.next()), latch.trigger]);
        if (chunk.done) break;
        yield chunk.value;
      }

      inner = null;
      latch.release(source);
    }
  } catch (err) {
    latch.fail(err);
    throw latch.error ?? err;
  } finally {
    // Destroy before releasing the iterators, not after: destroying is what
    // settles the read abandoned when the latch fired, and a stream iterator's
    // return() will not complete until it does.
    latch.teardown(latch.error, destroyOnReturn);
    closeQuietly(inner);
    closeQuietly(outer);
  }
}

module.exports = concat;
