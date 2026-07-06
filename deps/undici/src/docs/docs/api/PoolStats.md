# PoolStats

<!--introduced_in=v4.15.0-->
<!--type=module-->
<!-- source_link=lib/util/stats.js -->

> Stability: 2 - Stable

`PoolStats` exposes a read-only snapshot of the aggregate connection and request
counters for a [`Pool`][] or [`BalancedPool`][]. An instance is created lazily
each time the `pool.stats` getter is accessed, so the values reflect the state of
the pool at the moment of access.

`PoolStats` is not constructed directly in normal use; obtain an instance from
the `stats` property of a pool:

```mjs
import { Pool } from 'undici'

const pool = new Pool('http://localhost:3000')
const { stats } = pool

console.log(stats.connected, stats.running, stats.size)
```

## Class: `PoolStats`

<!-- YAML
added: v4.15.0
changes:
  - version: v7.9.0
    pr-url: https://github.com/nodejs/undici/pull/4157
    description: Pool and client stats are exposed through `Agent`.
-->

Each property is a plain number computed from the pool's internal state when the
instance is created. The instance is a snapshot; read `pool.stats` again to
observe later changes.

### `new PoolStats(pool)`

<!-- YAML
added: v4.15.0
changes:
  - version: v7.9.0
    pr-url: https://github.com/nodejs/undici/pull/4157
    description: Pool and client stats are exposed through `Agent`.
-->

* `pool` {Pool|BalancedPool} The pool to read counters from.

Creates a new `PoolStats` instance from the current state of `pool`. Prefer the
`pool.stats` getter, which returns a fresh `PoolStats` on each access.

### `poolStats.connected`

<!-- YAML
added: v4.15.0
changes:
  - version: v7.9.0
    pr-url: https://github.com/nodejs/undici/pull/4157
    description: Pool and client stats are exposed through `Agent`.
-->

* Type: {number}

Number of open socket connections in this pool.

### `poolStats.free`

<!-- YAML
added: v4.15.0
changes:
  - version: v7.9.0
    pr-url: https://github.com/nodejs/undici/pull/4157
    description: Pool and client stats are exposed through `Agent`.
-->

* Type: {number}

Number of open socket connections in this pool that do not have an active
request.

### `poolStats.pending`

<!-- YAML
added: v4.15.0
changes:
  - version: v7.9.0
    pr-url: https://github.com/nodejs/undici/pull/4157
    description: Pool and client stats are exposed through `Agent`.
-->

* Type: {number}

Number of pending requests across all clients in this pool.

### `poolStats.queued`

<!-- YAML
added: v4.15.0
changes:
  - version: v7.9.0
    pr-url: https://github.com/nodejs/undici/pull/4157
    description: Pool and client stats are exposed through `Agent`.
-->

* Type: {number}

Number of queued requests across all clients in this pool.

### `poolStats.running`

<!-- YAML
added: v4.15.0
changes:
  - version: v7.9.0
    pr-url: https://github.com/nodejs/undici/pull/4157
    description: Pool and client stats are exposed through `Agent`.
-->

* Type: {number}

Number of currently active requests across all clients in this pool.

### `poolStats.size`

<!-- YAML
added: v4.15.0
changes:
  - version: v7.9.0
    pr-url: https://github.com/nodejs/undici/pull/4157
    description: Pool and client stats are exposed through `Agent`.
-->

* Type: {number}

Number of active, pending, or queued requests across all clients in this pool.

[`BalancedPool`]: BalancedPool.md#class-balancedpool
[`Pool`]: Pool.md#class-pool
