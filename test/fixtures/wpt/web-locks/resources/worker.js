'use strict';

// Map of id => function that releases a lock.

const held = new Map();
let next_lock_id = 1;

function processMessage(e) {
  const target = this;

  function respond(data) {
    target.postMessage(Object.assign(data, {rqid: e.data.rqid}));
  }

  switch (e.data.op) {
  case 'request': {
    const controller = new AbortController();
    navigator.locks.request(
      e.data.name, {
        mode: e.data.mode || 'exclusive',
        ifAvailable: e.data.ifAvailable || false,
        signal: e.data.abortImmediately ? controller.signal : undefined,
      }, lock => {
        if (lock === null) {
          respond({ack: 'request', failed: true});
          return;
        }
        let lock_id = next_lock_id++;
        let release;
        const promise = new Promise(r => { release = r; });
        held.set(lock_id, release);
        respond({ack: 'request', lock_id: lock_id});
        return promise;
      }).catch(e => {
        respond({ack: 'request', error: e.name});
      });
    if (e.data.abortImmediately) {
      controller.abort();
    }
    break;
  }

  case 'release':
    held.get(e.data.lock_id)();
    held.delete(e.data.lock_id);
    respond({ack: 'release', lock_id: e.data.lock_id});
    break;
  }
}

self.addEventListener('message', processMessage);

self.addEventListener('connect', ev => {
  // Shared worker case
  ev.ports[0].onmessage = processMessage;
});
