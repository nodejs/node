# `RemoteContext`: API for script execution in another context

`RemoteContext` in `/common/dispatcher/dispatcher.js` provides an interface to
execute JavaScript in another global object (page or worker, the "executor"),
based on:

- [WPT RFC 88: context IDs from uuid searchParams in URL](https://github.com/web-platform-tests/rfcs/pull/88),
- [WPT RFC 89: execute_script](https://github.com/web-platform-tests/rfcs/pull/89) and
- [WPT RFC 91: RemoteContext](https://github.com/web-platform-tests/rfcs/pull/91).

Tests can send arbitrary javascript to executors to evaluate in its global
object, like:

```
// injector.html
const argOnLocalContext = ...;

async function execute() {
  window.open('executor.html?uuid=' + uuid);
  const ctx = new RemoteContext(uuid);
  await ctx.execute_script(
      (arg) => functionOnRemoteContext(arg),
      [argOnLocalContext]);
};
```

and on executor:

```
// executor.html
function functionOnRemoteContext(arg) { ... }

const uuid = new URLSearchParams(window.location.search).get('uuid');
const executor = new Executor(uuid);
```

For concrete examples, see
[events.html](../../html/browsers/browsing-the-web/back-forward-cache/events.html)
and
[executor.html](../../html/browsers/browsing-the-web/back-forward-cache/resources/executor.html)
in back-forward cache tests.

Note that `executor*` files under `/common/dispatcher/` are NOT for
`RemoteContext.execute_script()`. Use `remote-executor.html` instead.

This is universal and avoids introducing many specific `XXX-helper.html`
resources.
Moreover, tests are easier to read, because the whole logic of the test can be
defined in a single file.

## `new RemoteContext(uuid)`

- `uuid` is a UUID string that identifies the remote context and should match
  with the `uuid` parameter of the URL of the remote context.
- Callers should create the remote context outside this constructor (e.g.
  `window.open('executor.html?uuid=' + uuid)`).

## `RemoteContext.execute_script(fn, args)`

- `fn` is a JavaScript function to execute on the remote context, which is
  converted to a string using `toString()` and sent to the remote context.
- `args` is null or an array of arguments to pass to the function on the
  remote context. Arguments are passed as JSON.
- If the return value of `fn` when executed in the remote context is a promise,
  the promise returned by `execute_script` resolves to the resolved value of
  that promise. Otherwise the `execute_script` promise resolves to the return
  value of `fn`.

Note that `fn` is evaluated on the remote context (`executor.html` in the
example above), while `args` are evaluated on the caller context
(`injector.html`) and then passed to the remote context.

## Return value of injected functions and `execute_script()`

If the return value of the injected function when executed in the remote
context is a promise, the promise returned by `execute_script` resolves to the
resolved value of that promise. Otherwise the `execute_script` promise resolves
to the return value of the function.

When the return value of an injected script is a Promise, it should be resolved
before any navigation starts on the remote context. For example, it shouldn't
be resolved after navigating out and navigating back to the page again.
It's fine to create a Promise to be resolved after navigations, if it's not the
return value of the injected function.

## Calling timing of `execute_script()`

When `RemoteContext.execute_script()` is called when the remote context is not
active (for example before it is created, before navigation to the page, or
during the page is in back-forward cache), the injected script is evaluated
after the remote context becomes active.

Multiple calls to `RemoteContext.execute_script()` will result in multiple scripts
being executed in remote context and ordering will be maintained.

## Errors from `execute_script()`

Errors from `execute_script()` will result in promise rejections, so it is
important to await the result.  This can be `await ctx.execute_script(...)` for
every call but if there are multiple scripts to executed, it may be preferable
to wait on them in parallel to avoid incurring full round-trip time for each,
e.g.

```js
await Promise.all(
  ctx1.execute_script(...),
  ctx1.execute_script(...),
  ctx2.execute_script(...),
  ctx2.execute_script(...),
  ...
)
```

## Evaluation timing of injected functions

The script injected by `RemoteContext.execute_script()` can be evaluated any
time during the remote context is active.
For example, even before DOMContentLoaded events or even during navigation.
It's the responsibility of test-specific code/helpers to ensure evaluation
timing constraints (which can be also test-specific), if any needed.

### Ensuring evaluation timing around page load

For example, to ensure that injected functions (`mainFunction` below) are
evaluated after the first `pageshow` event, we can use pure JavaScript code
like below:

```
// executor.html
window.pageShowPromise = new Promise(resolve =>
    window.addEventListener('pageshow', resolve, {once: true}));


// injector.html
const waitForPageShow = async () => {
  while (!window.pageShowPromise) {
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  await window.pageShowPromise;
};

await ctx.execute(waitForPageShow);
await ctx.execute(mainFunction);
```

### Ensuring evaluation timing around navigation out/unloading

It can be important to ensure there are no injected functions nor code behind
`RemoteContext` (such as Fetch APIs accessing server-side stash) running after
navigation is initiated, for example in the case of back-forward cache testing.

To ensure this,

- Do not call the next `RemoteContext.execute()` for the remote context after
  triggering the navigation, until we are sure that the remote context is not
  active (e.g. after we confirm that the new page is loaded).
- Call `Executor.suspend(callback)` synchronously within the injected script.
  This suspends executor-related code, and calls `callback` when it is ready
  to start navigation.

The code on the injector side would be like:

```
// injector.html
await ctx.execute_script(() => {
  executor.suspend(() => {
    location.href = 'new-url.html';
  });
});
```

## Future Work: Possible integration with `test_driver`

Currently `RemoteContext` is implemented by JavaScript and WPT-server-side
stash, and not integrated with `test_driver` nor `testharness`.
There is a proposal of `test_driver`-integrated version (see the RFCs listed
above).

The API semantics and guidelines in this document are designed to be applicable
to both the current stash-based `RemoteContext` and `test_driver`-based
version, and thus the tests using `RemoteContext` will be migrated with minimum
modifications (mostly in `/common/dispatcher/dispatcher.js` and executors), for
example in a
[draft CL](https://chromium-review.googlesource.com/c/chromium/src/+/3082215/).


# `send()`/`receive()` Message passing APIs

`dispatcher.js` (and its server-side backend `dispatcher.py`) provides a
universal queue-based message passing API.
Each queue is identified by a UUID, and accessed via the following APIs:

-   `send(uuid, message)` pushes a string `message` to the queue `uuid`.
-   `receive(uuid)` pops the first item from the queue `uuid`.
-   `showRequestHeaders(origin, uuid)` and
    `cacheableShowRequestHeaders(origin, uuid)` return URLs, that push request
    headers to the queue `uuid` upon fetching.

It works cross-origin, and even access different browser context groups.

Messages are queued, this means one doesn't need to wait for the receiver to
listen, before sending the first message
(but still need to wait for the resolution of the promise returned by `send()`
to ensure the order between `send()`s).

## Executors

Similar to `RemoteContext.execute_script()`, `send()`/`receive()` can be used
for sending arbitrary javascript to be evaluated in another page or worker.

- `executor.html` (as a Document),
- `executor-worker.js` (as a Web Worker), and
- `executor-service-worker.js` (as a Service Worker)

are examples of executors.
Note that these executors are NOT compatible with
`RemoteContext.execute_script()`.

## Future Work

`send()`, `receive()` and the executors below are kept for COEP/COOP tests.

For remote script execution, new tests should use
`RemoteContext.execute_script()` instead.

For message passing,
[WPT RFC 90](https://github.com/web-platform-tests/rfcs/pull/90) is still under
discussion.
