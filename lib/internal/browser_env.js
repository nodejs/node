'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  FunctionPrototypeCall,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectSetPrototypeOf,
  RegExpPrototypeExec,
  String,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  StringPrototypeToUpperCase,
  Symbol,
} = primordials;

const { URL } = require('internal/url');

const { createDocumentAll } = internalBinding('browser_env');

const kVoidElements = new Set([
  'area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta',
  'param', 'source', 'track', 'wbr',
]);

const kDefaultNavigator = {
  appCodeName: 'Mozilla',
  appName: 'Netscape',
  appVersion: '5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
    + '(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
  cookieEnabled: true,
  connection: {
    downlink: 10,
    effectiveType: '4g',
    onchange: null,
    rtt: 200,
    saveData: false,
  },
  userAgent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
    + '(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
  mimeTypes: [
    { description: 'Portable Document Format', suffixes: 'pdf', type: 'application/pdf' },
    { description: 'Portable Document Format', suffixes: 'pdf', type: 'text/pdf' },
  ],
  platform: 'Win32',
  language: 'en-US',
  languages: ['en-US', 'en'],
  vendor: 'Google Inc.',
  maxTouchPoints: 0,
  hardwareConcurrency: 8,
  deviceMemory: 8,
  onLine: true,
  product: 'Gecko',
  productSub: '20030107',
  vendorSub: '',
  webdriver: false,
};

const kProtectedWindowProperties = new Set([
  'window', 'self', 'top', 'parent', 'globalThis', 'document', 'location',
  'navigator', 'history', 'screen', 'Window', 'Navigator', 'Event',
  'Node', 'Element', 'HTMLElement', 'Document', 'Text', 'localStorage',
  'sessionStorage', 'addEventListener', 'removeEventListener', 'dispatchEvent',
  'History', 'Screen', 'Location', 'DOMParser', 'XMLHttpRequest',
  'MutationObserver', 'HTMLAnchorElement', 'HTMLCanvasElement',
  'CanvasRenderingContext2D', 'HTMLFormElement', 'HTMLInputElement',
  'HTMLBodyElement', 'HTMLCollection', 'HTMLHeadElement', 'HTMLHtmlElement',
  'HTMLDivElement', 'HTMLIFrameElement', 'HTMLMetaElement', 'HTMLScriptElement',
  'HTMLTitleElement', 'MimeType', 'MimeTypeArray', 'Storage', 'BatteryManager',
  'NetworkInformation',
  'indexedDB', 'chrome', 'clientInformation', 'msCrypto', 'name', 'open',
  'prompt', 'webkitRequestFileSystem', 'TEMPORARY',
  'global', 'process', 'Buffer', 'require', 'module', 'exports',
  'setImmediate', 'clearImmediate',
]);

const kProtectedDocumentProperties = new Set([
  'all', 'location', 'documentElement', 'head', 'body', 'cookie',
  'createElement', 'createTextNode', 'appendChild', 'insertBefore',
  'removeChild', 'replaceChild', 'getElementById', 'getElementsByTagName',
  'getElementsByClassName', 'getElementsByName', 'querySelector',
  'querySelectorAll', 'defaultView', 'addEventListener', 'removeEventListener',
  'dispatchEvent', 'createExpression',
]);

let installed = false;
let installedEnvironment;

function assertObject(value, name) {
  if (value === null || typeof value !== 'object') {
    throw new TypeError(`${name} must be an object`);
  }
}

function getHref(options) {
  const href = options.url ?? options.location?.href;
  if (typeof href !== 'string' || href.length === 0) {
    throw new TypeError('browser environment requires a non-empty url');
  }
  return href;
}

function defineInternal(target, name, value) {
  ObjectDefineProperty(target, name, {
    __proto__: null,
    configurable: false,
    enumerable: false,
    value,
    writable: true,
  });
}

function markAsNativeFunction(value, name = value.name) {
  if (typeof value !== 'function') return value;
  if (value.name !== name) {
    ObjectDefineProperty(value, 'name', {
      __proto__: null,
      configurable: true,
      enumerable: false,
      value: name,
      writable: false,
    });
  }
  ObjectDefineProperty(value, 'toString', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: function toString() { return `function ${name}() { [native code] }`; },
    writable: true,
  });
  return value;
}

function markNativePrototype(prototype) {
  for (const name of ObjectGetOwnPropertyNames(prototype)) {
    if (name === 'constructor') continue;
    const descriptor = ObjectGetOwnPropertyDescriptor(prototype, name);
    if (typeof descriptor?.value === 'function') markAsNativeFunction(descriptor.value);
    if (typeof descriptor?.get === 'function') markAsNativeFunction(descriptor.get);
    if (typeof descriptor?.set === 'function') markAsNativeFunction(descriptor.set);
  }
}

function createEventTarget(target) {
  const listeners = new Map();
  ObjectDefineProperty(target, '_listeners', {
    __proto__: null,
    configurable: false,
    enumerable: false,
    value: listeners,
    writable: false,
  });
  ObjectDefineProperty(target, 'addEventListener', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: markAsNativeFunction(function addEventListener(type, callback) {
      if (typeof callback !== 'function') return;
      const key = String(type);
      let callbacks = listeners.get(key);
      if (callbacks === undefined) {
        callbacks = [];
        listeners.set(key, callbacks);
      }
      if (!ArrayPrototypeIncludes(callbacks, callback)) ArrayPrototypePush(callbacks, callback);
    }),
    writable: true,
  });
  ObjectDefineProperty(target, 'removeEventListener', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: markAsNativeFunction(function removeEventListener(type, callback) {
      const callbacks = listeners.get(String(type));
      if (callbacks === undefined) return;
      const index = callbacks.indexOf(callback);
      if (index !== -1) callbacks.splice(index, 1);
    }),
    writable: true,
  });
  ObjectDefineProperty(target, 'dispatchEvent', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: markAsNativeFunction(function dispatchEvent(event) {
      if (event === null || typeof event !== 'object' || !event.type) {
        throw new TypeError('dispatchEvent requires an event with a type');
      }
      event.target ??= this;
      event.currentTarget = this;
      const callbacks = listeners.get(String(event.type));
      if (callbacks !== undefined) {
        for (const callback of ArrayPrototypeSlice(callbacks)) {
          FunctionPrototypeCall(callback, this, event);
        }
      }
      const handler = this[`on${event.type}`];
      if (typeof handler === 'function') FunctionPrototypeCall(handler, this, event);
      return !event.defaultPrevented;
    }),
    writable: true,
  });
  return target;
}

class BrowserEvent {
  constructor(type, init = {}) {
    this.type = String(type);
    this.bubbles = Boolean(init.bubbles);
    this.cancelable = Boolean(init.cancelable);
    this.defaultPrevented = false;
  }

  preventDefault() {
    if (this.cancelable) this.defaultPrevented = true;
  }
}

class BrowserNode {
  constructor(ownerDocument, nodeType, nodeName) {
    createEventTarget(this);
    defineInternal(this, '_ownerDocument', ownerDocument);
    defineInternal(this, '_nodeType', nodeType);
    defineInternal(this, '_nodeName', nodeName);
    defineInternal(this, '_parentNode', null);
    defineInternal(this, '_childNodes', []);
  }

  get ownerDocument() {
    return this._ownerDocument;
  }

  set ownerDocument(value) {
    this._ownerDocument = value;
  }

  get nodeType() {
    return this._nodeType;
  }

  get nodeName() {
    return this._nodeName;
  }

  get parentNode() {
    return this._parentNode;
  }

  set parentNode(value) {
    this._parentNode = value;
  }

  get childNodes() {
    return this._childNodes;
  }

  get firstChild() {
    return this.childNodes[0] ?? null;
  }

  get lastChild() {
    return this.childNodes[this.childNodes.length - 1] ?? null;
  }

  get parentElement() {
    return this.parentNode?.nodeType === 1 ? this.parentNode : null;
  }

  get children() {
    return createCollection(() => this.childNodes.filter((node) => node.nodeType === 1));
  }

  appendChild(child) {
    return this.insertBefore(child, null);
  }

  insertBefore(child, referenceNode) {
    if (!(child instanceof BrowserNode)) throw new TypeError('child must be a Node');
    if (referenceNode !== null && referenceNode.parentNode !== this) {
      throw new Error('reference node is not a child of this node');
    }
    if (child.parentNode !== null) child.parentNode.removeChild(child);
    const index = referenceNode === null ? this.childNodes.length : this.childNodes.indexOf(referenceNode);
    this.childNodes.splice(index, 0, child);
    child.parentNode = this;
    return child;
  }

  removeChild(child) {
    const index = this.childNodes.indexOf(child);
    if (index === -1) throw new Error('child is not a child of this node');
    this.childNodes.splice(index, 1);
    child.parentNode = null;
    return child;
    return child;
  }

  replaceChild(child, oldChild) {
    const index = this.childNodes.indexOf(oldChild);
    if (index === -1) throw new Error('old child is not a child of this node');
    if (child.parentNode !== null) child.parentNode.removeChild(child);
    this.childNodes[index] = child;
    child.parentNode = this;
    oldChild.parentNode = null;
    return oldChild;
  }

  contains(node) {
    for (let current = node; current !== null; current = current.parentNode) {
      if (current === this) return true;
    }
    return false;
  }

  get textContent() {
    if (this.nodeType === 3) return this.data;
    return this.childNodes.map((node) => node.textContent).join('');
  }

  set textContent(value) {
    for (const child of this.childNodes) child.parentNode = null;
    this.childNodes.length = 0;
    const text = String(value);
    if (text.length > 0) this.appendChild(new BrowserText(this.ownerDocument, text));
  }

  get innerText() {
    return this.textContent;
  }

  set innerText(value) {
    this.textContent = value;
  }
}

class BrowserText extends BrowserNode {
  constructor(ownerDocument, data) {
    super(ownerDocument, 3, '#text');
    this.data = String(data);
  }

  get textContent() {
    return this.data;
  }

  set textContent(value) {
    this.data = String(value);
  }

  get [Symbol.toStringTag]() {
    return 'Text';
  }
}

class BrowserElement extends BrowserNode {
  constructor(ownerDocument, tagName) {
    const localName = StringPrototypeToLowerCase(String(tagName));
    super(ownerDocument, 1, StringPrototypeToUpperCase(localName));
    defineInternal(this, '_tagName', this.nodeName);
    defineInternal(this, '_localName', localName);
    defineInternal(this, '_attributes', ObjectCreate(null));
    defineInternal(this, '_style', ObjectCreate(null));
  }

  get tagName() {
    return this._tagName;
  }

  get localName() {
    return this._localName;
  }

  get attributes() {
    return this._attributes;
  }

  get style() {
    return this._style;
  }

  get [Symbol.toStringTag]() {
    switch (this.localName) {
      case 'body': return 'HTMLBodyElement';
      case 'head': return 'HTMLHeadElement';
      case 'html': return 'HTMLHtmlElement';
      case 'meta': return 'HTMLMetaElement';
      case 'script': return 'HTMLScriptElement';
      case 'title': return 'HTMLTitleElement';
      case 'iframe': return 'HTMLIFrameElement';
      default: return `HTML${this.localName[0].toUpperCase()}${this.localName.slice(1)}Element`;
    }
  }

  get id() {
    return this.getAttribute('id') ?? '';
  }

  set id(value) {
    this.setAttribute('id', value);
  }

  get className() {
    return this.getAttribute('class') ?? '';
  }

  set className(value) {
    this.setAttribute('class', value);
  }

  get name() {
    return this.getAttribute('name') ?? '';
  }

  set name(value) {
    this.setAttribute('name', value);
  }

  get value() {
    return this.getAttribute('value') ?? '';
  }

  set value(value) {
    this.setAttribute('value', value);
  }

  get type() {
    return this.getAttribute('type') ?? '';
  }

  set type(value) {
    this.setAttribute('type', value);
  }

  get src() {
    const value = this.getAttribute('src');
    return value === null ? '' : new URL(value, this.ownerDocument.location.href).href;
  }

  set src(value) {
    this.setAttribute('src', value);
  }

  get content() {
    return this.getAttribute('content') ?? '';
  }

  set content(value) {
    this.setAttribute('content', value);
  }

  get classList() {
    const element = this;
    return {
      add(...tokens) {
        const values = new Set(StringPrototypeSplit(element.className, ' ').filter(Boolean));
        for (const token of tokens) values.add(String(token));
        element.className = ArrayPrototypeJoin([...values], ' ');
      },
      remove(...tokens) {
        const values = new Set(StringPrototypeSplit(element.className, ' ').filter(Boolean));
        for (const token of tokens) values.delete(String(token));
        element.className = ArrayPrototypeJoin([...values], ' ');
      },
      contains(token) {
        return ArrayPrototypeIncludes(StringPrototypeSplit(element.className, ' '), String(token));
      },
      toggle(token, force) {
        const present = this.contains(token);
        const shouldAdd = force === undefined ? !present : Boolean(force);
        if (shouldAdd) this.add(token); else this.remove(token);
        return shouldAdd;
      },
    };
  }

  getAttribute(name) {
    const key = StringPrototypeToLowerCase(String(name));
    return Object.prototype.hasOwnProperty.call(this.attributes, key) ? this.attributes[key] : null;
  }

  hasAttribute(name) {
    return this.getAttribute(name) !== null;
  }

  setAttribute(name, value) {
    this.attributes[StringPrototypeToLowerCase(String(name))] = String(value);
  }

  removeAttribute(name) {
    delete this.attributes[StringPrototypeToLowerCase(String(name))];
  }

  get innerHTML() {
    return this.childNodes.map(serializeNode).join('');
  }

  set innerHTML(value) {
    for (const child of this.childNodes) child.parentNode = null;
    this.childNodes.length = 0;
    for (const node of parseFragment(this.ownerDocument, String(value))) this.appendChild(node);
  }

  getElementsByTagName(tagName) {
    const expected = StringPrototypeToLowerCase(String(tagName));
    return createLiveCollection(this, (element) => expected === '*' || element.localName === expected);
  }

  getElementsByClassName(className) {
    const expected = String(className);
    return createLiveCollection(this, (element) =>
      ArrayPrototypeIncludes(StringPrototypeSplit(element.className, ' '), expected));
  }

  getElementsByName(name) {
    const expected = String(name);
    return createLiveCollection(this, (element) => element.getAttribute('name') === expected);
  }

  querySelector(selector) {
    return findElements(this, selector)[0] ?? null;
  }

  querySelectorAll(selector) {
    return findElements(this, selector);
  }
}

class HTMLHtmlElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'html');
  }
}

class HTMLHeadElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'head');
  }
}

class HTMLBodyElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'body');
  }
}

class HTMLDivElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'div');
  }
}

class HTMLIFrameElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'iframe');
  }

  get contentDocument() {
    return null;
  }

  get contentWindow() {
    return null;
  }
}

class HTMLMetaElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'meta');
  }
}

class HTMLScriptElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'script');
  }
}

class HTMLTitleElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'title');
  }
}

class HTMLAnchorElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'a');
  }

  get _url() {
    return new URL(this.getAttribute('href') ?? '', this.ownerDocument.location.href);
  }

  get href() {
    return this.getAttribute('href') === null ? '' : this._url.href;
  }

  set href(value) {
    this.setAttribute('href', value);
  }

  get origin() { return this.getAttribute('href') === null ? '' : this._url.origin; }
  get protocol() { return this.getAttribute('href') === null ? '' : this._url.protocol; }
  get host() { return this.getAttribute('href') === null ? '' : this._url.host; }
  get hostname() { return this.getAttribute('href') === null ? '' : this._url.hostname; }
  get port() { return this.getAttribute('href') === null ? '' : this._url.port; }
  get pathname() { return this.getAttribute('href') === null ? '' : this._url.pathname; }
  get search() { return this.getAttribute('href') === null ? '' : this._url.search; }
  get hash() { return this.getAttribute('href') === null ? '' : this._url.hash; }
}

class CanvasRenderingContext2D {
  constructor(canvas) {
    this.canvas = canvas;
    this.fillStyle = '#000000';
    this.font = '10px sans-serif';
  }

  clearRect() {}
  fillRect() {}
  fillText() {}
  drawImage() {}
  beginPath() {}
  closePath() {}
  stroke() {}
  fill() {}

  getImageData() {
    return { data: new Uint8ClampedArray(0), height: 0, width: 0 };
  }

  measureText(value) {
    return { width: String(value).length * 6 };
  }
}

class HTMLCanvasElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'canvas');
    this.height = 150;
    this.width = 300;
    this._context2d = undefined;
  }

  getContext(type) {
    if (StringPrototypeToLowerCase(String(type)) !== '2d') return null;
    this._context2d ??= new CanvasRenderingContext2D(this);
    return this._context2d;
  }

  toDataURL() {
    return 'data:,';
  }
}

class HTMLFormElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'form');
  }

  get elements() {
    return createLiveCollection(this, (element) =>
      element !== this && ['button', 'fieldset', 'input', 'object', 'output', 'select', 'textarea'].includes(element.localName));
  }

  get action() {
    return new URL(this.getAttribute('action') ?? '', this.ownerDocument.location.href).href;
  }

  set action(value) {
    this.setAttribute('action', value);
  }
}

class HTMLInputElement extends BrowserElement {
  constructor(ownerDocument) {
    super(ownerDocument, 'input');
  }
}

class BrowserDocument extends BrowserNode {
  constructor(location) {
    super(null, 9, '#document');
    this.ownerDocument = this;
    defineInternal(this, '_location', location);
    defineInternal(this, '_cookieJar', new Map());
    defineInternal(this, '_documentElement', null);
    defineInternal(this, '_head', null);
    defineInternal(this, '_body', null);
    ObjectDefineProperty(this, 'characterSet', {
      __proto__: null,
      configurable: true,
      enumerable: false,
      value: 'UTF-8',
      writable: true,
    });
    ObjectDefineProperty(this, 'charset', {
      __proto__: null,
      configurable: true,
      enumerable: false,
      value: 'UTF-8',
      writable: true,
    });
    ObjectDefineProperty(this, 'visibilityState', {
      __proto__: null,
      configurable: true,
      enumerable: false,
      value: 'visible',
      writable: true,
    });
    this._resetDocumentElement();
  }

  get [Symbol.toStringTag]() {
    return 'HTMLDocument';
  }

  get documentElement() {
    return this._documentElement;
  }

  set documentElement(value) {
    this._documentElement = value;
  }

  get head() {
    return this._head;
  }

  set head(value) {
    this._head = value;
  }

  get body() {
    return this._body;
  }

  set body(value) {
    this._body = value;
  }

  _resetDocumentElement() {
    for (const child of this.childNodes) child.parentNode = null;
    this.childNodes.length = 0;
    this.documentElement = this.createElement('html');
    this.head = this.createElement('head');
    this.body = this.createElement('body');
    this.documentElement.appendChild(this.head);
    this.documentElement.appendChild(this.body);
    this.appendChild(this.documentElement);
  }

  get location() {
    return this._location;
  }

  get cookie() {
    return [...this._cookieJar].map(([key, value]) => `${key}=${value}`).join('; ');
  }

  set cookie(value) {
    const entry = StringPrototypeSplit(String(value), ';')[0];
    const index = entry.indexOf('=');
    if (index !== -1) this._cookieJar.set(entry.slice(0, index).trim(), entry.slice(index + 1).trim());
  }

  get title() {
    return this.querySelector('title')?.textContent ?? '';
  }

  set title(value) {
    let title = this.querySelector('title');
    if (title === null) {
      title = this.createElement('title');
      this.head.appendChild(title);
    }
    title.textContent = value;
  }

  get scripts() {
    return this.getElementsByTagName('script');
  }

  createElement(tagName) {
    switch (StringPrototypeToLowerCase(String(tagName))) {
      case 'a':
        return new HTMLAnchorElement(this);
      case 'body':
        return new HTMLBodyElement(this);
      case 'canvas':
        return new HTMLCanvasElement(this);
      case 'div':
        return new HTMLDivElement(this);
      case 'form':
        return new HTMLFormElement(this);
      case 'head':
        return new HTMLHeadElement(this);
      case 'html':
        return new HTMLHtmlElement(this);
      case 'input':
        return new HTMLInputElement(this);
      case 'iframe':
        return new HTMLIFrameElement(this);
      case 'meta':
        return new HTMLMetaElement(this);
      case 'script':
        return new HTMLScriptElement(this);
      case 'title':
        return new HTMLTitleElement(this);
      default:
        return new BrowserElement(this, tagName);
    }
  }

  createTextNode(data) {
    return new BrowserText(this, data);
  }

  createExpression() {
    return ObjectCreate(null);
  }

  getElementById(id) {
    return walkElements(this, (element) => element.id === String(id))[0] ?? null;
  }

  getElementsByTagName(tagName) {
    const expected = StringPrototypeToLowerCase(String(tagName));
    return createLiveCollection(this, (element) => expected === '*' || element.localName === expected);
  }

  getElementsByClassName(className) {
    return this.documentElement.getElementsByClassName(className);
  }

  getElementsByName(name) {
    return this.documentElement.getElementsByName(name);
  }

  querySelector(selector) {
    return findElements(this, selector)[0] ?? null;
  }

  querySelectorAll(selector) {
    return findElements(this, selector);
  }

  loadHTML(html) {
    const nodes = parseFragment(this, html);
    const parsedHtml = nodes.find((node) => node instanceof BrowserElement && node.localName === 'html');
    this._resetDocumentElement();
    if (parsedHtml !== undefined) {
      this.removeChild(this.documentElement);
      this.documentElement = parsedHtml;
      this.documentElement.parentNode = null;
      this.appendChild(this.documentElement);
      this.head = this.documentElement.childNodes.find((node) => node.localName === 'head') ?? this.createElement('head');
      this.body = this.documentElement.childNodes.find((node) => node.localName === 'body') ?? this.createElement('body');
      if (this.head.parentNode === null) this.documentElement.insertBefore(this.head, this.documentElement.firstChild);
      if (this.body.parentNode === null) this.documentElement.appendChild(this.body);
    } else {
      for (const node of nodes) this.body.appendChild(node);
    }
  }
}

function walkElements(root, predicate) {
  const result = [];
  const visit = (node) => {
    for (const child of node.childNodes) {
      if (child.nodeType === 1) {
        if (predicate(child)) ArrayPrototypePush(result, child);
        visit(child);
      }
    }
  };
  if (root.nodeType === 1 && predicate(root)) ArrayPrototypePush(result, root);
  visit(root);
  return result;
}

function HTMLCollection() {
  throw new TypeError('Illegal constructor');
}

ObjectDefineProperty(HTMLCollection.prototype, Symbol.toStringTag, {
  __proto__: null,
  configurable: true,
  value: 'HTMLCollection',
});

function createCollection(current) {
  const collection = ObjectCreate(HTMLCollection.prototype);
  return new Proxy(collection, {
    get(target, property, receiver) {
      if (property === 'length') return current().length;
      if (property === 'item') return (index) => current()[index] ?? null;
      if (property === 'namedItem') return (name) => current().find((element) =>
        element.id === name || element.getAttribute('name') === name) ?? null;
      if (property === Symbol.iterator) return function* iterator() { yield* current(); };
      if (typeof property === 'string' && /^\d+$/.test(property)) return current()[Number(property)];
      return Reflect.get(target, property, receiver);
    },
    ownKeys() {
      return [...current().keys()].map(String);
    },
    getOwnPropertyDescriptor(target, property) {
      if (property === 'length') {
        return {
          configurable: true,
          enumerable: false,
          value: current().length,
          writable: false,
        };
      }
      if (typeof property === 'string' && /^\d+$/.test(property)) {
        return {
          configurable: true,
          enumerable: true,
          value: current()[Number(property)],
          writable: false,
        };
      }
      return ObjectGetOwnPropertyDescriptor(target, property);
    },
  });
}

function createLiveCollection(root, predicate) {
  return createCollection(() => walkElements(root, predicate));
}

function DOMStringList() {
  throw new TypeError('Illegal constructor');
}

ObjectDefineProperty(DOMStringList.prototype, Symbol.toStringTag, {
  __proto__: null,
  configurable: true,
  value: 'DOMStringList',
});

function selectorMatches(element, selector) {
  const attribute = RegExpPrototypeExec(/^\[([\w-]+)(?:=["']?([^\]"']+)["']?)?\]$/, selector);
  if (attribute !== null) {
    return element.hasAttribute(attribute[1]) &&
      (attribute[2] === undefined || element.getAttribute(attribute[1]) === attribute[2]);
  }
  const id = RegExpPrototypeExec(/^#([\w-]+)$/, selector);
  if (id !== null) return element.id === id[1];
  const className = RegExpPrototypeExec(/^\.([\w-]+)$/, selector);
  if (className !== null) return element.classList.contains(className[1]);
  const tagClass = RegExpPrototypeExec(/^([\w-]+)\.([\w-]+)$/, selector);
  if (tagClass !== null) return element.localName === tagClass[1].toLowerCase() && element.classList.contains(tagClass[2]);
  return element.localName === StringPrototypeToLowerCase(selector);
}

function findElements(root, selector) {
  const selectors = StringPrototypeSplit(String(selector).trim(), /\s+/).filter(Boolean);
  if (selectors.length === 0) return [];
  const candidates = walkElements(root, (element) => selectorMatches(element, selectors[selectors.length - 1]));
  return candidates.filter((candidate) => {
    let current = candidate.parentElement;
    for (let index = selectors.length - 2; index >= 0; index--) {
      while (current !== null && !selectorMatches(current, selectors[index])) current = current.parentElement;
      if (current === null) return false;
      current = current.parentElement;
    }
    return true;
  });
}

function parseAttributes(element, source) {
  const attributePattern = /([^\s=/>]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'=<>`]+)))?/g;
  let match;
  while ((match = attributePattern.exec(source)) !== null) {
    element.setAttribute(match[1], match[2] ?? match[3] ?? match[4] ?? '');
  }
}

function parseFragment(document, source) {
  const root = new BrowserNode(document, 11, '#document-fragment');
  const stack = [root];
  const tokenPattern = /<!--[\s\S]*?-->|<\/?([A-Za-z][\w:-]*)([^>]*)>|([^<]+)/g;
  let match;
  while ((match = tokenPattern.exec(source)) !== null) {
    if (match[0].startsWith('<!--')) {
      // Conditional comments can contain markup which is ignored by modern
      // browsers. Do not accidentally turn that hidden markup into DOM nodes.
      // Inside a script element the same sequence is JavaScript text instead.
      if (stack[stack.length - 1].localName === 'script') {
        stack[stack.length - 1].appendChild(document.createTextNode(match[0]));
      }
      continue;
    }
    if (match[3] !== undefined) {
      stack[stack.length - 1].appendChild(document.createTextNode(match[3]));
      continue;
    }
    const tagName = StringPrototypeToLowerCase(match[1]);
    if (source[match.index + 1] === '/') {
      for (let index = stack.length - 1; index > 0; index--) {
        if (stack[index].localName === tagName) {
          stack.length = index;
          break;
        }
      }
      continue;
    }
    const element = document.createElement(tagName);
    parseAttributes(element, match[2]);
    stack[stack.length - 1].appendChild(element);
    if (!kVoidElements.has(tagName) && !/\/\s*$/.test(match[2])) ArrayPrototypePush(stack, element);
  }
  return root.childNodes;
}

function serializeNode(node) {
  if (node.nodeType === 3) return node.data;
  if (node.nodeType !== 1) return '';
  const attributes = ObjectKeys(node.attributes).map((key) => ` ${key}="${node.attributes[key]}"`).join('');
  if (kVoidElements.has(node.localName)) return `<${node.localName}${attributes}>`;
  return `<${node.localName}${attributes}>${node.childNodes.map(serializeNode).join('')}</${node.localName}>`;
}

function createLocation(href) {
  let url;
  const replace = (value) => {
    url = new URL(String(value), url);
  };
  replace(href);
  const location = {};
  for (const key of ['href', 'origin', 'protocol', 'host', 'hostname', 'port', 'pathname', 'search', 'hash']) {
    ObjectDefineProperty(location, key, {
      __proto__: null,
      configurable: false,
      enumerable: true,
      get() { return url[key]; },
      set(value) {
        if (key === 'href') replace(value);
        else url[key] = String(value);
      },
    });
  }
  location.assign = markAsNativeFunction(function assign(value) { replace(value); });
  location.replace = markAsNativeFunction(function replaceLocation(value) { replace(value); }, 'replace');
  location.reload = markAsNativeFunction(function reload() {});
  location.toString = markAsNativeFunction(function toString() { return url.href; });
  ObjectDefineProperty(location, 'ancestorOrigins', {
    __proto__: null,
    configurable: false,
    enumerable: true,
    value: ObjectCreate(DOMStringList.prototype),
    writable: false,
  });
  ObjectDefineProperty(location.ancestorOrigins, 'length', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: 0,
    writable: false,
  });
  ObjectDefineProperty(location.ancestorOrigins, 'item', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: function item() { return null; },
    writable: true,
  });
  return location;
}

function createHistory(location) {
  const entries = [location.href];
  let index = 0;
  return {
    get length() { return entries.length; },
    get state() { return null; },
    pushState(state, unused, url) {
      if (url !== undefined && url !== null) location.assign(url);
      entries.splice(index + 1);
      ArrayPrototypePush(entries, location.href);
      index = entries.length - 1;
    },
    replaceState(state, unused, url) {
      if (url !== undefined && url !== null) location.replace(url);
      entries[index] = location.href;
    },
    back() {
      if (index > 0) location.assign(entries[--index]);
    },
    forward() {
      if (index + 1 < entries.length) location.assign(entries[++index]);
    },
    go(delta = 0) {
      const next = index + Number(delta);
      if (next >= 0 && next < entries.length) {
        index = next;
        location.assign(entries[index]);
      }
    },
  };
}

function createNetworkInformation(values) {
  function NetworkInformation() {
    throw new TypeError('Illegal constructor');
  }
  const connection = ObjectCreate(NetworkInformation.prototype);
  defineInternal(connection, '_values', values);
  defineInternal(connection, '_onchange', values.onchange ?? null);
  for (const name of ['downlink', 'effectiveType', 'rtt', 'saveData']) {
    ObjectDefineProperty(NetworkInformation.prototype, name, {
      __proto__: null,
      configurable: true,
      enumerable: true,
      get() { return this._values[name]; },
    });
  }
  ObjectDefineProperty(NetworkInformation.prototype, 'onchange', {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() { return this._onchange; },
    set(value) { this._onchange = value; },
  });
  ObjectDefineProperty(NetworkInformation.prototype, Symbol.toStringTag, {
    __proto__: null,
    configurable: true,
    value: 'NetworkInformation',
  });
  markAsNativeFunction(NetworkInformation);
  markNativePrototype(NetworkInformation.prototype);
  return { NetworkInformation, connection };
}

function createBatteryManagerFactory() {
  function BatteryManager() {
    throw new TypeError('Illegal constructor');
  }
  for (const name of [
    'charging', 'chargingTime', 'dischargingTime', 'level',
    'onchargingchange', 'onchargingtimechange', 'ondischargingtimechange',
    'onlevelchange',
  ]) {
    ObjectDefineProperty(BatteryManager.prototype, name, {
      __proto__: null,
      configurable: true,
      enumerable: true,
      get() { return this._values[name]; },
      set(value) { this._values[name] = value; },
    });
  }
  ObjectDefineProperty(BatteryManager.prototype, Symbol.toStringTag, {
    __proto__: null,
    configurable: true,
    value: 'BatteryManager',
  });
  markAsNativeFunction(BatteryManager);
  markNativePrototype(BatteryManager.prototype);
  return {
    BatteryManager,
    createBattery() {
      const battery = ObjectCreate(BatteryManager.prototype);
      defineInternal(battery, '_values', {
        charging: true,
        chargingTime: Infinity,
        dischargingTime: Infinity,
        level: 0.8,
        onchargingchange: null,
        onchargingtimechange: null,
        ondischargingtimechange: null,
        onlevelchange: null,
      });
      createEventTarget(battery);
      return battery;
    },
  };
}

function createNavigator(values) {
  function Navigator() {
    throw new TypeError('Illegal constructor');
  }
  for (const [key, value] of Object.entries(values)) {
    ObjectDefineProperty(Navigator.prototype, key, {
      __proto__: null,
      configurable: true,
      enumerable: true,
      get: markAsNativeFunction(function getNavigatorValue() { return value; }, `get ${key}`),
    });
  }
  ObjectDefineProperty(Navigator.prototype, Symbol.toStringTag, {
    __proto__: null,
    configurable: true,
    value: 'Navigator',
  });
  const navigator = ObjectCreate(Navigator.prototype);
  markAsNativeFunction(Navigator);
  markNativePrototype(Navigator.prototype);
  return { Navigator, navigator };
}

function createStorage(seed) {
  const storage = ObjectCreate(Storage.prototype);
  defineInternal(storage, '_values', new Map(Object.entries(seed ?? {})));
  return storage;
}

function Storage() {
  throw new TypeError('Illegal constructor');
}

ObjectDefineProperty(Storage.prototype, 'length', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() { return this._values.size; },
});
Storage.prototype.key = function key(index) {
  return [...this._values.keys()][index] ?? null;
};
Storage.prototype.getItem = function getItem(key) {
  return this._values.get(String(key)) ?? null;
};
Storage.prototype.setItem = function setItem(key, value) {
  this._values.set(String(key), String(value));
};
Storage.prototype.removeItem = function removeItem(key) {
  this._values.delete(String(key));
};
Storage.prototype.clear = function clear() {
  this._values.clear();
};
ObjectDefineProperty(Storage.prototype, Symbol.toStringTag, {
  __proto__: null,
  configurable: true,
  value: 'Storage',
});

function MimeType() {
  throw new TypeError('Illegal constructor');
}

function MimeTypeArray() {
  throw new TypeError('Illegal constructor');
}

ObjectDefineProperty(MimeType.prototype, Symbol.toStringTag, {
  __proto__: null,
  configurable: true,
  value: 'MimeType',
});
ObjectDefineProperty(MimeTypeArray.prototype, Symbol.toStringTag, {
  __proto__: null,
  configurable: true,
  value: 'MimeTypeArray',
});

function createMimeTypeArray(values) {
  const entries = values.map((value) => {
    const entry = ObjectCreate(MimeType.prototype);
    for (const key of ['description', 'suffixes', 'type']) {
      ObjectDefineProperty(entry, key, {
        __proto__: null,
        configurable: true,
        enumerable: true,
        value: value[key] ?? '',
        writable: false,
      });
    }
    return entry;
  });
  const mimeTypes = ObjectCreate(MimeTypeArray.prototype);
  defineInternal(mimeTypes, '_entries', entries);
  for (let index = 0; index < entries.length; index++) {
    const entry = entries[index];
    ObjectDefineProperty(mimeTypes, index, {
      __proto__: null,
      configurable: true,
      enumerable: true,
      value: entry,
    });
    if (entry.type) {
      ObjectDefineProperty(mimeTypes, entry.type, {
        __proto__: null,
        configurable: true,
        enumerable: false,
        value: entry,
      });
    }
  }
  ObjectDefineProperty(mimeTypes, 'length', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    get() { return entries.length; },
  });
  return mimeTypes;
}

MimeTypeArray.prototype.item = function item(index) {
  return this._entries[index] ?? null;
};
MimeTypeArray.prototype.namedItem = function namedItem(name) {
  return this._entries.find((entry) => entry.type === String(name)) ?? null;
};

function createDOMParser(location) {
  return class DOMParser {
    parseFromString(source) {
      const document = new BrowserDocument(createLocation(location.href));
      document.loadHTML(String(source));
      return document;
    }
  };
}

function createXMLHttpRequest(window, location) {
  function XMLHttpRequest() {
    createEventTarget(this);
    this.readyState = XMLHttpRequest.UNSENT;
    this.response = null;
    this.responseText = '';
    this.responseType = '';
    this.responseURL = '';
    this.status = 0;
    this.statusText = '';
    this.timeout = 0;
    this.withCredentials = false;
    this._headers = new Map();
    this._requestHeaders = new Map();
  }

  XMLHttpRequest.UNSENT = 0;
  XMLHttpRequest.OPENED = 1;
  XMLHttpRequest.HEADERS_RECEIVED = 2;
  XMLHttpRequest.LOADING = 3;
  XMLHttpRequest.DONE = 4;
  for (const name of ['UNSENT', 'OPENED', 'HEADERS_RECEIVED', 'LOADING', 'DONE']) {
    XMLHttpRequest.prototype[name] = XMLHttpRequest[name];
  }
  XMLHttpRequest.prototype.open = function open(method, url, async = true) {
    this.method = String(method);
    this.async = Boolean(async);
    this.url = String(url);
    this.responseURL = new URL(this.url, location.href).href;
    this.readyState = XMLHttpRequest.OPENED;
    window.encode_url = this.url;
    this.dispatchEvent(new BrowserEvent('readystatechange'));
  };
  XMLHttpRequest.prototype.setRequestHeader = function setRequestHeader(name, value) {
    if (this.readyState !== XMLHttpRequest.OPENED) throw new Error('InvalidStateError');
    this._requestHeaders.set(String(name).toLowerCase(), String(value));
  };
  XMLHttpRequest.prototype.getResponseHeader = function getResponseHeader(name) {
    return this._headers.get(String(name).toLowerCase()) ?? null;
  };
  XMLHttpRequest.prototype.getAllResponseHeaders = function getAllResponseHeaders() {
    return [...this._headers].map(([name, value]) => `${name}: ${value}`).join('\r\n');
  };
  XMLHttpRequest.prototype.send = function send(data = null) {
    if (this.readyState !== XMLHttpRequest.OPENED) throw new Error('InvalidStateError');
    this.requestBody = data;
    window.encode_data = data;
    this.readyState = XMLHttpRequest.DONE;
    this.dispatchEvent(new BrowserEvent('readystatechange'));
    this.dispatchEvent(new BrowserEvent('loadend'));
  };
  XMLHttpRequest.prototype.abort = function abort() {
    this.readyState = XMLHttpRequest.UNSENT;
    this.dispatchEvent(new BrowserEvent('abort'));
    this.dispatchEvent(new BrowserEvent('loadend'));
  };
  ObjectDefineProperty(XMLHttpRequest.prototype, Symbol.toStringTag, {
    __proto__: null,
    configurable: true,
    value: 'XMLHttpRequest',
  });
  return XMLHttpRequest;
}

function createMutationObserver() {
  return class MutationObserver {
    constructor(callback) {
      if (typeof callback !== 'function') throw new TypeError('MutationObserver callback must be a function');
      this.callback = callback;
      this.records = [];
    }

    observe(target, options) {
      this.target = target;
      this.options = options;
    }

    disconnect() {
      this.target = undefined;
      this.records.length = 0;
    }

    takeRecords() {
      const records = ArrayPrototypeSlice(this.records);
      this.records.length = 0;
      return records;
    }
  };
}

function createIndexedDB() {
  const databases = new Map();
  return {
    deleteDatabase(name) {
      databases.delete(String(name));
      return { error: null, readyState: 'done', result: undefined };
    },
    open(name, version = 1) {
      const databaseName = String(name);
      let database = databases.get(databaseName);
      if (database === undefined) {
        database = {
          close() {},
          name: databaseName,
          objectStoreNames: createLiveCollection({ childNodes: [] }, () => false),
          version: Number(version),
        };
        databases.set(databaseName, database);
      }
      return {
        error: null,
        readyState: 'done',
        result: database,
        transaction() { throw new Error('IndexedDB transactions are not implemented'); },
      };
    },
  };
}

function createChrome() {
  return {
    app: {
      InstallState: {
        DISABLED: 'disabled',
        INSTALLED: 'installed',
        NOT_INSTALLED: 'not_installed',
      },
      RunningState: {
        CANNOT_RUN: 'cannot_run',
        READY_TO_RUN: 'ready_to_run',
        RUNNING: 'running',
      },
      isInstalled: false,
    },
    csi() { return {}; },
    loadTimes() { return {}; },
  };
}

function markBrowserInterfaces() {
  for (const constructor of [
    BrowserEvent,
    BrowserNode,
    BrowserText,
    BrowserElement,
    HTMLHtmlElement,
    HTMLHeadElement,
    HTMLBodyElement,
    HTMLDivElement,
    HTMLIFrameElement,
    HTMLMetaElement,
    HTMLScriptElement,
    HTMLTitleElement,
    HTMLAnchorElement,
    HTMLCanvasElement,
    CanvasRenderingContext2D,
    HTMLFormElement,
    HTMLInputElement,
    BrowserDocument,
    HTMLCollection,
    DOMStringList,
    Storage,
    MimeType,
    MimeTypeArray,
  ]) {
    markAsNativeFunction(constructor);
    markNativePrototype(constructor.prototype);
  }
}

function assertCustomPropertyName(name, protectedNames, targetName) {
  if (protectedNames.has(name)) {
    throw new TypeError(`${targetName}.${name} is a protected browser environment property`);
  }
}

function validateCustomProperties(source, protectedNames, targetName) {
  if (source === undefined) return;
  assertObject(source, `${targetName}.properties`);
  for (const key of ObjectKeys(source)) {
    assertCustomPropertyName(key, protectedNames, targetName);
  }
}

function applyCustomProperties(target, source, protectedNames, targetName) {
  if (source === undefined) return;
  validateCustomProperties(source, protectedNames, targetName);
  for (const key of ObjectKeys(source)) {
    ObjectDefineProperty(target, key, {
      __proto__: null,
      configurable: true,
      enumerable: true,
      value: source[key],
      writable: true,
    });
  }
}

function validateCustomDescriptors(source, protectedNames, targetName) {
  if (source === undefined) return;
  assertObject(source, `${targetName}.descriptors`);
  for (const key of ObjectKeys(source)) {
    assertCustomPropertyName(key, protectedNames, targetName);
    const descriptor = source[key];
    assertObject(descriptor, `${targetName}.descriptors.${key}`);
  }
}

function applyCustomDescriptors(target, source, protectedNames, targetName) {
  if (source === undefined) return;
  validateCustomDescriptors(source, protectedNames, targetName);
  for (const key of ObjectKeys(source)) {
    const descriptor = source[key];
    ObjectDefineProperty(target, key, descriptor);
  }
}

function hideNodeGlobals() {
  for (const name of [
    'global', 'process', 'require', 'module', 'exports',
    '__dirname', '__filename', 'setImmediate', 'clearImmediate',
  ]) {
    const descriptor = ObjectGetOwnPropertyDescriptor(globalThis, name);
    if (descriptor?.configurable) delete globalThis[name];
  }
  const prepareStackTrace = ObjectGetOwnPropertyDescriptor(Error, 'prepareStackTrace');
  if (prepareStackTrace?.configurable) delete Error.prepareStackTrace;
}

function install(options) {
  if (installed) throw new Error('A browser environment is already installed in this Realm');
  assertObject(options, 'browser environment options');
  if (options.hideNodeGlobals !== undefined && typeof options.hideNodeGlobals !== 'boolean') {
    throw new TypeError('hideNodeGlobals must be a boolean');
  }
  validateCustomProperties(options.window?.properties, kProtectedWindowProperties, 'window');
  validateCustomDescriptors(options.window?.descriptors, kProtectedWindowProperties, 'window');
  validateCustomProperties(options.document?.properties, kProtectedDocumentProperties, 'document');
  validateCustomDescriptors(options.document?.descriptors, kProtectedDocumentProperties, 'document');
  markBrowserInterfaces();

  const location = createLocation(getHref(options));
  const document = new BrowserDocument(location);
  const html = options.html ?? options.document?.html;
  if (html !== undefined) {
    if (typeof html !== 'string') throw new TypeError('html must be a string');
    document.loadHTML(html);
  }
  if (options.cookies !== undefined) {
    if (typeof options.cookies === 'string') document.cookie = options.cookies;
    else {
      assertObject(options.cookies, 'cookies');
      for (const key of ObjectKeys(options.cookies)) document.cookie = `${key}=${options.cookies[key]}`;
    }
  }

  const navigatorValues = {
    ...kDefaultNavigator,
    ...(options.navigator ?? {}),
  };
  if (!ArrayIsArray(navigatorValues.languages)) throw new TypeError('navigator.languages must be an array');
  if (options.navigator?.language === undefined) navigatorValues.language = navigatorValues.languages[0] ?? kDefaultNavigator.language;
  if (options.navigator?.appVersion === undefined) {
    navigatorValues.appVersion = String(navigatorValues.userAgent).replace(/^Mozilla\//, '');
  }
  if (ArrayIsArray(navigatorValues.mimeTypes)) navigatorValues.mimeTypes = createMimeTypeArray(navigatorValues.mimeTypes);
  assertObject(navigatorValues.connection, 'navigator.connection');
  const { NetworkInformation, connection } = createNetworkInformation(navigatorValues.connection);
  navigatorValues.connection = connection;
  const { BatteryManager, createBattery } = createBatteryManagerFactory();
  if (navigatorValues.sendBeacon === undefined) {
    navigatorValues.sendBeacon = markAsNativeFunction(function sendBeacon() { return true; });
  }
  if (navigatorValues.getBattery === undefined) {
    navigatorValues.getBattery = markAsNativeFunction(function getBattery() {
      return Promise.resolve(createBattery());
    });
  }
  const { Navigator, navigator } = createNavigator(navigatorValues);
  const screen = {
    availHeight: 1040,
    availLeft: 0,
    availTop: 0,
    availWidth: 1920,
    colorDepth: 24,
    height: 1080,
    orientation: {
      angle: 0,
      onchange: null,
      type: 'landscape-primary',
    },
    pixelDepth: 24,
    width: 1920,
    ...(options.screen ?? {}),
  };
  const history = createHistory(location);

  const allDelegate = {
    get(key) {
      const elements = walkElements(document, () => true);
      if (key === 'length') return elements.length;
      if (typeof key === 'number') return elements[key];
      if (typeof key === 'string' && /^\d+$/.test(key)) return elements[Number(key)];
      if (typeof key === 'string') return elements.find((element) =>
        element.id === key || element.getAttribute('name') === key);
      return undefined;
    },
    call(args) {
      return this.get(args[0]);
    },
  };
  ObjectDefineProperty(document, 'all', {
    __proto__: null,
    configurable: false,
    enumerable: false,
    value: createDocumentAll(allDelegate),
    writable: false,
  });

  function Window() {
    throw new TypeError('Illegal constructor');
  }
  function History() {
    throw new TypeError('Illegal constructor');
  }
  function Screen() {
    throw new TypeError('Illegal constructor');
  }
  function Location() {
    throw new TypeError('Illegal constructor');
  }
  for (const constructor of [Window, History, Screen, Location]) {
    markAsNativeFunction(constructor);
    markNativePrototype(constructor.prototype);
  }
  ObjectSetPrototypeOf(Window.prototype, ObjectGetPrototypeOf(globalThis));
  ObjectSetPrototypeOf(globalThis, Window.prototype);
  ObjectSetPrototypeOf(history, History.prototype);
  ObjectSetPrototypeOf(screen, Screen.prototype);
  ObjectSetPrototypeOf(location, Location.prototype);
  ObjectDefineProperty(Window.prototype, Symbol.toStringTag, {
    __proto__: null,
    configurable: true,
    value: 'Window',
  });
  ObjectDefineProperty(Screen.prototype, Symbol.toStringTag, {
    __proto__: null,
    configurable: true,
    value: 'Screen',
  });
  ObjectDefineProperty(Location.prototype, Symbol.toStringTag, {
    __proto__: null,
    configurable: true,
    value: 'Location',
  });

  const DOMParser = createDOMParser(location);
  const XMLHttpRequest = createXMLHttpRequest(globalThis, location);
  const MutationObserver = createMutationObserver();

  ObjectDefineProperty(globalThis, 'window', { __proto__: null, configurable: true, enumerable: true, value: globalThis, writable: false });
  ObjectDefineProperty(globalThis, 'self', { __proto__: null, configurable: true, enumerable: true, value: globalThis, writable: false });
  ObjectDefineProperty(globalThis, 'top', { __proto__: null, configurable: true, enumerable: true, value: globalThis, writable: false });
  ObjectDefineProperty(globalThis, 'parent', { __proto__: null, configurable: true, enumerable: true, value: globalThis, writable: false });
  ObjectDefineProperty(globalThis, 'Window', { __proto__: null, configurable: true, enumerable: false, value: Window, writable: true });
  ObjectDefineProperty(globalThis, 'History', { __proto__: null, configurable: true, enumerable: false, value: History, writable: true });
  ObjectDefineProperty(globalThis, 'Screen', { __proto__: null, configurable: true, enumerable: false, value: Screen, writable: true });
  ObjectDefineProperty(globalThis, 'Location', { __proto__: null, configurable: true, enumerable: false, value: Location, writable: true });
  ObjectDefineProperty(globalThis, 'Navigator', { __proto__: null, configurable: true, enumerable: false, value: Navigator, writable: true });
  ObjectDefineProperty(globalThis, 'Event', { __proto__: null, configurable: true, enumerable: false, value: BrowserEvent, writable: true });
  ObjectDefineProperty(globalThis, 'Node', { __proto__: null, configurable: true, enumerable: false, value: BrowserNode, writable: true });
  ObjectDefineProperty(globalThis, 'Element', { __proto__: null, configurable: true, enumerable: false, value: BrowserElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLElement', { __proto__: null, configurable: true, enumerable: false, value: BrowserElement, writable: true });
  ObjectDefineProperty(globalThis, 'Document', { __proto__: null, configurable: true, enumerable: false, value: BrowserDocument, writable: true });
  ObjectDefineProperty(globalThis, 'Text', { __proto__: null, configurable: true, enumerable: false, value: BrowserText, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLAnchorElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLAnchorElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLBodyElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLBodyElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLCanvasElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLCanvasElement, writable: true });
  ObjectDefineProperty(globalThis, 'CanvasRenderingContext2D', { __proto__: null, configurable: true, enumerable: false, value: CanvasRenderingContext2D, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLCollection', { __proto__: null, configurable: true, enumerable: false, value: HTMLCollection, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLDivElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLDivElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLFormElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLFormElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLHeadElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLHeadElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLHtmlElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLHtmlElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLIFrameElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLIFrameElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLInputElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLInputElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLMetaElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLMetaElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLScriptElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLScriptElement, writable: true });
  ObjectDefineProperty(globalThis, 'HTMLTitleElement', { __proto__: null, configurable: true, enumerable: false, value: HTMLTitleElement, writable: true });
  ObjectDefineProperty(globalThis, 'MimeType', { __proto__: null, configurable: true, enumerable: false, value: MimeType, writable: true });
  ObjectDefineProperty(globalThis, 'MimeTypeArray', { __proto__: null, configurable: true, enumerable: false, value: MimeTypeArray, writable: true });
  ObjectDefineProperty(globalThis, 'BatteryManager', { __proto__: null, configurable: true, enumerable: false, value: BatteryManager, writable: true });
  ObjectDefineProperty(globalThis, 'NetworkInformation', { __proto__: null, configurable: true, enumerable: false, value: NetworkInformation, writable: true });
  ObjectDefineProperty(globalThis, 'Storage', { __proto__: null, configurable: true, enumerable: false, value: Storage, writable: true });
  ObjectDefineProperty(globalThis, 'DOMParser', { __proto__: null, configurable: true, enumerable: false, value: DOMParser, writable: true });
  ObjectDefineProperty(globalThis, 'XMLHttpRequest', { __proto__: null, configurable: true, enumerable: false, value: XMLHttpRequest, writable: true });
  ObjectDefineProperty(globalThis, 'MutationObserver', { __proto__: null, configurable: true, enumerable: false, value: MutationObserver, writable: true });
  ObjectDefineProperty(globalThis, 'document', { __proto__: null, configurable: true, enumerable: true, value: document, writable: false });
  ObjectDefineProperty(globalThis, 'navigator', { __proto__: null, configurable: true, enumerable: true, value: navigator, writable: false });
  ObjectDefineProperty(globalThis, 'clientInformation', { __proto__: null, configurable: true, enumerable: true, value: navigator, writable: false });
  ObjectDefineProperty(globalThis, 'location', {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() { return location; },
    set(value) { location.assign(value); },
  });
  ObjectDefineProperty(globalThis, 'history', { __proto__: null, configurable: true, enumerable: true, value: history, writable: false });
  ObjectDefineProperty(globalThis, 'screen', { __proto__: null, configurable: true, enumerable: true, value: screen, writable: false });
  ObjectDefineProperty(globalThis, 'indexedDB', { __proto__: null, configurable: true, enumerable: true, value: createIndexedDB(), writable: false });
  ObjectDefineProperty(globalThis, 'chrome', { __proto__: null, configurable: true, enumerable: true, value: createChrome(), writable: false });
  ObjectDefineProperty(globalThis, 'TEMPORARY', { __proto__: null, configurable: true, enumerable: true, value: 0, writable: false });
  ObjectDefineProperty(globalThis, 'name', { __proto__: null, configurable: true, enumerable: true, value: '', writable: true });
  ObjectDefineProperty(globalThis, 'innerHeight', { __proto__: null, configurable: true, enumerable: true, value: 1080, writable: true });
  ObjectDefineProperty(globalThis, 'innerWidth', { __proto__: null, configurable: true, enumerable: true, value: 1920, writable: true });
  ObjectDefineProperty(globalThis, 'outerHeight', { __proto__: null, configurable: true, enumerable: true, value: 1080, writable: true });
  ObjectDefineProperty(globalThis, 'outerWidth', { __proto__: null, configurable: true, enumerable: true, value: 1920, writable: true });
  ObjectDefineProperty(globalThis, 'screenLeft', { __proto__: null, configurable: true, enumerable: true, value: 0, writable: true });
  ObjectDefineProperty(globalThis, 'screenTop', { __proto__: null, configurable: true, enumerable: true, value: 0, writable: true });
  ObjectDefineProperty(globalThis, 'screenX', { __proto__: null, configurable: true, enumerable: true, value: 0, writable: true });
  ObjectDefineProperty(globalThis, 'screenY', { __proto__: null, configurable: true, enumerable: true, value: 0, writable: true });
  ObjectDefineProperty(globalThis, 'open', {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: function open() {
      return { close() { this.closed = true; }, closed: false };
    },
    writable: true,
  });
  ObjectDefineProperty(globalThis, 'prompt', { __proto__: null, configurable: true, enumerable: true, value: function prompt() { return null; }, writable: true });
  ObjectDefineProperty(globalThis, 'webkitRequestFileSystem', { __proto__: null, configurable: true, enumerable: true, value: function webkitRequestFileSystem() { return {}; }, writable: true });
  if (globalThis.crypto !== undefined) {
    ObjectDefineProperty(globalThis, 'msCrypto', { __proto__: null, configurable: true, enumerable: true, value: globalThis.crypto, writable: false });
  }
  ObjectDefineProperty(document, 'defaultView', { __proto__: null, configurable: false, enumerable: false, value: globalThis, writable: false });
  createEventTarget(globalThis);

  applyCustomProperties(globalThis, options.window?.properties, kProtectedWindowProperties, 'window');
  applyCustomDescriptors(globalThis, options.window?.descriptors, kProtectedWindowProperties, 'window');
  applyCustomProperties(document, options.document?.properties, kProtectedDocumentProperties, 'document');
  applyCustomDescriptors(document, options.document?.descriptors, kProtectedDocumentProperties, 'document');

  ObjectDefineProperty(globalThis, 'localStorage', { __proto__: null, configurable: true, enumerable: true, value: createStorage(options.localStorage), writable: false });
  ObjectDefineProperty(globalThis, 'sessionStorage', { __proto__: null, configurable: true, enumerable: true, value: createStorage(options.sessionStorage), writable: false });

  if (options.hideNodeGlobals) hideNodeGlobals();
  installed = true;
  installedEnvironment = { window: globalThis, document, navigator, location };
  return installedEnvironment;
}

function isInstalled() {
  return installed;
}

function installFromProfile(path) {
  if (typeof path !== 'string' || path.length === 0) return;
  const { readFileSync } = require('fs');
  let profile;
  try {
    profile = JSON.parse(readFileSync(path, 'utf8'));
  } catch (error) {
    throw new Error(`Unable to load --browser-env-profile ${path}: ${error.message}`);
  }
  return install(profile);
}

module.exports = {
  install,
  installFromProfile,
  isInstalled,
};
