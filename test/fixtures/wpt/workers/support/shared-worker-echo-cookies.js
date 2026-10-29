let port;

self.onconnect = e => {
  port = e.ports[0];
  port.addEventListener('message', onMessage);
  port.start();
};

async function onMessage(e) {
  switch (e.data.type) {
    case 'test_message':
      port.postMessage({ok: true});
      return;
    case 'echo_cookies_http':
      return onEchoCookiesHttp(e);
    case 'echo_cookies_import':
      return onEchoCookiesImport();
    default:
      throw new Error('Unexpected message type ' + e.data.type);
  }
}

async function onEchoCookiesHttp(e) {
  try {
    const resp = await fetch(
        `${self.origin}/cookies/resources/list.py`, {credentials: 'include'});
    const cookies = await resp.json();
    port.postMessage({ok: true, cookies: Object.keys(cookies)});
  } catch (err) {
    port.postMessage({ok: false});
  }
}

// Sets `self._cookies` variable, array of the names of cookies available to
// the request.
importScripts(`${self.origin}/cookies/resources/list-cookies-for-script.py`);

function onEchoCookiesImport() {
  port.postMessage({ok: true, cookies: self._cookies});
}
