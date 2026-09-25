// This worker calls waitUntil() and respondWith() asynchronously and
// reports back to the test whether they threw.
//
// These test cases are confusing. Bear in mind that the event is active
// (calling waitUntil() is allowed) if:
// * The pending promise count is not 0, or
// * The event dispatch flag is set.

// Controlled by 'init'/'done' messages.
var resolveLockPromise;
var port;

self.addEventListener('message', function(event) {
    var waitPromise;
    var resolveTestPromise;

    switch (event.data.step) {
      case 'init':
        event.waitUntil(new Promise((res) => { resolveLockPromise = res; }));
        port = event.data.port;
        break;
      case 'done':
        resolveLockPromise();
        break;

      // Throws because waitUntil() is called in a task after event dispatch
      // finishes.
      case 'no-current-extension-different-task':
        async_task_waituntil(event).then(reportResultExpecting('InvalidStateError'));
        break;

      // OK because waitUntil() is called in a microtask that runs after the
      // event handler runs, while the event dispatch flag is still set.
      case 'no-current-extension-different-microtask':
        async_microtask_waituntil(event).then(reportResultExpecting('OK'));
        break;

      // OK because the second waitUntil() is called while the first waitUntil()
      // promise is still pending.
      case 'current-extension-different-task':
        event.waitUntil(new Promise((res) => { resolveTestPromise = res; }));
        async_task_waituntil(event).then(reportResultExpecting('OK')).then(resolveTestPromise);
        break;

      // OK because all promises involved resolve "immediately", so the second
      // waitUntil() is called during the microtask checkpoint at the end of
      // event dispatching, when the event dispatch flag is still set.
      case 'during-event-dispatch-current-extension-expired-same-microtask-turn':
        waitPromise = Promise.resolve();
        event.waitUntil(waitPromise);
        waitPromise.then(() => { return sync_waituntil(event); })
          .then(reportResultExpecting('OK'))
        break;

      // OK for the same reason as above.
      case 'during-event-dispatch-current-extension-expired-same-microtask-turn-extra':
        waitPromise = Promise.resolve();
        event.waitUntil(waitPromise);
        waitPromise.then(() => { return async_microtask_waituntil(event); })
          .then(reportResultExpecting('OK'))
        break;


      // OK because the pending promise count is decremented in a microtask
      // queued upon fulfillment of the first waitUntil() promise, so the second
      // waitUntil() is called while the pending promise count is still
      // positive.
      case 'after-event-dispatch-current-extension-expired-same-microtask-turn':
        waitPromise = makeNewTaskPromise();
        event.waitUntil(waitPromise);
        waitPromise.then(() => { return sync_waituntil(event); })
          .then(reportResultExpecting('OK'))
        break;

      // Throws because the second waitUntil() is called after the pending
      // promise count was decremented to 0.
      case 'after-event-dispatch-current-extension-expired-same-microtask-turn-extra':
        waitPromise = makeNewTaskPromise();
        event.waitUntil(waitPromise);
        waitPromise.then(() => { return async_microtask_waituntil(event); })
          .then(reportResultExpecting('InvalidStateError'))
        break;

      // Throws because the second waitUntil() is called in a new task, after
      // first waitUntil() promise settled and the event dispatch flag is unset.
      case 'current-extension-expired-different-task':
        event.waitUntil(Promise.resolve());
        async_task_waituntil(event).then(reportResultExpecting('InvalidStateError'));
        break;

      case 'script-extendable-event':
        self.dispatchEvent(new ExtendableEvent('nontrustedevent'));
        break;
    }

    event.source.postMessage('ACK');
  });

self.addEventListener('fetch', function(event) {
  const path = new URL(event.request.url).pathname;
  const step = path.substring(path.lastIndexOf('/') + 1);
  let response;
  switch (step) {
    // OK because waitUntil() is called while the respondWith() promise is still
    // unsettled, so the pending promise count is positive.
    case 'pending-respondwith-async-waituntil':
      var resolveFetch;
      response = new Promise((res) => { resolveFetch = res; });
      event.respondWith(response);
      async_task_waituntil(event)
        .then(reportResultExpecting('OK'))
        .then(() => { resolveFetch(new Response('OK')); });
      break;

    // OK because all promises involved resolve "immediately", so waitUntil() is
    // called during the microtask checkpoint at the end of event dispatching,
    // when the event dispatch flag is still set.
    case 'during-event-dispatch-respondwith-microtask-sync-waituntil':
      response = Promise.resolve(new Response('RESP'));
      event.respondWith(response);
      response.then(() => { return sync_waituntil(event); })
        .then(reportResultExpecting('OK'));
      break;

    // OK because all promises involved resolve "immediately", so waitUntil() is
    // called during the microtask checkpoint at the end of event dispatching,
    // when the event dispatch flag is still set.
    case 'during-event-dispatch-respondwith-microtask-async-waituntil':
      response = Promise.resolve(new Response('RESP'));
      event.respondWith(response);
      response.then(() => { return async_microtask_waituntil(event); })
        .then(reportResultExpecting('OK'));
      break;

    // OK because the pending promise count is decremented in a microtask queued
    // upon fulfillment of the respondWith() promise, so waitUntil() is called
    // while the pending promise count is still positive.
    case 'after-event-dispatch-respondwith-microtask-sync-waituntil':
      response = makeNewTaskPromise().then(() => {return new Response('RESP');});
      event.respondWith(response);
      response.then(() => { return sync_waituntil(event); })
        .then(reportResultExpecting('OK'));
      break;


    // Throws because waitUntil() is called after the pending promise count was
    // decremented to 0.
    case 'after-event-dispatch-respondwith-microtask-async-waituntil':
      response = makeNewTaskPromise().then(() => {return new Response('RESP');});
      event.respondWith(response);
      response.then(() => { return async_microtask_waituntil(event); })
        .then(reportResultExpecting('InvalidStateError'))
      break;
  }
});

self.addEventListener('nontrustedevent', function(event) {
    sync_waituntil(event).then(reportResultExpecting('InvalidStateError'));
  });

function reportResultExpecting(expectedResult) {
  return function (result) {
    port.postMessage({result : result, expected: expectedResult});
    return result;
  };
}

function sync_waituntil(event) {
  return new Promise((res, rej) => {
    try {
      event.waitUntil(Promise.resolve());
      res('OK');
    } catch (error) {
      res(error.name);
    }
  });
}

function async_microtask_waituntil(event) {
  return new Promise((res, rej) => {
    Promise.resolve().then(() => {
      try {
        event.waitUntil(Promise.resolve());
        res('OK');
      } catch (error) {
        res(error.name);
      }
    });
  });
}

function async_task_waituntil(event) {
  return new Promise((res, rej) => {
    setTimeout(() => {
      try {
        event.waitUntil(Promise.resolve());
        res('OK');
      } catch (error) {
        res(error.name);
      }
    }, 0);
  });
}

// Returns a promise that settles in a separate task.
function makeNewTaskPromise() {
  return new Promise(resolve => {
    setTimeout(resolve, 0);
  });
}
