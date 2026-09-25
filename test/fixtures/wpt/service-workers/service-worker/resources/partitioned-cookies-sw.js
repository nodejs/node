self.addEventListener('message', ev => ev.waitUntil(onMessage(ev)));

async function onMessage(event) {
  if (!event.data)
    return;
  switch (event.data.type) {
    case 'test_message':
      return onTestMessage(event);
    case 'echo_cookies_http':
      return onEchoCookiesHttp(event);
    case 'echo_cookies_js':
      return onEchoCookiesJs(event);
    case 'echo_cookies_import':
      return onEchoCookiesImport(event);
    default:
      return;
  }
}

// test_message just verifies that the message passing is working.
async function onTestMessage(event) {
  event.source.postMessage({ok: true});
}

async function onEchoCookiesHttp(event) {
  try {
    const resp = await fetch(
        `${self.origin}/cookies/resources/list.py`, {credentials: 'include'});
    const cookies = await resp.json();
    event.source.postMessage({ok: true, cookies: Object.keys(cookies)});
  } catch (err) {
    event.source.postMessage({ok: false});
  }
}

// echo_cookies returns the names of all of the cookies available to the worker.
async function onEchoCookiesJs(event) {
  try {
    const cookie_objects = await self.cookieStore.getAll();
    const cookies = cookie_objects.map(c => c.name);
    event.source.postMessage({ok: true, cookies});
  } catch (err) {
    event.source.postMessage({ok: false});
  }
}

// Sets `self._cookies` variable, array of the names of cookies available to
// the request.
importScripts(`${self.origin}/cookies/resources/list-cookies-for-script.py`);

function onEchoCookiesImport(event) {
  event.source.postMessage({ok: true, cookies: self._cookies});
}
