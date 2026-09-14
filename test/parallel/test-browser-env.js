'use strict';

require('../common');
const assert = require('node:assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

const moduleScript = `
  const assert = require('node:assert');
  const env = require('node:browser-env');
  let descriptorValue;
  assert.strictEqual(env.isInstalled(), false);
  assert.strictEqual(typeof globalThis.document, 'undefined');
  const result = env.install({
    url: 'https://example.test/start?x=1',
    html: '<html><head><title>Initial</title></head><body><div id="app" class="ready">hello</div></body></html>',
    navigator: {
      userAgent: 'TestBrowser/1.0',
      platform: 'TestOS',
      languages: ['zh-CN', 'zh'],
    },
    window: {
      properties: { customFlag: true },
    },
    document: {
      properties: { visibilityState: 'hidden' },
      descriptors: {
        challengeValue: { enumerable: true, get() { return 'challenge'; } },
        mutableValue: {
          enumerable: true,
          get() { return descriptorValue; },
          set(value) { descriptorValue = value; },
        },
      },
    },
  });
  assert.strictEqual(env.isInstalled(), true);
  assert.strictEqual(result.window, globalThis);
  assert.strictEqual(window, globalThis);
  assert.strictEqual(self, window);
  assert.strictEqual(top, window);
  assert.strictEqual(parent, window);
  assert.strictEqual(document.defaultView, window);
  assert.strictEqual(location.host, 'example.test');
  assert.strictEqual(document.title, 'Initial');
  assert.strictEqual(document.querySelector('#app').textContent, 'hello');
  assert.strictEqual(document.querySelector('.ready').id, 'app');
  assert.strictEqual(document.querySelectorAll('body #app').length, 1);
  assert.strictEqual(Object.prototype.toString.call(document), '[object HTMLDocument]');
  assert.strictEqual(Object.prototype.toString.call(document.documentElement), '[object HTMLHtmlElement]');
  assert.strictEqual(Object.prototype.toString.call(document.head), '[object HTMLHeadElement]');
  assert.strictEqual(Object.prototype.toString.call(document.body), '[object HTMLBodyElement]');
  assert.deepStrictEqual(Object.keys(document.body), []);
  const divs = document.getElementsByTagName('div');
  assert.strictEqual(divs[0], document.querySelector('#app'));
  assert.strictEqual(divs[0].getAttribute('id'), 'app');
  assert.strictEqual(navigator.userAgent, 'TestBrowser/1.0');
  assert.strictEqual(navigator.platform, 'TestOS');
  assert.deepStrictEqual(navigator.languages, ['zh-CN', 'zh']);
  assert.strictEqual(window.customFlag, true);
  assert.strictEqual(document.visibilityState, 'hidden');
  assert.strictEqual(document.challengeValue, 'challenge');
  document.mutableValue = 'updated';
  assert.strictEqual(document.mutableValue, 'updated');
  const all = document.all;
  assert.strictEqual(typeof all, 'undefined');
  assert(all == null);
  assert.strictEqual(Boolean(all), false);
  assert.strictEqual(all.length, 5);
  assert.strictEqual(all.app, document.querySelector('#app'));
  assert.strictEqual(all(0), document.documentElement);
  assert.strictEqual(all('app'), document.querySelector('#app'));
  const later = document.createElement('section');
  later.id = 'later';
  document.body.appendChild(later);
  assert.strictEqual(document.all.later, later);
  assert.strictEqual(document.all.length, 6);
  history.pushState(null, '', '/next');
  assert.strictEqual(location.pathname, '/next');
  document.cookie = 'token=value; Path=/';
  assert.strictEqual(document.cookie, 'token=value');
  localStorage.setItem('key', 'value');
  assert.strictEqual(localStorage.getItem('key'), 'value');
  assert.throws(() => env.install({ url: 'https://again.test/' }), /already installed/);
`;

spawnSyncAndAssert(process.execPath, ['-e', moduleScript], { status: 0, stderr: '' });

spawnSyncAndAssert(process.execPath, [
  '-e',
  `
    (async () => {
      const assert = require('node:assert');
      const { install } = require('node:browser-env');
      install({
        url: 'https://example.test/page?x=1',
        html: '<html><head><!--[if lt IE 9]><script>hidden-script</script><![endif]--><meta id="challenge" content="initial-content"><script src="/challenge.js">initial-script</script></head><body><form id="form"><input name="token" value="initial"></form></body></html>',
        navigator: { userAgent: 'Mozilla/5.0 ModeTest' },
      });

      assert.strictEqual(window.innerWidth, 1920);
      assert.strictEqual(window.outerHeight, 1080);
      assert.strictEqual(screen.orientation.type, 'landscape-primary');
      assert.strictEqual(location.ancestorOrigins.length, 0);
      assert.strictEqual(clientInformation, navigator);
      assert.strictEqual(navigator.appName, 'Netscape');
      assert.strictEqual(navigator.appVersion, '5.0 ModeTest');
      assert.strictEqual(navigator.connection.effectiveType, '4g');
      assert(navigator.connection instanceof NetworkInformation);
      assert.strictEqual(Object.prototype.toString.call(navigator.connection), '[object NetworkInformation]');
      assert.deepStrictEqual(Object.keys(navigator.connection), []);
      assert.strictEqual(navigator.mimeTypes.length, 2);
      assert.strictEqual(navigator.mimeTypes.namedItem('application/pdf').suffixes, 'pdf');
      assert.strictEqual(navigator.sendBeacon('/beacon', 'body'), true);
      assert.strictEqual(String(navigator.getBattery), 'function getBattery() { [native code] }');
      const battery = await navigator.getBattery();
      assert(battery instanceof BatteryManager);
      assert.strictEqual(Object.prototype.toString.call(battery), '[object BatteryManager]');
      assert.strictEqual(battery.charging, true);

      assert(document instanceof Document);
      assert(document.body instanceof HTMLElement);
      assert(document.createTextNode('text') instanceof Text);
      const inputs = document.getElementsByTagName('input');
      assert.strictEqual(Object.prototype.toString.call(inputs), '[object HTMLCollection]');
      assert(inputs instanceof HTMLCollection);
      assert.strictEqual(inputs.constructor, HTMLCollection);
      assert.deepStrictEqual(Object.keys(inputs), ['0']);
      assert.strictEqual(Object.prototype.toString.call(document), '[object HTMLDocument]');
      assert.strictEqual(Object.prototype.toString.call(document.head), '[object HTMLHeadElement]');
      assert.strictEqual(Object.prototype.toString.call(document.body), '[object HTMLBodyElement]');
      assert.deepStrictEqual(Object.keys(document), []);
      assert.deepStrictEqual(Object.keys(document.body), []);
      assert.deepStrictEqual(Object.keys(navigator.mimeTypes), ['0', '1']);
      assert.deepStrictEqual(document.createExpression(), Object.create(null));

      const anchor = document.createElement('a');
      assert.strictEqual(anchor.href, '');
      anchor.href = '/next?q=1#hash';
      assert(anchor instanceof HTMLAnchorElement);
      assert.strictEqual(anchor.href, 'https://example.test/next?q=1#hash');
      assert.strictEqual(anchor.host, 'example.test');

      const div = document.createElement('div');
      assert(div instanceof HTMLDivElement);
      assert.strictEqual(div.constructor, HTMLDivElement);
      assert.strictEqual(Object.prototype.toString.call(div), '[object HTMLDivElement]');
      assert.strictEqual(String(div.getAttribute), 'function getAttribute() { [native code] }');

      const iframe = document.createElement('iframe');
      assert(iframe instanceof HTMLIFrameElement);
      assert.strictEqual(iframe.constructor, HTMLIFrameElement);
      assert.strictEqual(Object.prototype.toString.call(iframe), '[object HTMLIFrameElement]');
      assert.strictEqual(iframe.contentDocument, null);
      assert.strictEqual(iframe.contentWindow, null);
      assert.strictEqual(String(location.assign), 'function assign() { [native code] }');

      const meta = document.querySelector('#challenge');
      assert(meta instanceof HTMLMetaElement);
      assert.strictEqual(Object.prototype.toString.call(meta), '[object HTMLMetaElement]');
      assert.strictEqual(meta.content, 'initial-content');
      meta.content = 'updated-content';
      assert.strictEqual(meta.getAttribute('content'), 'updated-content');
      const script = document.querySelector('script');
      assert(script instanceof HTMLScriptElement);
      assert.strictEqual(Object.prototype.toString.call(script), '[object HTMLScriptElement]');
      assert.strictEqual(script.innerText, 'initial-script');
      script.innerText = 'updated-script';
      assert.strictEqual(script.textContent, 'updated-script');
      assert.strictEqual(script.src, 'https://example.test/challenge.js');
      assert.strictEqual(document.getElementsByTagName('script').length, 1);

      const form = document.querySelector('#form');
      assert(form instanceof HTMLFormElement);
      assert.strictEqual(form.elements.namedItem('token').value, 'initial');

      const canvas = document.createElement('canvas');
      assert(canvas instanceof HTMLCanvasElement);
      assert(canvas.getContext('2d') instanceof CanvasRenderingContext2D);
      assert.strictEqual(canvas.toDataURL(), 'data:,');

      const parsed = new DOMParser().parseFromString('<html><body><p id="parsed">ok</p></body></html>', 'text/html');
      assert(parsed instanceof Document);
      assert.strictEqual(parsed.querySelector('#parsed').textContent, 'ok');

      const xhr = new XMLHttpRequest();
      const states = [];
      xhr.addEventListener('readystatechange', () => states.push(xhr.readyState));
      xhr.open('POST', '/challenge');
      xhr.setRequestHeader('x-test', 'yes');
      xhr.send('payload');
      assert.deepStrictEqual(states, [XMLHttpRequest.OPENED, XMLHttpRequest.DONE]);
      assert.strictEqual(xhr.status, 0);
      assert.strictEqual(xhr.responseURL, 'https://example.test/challenge');
      assert.strictEqual(window.encode_url, '/challenge');
      assert.strictEqual(window.encode_data, 'payload');

      let observed = false;
      const observer = new MutationObserver(() => { observed = true; });
      observer.observe(document.body, { childList: true });
      assert.strictEqual(observer.takeRecords().length, 0);
      observer.disconnect();
      assert.strictEqual(observed, false);
      assert.strictEqual(indexedDB.open('mode-test').result.name, 'mode-test');
      assert.strictEqual(chrome.app.isInstalled, false);
      assert.strictEqual(typeof chrome.loadTimes, 'function');
      assert.strictEqual(msCrypto, globalThis.crypto);
      assert.strictEqual(typeof msCrypto.getRandomValues, 'function');
      assert.strictEqual(window.open('https://example.test/').closed, false);
      assert.strictEqual(window.prompt('question'), null);
      assert.strictEqual(typeof webkitRequestFileSystem, 'function');

      assert.strictEqual(document.all[0], document.documentElement);
      assert.strictEqual(document.all.form, form);
    })().catch((error) => {
      console.error(error.stack);
      process.exitCode = 1;
    });
  `,
], { status: 0, stderr: '' });

spawnSyncAndAssert(process.execPath, [
  '-e',
  `
    const assert = require('node:assert');
    const { install } = require('node:browser-env');
    install({ url: 'https://example.test/', hideNodeGlobals: true });
    assert.deepStrictEqual(
      new Function('return [typeof global, typeof process, typeof require, typeof module, typeof exports, typeof __dirname, typeof __filename, typeof setImmediate, typeof clearImmediate].join(\",\")')(),
      'undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined',
    );
    assert.strictEqual(typeof Buffer, 'function');
    for (const name of ['global', 'process', 'require', 'module', 'exports', '__dirname', '__filename', 'setImmediate', 'clearImmediate']) {
      assert.strictEqual(Object.getOwnPropertyDescriptor(globalThis, name), undefined);
    }
  `,
], { status: 0, stderr: '' });

spawnSyncAndAssert(process.execPath, [
  '-e',
  `
    const assert = require('node:assert');
    const { install } = require('node:browser-env');
    assert.throws(
      () => install({ url: 'https://example.test/', document: { properties: { all: null } } }),
      /protected browser environment property/,
    );
    assert.strictEqual(typeof globalThis.document, 'undefined');
    assert.throws(
      () => install({ url: 'https://example.test/', document: { descriptors: { createElement: { value: null } } } }),
      /protected browser environment property/,
    );
    assert.throws(
      () => install({ url: 'https://example.test/', window: { properties: { location: null } } }),
      /protected browser environment property/,
    );
  `,
], { status: 0, stderr: '' });

spawnSyncAndAssert(process.execPath, [
  '--input-type=module',
  '-e',
  `
    import assert from 'node:assert';
    import { install, isInstalled } from 'node:browser-env';
    assert.strictEqual(isInstalled(), false);
    install({ url: 'https://esm.example.test/' });
    assert.strictEqual(document.location.hostname, 'esm.example.test');
  `,
], { status: 0, stderr: '' });

const profile = fixtures.path('browser-env', 'profile.json');
spawnSyncAndAssert(process.execPath, [
  `--browser-env-profile=${profile}`,
  '-e',
  `
    const assert = require('node:assert');
    const env = require('node:browser-env');
    assert.strictEqual(env.isInstalled(), true);
    assert.throws(() => env.install({ url: 'https://again.example.test/' }), /already installed/);
    assert.strictEqual(location.hostname, 'profile.example.test');
    assert.strictEqual(document.querySelector('#app').textContent, 'ok');
    assert.strictEqual(document.visibilityState, 'hidden');
    assert.strictEqual(window.profileEnabled, true);
    assert.strictEqual(navigator.userAgent, 'ProfileBrowser/1.0');
  `,
], { status: 0, stderr: '' });

spawnSyncAndAssert(process.execPath, [
  '--browser-env-profile=/definitely/missing/profile.json',
  '-e', '',
], { status: 1, stderr: /Unable to load --browser-env-profile/ });
