'use strict';

let stored_data = null;

function processMessage(e) {
  function respond(data) {
    e.currentTarget.postMessage(Object.assign(data, {reqid: e.data.reqid}));
  }

  switch (e.data.op) {
    case 'load': {
      respond({ack: 'load', status: 'OK', data: stored_data});
      break;
    }
    case 'store': {
      try {
        stored_data = e.data.data
      } catch (err) {
        respond({ack: 'store', status: 'ERROR', error: err.name});
        return;
      }
      respond({ack: 'store', status: 'OK'});
      break;
    }
  }
}

self.addEventListener('connect', e => {
  e.ports[0].onmessage = processMessage;
});
