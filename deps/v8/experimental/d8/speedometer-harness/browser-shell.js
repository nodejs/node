// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A minimal "browser shell" for d8 --experimental-mock-dom, written in JS.
//
// The mock DOM (experimental/d8/mock-dom) provides documents, events and
// the rest of the web platform surface natively. What it does not do is load
// pages. This file does that part: it turns `iframe.src = url` into a fresh
// Realm, reads the HTML from disk, hands it to the native parser, runs the
// page's scripts in order, serves fetch/XHR from local files and fires the
// load events.
//
// Everything here runs outside the measured part of a benchmark (page load
// and preparation), so it does not have to resemble a browser's performance.
// It does have to be loud: a script that fails to load or throws, a missing
// file or a console.error is recorded in `shell.errors`.

'use strict';

const shell = (() => {
  // Pages are served from this origin; paths map to files under `root`.
  const ORIGIN = 'http://localhost:8080';

  const state = {
    root: '',
    errors: [],
    frames: new Map(),  // iframe element -> {realm, window, url}
    started: new WeakSet(),  // script elements already handed to the loader
    verbose: false,
  };

  function recordError(where, error) {
    const text = error && error.stack ? String(error.stack) : String(error);
    state.errors.push(`${where}: ${text}`);
    console.log(`[shell] ERROR ${where}: ${text}`);
  }

  function urlToPath(url) {
    const parsed = new URL(url, ORIGIN + '/');
    if (parsed.origin !== ORIGIN) {
      throw new Error(`cross-origin URL is not available offline: ${url}`);
    }
    let path = decodeURIComponent(parsed.pathname);
    if (path.endsWith('/')) path += 'index.html';
    return state.root + path.replace(/^\/+/, '');
  }

  function readURL(url) {
    return read(urlToPath(url));
  }

  // --- Networking ----------------------------------------------------------

  function makeResponse(url, body, status) {
    const headers = {
      get(name) {
        if (name.toLowerCase() !== 'content-type') return null;
        if (url.endsWith('.json')) return 'application/json';
        if (url.endsWith('.js') || url.endsWith('.mjs')) {
          return 'text/javascript';
        }
        if (url.endsWith('.css')) return 'text/css';
        if (url.endsWith('.html')) return 'text/html';
        return 'text/plain';
      },
      has(name) { return this.get(name) !== null; },
      forEach() {},
    };
    const response = {
      ok: status >= 200 && status < 300,
      status,
      statusText: status === 200 ? 'OK' : 'Not Found',
      url,
      headers,
      redirected: false,
      type: 'basic',
      bodyUsed: false,
      text: async () => body,
      json: async () => JSON.parse(body),
      // The text body went through UTF-8 decoding, so binary content is read
      // from the file again.
      arrayBuffer: async () =>
          status === 200 ? readbuffer(urlToPath(url)) : new ArrayBuffer(0),
      clone: () => makeResponse(url, body, status),
    };
    return response;
  }

  function installNetworking(win) {
    const resolve = (url) => new URL(url, win.location.href).href;

    win.fetch = (input, init) => {
      const url = resolve(typeof input === 'string' ? input : input.url);
      return new Promise((resolveFetch) => {
        setTimeout(() => {
          let body;
          try {
            body = readURL(url);
          } catch (e) {
            recordError(`fetch ${url}`, e);
            resolveFetch(makeResponse(url, '', 404));
            return;
          }
          resolveFetch(makeResponse(url, body, 200));
        }, 0);
      });
    };

    // d8 has no streaming compilation; the response is already complete.
    const wasm = win.WebAssembly;
    wasm.compileStreaming = async (source, options) =>
        wasm.compile(await (await source).arrayBuffer(), options);
    wasm.instantiateStreaming = async (source, imports, options) =>
        wasm.instantiate(await (await source).arrayBuffer(), imports, options);

    win.XMLHttpRequest = class XMLHttpRequest {
      static UNSENT = 0;
      static OPENED = 1;
      static HEADERS_RECEIVED = 2;
      static LOADING = 3;
      static DONE = 4;
      #listeners = new Map();
      #url = '';
      #async = true;
      readyState = 0;
      status = 0;
      statusText = '';
      responseText = '';
      response = '';
      responseType = '';
      responseURL = '';
      timeout = 0;
      withCredentials = false;
      onload = null;
      onerror = null;
      onreadystatechange = null;
      onloadend = null;
      onabort = null;
      ontimeout = null;
      onprogress = null;
      upload = {addEventListener() {}, removeEventListener() {}};

      open(method, url, async = true) {
        this.#url = resolve(url);
        this.#async = async;
        this.readyState = 1;
      }
      setRequestHeader() {}
      overrideMimeType() {}
      getResponseHeader(name) {
        return makeResponse(this.#url, '', 200).headers.get(name);
      }
      getAllResponseHeaders() { return ''; }
      abort() {}
      addEventListener(type, listener) {
        if (!this.#listeners.has(type)) this.#listeners.set(type, []);
        this.#listeners.get(type).push(listener);
      }
      removeEventListener(type, listener) {
        const list = this.#listeners.get(type);
        if (list) this.#listeners.set(type, list.filter((l) => l !== listener));
      }
      #fire(type) {
        const event = {type, target: this, currentTarget: this, loaded: 0,
                       total: 0, lengthComputable: false};
        const handler = this['on' + type];
        if (typeof handler === 'function') handler.call(this, event);
        for (const listener of this.#listeners.get(type) ?? []) {
          listener.call(this, event);
        }
      }
      #complete() {
        let failed = false;
        try {
          this.responseText = readURL(this.#url);
          this.status = 200;
          this.statusText = 'OK';
        } catch (e) {
          recordError(`XMLHttpRequest ${this.#url}`, e);
          this.status = 404;
          this.statusText = 'Not Found';
          failed = true;
        }
        this.responseURL = this.#url;
        this.response = this.responseText;
        if (this.responseType === 'json' && !failed) {
          this.response = JSON.parse(this.responseText);
        }
        this.readyState = 4;
        this.#fire('readystatechange');
        this.#fire(failed ? 'error' : 'load');
        this.#fire('loadend');
      }
      send() {
        if (this.#async) {
          setTimeout(() => this.#complete(), 0);
        } else {
          this.#complete();
        }
      }
    };
  }

  // --- Script execution ----------------------------------------------------

  function isClassicScriptType(type) {
    return type === '' || type === 'text/javascript' ||
        type === 'application/javascript';
  }

  // Runs one classic script. Errors are reported to the page's window, as a
  // browser does, and do not stop later scripts.
  function runClassicScript(frame, script) {
    const win = frame.window;
    d8.mockDom.setCurrentScript(win, script);
    try {
      if (script.hasAttribute('src')) {
        const path = urlToPath(script.src);
        Realm.eval(frame.realm, `load(${JSON.stringify(path)})`);
      } else {
        Realm.eval(frame.realm, script.textContent);
      }
      return true;
    } catch (e) {
      recordError(`script ${script.src || '(inline)'} in ${frame.url}`, e);
      return false;
    } finally {
      d8.mockDom.setCurrentScript(win, null);
    }
  }

  async function runModuleScript(frame, script) {
    if (!script.hasAttribute('src')) {
      recordError(`script in ${frame.url}`,
                  new Error('inline module scripts are not supported'));
      return false;
    }
    try {
      const path = urlToPath(script.src);
      await Realm.eval(frame.realm, `import(${JSON.stringify(path)})`);
      return true;
    } catch (e) {
      recordError(`module ${script.src} in ${frame.url}`, e);
      return false;
    }
  }

  function fireSimpleEvent(target, type, EventCtor = Event) {
    target.dispatchEvent(new EventCtor(type));
  }

  // Called by the mock DOM whenever a script, link, img or iframe element is
  // connected or gets a new URL.
  function onResourceElement(frame, element) {
    const tag = element.localName;
    if (tag === 'iframe') {
      if (element.hasAttribute('src')) navigateFrame(element, element.src);
      return;
    }
    if (tag === 'script') {
      if (state.started.has(element)) return;
      const type = element.type.toLowerCase();
      if (type !== 'module' && !isClassicScriptType(type)) return;
      if (!element.hasAttribute('src')) {
        // A dynamically inserted inline script runs synchronously.
        if (element.textContent === '') return;
        state.started.add(element);
        runClassicScript(frame, element);
        return;
      }
      state.started.add(element);
      frame.pendingLoads++;
      setTimeout(async () => {
        const ok = type === 'module' ? await runModuleScript(frame, element) :
                                       runClassicScript(frame, element);
        fireSimpleEvent(element, ok ? 'load' : 'error', frame.window.Event);
        loadFinished(frame);
      }, 0);
      return;
    }
    // Images have nothing to decode here, but pages wait for the load
    // events of stylesheets and images (e.g. CSS chunk loaders).
    if (tag === 'link' || tag === 'img') {
      const attribute = tag === 'link' ? 'href' : 'src';
      if (!element.hasAttribute(attribute)) return;
      if (tag === 'link' && !/stylesheet|preload|modulepreload|prefetch/.test(
                                element.rel)) {
        return;
      }
      if (tag === 'link') applyStyleSheet(element);
      frame.pendingLoads++;
      setTimeout(() => {
        fireSimpleEvent(element, 'load', frame.window.Event);
        loadFinished(frame);
      }, 0);
    }
  }

  // Hands the text of a linked stylesheet to the mock DOM's style engine.
  function applyStyleSheet(link) {
    if (!/stylesheet/.test(link.rel) || !link.hasAttribute('href')) return;
    try {
      d8.mockDom.setStyleSheetText(link, readURL(link.href));
    } catch (e) {
      recordError(`stylesheet ${link.href}`, e);
    }
  }

  function loadFinished(frame) {
    if (--frame.pendingLoads === 0 && frame.onIdle) {
      const onIdle = frame.onIdle;
      frame.onIdle = null;
      onIdle();
    }
  }

  function waitForPendingLoads(frame) {
    if (frame.pendingLoads === 0) return Promise.resolve();
    return new Promise((resolve) => { frame.onIdle = resolve; });
  }

  // --- Navigation ----------------------------------------------------------

  function disposeFrame(element) {
    const frame = state.frames.get(element);
    if (!frame) return;
    state.frames.delete(element);
    d8.mockDom.setFrameWindow(element, null);
    d8.mockDom.detachWindow(frame.window);
    Realm.dispose(frame.realm);
  }

  async function loadDocument(frame, element) {
    const win = frame.window;
    d8.mockDom.setURL(win, frame.url);
    d8.mockDom.setViewport(win, state.viewport.width, state.viewport.height);
    installNetworking(win);

    // Loudness: anything the page logs as an error, and any exception nobody
    // caught, fails the run.
    const consoleError = win.console.error;
    win.console.error = (...args) => {
      recordError(`console.error in ${frame.url}`, args.map(String).join(' '));
    };
    win.addEventListener('error', (event) => {
      recordError(`uncaught in ${frame.url}`,
                  event.error ?? event.message);
    });
    win.addEventListener('unhandledrejection', (event) => {
      recordError(`unhandled rejection in ${frame.url}`, event.reason);
    });

    let html;
    try {
      html = readURL(frame.url);
    } catch (e) {
      recordError(`navigation to ${frame.url}`, e);
      fireSimpleEvent(element, 'error');
      return;
    }
    d8.mockDom.parseDocument(win, html);
    d8.mockDom.setResourceCallback(
        win, (resource) => onResourceElement(frame, resource));
    for (const link of win.document.querySelectorAll('link')) {
      applyStyleSheet(link);
    }

    // Parser-inserted scripts: classic ones run in document order; deferred
    // and module scripts run afterwards, also in order.
    const deferred = [];
    for (const script of win.document.querySelectorAll('script')) {
      state.started.add(script);
      const type = script.type.toLowerCase();
      if (type === 'module') {
        deferred.push(script);
      } else if (!isClassicScriptType(type) || script.noModule) {
        continue;
      } else if (script.hasAttribute('src') &&
                 (script.defer || script.async)) {
        deferred.push(script);
      } else {
        const ok = runClassicScript(frame, script);
        if (!ok && script.hasAttribute('onerror')) {
          Realm.eval(frame.realm, script.getAttribute('onerror'));
        }
      }
    }
    for (const script of deferred) {
      if (script.type.toLowerCase() === 'module') {
        await runModuleScript(frame, script);
      } else {
        runClassicScript(frame, script);
      }
    }
    if (!state.frames.has(element)) return;  // removed while loading

    d8.mockDom.setReadyState(win, 'interactive');
    const ready = new win.Event('DOMContentLoaded', {bubbles: true});
    win.document.dispatchEvent(ready);
    await waitForPendingLoads(frame);
    if (!state.frames.has(element)) return;

    // A browser has styled and laid out the page by the time it fires load.
    win.document.documentElement.getBoundingClientRect();
    d8.mockDom.setReadyState(win, 'complete');
    fireSimpleEvent(win, 'load', win.Event);
    fireSimpleEvent(element, 'load');
  }

  function navigateFrame(element, url) {
    disposeFrame(element);
    const realm = Realm.createAllowCrossRealmAccess();
    const frame = {
      realm,
      window: Realm.global(realm),
      url: new URL(url, ORIGIN + '/').href,
      pendingLoads: 0,
      onIdle: null,
    };
    state.frames.set(element, frame);
    d8.mockDom.setFrameWindow(element, frame.window);
    if (state.verbose) console.log(`[shell] navigate ${frame.url}`);
    // Loading is asynchronous, like a real navigation.
    setTimeout(() => {
      loadDocument(frame, element).catch((e) => {
        recordError(`loading ${frame.url}`, e);
      });
    }, 0);
  }

  // Makes iframes in the top-level realm work: navigation when `src` is set
  // on a connected iframe, and teardown of the nested realm when the iframe
  // is removed.
  function install({root, viewport = {width: 800, height: 600},
                    url = ORIGIN + '/index.html', verbose = false}) {
    state.root = root.endsWith('/') ? root : root + '/';
    state.viewport = viewport;
    state.verbose = verbose;
    d8.mockDom.setURL(globalThis, url);
    d8.mockDom.setViewport(globalThis, viewport.width, viewport.height);
    d8.mockDom.setReadyState(globalThis, 'complete');
    const topFrame = {realm: 0, window: globalThis, url, pendingLoads: 0,
                      onIdle: null};
    d8.mockDom.setResourceCallback(
        globalThis, (element) => onResourceElement(topFrame, element));
    new MutationObserver((records) => {
      for (const record of records) {
        for (const node of record.removedNodes) {
          if (node.nodeType !== 1) continue;
          if (node.localName === 'iframe') disposeFrame(node);
          for (const nested of node.querySelectorAll('iframe')) {
            disposeFrame(nested);
          }
        }
      }
    }).observe(document, {childList: true, subtree: true});
  }

  // Searches |root| and every shadow tree below it.
  function deepQuerySelectorAll(root, selector, results = []) {
    for (const match of root.querySelectorAll(selector)) results.push(match);
    for (const element of root.querySelectorAll('*')) {
      if (element.shadowRoot) {
        deepQuerySelectorAll(element.shadowRoot, selector, results);
      }
    }
    return results;
  }

  return {
    ORIGIN,
    install,
    urlToPath,
    deepQuerySelectorAll,
    get errors() { return state.errors; },
    clearErrors() { state.errors.length = 0; },
  };
})();
