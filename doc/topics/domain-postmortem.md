# Domain Module Postmortem

## Usability Issues

### Implicit Behavior

It's possible for a developer to create a new domain and then simply run
`domain.enter()`. Which then acts as a catch-all for any exception in the
future that couldn't be observed by the thrower. Allowing a module author to
intercept the exceptions of unrelated code in a different module. Preventing
the originator of the code from knowing about its own exceptions.

Here's an example of how one indirectly linked modules can affect another:

```js
// module a.js
const b = require('./b');
const c = require('./c');


// module b.js
const d = require('domain').create();
d.on('error', () => { /* silence everything */ });
d.enter();


// module c.js
const dep = require('some-dep');
dep.method();  // Uh-oh! This method doesn't actually exist.
```

Since module `b` enters the domain but never exits any uncaught exception will
be swallowed. Leaving module `c` in the dark as to why it didn't run the entire
script. Leaving a potentially partially populated `module.exports`. Doing this
is not the same as listening for `'uncaughtException'`. As the latter is
explicitly meant to globally catch errors. The other issue is that domains are
processed prior to any `'uncaughtException'` handlers, and prevent them from
running.

Another issue is that domains route errors automatically if no `'error'`
handler was set on the event emitter. There is no opt-in mechanism for this,
and automatically propagates across the entire asynchronous chain. This may
seem useful at first, but once asynchronous calls are two or more modules deep
and one of them doesn't include an error handler the creator of the domain will
suddenly be catching unexpected exceptions, and the thrower's exception will go
unnoticed by the author.

The following is a simple example of how a missing `'error'` handler allows
the active domain to hijack the error:

```js
const domain = require('domain');
const net = require('net');
const d = domain.create();
d.on('error', (err) => console.error(err.message));

d.run(() => net.createServer((c) => {
  c.end();
  c.write('bye');
}).listen(8000));
```

Even manually removing the connection via `d.remove(c)` does not prevent the
connection's error from being automatically intercepted.

Failures that plagues both error routing and exception handling are the
inconsistencies in how errors are bubbled. The following is an example of how
nested domains will and won't bubble the exception based on when they happen:

```js
const domain = require('domain');
const net = require('net');
const d = domain.create();
d.on('error', () => console.error('d intercepted an error'));

d.run(() => {
  const server = net.createServer((c) => {
    const e = domain.create();  // No 'error' handler being set.
    e.run(() => {
      // This will not be caught by d's error handler.
      setImmediate(() => {
        throw new Error('thrown from setImmediate');
      });
      // Though this one will bubble to d's error handler.
      throw new Error('immediately thrown');
    });
  }).listen(8080);
});
```

It may be expected that nested domains always remain nested, and will always
propagate the exception up the domain stack. Or that exceptions will never
automatically bubble. Unfortunately both these situations occur, leading to
potentially confusing behavior that may even be prone to difficult to debug
timing conflicts.


### API Gaps

While APIs based on using `EventEmitter` can use `bind()` and errback style
callbacks can use `intercept()`, alternative APIs that implicitly bind to the
active domain must be executed inside of `run()`. Meaning if module authors
wanted to support domains using a mechanism alternative to those mentioned they
must manually implement domain support themselves. Instead of being able to
leverage the implicit mechanisms already in place.


### Error Propagation

Propagating errors across nested domains is not straight forward, if even
possible. Existing documentation shows a simple example of how to `close()` an
`http` server if there is an error in the request handler. What it does not
explain is how to close the server if the request handler creates another
domain instance for another async request. Using the following as a simple
example of the failing of error propagation:

```js
const d1 = domain.create();
d1.foo = true;  // custom member to make more visible in console
d1.on('error', (er) => { /* handle error */ });

d1.run(() => setTimeout(() => {
  const d2 = domain.create();
  d2.bar = 43;
  d2.on('error', (er) => console.error(er.message, domain._stack));
  d2.run(() => {
    setTimeout(() => {
      setTimeout(() => {
        throw new Error('outer');
      });
      throw new Error('inner')
    });
  });
}));
```

Even in the case that the domain instances are being used for local storage so
access to resources are made available there is still no way to allow the error
to continue propagating from `d2` back to `d1`. Quick inspection may tell us
that simply throwing from `d2`'s domain `'error'` handler would allow `d1` to
then catch the exception and execute its own error handler. Though that is not
the case. Upon inspection of `domain._stack` you'll see that the stack only
contains `d2`.

This may be considered a failing of the API, but even if it did operate in this
way there is still the issue of transmitting the fact that a branch in the
asynchronous execution has failed, and that all further operations in that
branch must cease. In the example of the http request handler, if we fire off
several asynchronous requests and each one then `write()`'s data back to the
client many more errors will arise from attempting to `write()` to a closed
handle. More on this in _Resource Cleanup on Exception_.


### Resource Cleanup on Exception

The script [`domain-resource-cleanup-example.js`][]
contains a more complex example of properly cleaning up in a small resource
dependency tree in the case that an exception occurs in a given connection or
any of its dependencies. Breaking down the script into its basic operations:

- When a new connection happens, concurrently:
  - Open a file on the file system
  - Open Pipe to unique socket
- Read a chunk of the file asynchronously
- Write chunk to both the TCP connection and any listening sockets
- If any of these resources error, notify all other attached resources that
  they need to clean up and shutdown

As we can see from this example a lot more must be done to properly clean up
resources when something fails than what can be done strictly through the
domain API. All that domains offer is an exception aggregation mechanism. Even
the potentially useful ability to propagate data with the domain is easily
countered, in this example, by passing the needed resources as a function
argument.

One problem domains perpetuated was the supposed simplicity of being able to
continue execution, contrary to what the documentation stated, of the
application despite an unexpected exception. This example demonstrates the
fallacy behind that idea.

Attempting proper resource cleanup on unexpected exception becomes more complex
as the application itself grows in complexity. This example only has 3 basic
resources in play, and all of them with a clear dependency path. If an
application uses something like shared resources or resource reuse the ability
to cleanup, and properly test that cleanup has been done, grows greatly.

In the end, in terms of handling errors, domains aren't much more than a
glorified `'uncaughtException'` handler. Except with more implicit and
unobservable behavior by third-parties.


### Resource Propagation

Another use case for domains was to use it to propagate data along asynchronous
data paths. One problematic point is the ambiguity of when to expect the
correct domain when there are multiple in the stack (which must be assumed if
the async stack works with other modules). Also the conflict between being
able to depend on a domain for error handling while also having it available to
retrieve the necessary data.

The following is a involved example demonstrating the failing using domains to
propagate data along asynchronous stacks:

```js
const domain = require('domain');
const net = require('net');

const server = net.createServer((c) => {
  // Use a domain to propagate data across events within the
  // connection so that we don't have to pass arguments
  // everywhere.
  const d = domain.create();
  d.data = { connection: c };
  d.add(c);
  // Mock class that does some useless async data transformation
  // for demonstration purposes.
  const ds = new DataStream(dataTransformed);
  c.on('data', (chunk) => ds.data(chunk));
}).listen(8080, () => console.log(`listening on 8080`));

function dataTransformed(chunk) {
  // FAIL! Because the DataStream instance also created a
  // domain we have now lost the active domain we had
  // hoped to use.
  domain.active.data.connection.write(chunk);
}

function DataStream(cb) {
  this.cb = cb;
  // DataStream wants to use domains for data propagation too!
  // Unfortunately this will conflict with any domain that
  // already exists.
  this.domain = domain.create();
  this.domain.data = { inst: this };
}

DataStream.prototype.data = function data(chunk) {
  // This code is self contained, but pretend it's a complex
  // operation that crosses at least one other module. So
  // passing along "this", etc., is not easy.
  this.domain.run(function() {
    // Simulate an async operation that does the data transform.
    setImmediate(() => {
      for (var i = 0; i < chunk.length; i++)
        chunk[i] = ((chunk[i] + Math.random() * 100) % 96) + 33;
      // Grab the instance from the active domain and use that
      // to call the user's callback.
      const self = domain.active.data.inst;
      self.cb.call(self, chunk);
    });
  });
};
```

The above shows that it is difficult to have more than one asynchronous API
attempt to use domains to propagate data. This example could possibly be fixed
by assigning `parent: domain.active` in the `DataStream` constructor.  Then
restoring it via `domain.active = domain.active.data.parent` just before the
user's callback is called. Also the instantiation of `DataStream` in the
`'connection'` callback must be run inside `d.run()`, instead of simply using
`d.add(c)`, otherwise there will be no active domain.

In short, for this to have a prayer of a chance usage would need to strictly
adhere to a set of guidelines that would be difficult to enforce or test.


## Performance Issues

A significant deterrent from using domains is the overhead. Using node's
built-in http benchmark, `http_simple.js`, without domains it can handle over
22,000 requests/second. Whereas if it's run with `NODE_USE_DOMAINS=1` that
number drops down to under 17,000 requests/second. In this case there is only
a single global domain. If we edit the benchmark so the http request callback
creates a new domain instance performance drops further to 15,000
requests/second.

While this probably wouldn't affect a server only serving a few hundred or even
a thousand requests per second, the amount of overhead is directly proportional
to the number of asynchronous requests made. So if a single connection needs to
connect to several other services all of those will contribute to the overall
latency of delivering the final product to the client.

Using `AsyncWrap` and tracking the number of times
`init`/`pre`/`post`/`destroy` are called in the mentioned benchmark we find
that the sum of all events called is over 170,000 times per second. This means
even adding 1 microsecond overhead per call for any type of setup or tear down
will result in a 17% performance loss. Granted, this is for the optimized
scenario of the benchmark, but I believe this demonstrates the necessity for a
mechanism such as domain to be as cheap to run as possible.


## Looking Ahead

The domain module has been soft deprecated since Dec 2014, but has not yet been
removed because node offers no alternative functionality at the moment. As of
this writing there is ongoing work building out the `AsyncWrap` API and a
proposal for Zones being prepared for the TC39. At such time there is suitable
functionality to replace domains it will undergo the full deprecation cycle and
eventually be removed from core.

[`domain-resource-cleanup-example.js`]: ./domain-resource-cleanup-example.js
