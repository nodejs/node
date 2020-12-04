'use strict';

const common = require('../common');
const assert = require('assert');
const {
  BroadcastChannel,
} = require('worker_threads');

{
  const c1 = new BroadcastChannel('eventType').unref();
  const c2 = new BroadcastChannel('eventType');

  c2.onmessage = common.mustCall((e) => {
    assert(e instanceof MessageEvent);
    assert.strictEqual(e.target, c2);
    assert.strictEqual(e.type, 'message');
    assert.strictEqual(e.data, 'hello world');
    c2.close();
  });
  c1.postMessage('hello world');
}

{
  // Messages are delivered in port creation order.
  // TODO(@jasnell): The ordering here is different than
  // what the browsers would implement due to the different
  // dispatching algorithm under the covers. What's not
  // immediately clear is whether the ordering is spec
  // mandated. In this test, c1 should receive events
  // first, then c2, then c3. In the Node.js dispatching
  // algorithm this means the ordering is:
  //    from c3    (c1 from c3)
  //    done       (c1 from c2)
  //    from c1    (c2 from c1)
  //    from c3    (c2 from c3)
  //    from c1    (c3 from c1)
  //    done       (c3 from c2)
  //
  // Whereas in the browser-ordering (as illustrated in the
  // Web Platform Tests) it would be:
  //    from c1    (c2 from c1)
  //    from c1    (c3 from c1)
  //    from c3    (c1 from c3)
  //    from c3    (c2 from c3)
  //    done       (c1 from c2)
  //    done       (c3 from c2)
  const c1 = new BroadcastChannel('order');
  const c2 = new BroadcastChannel('order');
  const c3 = new BroadcastChannel('order');

  const events = [];
  let doneCount = 0;
  const handler = common.mustCall((e) => {
    events.push(e);
    if (e.data === 'done') {
      doneCount++;
      if (doneCount === 2) {
        assert.strictEqual(events.length, 6);
        // TODO: Don't skip Windows once ordering is fixed per comment above.
        // Right now, the ordering for Windows is unreliable.
        if (!common.isWindows) {
          assert.strictEqual(events[0].data, 'from c3');
          assert.strictEqual(events[1].data, 'done');
          assert.strictEqual(events[2].data, 'from c1');
          assert.strictEqual(events[3].data, 'from c3');
          assert.strictEqual(events[4].data, 'from c1');
          assert.strictEqual(events[5].data, 'done');
        }
        c1.close();
        c2.close();
        c3.close();
      }
    }
  }, 6);
  c1.onmessage = handler;
  c2.onmessage = handler;
  c3.onmessage = handler;

  c1.postMessage('from c1');
  c3.postMessage('from c3');
  c2.postMessage('done');
}

{
  // Messages aren't delivered to a closed port
  const c1 = new BroadcastChannel('closed1').unref();
  const c2 = new BroadcastChannel('closed1');
  const c3 = new BroadcastChannel('closed1');

  c2.onmessage = common.mustNotCall();
  c2.close();
  c3.onmessage = common.mustCall(() => c3.close());
  c1.postMessage('test');
}

{
  // Messages aren't delivered to a port closed after calling postMessage.
  const c1 = new BroadcastChannel('closed2').unref();
  const c2 = new BroadcastChannel('closed2');
  const c3 = new BroadcastChannel('closed2');

  c2.onmessage = common.mustNotCall();
  c3.onmessage = common.mustCall(() => c3.close());
  c1.postMessage('test');
  c2.close();
}

{
  // Closing and creating channels during message delivery works correctly
  const c1 = new BroadcastChannel('create-in-onmessage').unref();
  const c2 = new BroadcastChannel('create-in-onmessage');

  c2.onmessage = common.mustCall((e) => {
    assert.strictEqual(e.data, 'first');
    c2.close();
    const c3 = new BroadcastChannel('create-in-onmessage');
    c3.onmessage = common.mustCall((event) => {
      assert.strictEqual(event.data, 'done');
      c3.close();
    });
    c1.postMessage('done');
  });
  c1.postMessage('first');
  c2.postMessage('second');
}

{
  // TODO: Fix failure on Windows CI. Skipping for now.
  if (!common.isWindows) {
    // Closing a channel in onmessage prevents already queued tasks
    // from firing onmessage events
    const c1 = new BroadcastChannel('close-in-onmessage2').unref();
    const c2 = new BroadcastChannel('close-in-onmessage2');
    const c3 = new BroadcastChannel('close-in-onmessage2');
    const events = [];
    c1.onmessage = (e) => events.push('c1: ' + e.data);
    c2.onmessage = (e) => events.push('c2: ' + e.data);
    c3.onmessage = (e) => events.push('c3: ' + e.data);

    // c2 closes itself when it receives the first message
    c2.addEventListener('message', common.mustCall(() => c2.close()));

    c3.addEventListener('message', common.mustCall((e) => {
      if (e.data === 'done') {
        assert.deepStrictEqual(events, [
          'c2: first',
          'c3: first',
          'c3: done']);
        c3.close();
      }
    }, 2));
    c1.postMessage('first');
    c1.postMessage('done');
  }
}
