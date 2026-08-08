// The resolve function for the current pending event listener's promise.
// It is nulled once the promise is resolved.
var message_event_promise_resolve = null;

function messageEventHandler(evt) {
  if (message_event_promise_resolve) {
    local_resolve = message_event_promise_resolve;
    message_event_promise_resolve = null;
    local_resolve(evt.data);
  }
}

function makeMessagePromise() {
  if (message_event_promise_resolve != null) {
    // Do not create a new promise until the previous is settled.
    return;
  }

  return new Promise(resolve => {
    message_event_promise_resolve = resolve;
  });
}

// Loads a url for the frame type and then returns a promise for
// the data that was postMessage'd from the loaded frame.
// If the frame type is 'window' then `url` is encoded into the search param
// as the url the 3p window is meant to iframe.
function loadAndReturnSwData(t, url, frame_type) {
  if (frame_type !== 'iframe' && frame_type !== 'window') {
    return;
  }

  const message_promise = makeMessagePromise();

  // Create the iframe or window and then return the promise for data.
  if ( frame_type === 'iframe' ) {
    const frame = with_iframe(url, false);
    t.add_cleanup(async () => {
      const f = await frame;
      f.remove();
    });
  }
  else {
    // 'window' case.
    const search_param = new URLSearchParams();
    search_param.append('target', url);

    const third_party_window_url = new URL(
    './resources/partitioned-service-worker-third-party-window.html' +
    '?' + search_param,
    get_host_info().HTTPS_NOTSAMESITE_ORIGIN + self.location.pathname);

    const w = window.open(third_party_window_url);
    t.add_cleanup(() => w.close());
  }

  return message_promise;
}

// Checks for an existing service worker registration. If not present,
// registers and maintains a service worker. Used in windows or iframes
// that will be partitioned from the main frame.
async function setupServiceWorker() {

  const script = './partitioned-storage-sw.js';
  const scope = './partitioned-';

  var reg = await navigator.serviceWorker.register(script, { scope: scope });

  // We should keep track if we installed a worker or not. If we did then we
  // need to uninstall it. Otherwise we let the top level test uninstall it
  // (If partitioning is not working).
  var installed_a_worker = true;
  await new Promise(resolve => {
    // Check if a worker is already activated.
    var worker = reg.active;
    // If so, just resolve.
    if ( worker ) {
      installed_a_worker = false;
      resolve();
      return;
    }

    //Otherwise check if one is waiting.
    worker = reg.waiting;
    // If not waiting, grab the installing worker.
    if ( !worker ) {
      worker = reg.installing;
    }

    // Resolve once it's activated.
    worker.addEventListener('statechange', evt => {
      if (worker.state === 'activated') {
        resolve();
      }
    });
  });

  self.addEventListener('unload', async () => {
    // If we didn't install a worker then that means the top level test did, and
    // that test is therefore responsible for cleaning it up.
    if ( !installed_a_worker ) {
        return;
    }

    await reg.unregister();
  });

  return reg;
}