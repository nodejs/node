# FakeRegistry

This is a replacement for npm-registry-mock in times where its fixtures are
not used.  (Adding support for its standard fixtures is TODO, but should be
straightforward—tacks-ify them and call `mr.mock…`

# Usage

The intent is for this to be a drop in replacement for npm-registry-mock
(and by extension hock).  New scripts will be better served using its native
interface, however.

# Main Interface

## Logging

All requests the mock registry receives are logged at the `http` level. You can
see these by running your tests with:

```
npm --loglevel=http run tap test/tap/name-of-test.js
```

Or directly with:

```
npm_config_loglevel=http node test/tap/name-of-test.js
```

## Construction

Ordinarily there's no reason to construct more than one FakeRegistyr object
at a time, as it can be entirely reset between tests, so best practice
would be to use its singleton.

```
const common = require('../common-tap.js')
const mr = common.mockRegistry
```

If you have need of multiple registries at the same time, you can construct
them by hand:

```
const common = require('../common-tap.js')
const FakeRegistry = common.mockRegistry.FakeRegistry
const mr = new FakeRegistry(opts)
```

## new FakeRegistry(opts)

Valid options are:

* `opts.port` is the first port to try when looking for an available port.  If it
  is unavialable it will be incremented until one available is found.

  The default value of `port` is taken from `common.npm`.

* `opts.keepNodeAlive` will instruct FakeRegistry to not unref the
  underlying server.

## mr.reset() → this

Reset all mocks to their default values. Further requests

## mr.listen() → Promise(mr)

Start listening for connections.  The promise resolves when the server is
online and ready to accept connections.

`mr.port` and `mr.registry` contain the port that was actually selected.

To ease compatibility, `common` will also have its `port` and `registry`
values updated at this time. Note that this means `common.port` refers
to the port of the most recent listening server. Each server will maintain
its own `mr.port`.

Any errors emitted by the server while it was coming online will result in a
promise rejection.

## mr.mock(method, url, respondWith) → this

Adds a new route that matches `method` (should be all caps) and `url`.

`respondWith` can be:

* A function, that takes `(request, response)` as arguments and calls
  [`response` methods](https://nodejs.org/api/http.html#http_class_http_serverresponse)
  to do what it wants.  Does not have a return value.  This function may be
  async (the response isn't complete till its stream completes), typically
  either because you piped something into it or called `response.end()`.
* An array of `[statusCode, responseBody]`. `responseBody` may be a string or
  an object. Objects are serialized with `JSON.stringify`.

## mr.close() → Promise(mr)

Calls `mr.reset()` to clear the mocks.

Calls `.close()` on the http server.  The promise resolves when the http
server completes closing.  Any errors while the http server was closing will
result in a rejection. If running with `keepNodeAlive` set this call
is required for node to exit the event loop.

# Events

## mr.on('error', err => { … })

Error events from the http server are forwarded to the associated fake
registry instance.

The exception to this is while the `mr.listen()` and `mr.close()` promises
are waiting to complete. Those promises capture any errors during their duration
and turn them into rejections. (Errors during those phases are very rare.)

# Compat Interface

## Differences

### Ports

You aren't guaranteed to get the port you ask for.  If the port you asked
for is in use, it will be incremented until an available port is found.

`server.port` and `server.registry` contain the port that was actually selected.

For compatibility reasons:

`common.port` and `common.registry` will contain the port of the most recent
instance of FakeRegistry.  Usually these there is only one instance and so
this has the same value as the per-server attributes.

This means that if your fixtures make use of the port or server address they
need to be configured _after_ you construct

### Request Bodies

Request bodies are NOT matched against. Two routes for the same URL but different
request bodies will overwrite one another.

### Call Count Assertions

That is, things like `twice()` that assert that the end point will be hit
two times are not supported.  This library does not provide any assertions,
just a barebones http server.

### Default Route Behavior

If no route can be found then a `404` response will be provided.

## Construction

const common = require('../common-tap.js')
const mr = common.mockRegistry.compat

### mr(options[, callback]) → Promise(server)

Construct a new mock server.  Hybrid, callback/promise constructor. Options
are `port` and `keepNodeAlive`.  `keepNodeAlive` defaults to `true` for
compatibility mode and the default value of port comes from `common.port`.

### done()

Resets all of the configured mocks.

### close()

Calls `this.reset()` and `this.server.close()`.  To reopen this instance use
`this.listen()`.

### filteringRequestBody()

Does nothing. Bodies are never matched when routing anyway so this is unnecessary.

### get(url) → MockReply
### delete(url) → MockReply
### post(url, body) → MockReply
### put(url, body) → MockReply

Begins to add a route for an HTTP method and URL combo.  Does not actually
add it till `reply()` is called on the returned object.

Note that the body argument for post and put is always ignored.

## MockReply methods

### twice() → this

Does nothing. Call count assertions are not supported.

### reply(status, responseBody)

Actually adds the route, set to reply with the associated status and
responseBody.

Currently no mime-types are set.

If `responseBody` is `typeof === 'object'` then `JSON.stringify()` will be
called on it to serialize it, otherwise `String()` will be used.
