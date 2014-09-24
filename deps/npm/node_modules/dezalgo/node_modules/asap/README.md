
# ASAP

This `asap` CommonJS package contains a single `asap` module that
exports a single `asap` function that executes a function **as soon as
possible**.

```javascript
asap(function () {
    // ...
});
```

More formally, ASAP provides a fast event queue that will execute tasks
until it is empty before yielding to the JavaScript engine's underlying
event-loop.  When the event queue becomes non-empty, ASAP schedules a
flush event, preferring for that event to occur before the JavaScript
engine has an opportunity to perform IO tasks or rendering, thus making
the first task and subsequent tasks semantically indistinguishable.
ASAP uses a variety of techniques to preserve this invariant on
different versions of browsers and NodeJS.

By design, ASAP can starve the event loop on the theory that, if there
is enough work to be done synchronously, albeit in separate events, long
enough to starve input or output, it is a strong indicator that the
program needs to push back on scheduling more work.

Take care.  ASAP can sustain infinite recursive calls indefinitely
without warning.  This is behaviorally equivalent to an infinite loop.
It will not halt from a stack overflow, but it *will* chew through
memory (which is an oddity I cannot explain at this time).  Just as with
infinite loops, you can monitor a Node process for this behavior with a
heart-beat signal.  As with infinite loops, a very small amount of
caution goes a long way to avoiding problems.

```javascript
function loop() {
    asap(loop);
}
loop();
```

ASAP is distinct from `setImmediate` in that it does not suffer the
overhead of returning a handle and being possible to cancel.  For a
`setImmediate` shim, consider [setImmediate][].

[setImmediate]: https://github.com/noblejs/setimmediate

If a task throws an exception, it will not interrupt the flushing of
high-priority tasks.  The exception will be postponed to a later,
low-priority event to avoid slow-downs, when the underlying JavaScript
engine will treat it as it does any unhandled exception.

## Heritage

ASAP has been factored out of the [Q][] asynchronous promise library.
It originally had a naïve implementation in terms of `setTimeout`, but
[Malte Ubl][NonBlocking] provided an insight that `postMessage` might be
useful for creating a high-priority, no-delay event dispatch hack.
Since then, Internet Explorer proposed and implemented `setImmediate`.
Robert Kratić began contributing to Q by measuring the performance of
the internal implementation of `asap`, paying particular attention to
error recovery.  Domenic, Robert, and I collectively settled on the
current strategy of unrolling the high-priority event queue internally
regardless of what strategy we used to dispatch the potentially
lower-priority flush event.  Domenic went on to make ASAP cooperate with
NodeJS domains.

[Q]: https://github.com/kriskowal/q
[NonBlocking]: http://www.nonblocking.io/2011/06/windownexttick.html

For further reading, Nicholas Zakas provided a thorough article on [The
Case for setImmediate][NCZ].

[NCZ]: http://www.nczonline.net/blog/2013/07/09/the-case-for-setimmediate/

## License

Copyright 2009-2013 by Contributors
MIT License (enclosed)

