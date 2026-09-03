importScripts('/common/get-host-info.sub.js');

const text = 'worker loading intercepted by service worker';
const dedicated_worker_script = `postMessage('${text}');`;
const shared_worker_script =
    `onconnect = evt => evt.ports[0].postMessage('${text}');`;

let source;
let resolveDone;
let done = new Promise(resolve => resolveDone = resolve);

// The page messages this worker to ask for the result. Keep the worker alive
// via waitUntil() until the result is sent.
self.addEventListener('message', event => {
  source = event.data.port;
  source.postMessage({id: event.source.id});
  source.onmessage = resolveDone;
  event.waitUntil(done);
});

self.onfetch = event => {
  const url = event.request.url;
  const destination = event.request.destination;

  if (source)
     source.postMessage({clientId:event.clientId, resultingClientId: event.resultingClientId});

  // Request handler for a synthesized response.
  if (url.indexOf('synthesized') != -1) {
    let script_headers = new Headers({ "Content-Type": "text/javascript" });
    if (destination === 'worker')
      event.respondWith(new Response(dedicated_worker_script, { 'headers': script_headers }));
    else if (destination === 'sharedworker')
      event.respondWith(new Response(shared_worker_script, { 'headers': script_headers }));
    else
      event.respondWith(new Response('Unexpected request! ' + destination));
    return;
  }

  // Request handler for a same-origin response.
  if (url.indexOf('same-origin') != -1) {
    event.respondWith(fetch('postmessage-on-load-worker.js'));
    return;
  }

  // Request handler for a cross-origin response.
  if (url.indexOf('cors') != -1) {
    const filename = 'postmessage-on-load-worker.js';
    const path = (new URL(filename, self.location)).pathname;
    let new_url = get_host_info()['HTTPS_REMOTE_ORIGIN'] + path;
    let mode;
    if (url.indexOf('no-cors') != -1) {
      // Test no-cors mode.
      mode = 'no-cors';
    } else {
      // Test cors mode.
      new_url += '?pipe=header(Access-Control-Allow-Origin,*)';
      mode = 'cors';
    }
    event.respondWith(fetch(new_url, { mode: mode }));
  }
};
