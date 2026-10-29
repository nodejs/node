# ClientStats

<!--introduced_in=v7.9.0-->
<!--type=module-->
<!-- source_link=lib/util/stats.js -->

> Stability: 2 - Stable

A `ClientStats` is a point-in-time snapshot of the connection and request
state of a single [`Client`][]. Instances are not created directly; a fresh
snapshot is produced each time the [`client.stats`][] getter is read.

```mjs
import { Client } from 'undici'

const client = new Client('http://localhost:3000')
const stats = client.stats
console.log(stats.connected, stats.running)
```

```cjs
const { Client } = require('undici')

const client = new Client('http://localhost:3000')
const stats = client.stats
console.log(stats.connected, stats.running)
```

## Class: `ClientStats`

<!-- YAML
added: v7.9.0
-->

Captures the connection and request counters of a [`Client`][] at the moment
the snapshot is taken. The values are read from the client at construction time
and do not update afterwards; read [`client.stats`][] again for current values.

### `new ClientStats(client)`

<!-- YAML
added: v7.9.0
-->

* `client` {Client} The client to read the statistics from.

Creates a snapshot from the supplied `client`. This constructor is used
internally by the [`client.stats`][] getter and is not normally invoked by
application code.

### `clientStats.connected`

<!-- YAML
added: v7.9.0
-->

* Type: {boolean}

`true` when the client currently has an open socket connection.

### `clientStats.pending`

<!-- YAML
added: v7.9.0
-->

* Type: {number}

Number of open socket connections of this client that do not have an active
request.

### `clientStats.running`

<!-- YAML
added: v7.9.0
-->

* Type: {number}

Number of currently active requests of this client.

### `clientStats.size`

<!-- YAML
added: v7.9.0
-->

* Type: {number}

Number of active, pending, or queued requests of this client.

[`Client`]: Client.md#class-client
[`client.stats`]: Client.md#clientstats
