# SnapshotAgent

<!--introduced_in=v7.13.0-->
<!--type=module-->
<!-- source_link=lib/mock/snapshot-agent.js -->

> Stability: 1 - Experimental

`SnapshotAgent` records real HTTP responses and replays them on later requests,
allowing tests to run against deterministic, captured data instead of a live
network. It extends [`MockAgent`][], so it can be installed with
[`setGlobalDispatcher()`][] or passed explicitly through the `dispatcher`
option, and it works with every undici API (`fetch`, `request`, `stream`,
`pipeline`, and so on).

A `SnapshotAgent` operates in one of three modes. In `'record'` mode it performs
real requests and writes the responses to a snapshot file. In `'playback'` mode
it serves responses from the snapshot file without touching the network. In
`'update'` mode it replays existing snapshots and records any request that has no
matching snapshot.

```mjs
import { SnapshotAgent, setGlobalDispatcher, fetch } from 'undici'

const agent = new SnapshotAgent({
  mode: 'record',
  snapshotPath: './snapshots/api.json'
})
setGlobalDispatcher(agent)

const response = await fetch('https://api.example.com/users')
const users = await response.json()

await agent.close()
```

Constructing a `SnapshotAgent` emits an `ExperimentalWarning` once per process.

## Class: `SnapshotAgent`

<!-- YAML
added: v7.13.0
-->

* Extends: {MockAgent}

The captured interactions are managed by an internal `SnapshotRecorder`, which
is accessible through [`agent.getRecorder()`][]. Because `SnapshotAgent` extends
[`MockAgent`][], all `MockAgent` and [`Dispatcher`][] methods and events are also
available.

### `new SnapshotAgent([options])`

<!-- YAML
added: v7.13.0
-->

* `options` {Object} (optional) Accepts all [`MockAgent`][] options plus the
  following snapshot-specific options.
  * `mode` {string} The snapshot mode. One of `'record'`, `'playback'`, or
    `'update'`. **Default:** `'record'`.
  * `snapshotPath` {string} Path to the snapshot file used for loading and
    saving. Required when `mode` is `'playback'` or `'update'`. **Default:**
    `null`.
  * `maxSnapshots` {number} Maximum number of snapshots to keep in memory.
    **Default:** `Infinity`.
  * `autoFlush` {boolean} When `true`, snapshots are written to `snapshotPath`
    automatically as they are recorded. **Default:** `false`.
  * `flushInterval` {number} Interval, in milliseconds, between automatic flushes
    when `autoFlush` is enabled. **Default:** `30000`.
  * `matchHeaders` {string[]} Names of the only headers to include when matching
    a request against a snapshot. When omitted, all headers are matched.
  * `ignoreHeaders` {string[]} Header names to ignore when matching a request.
  * `excludeHeaders` {string[]} Header names to strip from snapshots entirely,
    so that sensitive values are never written to disk.
  * `matchBody` {boolean} When `true`, the request body is included in request
    matching. **Default:** `true`.
  * `normalizeBody` {Function} Normalizes the request body before it is matched
    or stored, for example to strip volatile fields such as timestamps. Only
    applied when `matchBody` is `true`.
    * `body` {string|Buffer|null|undefined} The raw request body.
    * Returns: {string} The normalized body used for matching.
  * `matchQuery` {boolean} When `true`, query parameters are included in request
    matching. **Default:** `true`.
  * `normalizeQuery` {Function} Normalizes the query string before it is matched,
    for example to strip cache-busting parameters. Only applied when `matchQuery`
    is `true`.
    * `query` {URLSearchParams} The parsed query parameters.
    * Returns: {string} The normalized query string used for matching.
  * `caseSensitive` {boolean} When `true`, header matching is case-sensitive.
    **Default:** `false`.
  * `shouldRecord` {Function} Decides whether a given request is recorded. Only
    used in `'record'` and `'update'` modes.
    * `requestOpts` {Object} The dispatch options of the request.
    * Returns: {boolean} `true` to record the request.
  * `shouldPlayback` {Function} Decides whether a given request is served from a
    snapshot. Only used in `'playback'` and `'update'` modes.
    * `requestOpts` {Object} The dispatch options of the request.
    * Returns: {boolean} `true` to play the request back from a snapshot.
  * `excludeUrls` {Array<string|RegExp>} URL patterns that are excluded from
    recording and playback. String patterns match case-insensitively as
    substrings of the URL; `RegExp` patterns are tested against the URL. Matching
    requests are passed through to a real [`Agent`][] instead.

Creates a new `SnapshotAgent`. Throws an [`InvalidArgumentError`][] when `mode`
is `'playback'` or `'update'` and `snapshotPath` is not provided, and throws an
`InvalidArgumentError` when `mode` is not one of the supported values.

When `mode` is `'playback'` or `'update'` and `snapshotPath` is set, snapshots
are loaded from the file automatically; a missing file is ignored until the first
request requires it.

```mjs
import { SnapshotAgent, setGlobalDispatcher } from 'undici'

const agent = new SnapshotAgent({
  mode: 'playback',
  snapshotPath: './snapshots/api.json'
})
setGlobalDispatcher(agent)
```

The mode controls how requests are handled:

* `'record'` performs the real request and stores the response.

  ```mjs
  import { SnapshotAgent, setGlobalDispatcher, fetch } from 'undici'

  const agent = new SnapshotAgent({
    mode: 'record',
    snapshotPath: './snapshots/api.json'
  })
  setGlobalDispatcher(agent)

  await fetch('https://api.example.com/users')
  await agent.saveSnapshots()
  ```

* `'playback'` serves the recorded response without any network access. A
  request with no matching snapshot rejects with an [`UndiciError`][] whose
  message begins with `No snapshot found`.

  ```mjs
  import { SnapshotAgent, setGlobalDispatcher, fetch } from 'undici'

  const agent = new SnapshotAgent({
    mode: 'playback',
    snapshotPath: './snapshots/api.json'
  })
  setGlobalDispatcher(agent)

  const response = await fetch('https://api.example.com/users')
  ```

* `'update'` replays an existing snapshot when one is found and otherwise records
  the request like `'record'` mode.

  ```mjs
  import { SnapshotAgent, setGlobalDispatcher, fetch } from 'undici'

  const agent = new SnapshotAgent({
    mode: 'update',
    snapshotPath: './snapshots/api.json'
  })
  setGlobalDispatcher(agent)

  await fetch('https://api.example.com/new-endpoint')
  ```

### `agent.loadSnapshots([filePath])`

<!-- YAML
added: v7.13.0
-->

* `filePath` {string} (optional) Path to read snapshots from. **Default:** the
  `snapshotPath` given to the constructor.
* Returns: {Promise<void>} Resolves when the snapshots have been loaded.

Loads snapshots from disk into memory. In `'playback'` mode this also installs
the corresponding [`MockAgent`][] interceptors so that subsequent requests are
matched against the loaded snapshots.

```mjs
await agent.loadSnapshots('./snapshots/api.json')
```

### `agent.saveSnapshots([filePath])`

<!-- YAML
added: v7.13.0
-->

* `filePath` {string} (optional) Path to write snapshots to. **Default:** the
  `snapshotPath` given to the constructor.
* Returns: {Promise<void>} Resolves when the snapshots have been written.

Writes all recorded snapshots to disk.

```mjs
await agent.saveSnapshots('./snapshots/api.json')
```

### `agent.getRecorder()`

<!-- YAML
added: v7.13.0
-->

* Returns: {SnapshotRecorder} The internal recorder that stores the captured
  interactions.

Returns the underlying `SnapshotRecorder`. The recorder exposes lower-level
operations over the captured data, including `record()`, `findSnapshot()`,
`getSnapshots()`, `size()`, and `clear()`. The recorder is an internal type
returned for inspection; it is not exported from the package root.

```mjs
const recorder = agent.getRecorder()
console.log(`Recorded ${recorder.size()} interactions`)
```

### `agent.getMode()`

<!-- YAML
added: v7.13.0
-->

* Returns: {string} The current mode: `'record'`, `'playback'`, or `'update'`.

Returns the mode the agent was constructed with.

### `agent.clearSnapshots()`

<!-- YAML
added: v7.13.0
-->

* Returns: {undefined}

Removes all snapshots from memory.

```mjs
agent.clearSnapshots()
```

### `agent.resetCallCounts()`

<!-- YAML
added: v7.13.0
-->

* Returns: {undefined}

Resets the call count of every snapshot to zero. Useful between tests when the
same agent is reused, since matching a snapshot in `'playback'` mode increments
its call count.

```mjs
agent.resetCallCounts()
```

### `agent.deleteSnapshot(requestOpts)`

<!-- YAML
added: v7.13.0
-->

* `requestOpts` {Object} Dispatch options identifying the snapshot to remove.
* Returns: {boolean} `true` if a matching snapshot was deleted, `false` if none
  was found.

Deletes a single snapshot that matches the given request options.

```mjs
const deleted = agent.deleteSnapshot({
  method: 'GET',
  origin: 'https://api.example.com',
  path: '/users'
})
```

### `agent.getSnapshotInfo(requestOpts)`

<!-- YAML
added: v7.13.0
-->

* `requestOpts` {Object} Dispatch options identifying the snapshot to inspect.
* Returns: {Object|null} Information about the matching snapshot, or `null` when
  none is found.
  * `hash` {string} The hash used to key the snapshot.
  * `request` {Object} The recorded request.
    * `method` {string} The request method.
    * `url` {string} The request URL.
    * `headers` {Record<string, string>} The recorded request headers.
    * `body` {string} The recorded request body, when present.
  * `responseCount` {number} The number of recorded responses for the request.
  * `callCount` {number} The number of times the snapshot has been matched.
  * `timestamp` {string} The ISO timestamp at which the snapshot was recorded.

Returns metadata about a snapshot without returning the response payload.

```mjs
const info = agent.getSnapshotInfo({
  method: 'GET',
  origin: 'https://api.example.com',
  path: '/users'
})
```

### `agent.replaceSnapshots(snapshotData)`

<!-- YAML
added: v7.13.0
-->

* `snapshotData` {Array} The snapshot data to load, replacing any existing
  snapshots.
  * `hash` {string} The hash used to key the snapshot.
  * `snapshot` {Object} The snapshot entry.
* Returns: {undefined}

Replaces all in-memory snapshots with the provided data.

```mjs
const recorder = agent.getRecorder()
const snapshots = recorder.getSnapshots()

agent.replaceSnapshots(snapshots.map((snapshot, index) => ({
  hash: `snapshot-${index}`,
  snapshot
})))
```

### `agent.close()`

<!-- YAML
added: v7.13.0
-->

* Returns: {Promise<void>} Resolves once the agent and its resources are closed.

Closes the agent and releases its resources. In `'record'` and `'update'` modes
the recorder is flushed to disk first. In `'playback'` mode nothing is written,
because matching a snapshot mutates its call count and saving would needlessly
rewrite the snapshot file. The real [`Agent`][], when one was created, and the
underlying [`MockAgent`][] are closed as well.

```mjs
await agent.close()
```

[`Agent`]: Agent.md#class-agent
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`InvalidArgumentError`]: Errors.md#undicierrorsundiciinvalidargumenterror
[`MockAgent`]: MockAgent.md#class-mockagent
[`UndiciError`]: Errors.md#undicierrorsundicierror
[`agent.getRecorder()`]: #agentgetrecorder
[`setGlobalDispatcher()`]: Dispatcher.md#setglobaldispatcherdispatcher
