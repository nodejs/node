'use strict';

// The Web IDL and specification prose quoted below is reproduced verbatim
// from the specification, including its original (lowercase) casing.
// Refs: https://html.spec.whatwg.org/multipage/workers.html
/* eslint-disable capitalized-comments */

const {
  ArrayPrototypePush,
  FunctionPrototypeBind,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  ReflectDeleteProperty,
  RegExpPrototypeExec,
  SafeSet,
  StringPrototypeSlice,
  Symbol,
  SymbolFor,
  SymbolIterator,
  SymbolToStringTag,
  TypedArrayPrototypeGetLength,
  Uint8Array,
  globalThis,
} = primordials;

const {
  ERR_ILLEGAL_CONSTRUCTOR,
  ERR_INVALID_STATE,
  ERR_NO_CRYPTO,
} = require('internal/errors').codes;

const {
  EventTarget,
  defineEventHandler,
  initEventTarget,
} = require('internal/event_target');

const {
  assignFunctionName,
  defineOperation,
  exposeInterface,
  getCWDURL,
  getLazy,
  kEmptyObject,
  kEnumerableProperty,
  lazyDOMException,
} = require('internal/util');

const {
  validateThisInternalField,
} = require('internal/validators');

const {
  converters,
  createDictionaryConverter,
  createEnumConverter,
  requiredArguments,
} = require('internal/webidl');

const {
  Worker: NodeWorker,
  kPublicPort,
  kWebWorkerData,
} = require('internal/worker');

const {
  lazyMessageEvent,
} = require('internal/worker/io');

const {
  vm_dynamic_import_default_internal,
} = internalBinding('symbols');

const {
  base64Slice,
} = internalBinding('buffer');

const {
  hasOpenSSL,
} = internalBinding('config');

const {
  Navigator,
  kInitialize: kInitializeNavigator,
} = require('internal/navigator');

const {
  URL,
  URLParse,
} = require('internal/url');

const {
  kType: kBlobType,
  getBlobDataSync,
  resolveObjectURL,
} = require('internal/blob');

const kCurrentlyReceivingPorts =
  SymbolFor('nodejs.internal.kCurrentlyReceivingPorts');

const kCreate = Symbol('kCreate');
const kInsidePort = Symbol('kInsidePort');
const kLocation = Symbol('kLocation');
const kName = Symbol('kName');
const kNavigator = Symbol('kNavigator');
const kNavigatorBrand = Symbol('kNavigatorBrand');
const kType = Symbol('kType');
const kURL = Symbol('kURL');
const kWorker = Symbol('kWorker');

let scopeBaseURL = null;

const lazyErrorEvent =
  getLazy(() => require('internal/deps/undici/undici').ErrorEvent);

function createErrorEvent(init) {
  const ErrorEvent = lazyErrorEvent();
  return new ErrorEvent('error', init);
}

// "Fire an event named message at messageEventTarget, using MessageEvent,
// with the data attribute initialized to messageClone and the ports
// attribute initialized to newPorts."
//
// "If this throws an exception, catch it, fire an event named messageerror
// at messageEventTarget, using MessageEvent, and then return."
function forwardMessageEvents(port, target) {
  port.on('message', (data) =>
    target.dispatchEvent(lazyMessageEvent('message', {
      data,
      ports: port[kCurrentlyReceivingPorts],
    })));
  port.on('messageerror', (data) =>
    target.dispatchEvent(lazyMessageEvent('messageerror', {
      data,
    })));
}

// Web IDL [Replaceable]: the setter replaces the accessor with an own
// data property on the receiver.
// Refs: https://webidl.spec.whatwg.org/#Replaceable
function replaceAttribute(receiver, key, value) {
  ObjectDefineProperty(receiver, key, {
    __proto__: null,
    configurable: true,
    enumerable: true,
    writable: true,
    value,
  });
}

// https://mimesniff.spec.whatwg.org/#javascript-mime-type
const kJavaScriptMIMETypes = new SafeSet([
  'application/ecmascript',
  'application/javascript',
  'application/x-ecmascript',
  'application/x-javascript',
  'text/ecmascript',
  'text/javascript',
  'text/javascript1.0',
  'text/javascript1.1',
  'text/javascript1.2',
  'text/javascript1.3',
  'text/javascript1.4',
  'text/javascript1.5',
  'text/jscript',
  'text/livescript',
  'text/x-ecmascript',
  'text/x-javascript',
]);

function isJavaScriptMIMEType(essence) {
  return kJavaScriptMIMETypes.has(essence);
}

// Extracts the essence ("type/subtype") of a raw MIME type string, or
// returns undefined if the string is not a parsable MIME type.
function mimeTypeEssence(type) {
  const { MIMEType } = require('internal/mime');
  try {
    return new MIMEType(type).essence;
  } catch {
    return undefined;
  }
}

function utf8Decode(bytes) {
  return require('internal/encoding').getUtf8Decoder().decode(bytes);
}

// Reads and decodes the source text of a JavaScript data: URL, or returns
// undefined if the URL fails to parse as a data: URL or its MIME type is
// not a JavaScript MIME type.
function readDataURLScript(url) {
  const { dataURLProcessor } = require('internal/data_url');
  const data = dataURLProcessor(url);
  if (data === 'failure' || !isJavaScriptMIMEType(data.mimeType.essence)) {
    return undefined;
  }
  return utf8Decode(data.body);
}

// Reads the data of a blob whose contents are to be run as a script, or
// returns undefined if the blob's MIME type is not a JavaScript MIME type
// or its data is not synchronously available.
function getScriptBlobData(blob, allowEmptyType = false) {
  if (blob === undefined ||
      (!(allowEmptyType && blob[kBlobType] === '') &&
       !isJavaScriptMIMEType(mimeTypeEssence(blob[kBlobType])))) {
    return undefined;
  }
  return getBlobDataSync(blob);
}

/**
 * Runs the source text of a classic script in the current global scope,
 * emulating "run a classic script". Dynamic import() from the script is
 * resolved by the ESM loader relative to the script's URL.
 * @param {string} source The source text of the script.
 * @param {string} url The URL of the script, used as its filename and as
 *   the base URL for dynamic imports.
 * @returns {any}
 */
function runClassicScriptSource(source, url) {
  const {
    makeContextifyScript,
    runScriptInThisContext,
  } = require('internal/vm');
  const script = makeContextifyScript(
    source,                             // code
    url,                                // filename
    0,                                  // lineOffset
    0,                                  // columnOffset
    undefined,                          // cachedData
    false,                              // produceCachedData
    undefined,                          // parsingContext
    vm_dynamic_import_default_internal, // hostDefinedOptionId
    vm_dynamic_import_default_internal, // importModuleDynamically
  );
  // "Run the classic script script, with rethrow errors set to true."
  return runScriptInThisContext(script, true, false);
}

/**
 * Runs the source text of a module script in the current global scope.
 * @param {string} source The source text of the script.
 * @param {string} url The URL of the script, used as its identifier and as
 *   the base URL for imports.
 * @returns {Promise}
 */
function runModuleScriptSource(source, url) {
  // Necessary to reset RegExp statics before user code runs.
  RegExpPrototypeExec(/^/, '');
  return require('internal/modules/run_main').runEntryPointWithESMLoader(
    (loader) => loader.eval(source, url, true),
  );
}

/**
 * Synchronously obtains the source text of a classic script, emulating
 * "fetch a classic worker-imported script" for locally resolvable URLs.
 * @param {URL} url The parsed URL of the script.
 * @param {Blob|undefined} blob The blob URL entry captured when url was
 *   parsed, for blob: URLs.
 * @returns {string}
 */
function fetchClassicScriptSourceSync(url, blob) {
  switch (url.protocol) {
    case 'file:':
      try {
        return require('fs').readFileSync(url, 'utf8');
      } catch {
        break;
      }
    case 'data:': {
      const source = readDataURLScript(url);
      if (source !== undefined) {
        return source;
      }
      break;
    }
    case 'blob:': {
      const data = getScriptBlobData(blob);
      if (data !== undefined) {
        return utf8Decode(data);
      }
      break;
    }
  }
  // "If response's ... or its Content-Type metadata is not a JavaScript
  // MIME type, then throw a "NetworkError" DOMException."
  throw lazyDOMException(`Failed to load script: ${url.href}`, 'NetworkError');
}

// Web IDL overload resolution for the postMessage(message, transfer) /
// postMessage(message, options) overload pair: iterable objects select the
// sequence<object> overload, everything else is converted as a
// StructuredSerializeOptions dictionary.
function normalizeTransfer(transferOrOptions, options) {
  if (typeof transferOrOptions === 'object' && transferOrOptions !== null &&
      transferOrOptions[SymbolIterator] !== undefined) {
    return converters['sequence<object>'](transferOrOptions, options);
  }
  return converters.StructuredSerializeOptions(transferOrOptions, options)
    .transfer;
}

const kScopePostMessageOptions = {
  __proto__: null,
  prefix: "Failed to execute 'postMessage' on 'DedicatedWorkerGlobalScope'",
  context: 'Argument 2',
};

const kWorkerPostMessageOptions = {
  __proto__: null,
  prefix: "Failed to execute 'postMessage' on 'Worker'",
  context: 'Argument 2',
};

// https://html.spec.whatwg.org/multipage/workers.html#the-workernavigator-interface
//
// [Exposed=Worker]
// interface WorkerNavigator {};
//
// The NavigatorLanguage, NavigatorConcurrentHardware and NavigatorLocks
// mixins, as well as userAgent and platform of NavigatorID, are inherited
// from the Node.js Navigator implementation; the members below are the
// remainder of the mixins WorkerNavigator includes.
class WorkerNavigator extends Navigator {
  constructor(guard = undefined) {
    if (guard !== kCreate)
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    super(kInitializeNavigator);
    this[kNavigatorBrand] = true;
  }

  // WorkerNavigator includes NavigatorID;
  // Refs: https://html.spec.whatwg.org/multipage/system-state.html#navigatorid
  //
  // interface mixin NavigatorID {
  // readonly attribute DOMString appVersion;
  get appVersion() {
    validateThisInternalField(this, kNavigatorBrand, 'WorkerNavigator');
    return StringPrototypeSlice(process.version, 1);
  }
  // };

  // WorkerNavigator includes NavigatorLocks;
  get locks() {
    return super.locks;
  }
}

// Refs: https://html.spec.whatwg.org/multipage/system-state.html#navigatoronline
//
// interface mixin NavigatorOnLine {
// readonly attribute boolean onLine;
//   "Returns false if the user agent is definitely offline. Returns true
//   if the user agent might be online."
// };
const kNavigatorConstants = [
  ['appCodeName', 'Mozilla'],
  ['appName', 'Netscape'],
  ['product', 'Gecko'],
  ['onLine', true],
];
for (let i = 0; i < kNavigatorConstants.length; i++) {
  const { 0: key, 1: value } = kNavigatorConstants[i];
  ObjectDefineProperty(WorkerNavigator.prototype, key, {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get: assignFunctionName(`get ${key}`, function() {
      validateThisInternalField(this, kNavigatorBrand, 'WorkerNavigator');
      return value;
    }),
  });
}

ObjectDefineProperties(WorkerNavigator.prototype, {
  appVersion: kEnumerableProperty,
  locks: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'WorkerNavigator',
  },
});

// https://html.spec.whatwg.org/multipage/workers.html#worker-locations
//
// [Exposed=Worker]
// interface WorkerLocation {
class WorkerLocation {
  constructor(guard = undefined, url = undefined) {
    if (guard !== kCreate)
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    // "A WorkerLocation object has an associated WorkerGlobalScope object
    // (a WorkerGlobalScope object)."
    this[kURL] = url;
  }

  // "stringifier", for the href attribute defined below
  toString() {
    validateThisInternalField(this, kURL, 'WorkerLocation');
    return this[kURL].href;
  }
}
// };

// stringifier readonly attribute USVString href;
// readonly attribute USVString origin;
// readonly attribute USVString protocol;
// readonly attribute USVString host;
// readonly attribute USVString hostname;
// readonly attribute USVString port;
// readonly attribute USVString pathname;
// readonly attribute USVString search;
// readonly attribute USVString hash;
const kWorkerLocationAttributes = [
  'href', 'origin', 'protocol', 'host', 'hostname', 'port', 'pathname',
  'search', 'hash',
];
for (let i = 0; i < kWorkerLocationAttributes.length; i++) {
  const key = kWorkerLocationAttributes[i];
  ObjectDefineProperty(WorkerLocation.prototype, key, {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get: assignFunctionName(`get ${key}`, function() {
      validateThisInternalField(this, kURL, 'WorkerLocation');
      return this[kURL][key];
    }),
  });
}

ObjectDefineProperties(WorkerLocation.prototype, {
  toString: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'WorkerLocation',
  },
});

// https://html.spec.whatwg.org/multipage/workers.html#the-workerglobalscope-common-interface
//
// [Exposed=Worker]
// interface WorkerGlobalScope : EventTarget {
class WorkerGlobalScope extends EventTarget {
  constructor(guard = undefined) {
    if (guard !== kCreate)
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    super();
  }

  // readonly attribute WorkerGlobalScope self;
  get self() {
    validateThisInternalField(this, kLocation, 'WorkerGlobalScope');
    // "The self attribute must return the WorkerGlobalScope object itself."
    return this;
  }

  // readonly attribute WorkerLocation location;
  get location() {
    validateThisInternalField(this, kLocation, 'WorkerGlobalScope');
    // "The location attribute must return the WorkerLocation object whose
    // associated WorkerGlobalScope object is the WorkerGlobalScope object."
    return this[kLocation];
  }

  // readonly attribute WorkerNavigator navigator;
  get navigator() {
    validateThisInternalField(this, kLocation, 'WorkerGlobalScope');
    // "The navigator attribute of the WorkerGlobalScope interface must
    // return an instance of the WorkerNavigator interface, which represents
    // the identity and state of the user agent (the client)."
    return this[kNavigator] ??= new WorkerNavigator(kCreate);
  }

  // undefined importScripts((TrustedScriptURL or USVString)... urls);
  importScripts(...urls) {
    validateThisInternalField(this, kLocation, 'WorkerGlobalScope');
    const prefix = "Failed to execute 'importScripts' on 'WorkerGlobalScope'";
    // "To import scripts into worker global scope, given a
    // WorkerGlobalScope object worker global scope, a list of scalar value
    // strings urls, and an optional perform the fetch hook performFetch:"
    //
    // "If worker global scope's type is "module", throw a TypeError
    // exception."
    if (this[kType] === 'module') {
      throw new ERR_INVALID_STATE.TypeError(
        'importScripts() cannot be used in a module worker');
    }
    // "If urls is empty, return."
    if (urls.length === 0)
      return;

    // "Let urlRecords be [an empty list]"
    const urlRecords = [];
    // "For each url of urls:"
    for (let i = 0; i < urls.length; i++) {
      const url = converters.USVString(
        urls[i], { prefix, context: `Argument ${i + 1}` });
      // "Let urlRecord be the result of encoding-parsing a URL given url,
      // relative to settings object."
      const urlRecord = URLParse(url, this[kLocation].href);

      if (urlRecord === null) {
        // "If urlRecord is failure, then throw a "SyntaxError"
        // DOMException."
        throw lazyDOMException(`Failed to parse URL: ${url}`, 'SyntaxError');
      }
      // Refs: https://url.spec.whatwg.org/#concept-url-parser
      const blob = urlRecord.protocol === 'blob:' ?
        resolveObjectURL(urlRecord.href) : undefined;
      // "Append urlRecord to urlRecords."
      ArrayPrototypePush(urlRecords, { url: urlRecord, blob });
    }
    // "For each urlRecord of urlRecords:"
    for (let i = 0; i < urlRecords.length; i++) {
      // "Fetch a classic worker-imported script given urlRecord and
      // settings object, passing along performFetch if provided. If this
      // succeeds, let script be the result. Otherwise, rethrow the
      // exception."
      const { url, blob } = urlRecords[i];
      const script = fetchClassicScriptSourceSync(url, blob);
      // "Run the classic script script, with rethrow errors set to true."
      runClassicScriptSource(script, url.href);
    }
  }
}

ObjectDefineProperties(WorkerGlobalScope.prototype, {
  self: kEnumerableProperty,
  location: kEnumerableProperty,
  navigator: kEnumerableProperty,
  importScripts: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'WorkerGlobalScope',
  },
});

// attribute OnErrorEventHandler onerror;
defineEventHandler(WorkerGlobalScope.prototype, 'error');
// attribute EventHandler onlanguagechange;
defineEventHandler(WorkerGlobalScope.prototype, 'languagechange');
// attribute EventHandler onoffline;
defineEventHandler(WorkerGlobalScope.prototype, 'offline');
// attribute EventHandler ononline;
defineEventHandler(WorkerGlobalScope.prototype, 'online');
// attribute EventHandler onrejectionhandled;
defineEventHandler(WorkerGlobalScope.prototype, 'rejectionhandled');
// attribute EventHandler onunhandledrejection;
defineEventHandler(WorkerGlobalScope.prototype, 'unhandledrejection');
// };

// https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope
//
// WorkerGlobalScope includes WindowOrWorkerGlobalScope;

// https://w3c.github.io/webcrypto/#crypto-interface
//
// partial interface mixin WindowOrWorkerGlobalScope {
//   [SameObject] readonly attribute Crypto crypto;
// };
const lazyCrypto = getLazy(() => require('internal/crypto/webcrypto').crypto);
ObjectDefineProperty(WorkerGlobalScope.prototype, 'crypto', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get: assignFunctionName('get crypto', function() {
    validateThisInternalField(this, kLocation, 'WorkerGlobalScope');
    if (!hasOpenSSL)
      throw new ERR_NO_CRYPTO();
    return lazyCrypto();
  }),
});

// https://w3c.github.io/hr-time/#the-performance-attribute
//
// partial interface mixin WindowOrWorkerGlobalScope {
//   [Replaceable] readonly attribute Performance performance;
// };
const lazyPerformance = getLazy(() => require('perf_hooks').performance);
ObjectDefineProperty(WorkerGlobalScope.prototype, 'performance', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get: assignFunctionName('get performance', function() {
    validateThisInternalField(this, kLocation, 'WorkerGlobalScope');
    return lazyPerformance();
  }),
  set: assignFunctionName('set performance', function(value) {
    validateThisInternalField(this, kLocation, 'WorkerGlobalScope');
    replaceAttribute(this, 'performance', value);
  }),
});

// Refs: https://html.spec.whatwg.org/multipage/web-messaging.html#messageeventtarget
//
// interface mixin MessageEventTarget {
function includeMessageEventTarget(prototype) {
  // attribute EventHandler onmessage;
  defineEventHandler(prototype, 'message');
  // attribute EventHandler onmessageerror;
  defineEventHandler(prototype, 'messageerror');
}
// };

// https://html.spec.whatwg.org/multipage/workers.html#dedicated-workers-and-the-dedicatedworkerglobalscope-interface
//
// [Global=(Worker,DedicatedWorker),Exposed=DedicatedWorker]
// interface DedicatedWorkerGlobalScope : WorkerGlobalScope {
class DedicatedWorkerGlobalScope extends WorkerGlobalScope {
  // [Replaceable] readonly attribute DOMString name;
  get name() {
    validateThisInternalField(this, kInsidePort, 'DedicatedWorkerGlobalScope');
    // "The name getter steps are to return this's name. Its value
    // represents the name given to the worker using the Worker constructor,
    // used primarily for debugging purposes."
    return this[kName];
  }

  set name(value) {
    validateThisInternalField(this, kInsidePort, 'DedicatedWorkerGlobalScope');
    replaceAttribute(this, 'name', value);
  }

  // undefined postMessage(any message, sequence<object> transfer);
  // undefined postMessage(any message, optional StructuredSerializeOptions options = {});
  postMessage(message, transferOrOptions = kEmptyObject) {
    validateThisInternalField(this, kInsidePort, 'DedicatedWorkerGlobalScope');
    requiredArguments(arguments.length, 1, kScopePostMessageOptions);
    // When the second argument was omitted there is nothing to convert.
    const transfer = transferOrOptions === kEmptyObject ? undefined :
      normalizeTransfer(transferOrOptions, kScopePostMessageOptions);
    // "The postMessage(message, transfer) and postMessage(message, options)
    // methods on DedicatedWorkerGlobalScope objects act as if, when
    // invoked, it immediately invoked the respective postMessage(message,
    // transfer) and postMessage(message, options) on the port, with the
    // same arguments, and returned the same return value."
    this[kInsidePort]?.postMessage(message, transfer);
  }

  // undefined close();
  close() {
    validateThisInternalField(this, kInsidePort, 'DedicatedWorkerGlobalScope');
    // "The close() method steps are to close a worker given this."
    //
    // "To close a worker, given a workerGlobal, run these steps:
    //  1. Discard any tasks that have been added to workerGlobal's relevant
    //   agent's event loop's task queues.
    //  2. Set workerGlobal's closing flag to true. (This prevents any
    //   further tasks from being queued.)"
    process.exit();
  }
}

ObjectDefineProperties(DedicatedWorkerGlobalScope.prototype, {
  name: kEnumerableProperty,
  postMessage: kEnumerableProperty,
  close: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'DedicatedWorkerGlobalScope',
  },
});
// };

// DedicatedWorkerGlobalScope includes MessageEventTarget;
includeMessageEventTarget(DedicatedWorkerGlobalScope.prototype);

// https://html.spec.whatwg.org/multipage/workers.html#abstractworker
//
// interface mixin AbstractWorker {
function includeAbstractWorker(prototype) {
  // attribute EventHandler onerror;
  defineEventHandler(prototype, 'error');
}
// };

// https://html.spec.whatwg.org/multipage/workers.html#worker-options
//
// enum WorkerType { "classic", "module" };
const convertWorkerType = createEnumConverter('WorkerType', [
  'classic',
  'module',
]);

// RequestCredentials is defined by the Fetch Standard.
// Refs: https://fetch.spec.whatwg.org/#requestcredentials
//
// enum RequestCredentials { "omit", "same-origin", "include" };
const convertRequestCredentials = createEnumConverter('RequestCredentials', [
  'omit',
  'same-origin',
  'include',
]);

// dictionary WorkerOptions {
const convertWorkerOptions = createDictionaryConverter(
  'WorkerOptions', [
    {
      // DOMString name = "";
      key: 'name',
      converter: converters.DOMString,
      defaultValue: () => '',
    },
    {
      // WorkerType type = "classic";
      key: 'type',
      converter: convertWorkerType,
      defaultValue: () => 'classic',
    },
    {
      // RequestCredentials credentials = "same-origin"; // credentials is only used if type is "module"
      // (We don't actually use this, but parity with the spec is good, right?)
      key: 'credentials',
      converter: convertRequestCredentials,
      defaultValue: () => 'same-origin',
    },
  ]);
// };

/**
 * @param {URL} workerURL The parsed URL of the worker script.
 * @param {'classic'|'module'} type The worker's type.
 * @returns {{ value?: URL, source?: string }|null}
 */
function resolveWorkerEntry(workerURL, type) {
  switch (workerURL.protocol) {
    case 'file:':
      return { value: workerURL };
    case 'data:': {
      const source = readDataURLScript(workerURL);
      if (source === undefined) {
        return null;
      }
      return { source };
    }
    case 'blob:': {
      const blob = resolveObjectURL(workerURL.href);
      // Blobs are commonly created without an explicit content type, so an
      // empty type is accepted in addition to JavaScript MIME types.
      const data = getScriptBlobData(blob, true);
      if (data === undefined) {
        return null;
      }
      if (type === 'module') {
        // The blob's contents are re-wrapped as a data: URL so that the
        // module loader evaluates them as a module script.
        // TODO(@avivkeller): What if we update the ESM loader to accept blob:
        // urls?
        const bytes = new Uint8Array(data);
        return {
          value: new URL('data:text/javascript;base64,' + base64Slice(
            bytes, 0, TypedArrayPrototypeGetLength(bytes))),
        };
      }
      return { source: utf8Decode(data) };
    }
    default:
      throw lazyDOMException(
        `Worker scripts must be file:, data: or blob: URLs: ${workerURL.href}`,
        'NotSupportedError');
  }
}

// https://html.spec.whatwg.org/multipage/workers.html#dedicated-workers-and-the-worker-interface
//
// [Exposed=(Window,DedicatedWorker,SharedWorker)]
// interface Worker : EventTarget {
class Worker extends EventTarget {
  // constructor((TrustedScriptURL or USVString) scriptURL, optional WorkerOptions options = {});
  constructor(scriptURL, options = kEmptyObject) {
    const prefix = "Failed to construct 'Worker'";
    requiredArguments(arguments.length, 1, { prefix });
    super();
    scriptURL = converters.USVString(
      scriptURL, { prefix, context: 'Argument 1' });
    options = convertWorkerOptions(options, { prefix, context: 'Argument 2' });

    // "Let workerURL be the result of encoding-parsing a URL given
    // compliantScriptURL, relative to outsideSettings."
    const workerURL = URLParse(scriptURL, scopeBaseURL ?? getCWDURL());

    if (workerURL === null) {
      // "If workerURL is failure, then throw a "SyntaxError" DOMException."
      throw lazyDOMException(
        `Failed to parse URL: ${scriptURL}`, 'SyntaxError');
    }

    this[kWorker] = null;
    const entry = resolveWorkerEntry(workerURL, options.type);
    if (entry === null) {
      // "If the algorithm asynchronously completes with null or with a
      // script whose error to rethrow is non-null, then: Queue a global
      // task on the DOM manipulation task source given worker's relevant
      // global object to fire an event named error at worker."
      const { setImmediate } = require('timers');
      setImmediate(() => {
        this.dispatchEvent(createErrorEvent({
          message: `Failed to fetch the worker script: ${workerURL.href}`,
          cancelable: true,
        }));
      });
      return;
    }

    // "Run this step in parallel:"
    //
    // "Run a worker given worker, workerURL, outsideSettings, outsidePort,
    // and options."
    this[kWorker] = new NodeWorker(entry.value ?? '', {
      name: options.name,
      eval: entry.source !== undefined,
      [kWebWorkerData]: {
        url: workerURL.href,
        name: options.name,
        type: options.type,
        source: entry.source,
      },
    });

    forwardMessageEvents(this[kWorker][kPublicPort], this);

    // "Set notHandled to the result of firing an event named error at
    // workerObject, using ErrorEvent, with the cancelable attribute
    // initialized to true, and additional attributes initialized according
    // to errorInfo."
    this[kWorker].on('error', (error) =>
      this.dispatchEvent(createErrorEvent({
        message: `${error?.message ?? error}`,
        error,
        cancelable: true,
      })));
  }

  // undefined terminate();
  terminate() {
    validateThisInternalField(this, kWorker, 'Worker');
    // "The terminate() method steps are to terminate a worker given this's
    // worker."
    this[kWorker]?.terminate();
  }

  // undefined postMessage(any message, sequence<object> transfer);
  // undefined postMessage(any message, optional StructuredSerializeOptions options = {});
  postMessage(message, transferOrOptions = kEmptyObject) {
    validateThisInternalField(this, kWorker, 'Worker');
    requiredArguments(arguments.length, 1, kWorkerPostMessageOptions);
    // When the second argument was omitted there is nothing to convert.
    const transfer = transferOrOptions === kEmptyObject ? undefined :
      normalizeTransfer(transferOrOptions, kWorkerPostMessageOptions);
    // "The postMessage(message, transfer) and postMessage(message, options)
    // methods on Worker objects act as if, when invoked, they immediately
    // invoked the respective postMessage(message, transfer) and
    // postMessage(message, options) on this's outside port, with the same
    // arguments, and returned the same return value."
    this[kWorker]?.postMessage(message, transfer);
  }
}

ObjectDefineProperties(Worker.prototype, {
  terminate: kEnumerableProperty,
  postMessage: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'Worker',
  },
});
// };

// Worker includes AbstractWorker;
includeAbstractWorker(Worker.prototype);
// Worker includes MessageEventTarget;
includeMessageEventTarget(Worker.prototype);

// Operations of the worker global scope that are additionally exposed as
// own properties of the global object
const kScopeOperations = [
  // EventTarget
  'addEventListener', 'dispatchEvent', 'removeEventListener',
  // WorkerGlobalScope
  'importScripts',
  // DedicatedWorkerGlobalScope
  'postMessage', 'close',
];

/**
 * @param {URL | string} url The worker's script URL.
 * @param {{ name?: string, type?: string, port?: MessagePort }} [options]
 * @returns {DedicatedWorkerGlobalScope} globalThis
 */
function installDedicatedWorkerGlobalScope(url, {
  name = '',
  type = 'classic',
  port = require('worker_threads').parentPort,
} = kEmptyObject) {
  const locationURL = new URL(url);

  if (type === 'classic' && locationURL.protocol === 'file:') {
    const { Module } = require('internal/modules/cjs/loader');
    ObjectDefineProperty(globalThis, 'require', {
      __proto__: null,
      configurable: true,
      writable: true,
      value: Module.createRequire(locationURL),
    });
  }

  const globalProto = ObjectGetPrototypeOf(globalThis);
  ObjectSetPrototypeOf(globalProto, DedicatedWorkerGlobalScope.prototype);
  ObjectDefineProperty(globalProto, 'constructor', {
    __proto__: null,
    configurable: true,
    writable: true,
    value: DedicatedWorkerGlobalScope,
  });

  initEventTarget(globalThis);

  // "Create a new WorkerLocation object and associate it with worker global
  // scope."
  globalThis[kLocation] = new WorkerLocation(kCreate, locationURL);
  globalThis[kName] = `${name}`;
  globalThis[kType] = type;
  // "DedicatedWorkerGlobalScope objects have an associated inside port (a
  // MessagePort)."
  // "Set worker global scope's inside port to inside port."
  globalThis[kInsidePort] = port;
  scopeBaseURL = locationURL;

  if (port !== null) {
    // "Set inside port's message event target to worker global scope."
    forwardMessageEvents(port, globalThis);
  }

  // We install our own navigator, crypto and performance, see
  // WorkerGlobalScope
  ReflectDeleteProperty(globalThis, 'navigator');
  ReflectDeleteProperty(globalThis, 'Navigator');
  ReflectDeleteProperty(globalThis, 'crypto');
  ReflectDeleteProperty(globalThis, 'performance');

  for (let i = 0; i < kScopeOperations.length; i++) {
    const key = kScopeOperations[i];
    defineOperation(globalThis, key, assignFunctionName(
      key, FunctionPrototypeBind(globalThis[key], globalThis)));
  }

  // [Exposed=Worker] and [Exposed=DedicatedWorker]
  exposeInterface(globalThis, 'WorkerGlobalScope', WorkerGlobalScope);
  exposeInterface(
    globalThis, 'DedicatedWorkerGlobalScope', DedicatedWorkerGlobalScope);
  exposeInterface(globalThis, 'WorkerNavigator', WorkerNavigator);
  exposeInterface(globalThis, 'WorkerLocation', WorkerLocation);
  return globalThis;
}

module.exports = {
  Worker,
  installDedicatedWorkerGlobalScope,
  runClassicScriptSource,
  runModuleScriptSource,
};
