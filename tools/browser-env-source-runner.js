'use strict';

// Executes the browser environment JavaScript from the working tree without
// rebuilding Node. The running Mode binary supplies only the native
// `document.all` binding; `lib/internal/browser_env.js` is loaded from disk.
//
// Run with a previously built Mode binary:
//   mode --expose-internals tools/browser-env-source-runner.js \
//     js_reverse_cache/.../first_412.html --deterministic
// Add --trace only when inspecting environment accesses. It instruments DOM
// methods and is therefore deliberately not part of the production-equivalent
// default execution.

const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { internalBinding, primordials } = require('internal/test/binding');
const nodeProcess = process;

const [htmlPath, ...flags] = nodeProcess.argv.slice(2);
if (!htmlPath || flags.some((flag) => !['--deterministic', '--trace'].includes(flag))) {
  throw new Error('usage: mode --expose-internals tools/browser-env-source-runner.js <challenge.html> [--deterministic] [--trace]');
}

const root = path.resolve(__dirname, '..');
const url = 'https://etax.qingdao.chinatax.gov.cn:8443/';
const ua = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
  + '(KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36';
const html = fs.readFileSync(htmlPath, 'utf8');

function loadBrowserEnvironmentFromSource() {
  const sourcePath = path.join(root, 'lib', 'internal', 'browser_env.js');
  const source = fs.readFileSync(sourcePath, 'utf8');
  const module = { exports: {} };
  const factory = vm.runInThisContext(
    `(function(exports, module, require, primordials, internalBinding) {\n${source}\n})`,
    { filename: sourcePath },
  );
  factory(module.exports, module, require, primordials, internalBinding);
  return module.exports;
}

function parseAttributes(source) {
  const attributes = {};
  const matcher = /([^\s=/>]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'=<>`]+)))?/g;
  let match;
  while ((match = matcher.exec(source)) !== null) {
    attributes[match[1].toLowerCase()] = match[2] ?? match[3] ?? match[4] ?? '';
  }
  return attributes;
}

function parseChallenge(source) {
  const metas = Array.from(source.matchAll(/<meta\b([^>]*)>/gi), (match) => parseAttributes(match[1]));
  const scripts = Array.from(source.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script\s*>/gi), (match) => ({
    attributes: parseAttributes(match[1]),
    content: match[2],
  }));
  const meta = metas.find((tag) => tag.r === 'm');
  const inline = scripts.find((tag) => tag.attributes.r === 'm' && !tag.attributes.src && tag.content.includes('$_'));
  const external = scripts.find((tag) => tag.attributes.r === 'm' && tag.attributes.src);
  if (!meta || !inline || !external) throw new Error('missing r=m challenge parts');
  return { external: external.attributes.src, inline: inline.content };
}

function deterministicPrelude() {
  if (!flags.includes('--deterministic')) return '';
  return `
    var __modeSeed = 0x13579bdf;
    Math.random = function random() {
      __modeSeed = (__modeSeed * 1664525 + 1013904223) >>> 0;
      return __modeSeed / 0x100000000;
    };
    var __modeRealDate = Date;
    var __modeTime = 1700000000000;
    Date = function Date() {
      if (!new.target) return __modeRealDate();
      return arguments.length ? Reflect.construct(__modeRealDate, arguments) : new __modeRealDate(__modeTime++);
    };
    Date.now = function now() { return __modeTime++; };
    Date.parse = __modeRealDate.parse;
    Date.UTC = __modeRealDate.UTC;
    Date.prototype = __modeRealDate.prototype;
  `;
}

function cookieSummary(cookieHeader) {
  return String(cookieHeader).split(';').flatMap((part) => {
    const separator = part.indexOf('=');
    if (separator <= 0) return [];
    const value = part.slice(separator + 1).trim();
    let hash = 2166136261;
    for (let index = 0; index < value.length; index++) {
      hash = Math.imul(hash ^ value.charCodeAt(index), 16777619);
    }
    return [{
      name: part.slice(0, separator).trim(),
      valueHash: (hash >>> 0).toString(16),
      valueLength: value.length,
    }];
  });
}

const traceEnabled = flags.includes('--trace');
const hook = traceEnabled ?
  fs.readFileSync(path.join(root, 'js_reverse_cache', 'compare_browser_env_hook.js'), 'utf8') : '';
const challenge = parseChallenge(html);
const fileName = path.basename(new URL(challenge.external, url).pathname);
const outJs = fs.readFileSync(path.join(root, 'rs_mode_server', 'out_js', fileName), 'utf8');
const { install } = loadBrowserEnvironmentFromSource();

install({
  url,
  html,
  hideNodeGlobals: true,
  navigator: {
    appName: 'Netscape',
    appVersion: ua.replace(/^Mozilla\//, ''),
    language: 'zh-CN',
    languages: ['zh-CN', 'zh'],
    maxTouchPoints: 0,
    platform: 'Win32',
    userAgent: ua,
    vendor: 'Google Inc.',
    webdriver: false,
  },
  screen: {
    availHeight: 1040,
    availLeft: 0,
    availTop: 0,
    availWidth: 1920,
    colorDepth: 24,
    height: 1080,
    pixelDepth: 24,
    width: 1920,
  },
  window: {
    properties: {
      innerHeight: 1080,
      innerWidth: 1920,
      outerHeight: 1080,
      outerWidth: 1920,
      screenLeft: 0,
      screenTop: 0,
      screenX: 0,
      screenY: 0,
    },
  },
  document: { properties: { visibilityState: 'hidden' } },
});

const originalConsole = globalThis.console;
globalThis.console = { debug() {}, error() {}, info() {}, log() {}, warn() {} };
try {
  const result = new Function(`${deterministicPrelude()}\n${hook}\n${challenge.inline}\n${outJs}
    const result = {
      label: 'source',
      source: 'lib/internal/browser_env.js',
      traceEnabled: ${traceEnabled},
      nodeGlobals: {
      Buffer: typeof Buffer,
      clearImmediate: typeof clearImmediate,
      exports: typeof exports,
      global: typeof global,
      module: typeof module,
      process: typeof process,
      require: typeof require,
      setImmediate: typeof setImmediate,
      },
      cookies: (${cookieSummary.toString()})(document.cookie),
    };
    if (${traceEnabled}) Object.assign(result, __modeEnvTrace.dump('source'));
    return result;`)();
  nodeProcess.stdout.write(JSON.stringify(result), () => nodeProcess.exit(0));
} finally {
  globalThis.console = originalConsole;
}
