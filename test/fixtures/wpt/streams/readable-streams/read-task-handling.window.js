// META: global=window,worker
'use strict';

function performMicrotaskCheckpoint() {
    document.createNodeIterator(document, -1, {
        acceptNode() {
            return NodeFilter.FILTER_ACCEPT;
        }
    }).nextNode();
}

test(() => {
    // Add a getter for "then" that will incidentally be invoked
    // during promise resolution.
    Object.prototype.__defineGetter__('then', () => {
        // Clean up behind ourselves.
        delete Object.prototype.then;

        // This promise should (like all promises) be resolved
        // asynchronously.
        var executed = false;
        Promise.resolve().then(_ => { executed = true; });

        // This shouldn't run microtasks!  They should only run
        // after the fetch is resolved.
        performMicrotaskCheckpoint();

        // The fulfill handler above shouldn't have run yet.  If it has run,
        // throw to reject this promise and fail the test.
        assert_false(executed, "shouldn't have run microtasks yet");

        // Otherwise act as if there's no "then" property so the promise
        // fulfills and the test passes.
        return undefined;
    });

    const readable = new ReadableStream({
        pull(c) {
            c.enqueue({});
        }
    }, { highWaterMark: 0 });

    // Create a read request, incidentally resolving a promise with an
    // object value, thereby invoking the getter installed above.
    readable.getReader().read();
}, "reading from a stream should occur in a microtask scope");
