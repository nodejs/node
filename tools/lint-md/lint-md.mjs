import fs from 'fs';
import path$2 from 'path';
import { pathToFileURL } from 'url';
import path$1 from 'node:path';
import process$1 from 'node:process';
import { fileURLToPath } from 'node:url';
import fs$1 from 'node:fs';
import os from 'node:os';
import tty from 'node:tty';

function bail(error) {
  if (error) {
    throw error
  }
}

var commonjsGlobal = typeof globalThis !== 'undefined' ? globalThis : typeof window !== 'undefined' ? window : typeof global !== 'undefined' ? global : typeof self !== 'undefined' ? self : {};

function getDefaultExportFromCjs (x) {
	return x && x.__esModule && Object.prototype.hasOwnProperty.call(x, 'default') ? x['default'] : x;
}

var hasOwn = Object.prototype.hasOwnProperty;
var toStr = Object.prototype.toString;
var defineProperty = Object.defineProperty;
var gOPD = Object.getOwnPropertyDescriptor;
var isArray = function isArray(arr) {
	if (typeof Array.isArray === 'function') {
		return Array.isArray(arr);
	}
	return toStr.call(arr) === '[object Array]';
};
var isPlainObject$1 = function isPlainObject(obj) {
	if (!obj || toStr.call(obj) !== '[object Object]') {
		return false;
	}
	var hasOwnConstructor = hasOwn.call(obj, 'constructor');
	var hasIsPrototypeOf = obj.constructor && obj.constructor.prototype && hasOwn.call(obj.constructor.prototype, 'isPrototypeOf');
	if (obj.constructor && !hasOwnConstructor && !hasIsPrototypeOf) {
		return false;
	}
	var key;
	for (key in obj) {  }
	return typeof key === 'undefined' || hasOwn.call(obj, key);
};
var setProperty = function setProperty(target, options) {
	if (defineProperty && options.name === '__proto__') {
		defineProperty(target, options.name, {
			enumerable: true,
			configurable: true,
			value: options.newValue,
			writable: true
		});
	} else {
		target[options.name] = options.newValue;
	}
};
var getProperty = function getProperty(obj, name) {
	if (name === '__proto__') {
		if (!hasOwn.call(obj, name)) {
			return void 0;
		} else if (gOPD) {
			return gOPD(obj, name).value;
		}
	}
	return obj[name];
};
var extend$1 = function extend() {
	var options, name, src, copy, copyIsArray, clone;
	var target = arguments[0];
	var i = 1;
	var length = arguments.length;
	var deep = false;
	if (typeof target === 'boolean') {
		deep = target;
		target = arguments[1] || {};
		i = 2;
	}
	if (target == null || (typeof target !== 'object' && typeof target !== 'function')) {
		target = {};
	}
	for (; i < length; ++i) {
		options = arguments[i];
		if (options != null) {
			for (name in options) {
				src = getProperty(target, name);
				copy = getProperty(options, name);
				if (target !== copy) {
					if (deep && copy && (isPlainObject$1(copy) || (copyIsArray = isArray(copy)))) {
						if (copyIsArray) {
							copyIsArray = false;
							clone = src && isArray(src) ? src : [];
						} else {
							clone = src && isPlainObject$1(src) ? src : {};
						}
						setProperty(target, { name: name, newValue: extend(deep, clone, copy) });
					} else if (typeof copy !== 'undefined') {
						setProperty(target, { name: name, newValue: copy });
					}
				}
			}
		}
	}
	return target;
};
var extend$2 = getDefaultExportFromCjs(extend$1);

function ok$B() {}

function isPlainObject(value) {
	if (typeof value !== 'object' || value === null) {
		return false;
	}
	const prototype = Object.getPrototypeOf(value);
	return (prototype === null || prototype === Object.prototype || Object.getPrototypeOf(prototype) === null) && !(Symbol.toStringTag in value) && !(Symbol.iterator in value);
}

function trough() {
  const fns = [];
  const pipeline = {run, use};
  return pipeline
  function run(...values) {
    let middlewareIndex = -1;
    const callback = values.pop();
    if (typeof callback !== 'function') {
      throw new TypeError('Expected function as last argument, not ' + callback)
    }
    next(null, ...values);
    function next(error, ...output) {
      const fn = fns[++middlewareIndex];
      let index = -1;
      if (error) {
        callback(error);
        return
      }
      while (++index < values.length) {
        if (output[index] === null || output[index] === undefined) {
          output[index] = values[index];
        }
      }
      values = output;
      if (fn) {
        wrap(fn, next)(...output);
      } else {
        callback(null, ...output);
      }
    }
  }
  function use(middelware) {
    if (typeof middelware !== 'function') {
      throw new TypeError(
        'Expected `middelware` to be a function, not ' + middelware
      )
    }
    fns.push(middelware);
    return pipeline
  }
}
function wrap(middleware, callback) {
  let called;
  return wrapped
  function wrapped(...parameters) {
    const fnExpectsCallback = middleware.length > parameters.length;
    let result;
    if (fnExpectsCallback) {
      parameters.push(done);
    }
    try {
      result = middleware.apply(this, parameters);
    } catch (error) {
      const exception =  (error);
      if (fnExpectsCallback && called) {
        throw exception
      }
      return done(exception)
    }
    if (!fnExpectsCallback) {
      if (result instanceof Promise) {
        result.then(then, done);
      } else if (result instanceof Error) {
        done(result);
      } else {
        then(result);
      }
    }
  }
  function done(error, ...output) {
    if (!called) {
      called = true;
      callback(error, ...output);
    }
  }
  function then(value) {
    done(null, value);
  }
}

function stringifyPosition$2(value) {
  if (!value || typeof value !== 'object') {
    return ''
  }
  if ('position' in value || 'type' in value) {
    return position$2(value.position)
  }
  if ('start' in value || 'end' in value) {
    return position$2(value)
  }
  if ('line' in value || 'column' in value) {
    return point$4(value)
  }
  return ''
}
function point$4(point) {
  return index$2(point && point.line) + ':' + index$2(point && point.column)
}
function position$2(pos) {
  return point$4(pos && pos.start) + '-' + point$4(pos && pos.end)
}
function index$2(value) {
  return value && typeof value === 'number' ? value : 1
}

let VFileMessage$1 = class VFileMessage extends Error {
  constructor(causeOrReason, optionsOrParentOrPlace, origin) {
    super();
    if (typeof optionsOrParentOrPlace === 'string') {
      origin = optionsOrParentOrPlace;
      optionsOrParentOrPlace = undefined;
    }
    let reason = '';
    let options = {};
    let legacyCause = false;
    if (optionsOrParentOrPlace) {
      if (
        'line' in optionsOrParentOrPlace &&
        'column' in optionsOrParentOrPlace
      ) {
        options = {place: optionsOrParentOrPlace};
      }
      else if (
        'start' in optionsOrParentOrPlace &&
        'end' in optionsOrParentOrPlace
      ) {
        options = {place: optionsOrParentOrPlace};
      }
      else if ('type' in optionsOrParentOrPlace) {
        options = {
          ancestors: [optionsOrParentOrPlace],
          place: optionsOrParentOrPlace.position
        };
      }
      else {
        options = {...optionsOrParentOrPlace};
      }
    }
    if (typeof causeOrReason === 'string') {
      reason = causeOrReason;
    }
    else if (!options.cause && causeOrReason) {
      legacyCause = true;
      reason = causeOrReason.message;
      options.cause = causeOrReason;
    }
    if (!options.ruleId && !options.source && typeof origin === 'string') {
      const index = origin.indexOf(':');
      if (index === -1) {
        options.ruleId = origin;
      } else {
        options.source = origin.slice(0, index);
        options.ruleId = origin.slice(index + 1);
      }
    }
    if (!options.place && options.ancestors && options.ancestors) {
      const parent = options.ancestors[options.ancestors.length - 1];
      if (parent) {
        options.place = parent.position;
      }
    }
    const start =
      options.place && 'start' in options.place
        ? options.place.start
        : options.place;
    this.ancestors = options.ancestors || undefined;
    this.cause = options.cause || undefined;
    this.column = start ? start.column : undefined;
    this.fatal = undefined;
    this.file;
    this.message = reason;
    this.line = start ? start.line : undefined;
    this.name = stringifyPosition$2(options.place) || '1:1';
    this.place = options.place || undefined;
    this.reason = this.message;
    this.ruleId = options.ruleId || undefined;
    this.source = options.source || undefined;
    this.stack =
      legacyCause && options.cause && typeof options.cause.stack === 'string'
        ? options.cause.stack
        : '';
    this.actual;
    this.expected;
    this.note;
    this.url;
  }
};
VFileMessage$1.prototype.file = '';
VFileMessage$1.prototype.name = '';
VFileMessage$1.prototype.reason = '';
VFileMessage$1.prototype.message = '';
VFileMessage$1.prototype.stack = '';
VFileMessage$1.prototype.column = undefined;
VFileMessage$1.prototype.line = undefined;
VFileMessage$1.prototype.ancestors = undefined;
VFileMessage$1.prototype.cause = undefined;
VFileMessage$1.prototype.fatal = undefined;
VFileMessage$1.prototype.place = undefined;
VFileMessage$1.prototype.ruleId = undefined;
VFileMessage$1.prototype.source = undefined;

function isUrl$1(fileUrlOrPath) {
  return Boolean(
    fileUrlOrPath !== null &&
      typeof fileUrlOrPath === 'object' &&
      'href' in fileUrlOrPath &&
      fileUrlOrPath.href &&
      'protocol' in fileUrlOrPath &&
      fileUrlOrPath.protocol &&
      fileUrlOrPath.auth === undefined
  )
}

const order$1 =  ([
  'history',
  'path',
  'basename',
  'stem',
  'extname',
  'dirname'
]);
let VFile$1 = class VFile {
  constructor(value) {
    let options;
    if (!value) {
      options = {};
    } else if (isUrl$1(value)) {
      options = {path: value};
    } else if (typeof value === 'string' || isUint8Array$3(value)) {
      options = {value};
    } else {
      options = value;
    }
    this.cwd = process$1.cwd();
    this.data = {};
    this.history = [];
    this.messages = [];
    this.value;
    this.map;
    this.result;
    this.stored;
    let index = -1;
    while (++index < order$1.length) {
      const prop = order$1[index];
      if (
        prop in options &&
        options[prop] !== undefined &&
        options[prop] !== null
      ) {
        this[prop] = prop === 'history' ? [...options[prop]] : options[prop];
      }
    }
    let prop;
    for (prop in options) {
      if (!order$1.includes(prop)) {
        this[prop] = options[prop];
      }
    }
  }
  get basename() {
    return typeof this.path === 'string' ? path$1.basename(this.path) : undefined
  }
  set basename(basename) {
    assertNonEmpty$1(basename, 'basename');
    assertPart$1(basename, 'basename');
    this.path = path$1.join(this.dirname || '', basename);
  }
  get dirname() {
    return typeof this.path === 'string' ? path$1.dirname(this.path) : undefined
  }
  set dirname(dirname) {
    assertPath$1(this.basename, 'dirname');
    this.path = path$1.join(dirname || '', this.basename);
  }
  get extname() {
    return typeof this.path === 'string' ? path$1.extname(this.path) : undefined
  }
  set extname(extname) {
    assertPart$1(extname, 'extname');
    assertPath$1(this.dirname, 'extname');
    if (extname) {
      if (extname.codePointAt(0) !== 46 ) {
        throw new Error('`extname` must start with `.`')
      }
      if (extname.includes('.', 1)) {
        throw new Error('`extname` cannot contain multiple dots')
      }
    }
    this.path = path$1.join(this.dirname, this.stem + (extname || ''));
  }
  get path() {
    return this.history[this.history.length - 1]
  }
  set path(path) {
    if (isUrl$1(path)) {
      path = fileURLToPath(path);
    }
    assertNonEmpty$1(path, 'path');
    if (this.path !== path) {
      this.history.push(path);
    }
  }
  get stem() {
    return typeof this.path === 'string'
      ? path$1.basename(this.path, this.extname)
      : undefined
  }
  set stem(stem) {
    assertNonEmpty$1(stem, 'stem');
    assertPart$1(stem, 'stem');
    this.path = path$1.join(this.dirname || '', stem + (this.extname || ''));
  }
  fail(causeOrReason, optionsOrParentOrPlace, origin) {
    const message = this.message(causeOrReason, optionsOrParentOrPlace, origin);
    message.fatal = true;
    throw message
  }
  info(causeOrReason, optionsOrParentOrPlace, origin) {
    const message = this.message(causeOrReason, optionsOrParentOrPlace, origin);
    message.fatal = undefined;
    return message
  }
  message(causeOrReason, optionsOrParentOrPlace, origin) {
    const message = new VFileMessage$1(
      causeOrReason,
      optionsOrParentOrPlace,
      origin
    );
    if (this.path) {
      message.name = this.path + ':' + message.name;
      message.file = this.path;
    }
    message.fatal = false;
    this.messages.push(message);
    return message
  }
  toString(encoding) {
    if (this.value === undefined) {
      return ''
    }
    if (typeof this.value === 'string') {
      return this.value
    }
    const decoder = new TextDecoder(encoding || undefined);
    return decoder.decode(this.value)
  }
};
function assertPart$1(part, name) {
  if (part && part.includes(path$1.sep)) {
    throw new Error(
      '`' + name + '` cannot be a path: did not expect `' + path$1.sep + '`'
    )
  }
}
function assertNonEmpty$1(part, name) {
  if (!part) {
    throw new Error('`' + name + '` cannot be empty')
  }
}
function assertPath$1(path, name) {
  if (!path) {
    throw new Error('Setting `' + name + '` requires `path` to be set too')
  }
}
function isUint8Array$3(value) {
  return Boolean(
    value &&
      typeof value === 'object' &&
      'byteLength' in value &&
      'byteOffset' in value
  )
}

const CallableInstance =
  (
    (
      function (property) {
        const self = this;
        const constr = self.constructor;
        const proto =  (
          constr.prototype
        );
        const func = proto[property];
        const apply = function () {
          return func.apply(apply, arguments)
        };
        Object.setPrototypeOf(apply, proto);
        const names = Object.getOwnPropertyNames(func);
        for (const p of names) {
          const descriptor = Object.getOwnPropertyDescriptor(func, p);
          if (descriptor) Object.defineProperty(apply, p, descriptor);
        }
        return apply
      }
    )
  );

const own$6 = {}.hasOwnProperty;
class Processor extends CallableInstance {
  constructor() {
    super('copy');
    this.Compiler = undefined;
    this.Parser = undefined;
    this.attachers = [];
    this.compiler = undefined;
    this.freezeIndex = -1;
    this.frozen = undefined;
    this.namespace = {};
    this.parser = undefined;
    this.transformers = trough();
  }
  copy() {
    const destination =
       (
        new Processor()
      );
    let index = -1;
    while (++index < this.attachers.length) {
      const attacher = this.attachers[index];
      destination.use(...attacher);
    }
    destination.data(extend$2(true, {}, this.namespace));
    return destination
  }
  data(key, value) {
    if (typeof key === 'string') {
      if (arguments.length === 2) {
        assertUnfrozen('data', this.frozen);
        this.namespace[key] = value;
        return this
      }
      return (own$6.call(this.namespace, key) && this.namespace[key]) || undefined
    }
    if (key) {
      assertUnfrozen('data', this.frozen);
      this.namespace = key;
      return this
    }
    return this.namespace
  }
  freeze() {
    if (this.frozen) {
      return this
    }
    const self =  ( (this));
    while (++this.freezeIndex < this.attachers.length) {
      const [attacher, ...options] = this.attachers[this.freezeIndex];
      if (options[0] === false) {
        continue
      }
      if (options[0] === true) {
        options[0] = undefined;
      }
      const transformer = attacher.call(self, ...options);
      if (typeof transformer === 'function') {
        this.transformers.use(transformer);
      }
    }
    this.frozen = true;
    this.freezeIndex = Number.POSITIVE_INFINITY;
    return this
  }
  parse(file) {
    this.freeze();
    const realFile = vfile(file);
    const parser = this.parser || this.Parser;
    assertParser('parse', parser);
    return parser(String(realFile), realFile)
  }
  process(file, done) {
    const self = this;
    this.freeze();
    assertParser('process', this.parser || this.Parser);
    assertCompiler('process', this.compiler || this.Compiler);
    return done ? executor(undefined, done) : new Promise(executor)
    function executor(resolve, reject) {
      const realFile = vfile(file);
      const parseTree =
         (
           (self.parse(realFile))
        );
      self.run(parseTree, realFile, function (error, tree, file) {
        if (error || !tree || !file) {
          return realDone(error)
        }
        const compileTree =
           (
             (tree)
          );
        const compileResult = self.stringify(compileTree, file);
        if (looksLikeAValue(compileResult)) {
          file.value = compileResult;
        } else {
          file.result = compileResult;
        }
        realDone(error,  (file));
      });
      function realDone(error, file) {
        if (error || !file) {
          reject(error);
        } else if (resolve) {
          resolve(file);
        } else {
          done(undefined, file);
        }
      }
    }
  }
  processSync(file) {
    let complete = false;
    let result;
    this.freeze();
    assertParser('processSync', this.parser || this.Parser);
    assertCompiler('processSync', this.compiler || this.Compiler);
    this.process(file, realDone);
    assertDone('processSync', 'process', complete);
    return result
    function realDone(error, file) {
      complete = true;
      bail(error);
      result = file;
    }
  }
  run(tree, file, done) {
    assertNode(tree);
    this.freeze();
    const transformers = this.transformers;
    if (!done && typeof file === 'function') {
      done = file;
      file = undefined;
    }
    return done ? executor(undefined, done) : new Promise(executor)
    function executor(resolve, reject) {
      const realFile = vfile(file);
      transformers.run(tree, realFile, realDone);
      function realDone(error, outputTree, file) {
        const resultingTree =
           (
            outputTree || tree
          );
        if (error) {
          reject(error);
        } else if (resolve) {
          resolve(resultingTree);
        } else {
          done(undefined, resultingTree, file);
        }
      }
    }
  }
  runSync(tree, file) {
    let complete = false;
    let result;
    this.run(tree, file, realDone);
    assertDone('runSync', 'run', complete);
    return result
    function realDone(error, tree) {
      bail(error);
      result = tree;
      complete = true;
    }
  }
  stringify(tree, file) {
    this.freeze();
    const realFile = vfile(file);
    const compiler = this.compiler || this.Compiler;
    assertCompiler('stringify', compiler);
    assertNode(tree);
    return compiler(tree, realFile)
  }
  use(value, ...parameters) {
    const attachers = this.attachers;
    const namespace = this.namespace;
    assertUnfrozen('use', this.frozen);
    if (value === null || value === undefined) ; else if (typeof value === 'function') {
      addPlugin(value, parameters);
    } else if (typeof value === 'object') {
      if (Array.isArray(value)) {
        addList(value);
      } else {
        addPreset(value);
      }
    } else {
      throw new TypeError('Expected usable value, not `' + value + '`')
    }
    return this
    function add(value) {
      if (typeof value === 'function') {
        addPlugin(value, []);
      } else if (typeof value === 'object') {
        if (Array.isArray(value)) {
          const [plugin, ...parameters] =
             (value);
          addPlugin(plugin, parameters);
        } else {
          addPreset(value);
        }
      } else {
        throw new TypeError('Expected usable value, not `' + value + '`')
      }
    }
    function addPreset(result) {
      if (!('plugins' in result) && !('settings' in result)) {
        throw new Error(
          'Expected usable value but received an empty preset, which is probably a mistake: presets typically come with `plugins` and sometimes with `settings`, but this has neither'
        )
      }
      addList(result.plugins);
      if (result.settings) {
        namespace.settings = extend$2(true, namespace.settings, result.settings);
      }
    }
    function addList(plugins) {
      let index = -1;
      if (plugins === null || plugins === undefined) ; else if (Array.isArray(plugins)) {
        while (++index < plugins.length) {
          const thing = plugins[index];
          add(thing);
        }
      } else {
        throw new TypeError('Expected a list of plugins, not `' + plugins + '`')
      }
    }
    function addPlugin(plugin, parameters) {
      let index = -1;
      let entryIndex = -1;
      while (++index < attachers.length) {
        if (attachers[index][0] === plugin) {
          entryIndex = index;
          break
        }
      }
      if (entryIndex === -1) {
        attachers.push([plugin, ...parameters]);
      }
      else if (parameters.length > 0) {
        let [primary, ...rest] = parameters;
        const currentPrimary = attachers[entryIndex][1];
        if (isPlainObject(currentPrimary) && isPlainObject(primary)) {
          primary = extend$2(true, currentPrimary, primary);
        }
        attachers[entryIndex] = [plugin, primary, ...rest];
      }
    }
  }
}
const unified = new Processor().freeze();
function assertParser(name, value) {
  if (typeof value !== 'function') {
    throw new TypeError('Cannot `' + name + '` without `parser`')
  }
}
function assertCompiler(name, value) {
  if (typeof value !== 'function') {
    throw new TypeError('Cannot `' + name + '` without `compiler`')
  }
}
function assertUnfrozen(name, frozen) {
  if (frozen) {
    throw new Error(
      'Cannot call `' +
        name +
        '` on a frozen processor.\nCreate a new processor first, by calling it: use `processor()` instead of `processor`.'
    )
  }
}
function assertNode(node) {
  if (!isPlainObject(node) || typeof node.type !== 'string') {
    throw new TypeError('Expected node, got `' + node + '`')
  }
}
function assertDone(name, asyncName, complete) {
  if (!complete) {
    throw new Error(
      '`' + name + '` finished async. Use `' + asyncName + '` instead'
    )
  }
}
function vfile(value) {
  return looksLikeAVFile$1(value) ? value : new VFile$1(value)
}
function looksLikeAVFile$1(value) {
  return Boolean(
    value &&
      typeof value === 'object' &&
      'message' in value &&
      'messages' in value
  )
}
function looksLikeAValue(value) {
  return typeof value === 'string' || isUint8Array$2(value)
}
function isUint8Array$2(value) {
  return Boolean(
    value &&
      typeof value === 'object' &&
      'byteLength' in value &&
      'byteOffset' in value
  )
}

const emptyOptions$3 = {};
function toString$2(value, options) {
  const settings = options || emptyOptions$3;
  const includeImageAlt =
    typeof settings.includeImageAlt === 'boolean'
      ? settings.includeImageAlt
      : true;
  const includeHtml =
    typeof settings.includeHtml === 'boolean' ? settings.includeHtml : true;
  return one$2(value, includeImageAlt, includeHtml)
}
function one$2(value, includeImageAlt, includeHtml) {
  if (node$2(value)) {
    if ('value' in value) {
      return value.type === 'html' && !includeHtml ? '' : value.value
    }
    if (includeImageAlt && 'alt' in value && value.alt) {
      return value.alt
    }
    if ('children' in value) {
      return all$2(value.children, includeImageAlt, includeHtml)
    }
  }
  if (Array.isArray(value)) {
    return all$2(value, includeImageAlt, includeHtml)
  }
  return ''
}
function all$2(values, includeImageAlt, includeHtml) {
  const result = [];
  let index = -1;
  while (++index < values.length) {
    result[index] = one$2(values[index], includeImageAlt, includeHtml);
  }
  return result.join('')
}
function node$2(value) {
  return Boolean(value && typeof value === 'object')
}

const characterEntities = {
  AElig: 'Æ',
  AMP: '&',
  Aacute: 'Á',
  Abreve: 'Ă',
  Acirc: 'Â',
  Acy: 'А',
  Afr: '𝔄',
  Agrave: 'À',
  Alpha: 'Α',
  Amacr: 'Ā',
  And: '⩓',
  Aogon: 'Ą',
  Aopf: '𝔸',
  ApplyFunction: '⁡',
  Aring: 'Å',
  Ascr: '𝒜',
  Assign: '≔',
  Atilde: 'Ã',
  Auml: 'Ä',
  Backslash: '∖',
  Barv: '⫧',
  Barwed: '⌆',
  Bcy: 'Б',
  Because: '∵',
  Bernoullis: 'ℬ',
  Beta: 'Β',
  Bfr: '𝔅',
  Bopf: '𝔹',
  Breve: '˘',
  Bscr: 'ℬ',
  Bumpeq: '≎',
  CHcy: 'Ч',
  COPY: '©',
  Cacute: 'Ć',
  Cap: '⋒',
  CapitalDifferentialD: 'ⅅ',
  Cayleys: 'ℭ',
  Ccaron: 'Č',
  Ccedil: 'Ç',
  Ccirc: 'Ĉ',
  Cconint: '∰',
  Cdot: 'Ċ',
  Cedilla: '¸',
  CenterDot: '·',
  Cfr: 'ℭ',
  Chi: 'Χ',
  CircleDot: '⊙',
  CircleMinus: '⊖',
  CirclePlus: '⊕',
  CircleTimes: '⊗',
  ClockwiseContourIntegral: '∲',
  CloseCurlyDoubleQuote: '”',
  CloseCurlyQuote: '’',
  Colon: '∷',
  Colone: '⩴',
  Congruent: '≡',
  Conint: '∯',
  ContourIntegral: '∮',
  Copf: 'ℂ',
  Coproduct: '∐',
  CounterClockwiseContourIntegral: '∳',
  Cross: '⨯',
  Cscr: '𝒞',
  Cup: '⋓',
  CupCap: '≍',
  DD: 'ⅅ',
  DDotrahd: '⤑',
  DJcy: 'Ђ',
  DScy: 'Ѕ',
  DZcy: 'Џ',
  Dagger: '‡',
  Darr: '↡',
  Dashv: '⫤',
  Dcaron: 'Ď',
  Dcy: 'Д',
  Del: '∇',
  Delta: 'Δ',
  Dfr: '𝔇',
  DiacriticalAcute: '´',
  DiacriticalDot: '˙',
  DiacriticalDoubleAcute: '˝',
  DiacriticalGrave: '`',
  DiacriticalTilde: '˜',
  Diamond: '⋄',
  DifferentialD: 'ⅆ',
  Dopf: '𝔻',
  Dot: '¨',
  DotDot: '⃜',
  DotEqual: '≐',
  DoubleContourIntegral: '∯',
  DoubleDot: '¨',
  DoubleDownArrow: '⇓',
  DoubleLeftArrow: '⇐',
  DoubleLeftRightArrow: '⇔',
  DoubleLeftTee: '⫤',
  DoubleLongLeftArrow: '⟸',
  DoubleLongLeftRightArrow: '⟺',
  DoubleLongRightArrow: '⟹',
  DoubleRightArrow: '⇒',
  DoubleRightTee: '⊨',
  DoubleUpArrow: '⇑',
  DoubleUpDownArrow: '⇕',
  DoubleVerticalBar: '∥',
  DownArrow: '↓',
  DownArrowBar: '⤓',
  DownArrowUpArrow: '⇵',
  DownBreve: '̑',
  DownLeftRightVector: '⥐',
  DownLeftTeeVector: '⥞',
  DownLeftVector: '↽',
  DownLeftVectorBar: '⥖',
  DownRightTeeVector: '⥟',
  DownRightVector: '⇁',
  DownRightVectorBar: '⥗',
  DownTee: '⊤',
  DownTeeArrow: '↧',
  Downarrow: '⇓',
  Dscr: '𝒟',
  Dstrok: 'Đ',
  ENG: 'Ŋ',
  ETH: 'Ð',
  Eacute: 'É',
  Ecaron: 'Ě',
  Ecirc: 'Ê',
  Ecy: 'Э',
  Edot: 'Ė',
  Efr: '𝔈',
  Egrave: 'È',
  Element: '∈',
  Emacr: 'Ē',
  EmptySmallSquare: '◻',
  EmptyVerySmallSquare: '▫',
  Eogon: 'Ę',
  Eopf: '𝔼',
  Epsilon: 'Ε',
  Equal: '⩵',
  EqualTilde: '≂',
  Equilibrium: '⇌',
  Escr: 'ℰ',
  Esim: '⩳',
  Eta: 'Η',
  Euml: 'Ë',
  Exists: '∃',
  ExponentialE: 'ⅇ',
  Fcy: 'Ф',
  Ffr: '𝔉',
  FilledSmallSquare: '◼',
  FilledVerySmallSquare: '▪',
  Fopf: '𝔽',
  ForAll: '∀',
  Fouriertrf: 'ℱ',
  Fscr: 'ℱ',
  GJcy: 'Ѓ',
  GT: '>',
  Gamma: 'Γ',
  Gammad: 'Ϝ',
  Gbreve: 'Ğ',
  Gcedil: 'Ģ',
  Gcirc: 'Ĝ',
  Gcy: 'Г',
  Gdot: 'Ġ',
  Gfr: '𝔊',
  Gg: '⋙',
  Gopf: '𝔾',
  GreaterEqual: '≥',
  GreaterEqualLess: '⋛',
  GreaterFullEqual: '≧',
  GreaterGreater: '⪢',
  GreaterLess: '≷',
  GreaterSlantEqual: '⩾',
  GreaterTilde: '≳',
  Gscr: '𝒢',
  Gt: '≫',
  HARDcy: 'Ъ',
  Hacek: 'ˇ',
  Hat: '^',
  Hcirc: 'Ĥ',
  Hfr: 'ℌ',
  HilbertSpace: 'ℋ',
  Hopf: 'ℍ',
  HorizontalLine: '─',
  Hscr: 'ℋ',
  Hstrok: 'Ħ',
  HumpDownHump: '≎',
  HumpEqual: '≏',
  IEcy: 'Е',
  IJlig: 'Ĳ',
  IOcy: 'Ё',
  Iacute: 'Í',
  Icirc: 'Î',
  Icy: 'И',
  Idot: 'İ',
  Ifr: 'ℑ',
  Igrave: 'Ì',
  Im: 'ℑ',
  Imacr: 'Ī',
  ImaginaryI: 'ⅈ',
  Implies: '⇒',
  Int: '∬',
  Integral: '∫',
  Intersection: '⋂',
  InvisibleComma: '⁣',
  InvisibleTimes: '⁢',
  Iogon: 'Į',
  Iopf: '𝕀',
  Iota: 'Ι',
  Iscr: 'ℐ',
  Itilde: 'Ĩ',
  Iukcy: 'І',
  Iuml: 'Ï',
  Jcirc: 'Ĵ',
  Jcy: 'Й',
  Jfr: '𝔍',
  Jopf: '𝕁',
  Jscr: '𝒥',
  Jsercy: 'Ј',
  Jukcy: 'Є',
  KHcy: 'Х',
  KJcy: 'Ќ',
  Kappa: 'Κ',
  Kcedil: 'Ķ',
  Kcy: 'К',
  Kfr: '𝔎',
  Kopf: '𝕂',
  Kscr: '𝒦',
  LJcy: 'Љ',
  LT: '<',
  Lacute: 'Ĺ',
  Lambda: 'Λ',
  Lang: '⟪',
  Laplacetrf: 'ℒ',
  Larr: '↞',
  Lcaron: 'Ľ',
  Lcedil: 'Ļ',
  Lcy: 'Л',
  LeftAngleBracket: '⟨',
  LeftArrow: '←',
  LeftArrowBar: '⇤',
  LeftArrowRightArrow: '⇆',
  LeftCeiling: '⌈',
  LeftDoubleBracket: '⟦',
  LeftDownTeeVector: '⥡',
  LeftDownVector: '⇃',
  LeftDownVectorBar: '⥙',
  LeftFloor: '⌊',
  LeftRightArrow: '↔',
  LeftRightVector: '⥎',
  LeftTee: '⊣',
  LeftTeeArrow: '↤',
  LeftTeeVector: '⥚',
  LeftTriangle: '⊲',
  LeftTriangleBar: '⧏',
  LeftTriangleEqual: '⊴',
  LeftUpDownVector: '⥑',
  LeftUpTeeVector: '⥠',
  LeftUpVector: '↿',
  LeftUpVectorBar: '⥘',
  LeftVector: '↼',
  LeftVectorBar: '⥒',
  Leftarrow: '⇐',
  Leftrightarrow: '⇔',
  LessEqualGreater: '⋚',
  LessFullEqual: '≦',
  LessGreater: '≶',
  LessLess: '⪡',
  LessSlantEqual: '⩽',
  LessTilde: '≲',
  Lfr: '𝔏',
  Ll: '⋘',
  Lleftarrow: '⇚',
  Lmidot: 'Ŀ',
  LongLeftArrow: '⟵',
  LongLeftRightArrow: '⟷',
  LongRightArrow: '⟶',
  Longleftarrow: '⟸',
  Longleftrightarrow: '⟺',
  Longrightarrow: '⟹',
  Lopf: '𝕃',
  LowerLeftArrow: '↙',
  LowerRightArrow: '↘',
  Lscr: 'ℒ',
  Lsh: '↰',
  Lstrok: 'Ł',
  Lt: '≪',
  Map: '⤅',
  Mcy: 'М',
  MediumSpace: ' ',
  Mellintrf: 'ℳ',
  Mfr: '𝔐',
  MinusPlus: '∓',
  Mopf: '𝕄',
  Mscr: 'ℳ',
  Mu: 'Μ',
  NJcy: 'Њ',
  Nacute: 'Ń',
  Ncaron: 'Ň',
  Ncedil: 'Ņ',
  Ncy: 'Н',
  NegativeMediumSpace: '​',
  NegativeThickSpace: '​',
  NegativeThinSpace: '​',
  NegativeVeryThinSpace: '​',
  NestedGreaterGreater: '≫',
  NestedLessLess: '≪',
  NewLine: '\n',
  Nfr: '𝔑',
  NoBreak: '⁠',
  NonBreakingSpace: ' ',
  Nopf: 'ℕ',
  Not: '⫬',
  NotCongruent: '≢',
  NotCupCap: '≭',
  NotDoubleVerticalBar: '∦',
  NotElement: '∉',
  NotEqual: '≠',
  NotEqualTilde: '≂̸',
  NotExists: '∄',
  NotGreater: '≯',
  NotGreaterEqual: '≱',
  NotGreaterFullEqual: '≧̸',
  NotGreaterGreater: '≫̸',
  NotGreaterLess: '≹',
  NotGreaterSlantEqual: '⩾̸',
  NotGreaterTilde: '≵',
  NotHumpDownHump: '≎̸',
  NotHumpEqual: '≏̸',
  NotLeftTriangle: '⋪',
  NotLeftTriangleBar: '⧏̸',
  NotLeftTriangleEqual: '⋬',
  NotLess: '≮',
  NotLessEqual: '≰',
  NotLessGreater: '≸',
  NotLessLess: '≪̸',
  NotLessSlantEqual: '⩽̸',
  NotLessTilde: '≴',
  NotNestedGreaterGreater: '⪢̸',
  NotNestedLessLess: '⪡̸',
  NotPrecedes: '⊀',
  NotPrecedesEqual: '⪯̸',
  NotPrecedesSlantEqual: '⋠',
  NotReverseElement: '∌',
  NotRightTriangle: '⋫',
  NotRightTriangleBar: '⧐̸',
  NotRightTriangleEqual: '⋭',
  NotSquareSubset: '⊏̸',
  NotSquareSubsetEqual: '⋢',
  NotSquareSuperset: '⊐̸',
  NotSquareSupersetEqual: '⋣',
  NotSubset: '⊂⃒',
  NotSubsetEqual: '⊈',
  NotSucceeds: '⊁',
  NotSucceedsEqual: '⪰̸',
  NotSucceedsSlantEqual: '⋡',
  NotSucceedsTilde: '≿̸',
  NotSuperset: '⊃⃒',
  NotSupersetEqual: '⊉',
  NotTilde: '≁',
  NotTildeEqual: '≄',
  NotTildeFullEqual: '≇',
  NotTildeTilde: '≉',
  NotVerticalBar: '∤',
  Nscr: '𝒩',
  Ntilde: 'Ñ',
  Nu: 'Ν',
  OElig: 'Œ',
  Oacute: 'Ó',
  Ocirc: 'Ô',
  Ocy: 'О',
  Odblac: 'Ő',
  Ofr: '𝔒',
  Ograve: 'Ò',
  Omacr: 'Ō',
  Omega: 'Ω',
  Omicron: 'Ο',
  Oopf: '𝕆',
  OpenCurlyDoubleQuote: '“',
  OpenCurlyQuote: '‘',
  Or: '⩔',
  Oscr: '𝒪',
  Oslash: 'Ø',
  Otilde: 'Õ',
  Otimes: '⨷',
  Ouml: 'Ö',
  OverBar: '‾',
  OverBrace: '⏞',
  OverBracket: '⎴',
  OverParenthesis: '⏜',
  PartialD: '∂',
  Pcy: 'П',
  Pfr: '𝔓',
  Phi: 'Φ',
  Pi: 'Π',
  PlusMinus: '±',
  Poincareplane: 'ℌ',
  Popf: 'ℙ',
  Pr: '⪻',
  Precedes: '≺',
  PrecedesEqual: '⪯',
  PrecedesSlantEqual: '≼',
  PrecedesTilde: '≾',
  Prime: '″',
  Product: '∏',
  Proportion: '∷',
  Proportional: '∝',
  Pscr: '𝒫',
  Psi: 'Ψ',
  QUOT: '"',
  Qfr: '𝔔',
  Qopf: 'ℚ',
  Qscr: '𝒬',
  RBarr: '⤐',
  REG: '®',
  Racute: 'Ŕ',
  Rang: '⟫',
  Rarr: '↠',
  Rarrtl: '⤖',
  Rcaron: 'Ř',
  Rcedil: 'Ŗ',
  Rcy: 'Р',
  Re: 'ℜ',
  ReverseElement: '∋',
  ReverseEquilibrium: '⇋',
  ReverseUpEquilibrium: '⥯',
  Rfr: 'ℜ',
  Rho: 'Ρ',
  RightAngleBracket: '⟩',
  RightArrow: '→',
  RightArrowBar: '⇥',
  RightArrowLeftArrow: '⇄',
  RightCeiling: '⌉',
  RightDoubleBracket: '⟧',
  RightDownTeeVector: '⥝',
  RightDownVector: '⇂',
  RightDownVectorBar: '⥕',
  RightFloor: '⌋',
  RightTee: '⊢',
  RightTeeArrow: '↦',
  RightTeeVector: '⥛',
  RightTriangle: '⊳',
  RightTriangleBar: '⧐',
  RightTriangleEqual: '⊵',
  RightUpDownVector: '⥏',
  RightUpTeeVector: '⥜',
  RightUpVector: '↾',
  RightUpVectorBar: '⥔',
  RightVector: '⇀',
  RightVectorBar: '⥓',
  Rightarrow: '⇒',
  Ropf: 'ℝ',
  RoundImplies: '⥰',
  Rrightarrow: '⇛',
  Rscr: 'ℛ',
  Rsh: '↱',
  RuleDelayed: '⧴',
  SHCHcy: 'Щ',
  SHcy: 'Ш',
  SOFTcy: 'Ь',
  Sacute: 'Ś',
  Sc: '⪼',
  Scaron: 'Š',
  Scedil: 'Ş',
  Scirc: 'Ŝ',
  Scy: 'С',
  Sfr: '𝔖',
  ShortDownArrow: '↓',
  ShortLeftArrow: '←',
  ShortRightArrow: '→',
  ShortUpArrow: '↑',
  Sigma: 'Σ',
  SmallCircle: '∘',
  Sopf: '𝕊',
  Sqrt: '√',
  Square: '□',
  SquareIntersection: '⊓',
  SquareSubset: '⊏',
  SquareSubsetEqual: '⊑',
  SquareSuperset: '⊐',
  SquareSupersetEqual: '⊒',
  SquareUnion: '⊔',
  Sscr: '𝒮',
  Star: '⋆',
  Sub: '⋐',
  Subset: '⋐',
  SubsetEqual: '⊆',
  Succeeds: '≻',
  SucceedsEqual: '⪰',
  SucceedsSlantEqual: '≽',
  SucceedsTilde: '≿',
  SuchThat: '∋',
  Sum: '∑',
  Sup: '⋑',
  Superset: '⊃',
  SupersetEqual: '⊇',
  Supset: '⋑',
  THORN: 'Þ',
  TRADE: '™',
  TSHcy: 'Ћ',
  TScy: 'Ц',
  Tab: '\t',
  Tau: 'Τ',
  Tcaron: 'Ť',
  Tcedil: 'Ţ',
  Tcy: 'Т',
  Tfr: '𝔗',
  Therefore: '∴',
  Theta: 'Θ',
  ThickSpace: '  ',
  ThinSpace: ' ',
  Tilde: '∼',
  TildeEqual: '≃',
  TildeFullEqual: '≅',
  TildeTilde: '≈',
  Topf: '𝕋',
  TripleDot: '⃛',
  Tscr: '𝒯',
  Tstrok: 'Ŧ',
  Uacute: 'Ú',
  Uarr: '↟',
  Uarrocir: '⥉',
  Ubrcy: 'Ў',
  Ubreve: 'Ŭ',
  Ucirc: 'Û',
  Ucy: 'У',
  Udblac: 'Ű',
  Ufr: '𝔘',
  Ugrave: 'Ù',
  Umacr: 'Ū',
  UnderBar: '_',
  UnderBrace: '⏟',
  UnderBracket: '⎵',
  UnderParenthesis: '⏝',
  Union: '⋃',
  UnionPlus: '⊎',
  Uogon: 'Ų',
  Uopf: '𝕌',
  UpArrow: '↑',
  UpArrowBar: '⤒',
  UpArrowDownArrow: '⇅',
  UpDownArrow: '↕',
  UpEquilibrium: '⥮',
  UpTee: '⊥',
  UpTeeArrow: '↥',
  Uparrow: '⇑',
  Updownarrow: '⇕',
  UpperLeftArrow: '↖',
  UpperRightArrow: '↗',
  Upsi: 'ϒ',
  Upsilon: 'Υ',
  Uring: 'Ů',
  Uscr: '𝒰',
  Utilde: 'Ũ',
  Uuml: 'Ü',
  VDash: '⊫',
  Vbar: '⫫',
  Vcy: 'В',
  Vdash: '⊩',
  Vdashl: '⫦',
  Vee: '⋁',
  Verbar: '‖',
  Vert: '‖',
  VerticalBar: '∣',
  VerticalLine: '|',
  VerticalSeparator: '❘',
  VerticalTilde: '≀',
  VeryThinSpace: ' ',
  Vfr: '𝔙',
  Vopf: '𝕍',
  Vscr: '𝒱',
  Vvdash: '⊪',
  Wcirc: 'Ŵ',
  Wedge: '⋀',
  Wfr: '𝔚',
  Wopf: '𝕎',
  Wscr: '𝒲',
  Xfr: '𝔛',
  Xi: 'Ξ',
  Xopf: '𝕏',
  Xscr: '𝒳',
  YAcy: 'Я',
  YIcy: 'Ї',
  YUcy: 'Ю',
  Yacute: 'Ý',
  Ycirc: 'Ŷ',
  Ycy: 'Ы',
  Yfr: '𝔜',
  Yopf: '𝕐',
  Yscr: '𝒴',
  Yuml: 'Ÿ',
  ZHcy: 'Ж',
  Zacute: 'Ź',
  Zcaron: 'Ž',
  Zcy: 'З',
  Zdot: 'Ż',
  ZeroWidthSpace: '​',
  Zeta: 'Ζ',
  Zfr: 'ℨ',
  Zopf: 'ℤ',
  Zscr: '𝒵',
  aacute: 'á',
  abreve: 'ă',
  ac: '∾',
  acE: '∾̳',
  acd: '∿',
  acirc: 'â',
  acute: '´',
  acy: 'а',
  aelig: 'æ',
  af: '⁡',
  afr: '𝔞',
  agrave: 'à',
  alefsym: 'ℵ',
  aleph: 'ℵ',
  alpha: 'α',
  amacr: 'ā',
  amalg: '⨿',
  amp: '&',
  and: '∧',
  andand: '⩕',
  andd: '⩜',
  andslope: '⩘',
  andv: '⩚',
  ang: '∠',
  ange: '⦤',
  angle: '∠',
  angmsd: '∡',
  angmsdaa: '⦨',
  angmsdab: '⦩',
  angmsdac: '⦪',
  angmsdad: '⦫',
  angmsdae: '⦬',
  angmsdaf: '⦭',
  angmsdag: '⦮',
  angmsdah: '⦯',
  angrt: '∟',
  angrtvb: '⊾',
  angrtvbd: '⦝',
  angsph: '∢',
  angst: 'Å',
  angzarr: '⍼',
  aogon: 'ą',
  aopf: '𝕒',
  ap: '≈',
  apE: '⩰',
  apacir: '⩯',
  ape: '≊',
  apid: '≋',
  apos: "'",
  approx: '≈',
  approxeq: '≊',
  aring: 'å',
  ascr: '𝒶',
  ast: '*',
  asymp: '≈',
  asympeq: '≍',
  atilde: 'ã',
  auml: 'ä',
  awconint: '∳',
  awint: '⨑',
  bNot: '⫭',
  backcong: '≌',
  backepsilon: '϶',
  backprime: '‵',
  backsim: '∽',
  backsimeq: '⋍',
  barvee: '⊽',
  barwed: '⌅',
  barwedge: '⌅',
  bbrk: '⎵',
  bbrktbrk: '⎶',
  bcong: '≌',
  bcy: 'б',
  bdquo: '„',
  becaus: '∵',
  because: '∵',
  bemptyv: '⦰',
  bepsi: '϶',
  bernou: 'ℬ',
  beta: 'β',
  beth: 'ℶ',
  between: '≬',
  bfr: '𝔟',
  bigcap: '⋂',
  bigcirc: '◯',
  bigcup: '⋃',
  bigodot: '⨀',
  bigoplus: '⨁',
  bigotimes: '⨂',
  bigsqcup: '⨆',
  bigstar: '★',
  bigtriangledown: '▽',
  bigtriangleup: '△',
  biguplus: '⨄',
  bigvee: '⋁',
  bigwedge: '⋀',
  bkarow: '⤍',
  blacklozenge: '⧫',
  blacksquare: '▪',
  blacktriangle: '▴',
  blacktriangledown: '▾',
  blacktriangleleft: '◂',
  blacktriangleright: '▸',
  blank: '␣',
  blk12: '▒',
  blk14: '░',
  blk34: '▓',
  block: '█',
  bne: '=⃥',
  bnequiv: '≡⃥',
  bnot: '⌐',
  bopf: '𝕓',
  bot: '⊥',
  bottom: '⊥',
  bowtie: '⋈',
  boxDL: '╗',
  boxDR: '╔',
  boxDl: '╖',
  boxDr: '╓',
  boxH: '═',
  boxHD: '╦',
  boxHU: '╩',
  boxHd: '╤',
  boxHu: '╧',
  boxUL: '╝',
  boxUR: '╚',
  boxUl: '╜',
  boxUr: '╙',
  boxV: '║',
  boxVH: '╬',
  boxVL: '╣',
  boxVR: '╠',
  boxVh: '╫',
  boxVl: '╢',
  boxVr: '╟',
  boxbox: '⧉',
  boxdL: '╕',
  boxdR: '╒',
  boxdl: '┐',
  boxdr: '┌',
  boxh: '─',
  boxhD: '╥',
  boxhU: '╨',
  boxhd: '┬',
  boxhu: '┴',
  boxminus: '⊟',
  boxplus: '⊞',
  boxtimes: '⊠',
  boxuL: '╛',
  boxuR: '╘',
  boxul: '┘',
  boxur: '└',
  boxv: '│',
  boxvH: '╪',
  boxvL: '╡',
  boxvR: '╞',
  boxvh: '┼',
  boxvl: '┤',
  boxvr: '├',
  bprime: '‵',
  breve: '˘',
  brvbar: '¦',
  bscr: '𝒷',
  bsemi: '⁏',
  bsim: '∽',
  bsime: '⋍',
  bsol: '\\',
  bsolb: '⧅',
  bsolhsub: '⟈',
  bull: '•',
  bullet: '•',
  bump: '≎',
  bumpE: '⪮',
  bumpe: '≏',
  bumpeq: '≏',
  cacute: 'ć',
  cap: '∩',
  capand: '⩄',
  capbrcup: '⩉',
  capcap: '⩋',
  capcup: '⩇',
  capdot: '⩀',
  caps: '∩︀',
  caret: '⁁',
  caron: 'ˇ',
  ccaps: '⩍',
  ccaron: 'č',
  ccedil: 'ç',
  ccirc: 'ĉ',
  ccups: '⩌',
  ccupssm: '⩐',
  cdot: 'ċ',
  cedil: '¸',
  cemptyv: '⦲',
  cent: '¢',
  centerdot: '·',
  cfr: '𝔠',
  chcy: 'ч',
  check: '✓',
  checkmark: '✓',
  chi: 'χ',
  cir: '○',
  cirE: '⧃',
  circ: 'ˆ',
  circeq: '≗',
  circlearrowleft: '↺',
  circlearrowright: '↻',
  circledR: '®',
  circledS: 'Ⓢ',
  circledast: '⊛',
  circledcirc: '⊚',
  circleddash: '⊝',
  cire: '≗',
  cirfnint: '⨐',
  cirmid: '⫯',
  cirscir: '⧂',
  clubs: '♣',
  clubsuit: '♣',
  colon: ':',
  colone: '≔',
  coloneq: '≔',
  comma: ',',
  commat: '@',
  comp: '∁',
  compfn: '∘',
  complement: '∁',
  complexes: 'ℂ',
  cong: '≅',
  congdot: '⩭',
  conint: '∮',
  copf: '𝕔',
  coprod: '∐',
  copy: '©',
  copysr: '℗',
  crarr: '↵',
  cross: '✗',
  cscr: '𝒸',
  csub: '⫏',
  csube: '⫑',
  csup: '⫐',
  csupe: '⫒',
  ctdot: '⋯',
  cudarrl: '⤸',
  cudarrr: '⤵',
  cuepr: '⋞',
  cuesc: '⋟',
  cularr: '↶',
  cularrp: '⤽',
  cup: '∪',
  cupbrcap: '⩈',
  cupcap: '⩆',
  cupcup: '⩊',
  cupdot: '⊍',
  cupor: '⩅',
  cups: '∪︀',
  curarr: '↷',
  curarrm: '⤼',
  curlyeqprec: '⋞',
  curlyeqsucc: '⋟',
  curlyvee: '⋎',
  curlywedge: '⋏',
  curren: '¤',
  curvearrowleft: '↶',
  curvearrowright: '↷',
  cuvee: '⋎',
  cuwed: '⋏',
  cwconint: '∲',
  cwint: '∱',
  cylcty: '⌭',
  dArr: '⇓',
  dHar: '⥥',
  dagger: '†',
  daleth: 'ℸ',
  darr: '↓',
  dash: '‐',
  dashv: '⊣',
  dbkarow: '⤏',
  dblac: '˝',
  dcaron: 'ď',
  dcy: 'д',
  dd: 'ⅆ',
  ddagger: '‡',
  ddarr: '⇊',
  ddotseq: '⩷',
  deg: '°',
  delta: 'δ',
  demptyv: '⦱',
  dfisht: '⥿',
  dfr: '𝔡',
  dharl: '⇃',
  dharr: '⇂',
  diam: '⋄',
  diamond: '⋄',
  diamondsuit: '♦',
  diams: '♦',
  die: '¨',
  digamma: 'ϝ',
  disin: '⋲',
  div: '÷',
  divide: '÷',
  divideontimes: '⋇',
  divonx: '⋇',
  djcy: 'ђ',
  dlcorn: '⌞',
  dlcrop: '⌍',
  dollar: '$',
  dopf: '𝕕',
  dot: '˙',
  doteq: '≐',
  doteqdot: '≑',
  dotminus: '∸',
  dotplus: '∔',
  dotsquare: '⊡',
  doublebarwedge: '⌆',
  downarrow: '↓',
  downdownarrows: '⇊',
  downharpoonleft: '⇃',
  downharpoonright: '⇂',
  drbkarow: '⤐',
  drcorn: '⌟',
  drcrop: '⌌',
  dscr: '𝒹',
  dscy: 'ѕ',
  dsol: '⧶',
  dstrok: 'đ',
  dtdot: '⋱',
  dtri: '▿',
  dtrif: '▾',
  duarr: '⇵',
  duhar: '⥯',
  dwangle: '⦦',
  dzcy: 'џ',
  dzigrarr: '⟿',
  eDDot: '⩷',
  eDot: '≑',
  eacute: 'é',
  easter: '⩮',
  ecaron: 'ě',
  ecir: '≖',
  ecirc: 'ê',
  ecolon: '≕',
  ecy: 'э',
  edot: 'ė',
  ee: 'ⅇ',
  efDot: '≒',
  efr: '𝔢',
  eg: '⪚',
  egrave: 'è',
  egs: '⪖',
  egsdot: '⪘',
  el: '⪙',
  elinters: '⏧',
  ell: 'ℓ',
  els: '⪕',
  elsdot: '⪗',
  emacr: 'ē',
  empty: '∅',
  emptyset: '∅',
  emptyv: '∅',
  emsp13: ' ',
  emsp14: ' ',
  emsp: ' ',
  eng: 'ŋ',
  ensp: ' ',
  eogon: 'ę',
  eopf: '𝕖',
  epar: '⋕',
  eparsl: '⧣',
  eplus: '⩱',
  epsi: 'ε',
  epsilon: 'ε',
  epsiv: 'ϵ',
  eqcirc: '≖',
  eqcolon: '≕',
  eqsim: '≂',
  eqslantgtr: '⪖',
  eqslantless: '⪕',
  equals: '=',
  equest: '≟',
  equiv: '≡',
  equivDD: '⩸',
  eqvparsl: '⧥',
  erDot: '≓',
  erarr: '⥱',
  escr: 'ℯ',
  esdot: '≐',
  esim: '≂',
  eta: 'η',
  eth: 'ð',
  euml: 'ë',
  euro: '€',
  excl: '!',
  exist: '∃',
  expectation: 'ℰ',
  exponentiale: 'ⅇ',
  fallingdotseq: '≒',
  fcy: 'ф',
  female: '♀',
  ffilig: 'ﬃ',
  fflig: 'ﬀ',
  ffllig: 'ﬄ',
  ffr: '𝔣',
  filig: 'ﬁ',
  fjlig: 'fj',
  flat: '♭',
  fllig: 'ﬂ',
  fltns: '▱',
  fnof: 'ƒ',
  fopf: '𝕗',
  forall: '∀',
  fork: '⋔',
  forkv: '⫙',
  fpartint: '⨍',
  frac12: '½',
  frac13: '⅓',
  frac14: '¼',
  frac15: '⅕',
  frac16: '⅙',
  frac18: '⅛',
  frac23: '⅔',
  frac25: '⅖',
  frac34: '¾',
  frac35: '⅗',
  frac38: '⅜',
  frac45: '⅘',
  frac56: '⅚',
  frac58: '⅝',
  frac78: '⅞',
  frasl: '⁄',
  frown: '⌢',
  fscr: '𝒻',
  gE: '≧',
  gEl: '⪌',
  gacute: 'ǵ',
  gamma: 'γ',
  gammad: 'ϝ',
  gap: '⪆',
  gbreve: 'ğ',
  gcirc: 'ĝ',
  gcy: 'г',
  gdot: 'ġ',
  ge: '≥',
  gel: '⋛',
  geq: '≥',
  geqq: '≧',
  geqslant: '⩾',
  ges: '⩾',
  gescc: '⪩',
  gesdot: '⪀',
  gesdoto: '⪂',
  gesdotol: '⪄',
  gesl: '⋛︀',
  gesles: '⪔',
  gfr: '𝔤',
  gg: '≫',
  ggg: '⋙',
  gimel: 'ℷ',
  gjcy: 'ѓ',
  gl: '≷',
  glE: '⪒',
  gla: '⪥',
  glj: '⪤',
  gnE: '≩',
  gnap: '⪊',
  gnapprox: '⪊',
  gne: '⪈',
  gneq: '⪈',
  gneqq: '≩',
  gnsim: '⋧',
  gopf: '𝕘',
  grave: '`',
  gscr: 'ℊ',
  gsim: '≳',
  gsime: '⪎',
  gsiml: '⪐',
  gt: '>',
  gtcc: '⪧',
  gtcir: '⩺',
  gtdot: '⋗',
  gtlPar: '⦕',
  gtquest: '⩼',
  gtrapprox: '⪆',
  gtrarr: '⥸',
  gtrdot: '⋗',
  gtreqless: '⋛',
  gtreqqless: '⪌',
  gtrless: '≷',
  gtrsim: '≳',
  gvertneqq: '≩︀',
  gvnE: '≩︀',
  hArr: '⇔',
  hairsp: ' ',
  half: '½',
  hamilt: 'ℋ',
  hardcy: 'ъ',
  harr: '↔',
  harrcir: '⥈',
  harrw: '↭',
  hbar: 'ℏ',
  hcirc: 'ĥ',
  hearts: '♥',
  heartsuit: '♥',
  hellip: '…',
  hercon: '⊹',
  hfr: '𝔥',
  hksearow: '⤥',
  hkswarow: '⤦',
  hoarr: '⇿',
  homtht: '∻',
  hookleftarrow: '↩',
  hookrightarrow: '↪',
  hopf: '𝕙',
  horbar: '―',
  hscr: '𝒽',
  hslash: 'ℏ',
  hstrok: 'ħ',
  hybull: '⁃',
  hyphen: '‐',
  iacute: 'í',
  ic: '⁣',
  icirc: 'î',
  icy: 'и',
  iecy: 'е',
  iexcl: '¡',
  iff: '⇔',
  ifr: '𝔦',
  igrave: 'ì',
  ii: 'ⅈ',
  iiiint: '⨌',
  iiint: '∭',
  iinfin: '⧜',
  iiota: '℩',
  ijlig: 'ĳ',
  imacr: 'ī',
  image: 'ℑ',
  imagline: 'ℐ',
  imagpart: 'ℑ',
  imath: 'ı',
  imof: '⊷',
  imped: 'Ƶ',
  in: '∈',
  incare: '℅',
  infin: '∞',
  infintie: '⧝',
  inodot: 'ı',
  int: '∫',
  intcal: '⊺',
  integers: 'ℤ',
  intercal: '⊺',
  intlarhk: '⨗',
  intprod: '⨼',
  iocy: 'ё',
  iogon: 'į',
  iopf: '𝕚',
  iota: 'ι',
  iprod: '⨼',
  iquest: '¿',
  iscr: '𝒾',
  isin: '∈',
  isinE: '⋹',
  isindot: '⋵',
  isins: '⋴',
  isinsv: '⋳',
  isinv: '∈',
  it: '⁢',
  itilde: 'ĩ',
  iukcy: 'і',
  iuml: 'ï',
  jcirc: 'ĵ',
  jcy: 'й',
  jfr: '𝔧',
  jmath: 'ȷ',
  jopf: '𝕛',
  jscr: '𝒿',
  jsercy: 'ј',
  jukcy: 'є',
  kappa: 'κ',
  kappav: 'ϰ',
  kcedil: 'ķ',
  kcy: 'к',
  kfr: '𝔨',
  kgreen: 'ĸ',
  khcy: 'х',
  kjcy: 'ќ',
  kopf: '𝕜',
  kscr: '𝓀',
  lAarr: '⇚',
  lArr: '⇐',
  lAtail: '⤛',
  lBarr: '⤎',
  lE: '≦',
  lEg: '⪋',
  lHar: '⥢',
  lacute: 'ĺ',
  laemptyv: '⦴',
  lagran: 'ℒ',
  lambda: 'λ',
  lang: '⟨',
  langd: '⦑',
  langle: '⟨',
  lap: '⪅',
  laquo: '«',
  larr: '←',
  larrb: '⇤',
  larrbfs: '⤟',
  larrfs: '⤝',
  larrhk: '↩',
  larrlp: '↫',
  larrpl: '⤹',
  larrsim: '⥳',
  larrtl: '↢',
  lat: '⪫',
  latail: '⤙',
  late: '⪭',
  lates: '⪭︀',
  lbarr: '⤌',
  lbbrk: '❲',
  lbrace: '{',
  lbrack: '[',
  lbrke: '⦋',
  lbrksld: '⦏',
  lbrkslu: '⦍',
  lcaron: 'ľ',
  lcedil: 'ļ',
  lceil: '⌈',
  lcub: '{',
  lcy: 'л',
  ldca: '⤶',
  ldquo: '“',
  ldquor: '„',
  ldrdhar: '⥧',
  ldrushar: '⥋',
  ldsh: '↲',
  le: '≤',
  leftarrow: '←',
  leftarrowtail: '↢',
  leftharpoondown: '↽',
  leftharpoonup: '↼',
  leftleftarrows: '⇇',
  leftrightarrow: '↔',
  leftrightarrows: '⇆',
  leftrightharpoons: '⇋',
  leftrightsquigarrow: '↭',
  leftthreetimes: '⋋',
  leg: '⋚',
  leq: '≤',
  leqq: '≦',
  leqslant: '⩽',
  les: '⩽',
  lescc: '⪨',
  lesdot: '⩿',
  lesdoto: '⪁',
  lesdotor: '⪃',
  lesg: '⋚︀',
  lesges: '⪓',
  lessapprox: '⪅',
  lessdot: '⋖',
  lesseqgtr: '⋚',
  lesseqqgtr: '⪋',
  lessgtr: '≶',
  lesssim: '≲',
  lfisht: '⥼',
  lfloor: '⌊',
  lfr: '𝔩',
  lg: '≶',
  lgE: '⪑',
  lhard: '↽',
  lharu: '↼',
  lharul: '⥪',
  lhblk: '▄',
  ljcy: 'љ',
  ll: '≪',
  llarr: '⇇',
  llcorner: '⌞',
  llhard: '⥫',
  lltri: '◺',
  lmidot: 'ŀ',
  lmoust: '⎰',
  lmoustache: '⎰',
  lnE: '≨',
  lnap: '⪉',
  lnapprox: '⪉',
  lne: '⪇',
  lneq: '⪇',
  lneqq: '≨',
  lnsim: '⋦',
  loang: '⟬',
  loarr: '⇽',
  lobrk: '⟦',
  longleftarrow: '⟵',
  longleftrightarrow: '⟷',
  longmapsto: '⟼',
  longrightarrow: '⟶',
  looparrowleft: '↫',
  looparrowright: '↬',
  lopar: '⦅',
  lopf: '𝕝',
  loplus: '⨭',
  lotimes: '⨴',
  lowast: '∗',
  lowbar: '_',
  loz: '◊',
  lozenge: '◊',
  lozf: '⧫',
  lpar: '(',
  lparlt: '⦓',
  lrarr: '⇆',
  lrcorner: '⌟',
  lrhar: '⇋',
  lrhard: '⥭',
  lrm: '‎',
  lrtri: '⊿',
  lsaquo: '‹',
  lscr: '𝓁',
  lsh: '↰',
  lsim: '≲',
  lsime: '⪍',
  lsimg: '⪏',
  lsqb: '[',
  lsquo: '‘',
  lsquor: '‚',
  lstrok: 'ł',
  lt: '<',
  ltcc: '⪦',
  ltcir: '⩹',
  ltdot: '⋖',
  lthree: '⋋',
  ltimes: '⋉',
  ltlarr: '⥶',
  ltquest: '⩻',
  ltrPar: '⦖',
  ltri: '◃',
  ltrie: '⊴',
  ltrif: '◂',
  lurdshar: '⥊',
  luruhar: '⥦',
  lvertneqq: '≨︀',
  lvnE: '≨︀',
  mDDot: '∺',
  macr: '¯',
  male: '♂',
  malt: '✠',
  maltese: '✠',
  map: '↦',
  mapsto: '↦',
  mapstodown: '↧',
  mapstoleft: '↤',
  mapstoup: '↥',
  marker: '▮',
  mcomma: '⨩',
  mcy: 'м',
  mdash: '—',
  measuredangle: '∡',
  mfr: '𝔪',
  mho: '℧',
  micro: 'µ',
  mid: '∣',
  midast: '*',
  midcir: '⫰',
  middot: '·',
  minus: '−',
  minusb: '⊟',
  minusd: '∸',
  minusdu: '⨪',
  mlcp: '⫛',
  mldr: '…',
  mnplus: '∓',
  models: '⊧',
  mopf: '𝕞',
  mp: '∓',
  mscr: '𝓂',
  mstpos: '∾',
  mu: 'μ',
  multimap: '⊸',
  mumap: '⊸',
  nGg: '⋙̸',
  nGt: '≫⃒',
  nGtv: '≫̸',
  nLeftarrow: '⇍',
  nLeftrightarrow: '⇎',
  nLl: '⋘̸',
  nLt: '≪⃒',
  nLtv: '≪̸',
  nRightarrow: '⇏',
  nVDash: '⊯',
  nVdash: '⊮',
  nabla: '∇',
  nacute: 'ń',
  nang: '∠⃒',
  nap: '≉',
  napE: '⩰̸',
  napid: '≋̸',
  napos: 'ŉ',
  napprox: '≉',
  natur: '♮',
  natural: '♮',
  naturals: 'ℕ',
  nbsp: ' ',
  nbump: '≎̸',
  nbumpe: '≏̸',
  ncap: '⩃',
  ncaron: 'ň',
  ncedil: 'ņ',
  ncong: '≇',
  ncongdot: '⩭̸',
  ncup: '⩂',
  ncy: 'н',
  ndash: '–',
  ne: '≠',
  neArr: '⇗',
  nearhk: '⤤',
  nearr: '↗',
  nearrow: '↗',
  nedot: '≐̸',
  nequiv: '≢',
  nesear: '⤨',
  nesim: '≂̸',
  nexist: '∄',
  nexists: '∄',
  nfr: '𝔫',
  ngE: '≧̸',
  nge: '≱',
  ngeq: '≱',
  ngeqq: '≧̸',
  ngeqslant: '⩾̸',
  nges: '⩾̸',
  ngsim: '≵',
  ngt: '≯',
  ngtr: '≯',
  nhArr: '⇎',
  nharr: '↮',
  nhpar: '⫲',
  ni: '∋',
  nis: '⋼',
  nisd: '⋺',
  niv: '∋',
  njcy: 'њ',
  nlArr: '⇍',
  nlE: '≦̸',
  nlarr: '↚',
  nldr: '‥',
  nle: '≰',
  nleftarrow: '↚',
  nleftrightarrow: '↮',
  nleq: '≰',
  nleqq: '≦̸',
  nleqslant: '⩽̸',
  nles: '⩽̸',
  nless: '≮',
  nlsim: '≴',
  nlt: '≮',
  nltri: '⋪',
  nltrie: '⋬',
  nmid: '∤',
  nopf: '𝕟',
  not: '¬',
  notin: '∉',
  notinE: '⋹̸',
  notindot: '⋵̸',
  notinva: '∉',
  notinvb: '⋷',
  notinvc: '⋶',
  notni: '∌',
  notniva: '∌',
  notnivb: '⋾',
  notnivc: '⋽',
  npar: '∦',
  nparallel: '∦',
  nparsl: '⫽⃥',
  npart: '∂̸',
  npolint: '⨔',
  npr: '⊀',
  nprcue: '⋠',
  npre: '⪯̸',
  nprec: '⊀',
  npreceq: '⪯̸',
  nrArr: '⇏',
  nrarr: '↛',
  nrarrc: '⤳̸',
  nrarrw: '↝̸',
  nrightarrow: '↛',
  nrtri: '⋫',
  nrtrie: '⋭',
  nsc: '⊁',
  nsccue: '⋡',
  nsce: '⪰̸',
  nscr: '𝓃',
  nshortmid: '∤',
  nshortparallel: '∦',
  nsim: '≁',
  nsime: '≄',
  nsimeq: '≄',
  nsmid: '∤',
  nspar: '∦',
  nsqsube: '⋢',
  nsqsupe: '⋣',
  nsub: '⊄',
  nsubE: '⫅̸',
  nsube: '⊈',
  nsubset: '⊂⃒',
  nsubseteq: '⊈',
  nsubseteqq: '⫅̸',
  nsucc: '⊁',
  nsucceq: '⪰̸',
  nsup: '⊅',
  nsupE: '⫆̸',
  nsupe: '⊉',
  nsupset: '⊃⃒',
  nsupseteq: '⊉',
  nsupseteqq: '⫆̸',
  ntgl: '≹',
  ntilde: 'ñ',
  ntlg: '≸',
  ntriangleleft: '⋪',
  ntrianglelefteq: '⋬',
  ntriangleright: '⋫',
  ntrianglerighteq: '⋭',
  nu: 'ν',
  num: '#',
  numero: '№',
  numsp: ' ',
  nvDash: '⊭',
  nvHarr: '⤄',
  nvap: '≍⃒',
  nvdash: '⊬',
  nvge: '≥⃒',
  nvgt: '>⃒',
  nvinfin: '⧞',
  nvlArr: '⤂',
  nvle: '≤⃒',
  nvlt: '<⃒',
  nvltrie: '⊴⃒',
  nvrArr: '⤃',
  nvrtrie: '⊵⃒',
  nvsim: '∼⃒',
  nwArr: '⇖',
  nwarhk: '⤣',
  nwarr: '↖',
  nwarrow: '↖',
  nwnear: '⤧',
  oS: 'Ⓢ',
  oacute: 'ó',
  oast: '⊛',
  ocir: '⊚',
  ocirc: 'ô',
  ocy: 'о',
  odash: '⊝',
  odblac: 'ő',
  odiv: '⨸',
  odot: '⊙',
  odsold: '⦼',
  oelig: 'œ',
  ofcir: '⦿',
  ofr: '𝔬',
  ogon: '˛',
  ograve: 'ò',
  ogt: '⧁',
  ohbar: '⦵',
  ohm: 'Ω',
  oint: '∮',
  olarr: '↺',
  olcir: '⦾',
  olcross: '⦻',
  oline: '‾',
  olt: '⧀',
  omacr: 'ō',
  omega: 'ω',
  omicron: 'ο',
  omid: '⦶',
  ominus: '⊖',
  oopf: '𝕠',
  opar: '⦷',
  operp: '⦹',
  oplus: '⊕',
  or: '∨',
  orarr: '↻',
  ord: '⩝',
  order: 'ℴ',
  orderof: 'ℴ',
  ordf: 'ª',
  ordm: 'º',
  origof: '⊶',
  oror: '⩖',
  orslope: '⩗',
  orv: '⩛',
  oscr: 'ℴ',
  oslash: 'ø',
  osol: '⊘',
  otilde: 'õ',
  otimes: '⊗',
  otimesas: '⨶',
  ouml: 'ö',
  ovbar: '⌽',
  par: '∥',
  para: '¶',
  parallel: '∥',
  parsim: '⫳',
  parsl: '⫽',
  part: '∂',
  pcy: 'п',
  percnt: '%',
  period: '.',
  permil: '‰',
  perp: '⊥',
  pertenk: '‱',
  pfr: '𝔭',
  phi: 'φ',
  phiv: 'ϕ',
  phmmat: 'ℳ',
  phone: '☎',
  pi: 'π',
  pitchfork: '⋔',
  piv: 'ϖ',
  planck: 'ℏ',
  planckh: 'ℎ',
  plankv: 'ℏ',
  plus: '+',
  plusacir: '⨣',
  plusb: '⊞',
  pluscir: '⨢',
  plusdo: '∔',
  plusdu: '⨥',
  pluse: '⩲',
  plusmn: '±',
  plussim: '⨦',
  plustwo: '⨧',
  pm: '±',
  pointint: '⨕',
  popf: '𝕡',
  pound: '£',
  pr: '≺',
  prE: '⪳',
  prap: '⪷',
  prcue: '≼',
  pre: '⪯',
  prec: '≺',
  precapprox: '⪷',
  preccurlyeq: '≼',
  preceq: '⪯',
  precnapprox: '⪹',
  precneqq: '⪵',
  precnsim: '⋨',
  precsim: '≾',
  prime: '′',
  primes: 'ℙ',
  prnE: '⪵',
  prnap: '⪹',
  prnsim: '⋨',
  prod: '∏',
  profalar: '⌮',
  profline: '⌒',
  profsurf: '⌓',
  prop: '∝',
  propto: '∝',
  prsim: '≾',
  prurel: '⊰',
  pscr: '𝓅',
  psi: 'ψ',
  puncsp: ' ',
  qfr: '𝔮',
  qint: '⨌',
  qopf: '𝕢',
  qprime: '⁗',
  qscr: '𝓆',
  quaternions: 'ℍ',
  quatint: '⨖',
  quest: '?',
  questeq: '≟',
  quot: '"',
  rAarr: '⇛',
  rArr: '⇒',
  rAtail: '⤜',
  rBarr: '⤏',
  rHar: '⥤',
  race: '∽̱',
  racute: 'ŕ',
  radic: '√',
  raemptyv: '⦳',
  rang: '⟩',
  rangd: '⦒',
  range: '⦥',
  rangle: '⟩',
  raquo: '»',
  rarr: '→',
  rarrap: '⥵',
  rarrb: '⇥',
  rarrbfs: '⤠',
  rarrc: '⤳',
  rarrfs: '⤞',
  rarrhk: '↪',
  rarrlp: '↬',
  rarrpl: '⥅',
  rarrsim: '⥴',
  rarrtl: '↣',
  rarrw: '↝',
  ratail: '⤚',
  ratio: '∶',
  rationals: 'ℚ',
  rbarr: '⤍',
  rbbrk: '❳',
  rbrace: '}',
  rbrack: ']',
  rbrke: '⦌',
  rbrksld: '⦎',
  rbrkslu: '⦐',
  rcaron: 'ř',
  rcedil: 'ŗ',
  rceil: '⌉',
  rcub: '}',
  rcy: 'р',
  rdca: '⤷',
  rdldhar: '⥩',
  rdquo: '”',
  rdquor: '”',
  rdsh: '↳',
  real: 'ℜ',
  realine: 'ℛ',
  realpart: 'ℜ',
  reals: 'ℝ',
  rect: '▭',
  reg: '®',
  rfisht: '⥽',
  rfloor: '⌋',
  rfr: '𝔯',
  rhard: '⇁',
  rharu: '⇀',
  rharul: '⥬',
  rho: 'ρ',
  rhov: 'ϱ',
  rightarrow: '→',
  rightarrowtail: '↣',
  rightharpoondown: '⇁',
  rightharpoonup: '⇀',
  rightleftarrows: '⇄',
  rightleftharpoons: '⇌',
  rightrightarrows: '⇉',
  rightsquigarrow: '↝',
  rightthreetimes: '⋌',
  ring: '˚',
  risingdotseq: '≓',
  rlarr: '⇄',
  rlhar: '⇌',
  rlm: '‏',
  rmoust: '⎱',
  rmoustache: '⎱',
  rnmid: '⫮',
  roang: '⟭',
  roarr: '⇾',
  robrk: '⟧',
  ropar: '⦆',
  ropf: '𝕣',
  roplus: '⨮',
  rotimes: '⨵',
  rpar: ')',
  rpargt: '⦔',
  rppolint: '⨒',
  rrarr: '⇉',
  rsaquo: '›',
  rscr: '𝓇',
  rsh: '↱',
  rsqb: ']',
  rsquo: '’',
  rsquor: '’',
  rthree: '⋌',
  rtimes: '⋊',
  rtri: '▹',
  rtrie: '⊵',
  rtrif: '▸',
  rtriltri: '⧎',
  ruluhar: '⥨',
  rx: '℞',
  sacute: 'ś',
  sbquo: '‚',
  sc: '≻',
  scE: '⪴',
  scap: '⪸',
  scaron: 'š',
  sccue: '≽',
  sce: '⪰',
  scedil: 'ş',
  scirc: 'ŝ',
  scnE: '⪶',
  scnap: '⪺',
  scnsim: '⋩',
  scpolint: '⨓',
  scsim: '≿',
  scy: 'с',
  sdot: '⋅',
  sdotb: '⊡',
  sdote: '⩦',
  seArr: '⇘',
  searhk: '⤥',
  searr: '↘',
  searrow: '↘',
  sect: '§',
  semi: ';',
  seswar: '⤩',
  setminus: '∖',
  setmn: '∖',
  sext: '✶',
  sfr: '𝔰',
  sfrown: '⌢',
  sharp: '♯',
  shchcy: 'щ',
  shcy: 'ш',
  shortmid: '∣',
  shortparallel: '∥',
  shy: '­',
  sigma: 'σ',
  sigmaf: 'ς',
  sigmav: 'ς',
  sim: '∼',
  simdot: '⩪',
  sime: '≃',
  simeq: '≃',
  simg: '⪞',
  simgE: '⪠',
  siml: '⪝',
  simlE: '⪟',
  simne: '≆',
  simplus: '⨤',
  simrarr: '⥲',
  slarr: '←',
  smallsetminus: '∖',
  smashp: '⨳',
  smeparsl: '⧤',
  smid: '∣',
  smile: '⌣',
  smt: '⪪',
  smte: '⪬',
  smtes: '⪬︀',
  softcy: 'ь',
  sol: '/',
  solb: '⧄',
  solbar: '⌿',
  sopf: '𝕤',
  spades: '♠',
  spadesuit: '♠',
  spar: '∥',
  sqcap: '⊓',
  sqcaps: '⊓︀',
  sqcup: '⊔',
  sqcups: '⊔︀',
  sqsub: '⊏',
  sqsube: '⊑',
  sqsubset: '⊏',
  sqsubseteq: '⊑',
  sqsup: '⊐',
  sqsupe: '⊒',
  sqsupset: '⊐',
  sqsupseteq: '⊒',
  squ: '□',
  square: '□',
  squarf: '▪',
  squf: '▪',
  srarr: '→',
  sscr: '𝓈',
  ssetmn: '∖',
  ssmile: '⌣',
  sstarf: '⋆',
  star: '☆',
  starf: '★',
  straightepsilon: 'ϵ',
  straightphi: 'ϕ',
  strns: '¯',
  sub: '⊂',
  subE: '⫅',
  subdot: '⪽',
  sube: '⊆',
  subedot: '⫃',
  submult: '⫁',
  subnE: '⫋',
  subne: '⊊',
  subplus: '⪿',
  subrarr: '⥹',
  subset: '⊂',
  subseteq: '⊆',
  subseteqq: '⫅',
  subsetneq: '⊊',
  subsetneqq: '⫋',
  subsim: '⫇',
  subsub: '⫕',
  subsup: '⫓',
  succ: '≻',
  succapprox: '⪸',
  succcurlyeq: '≽',
  succeq: '⪰',
  succnapprox: '⪺',
  succneqq: '⪶',
  succnsim: '⋩',
  succsim: '≿',
  sum: '∑',
  sung: '♪',
  sup1: '¹',
  sup2: '²',
  sup3: '³',
  sup: '⊃',
  supE: '⫆',
  supdot: '⪾',
  supdsub: '⫘',
  supe: '⊇',
  supedot: '⫄',
  suphsol: '⟉',
  suphsub: '⫗',
  suplarr: '⥻',
  supmult: '⫂',
  supnE: '⫌',
  supne: '⊋',
  supplus: '⫀',
  supset: '⊃',
  supseteq: '⊇',
  supseteqq: '⫆',
  supsetneq: '⊋',
  supsetneqq: '⫌',
  supsim: '⫈',
  supsub: '⫔',
  supsup: '⫖',
  swArr: '⇙',
  swarhk: '⤦',
  swarr: '↙',
  swarrow: '↙',
  swnwar: '⤪',
  szlig: 'ß',
  target: '⌖',
  tau: 'τ',
  tbrk: '⎴',
  tcaron: 'ť',
  tcedil: 'ţ',
  tcy: 'т',
  tdot: '⃛',
  telrec: '⌕',
  tfr: '𝔱',
  there4: '∴',
  therefore: '∴',
  theta: 'θ',
  thetasym: 'ϑ',
  thetav: 'ϑ',
  thickapprox: '≈',
  thicksim: '∼',
  thinsp: ' ',
  thkap: '≈',
  thksim: '∼',
  thorn: 'þ',
  tilde: '˜',
  times: '×',
  timesb: '⊠',
  timesbar: '⨱',
  timesd: '⨰',
  tint: '∭',
  toea: '⤨',
  top: '⊤',
  topbot: '⌶',
  topcir: '⫱',
  topf: '𝕥',
  topfork: '⫚',
  tosa: '⤩',
  tprime: '‴',
  trade: '™',
  triangle: '▵',
  triangledown: '▿',
  triangleleft: '◃',
  trianglelefteq: '⊴',
  triangleq: '≜',
  triangleright: '▹',
  trianglerighteq: '⊵',
  tridot: '◬',
  trie: '≜',
  triminus: '⨺',
  triplus: '⨹',
  trisb: '⧍',
  tritime: '⨻',
  trpezium: '⏢',
  tscr: '𝓉',
  tscy: 'ц',
  tshcy: 'ћ',
  tstrok: 'ŧ',
  twixt: '≬',
  twoheadleftarrow: '↞',
  twoheadrightarrow: '↠',
  uArr: '⇑',
  uHar: '⥣',
  uacute: 'ú',
  uarr: '↑',
  ubrcy: 'ў',
  ubreve: 'ŭ',
  ucirc: 'û',
  ucy: 'у',
  udarr: '⇅',
  udblac: 'ű',
  udhar: '⥮',
  ufisht: '⥾',
  ufr: '𝔲',
  ugrave: 'ù',
  uharl: '↿',
  uharr: '↾',
  uhblk: '▀',
  ulcorn: '⌜',
  ulcorner: '⌜',
  ulcrop: '⌏',
  ultri: '◸',
  umacr: 'ū',
  uml: '¨',
  uogon: 'ų',
  uopf: '𝕦',
  uparrow: '↑',
  updownarrow: '↕',
  upharpoonleft: '↿',
  upharpoonright: '↾',
  uplus: '⊎',
  upsi: 'υ',
  upsih: 'ϒ',
  upsilon: 'υ',
  upuparrows: '⇈',
  urcorn: '⌝',
  urcorner: '⌝',
  urcrop: '⌎',
  uring: 'ů',
  urtri: '◹',
  uscr: '𝓊',
  utdot: '⋰',
  utilde: 'ũ',
  utri: '▵',
  utrif: '▴',
  uuarr: '⇈',
  uuml: 'ü',
  uwangle: '⦧',
  vArr: '⇕',
  vBar: '⫨',
  vBarv: '⫩',
  vDash: '⊨',
  vangrt: '⦜',
  varepsilon: 'ϵ',
  varkappa: 'ϰ',
  varnothing: '∅',
  varphi: 'ϕ',
  varpi: 'ϖ',
  varpropto: '∝',
  varr: '↕',
  varrho: 'ϱ',
  varsigma: 'ς',
  varsubsetneq: '⊊︀',
  varsubsetneqq: '⫋︀',
  varsupsetneq: '⊋︀',
  varsupsetneqq: '⫌︀',
  vartheta: 'ϑ',
  vartriangleleft: '⊲',
  vartriangleright: '⊳',
  vcy: 'в',
  vdash: '⊢',
  vee: '∨',
  veebar: '⊻',
  veeeq: '≚',
  vellip: '⋮',
  verbar: '|',
  vert: '|',
  vfr: '𝔳',
  vltri: '⊲',
  vnsub: '⊂⃒',
  vnsup: '⊃⃒',
  vopf: '𝕧',
  vprop: '∝',
  vrtri: '⊳',
  vscr: '𝓋',
  vsubnE: '⫋︀',
  vsubne: '⊊︀',
  vsupnE: '⫌︀',
  vsupne: '⊋︀',
  vzigzag: '⦚',
  wcirc: 'ŵ',
  wedbar: '⩟',
  wedge: '∧',
  wedgeq: '≙',
  weierp: '℘',
  wfr: '𝔴',
  wopf: '𝕨',
  wp: '℘',
  wr: '≀',
  wreath: '≀',
  wscr: '𝓌',
  xcap: '⋂',
  xcirc: '◯',
  xcup: '⋃',
  xdtri: '▽',
  xfr: '𝔵',
  xhArr: '⟺',
  xharr: '⟷',
  xi: 'ξ',
  xlArr: '⟸',
  xlarr: '⟵',
  xmap: '⟼',
  xnis: '⋻',
  xodot: '⨀',
  xopf: '𝕩',
  xoplus: '⨁',
  xotime: '⨂',
  xrArr: '⟹',
  xrarr: '⟶',
  xscr: '𝓍',
  xsqcup: '⨆',
  xuplus: '⨄',
  xutri: '△',
  xvee: '⋁',
  xwedge: '⋀',
  yacute: 'ý',
  yacy: 'я',
  ycirc: 'ŷ',
  ycy: 'ы',
  yen: '¥',
  yfr: '𝔶',
  yicy: 'ї',
  yopf: '𝕪',
  yscr: '𝓎',
  yucy: 'ю',
  yuml: 'ÿ',
  zacute: 'ź',
  zcaron: 'ž',
  zcy: 'з',
  zdot: 'ż',
  zeetrf: 'ℨ',
  zeta: 'ζ',
  zfr: '𝔷',
  zhcy: 'ж',
  zigrarr: '⇝',
  zopf: '𝕫',
  zscr: '𝓏',
  zwj: '‍',
  zwnj: '‌'
};

const own$5 = {}.hasOwnProperty;
function decodeNamedCharacterReference(value) {
  return own$5.call(characterEntities, value) ? characterEntities[value] : false
}

function splice(list, start, remove, items) {
  const end = list.length;
  let chunkStart = 0;
  let parameters;
  if (start < 0) {
    start = -start > end ? 0 : end + start;
  } else {
    start = start > end ? end : start;
  }
  remove = remove > 0 ? remove : 0;
  if (items.length < 10000) {
    parameters = Array.from(items);
    parameters.unshift(start, remove);
    list.splice(...parameters);
  } else {
    if (remove) list.splice(start, remove);
    while (chunkStart < items.length) {
      parameters = items.slice(chunkStart, chunkStart + 10000);
      parameters.unshift(start, 0);
      list.splice(...parameters);
      chunkStart += 10000;
      start += 10000;
    }
  }
}
function push(list, items) {
  if (list.length > 0) {
    splice(list, list.length, 0, items);
    return list
  }
  return items
}

const hasOwnProperty = {}.hasOwnProperty;
function combineExtensions(extensions) {
  const all = {};
  let index = -1;
  while (++index < extensions.length) {
    syntaxExtension(all, extensions[index]);
  }
  return all
}
function syntaxExtension(all, extension) {
  let hook;
  for (hook in extension) {
    const maybe = hasOwnProperty.call(all, hook) ? all[hook] : undefined;
    const left = maybe || (all[hook] = {});
    const right = extension[hook];
    let code;
    if (right) {
      for (code in right) {
        if (!hasOwnProperty.call(left, code)) left[code] = [];
        const value = right[code];
        constructs(
          left[code],
          Array.isArray(value) ? value : value ? [value] : []
        );
      }
    }
  }
}
function constructs(existing, list) {
  let index = -1;
  const before = [];
  while (++index < list.length) {
(list[index].add === 'after' ? existing : before).push(list[index]);
  }
  splice(existing, 0, 0, before);
}

function decodeNumericCharacterReference(value, base) {
  const code = Number.parseInt(value, base);
  if (
  code < 9 || code === 11 || code > 13 && code < 32 ||
  code > 126 && code < 160 ||
  code > 55_295 && code < 57_344 ||
  code > 64_975 && code < 65_008 ||
  (code & 65_535) === 65_535 || (code & 65_535) === 65_534 ||
  code > 1_114_111) {
    return "\uFFFD";
  }
  return String.fromCodePoint(code);
}

function normalizeIdentifier$1(value) {
  return (
    value
      .replace(/[\t\n\r ]+/g, ' ')
      .replace(/^ | $/g, '')
      .toLowerCase()
      .toUpperCase()
  )
}

const unicodePunctuationInternal = regexCheck(/\p{P}/u);
const asciiAlpha = regexCheck(/[A-Za-z]/);
const asciiAlphanumeric = regexCheck(/[\dA-Za-z]/);
const asciiAtext = regexCheck(/[#-'*+\--9=?A-Z^-~]/);
function asciiControl(code) {
  return (
    code !== null && (code < 32 || code === 127)
  )
}
const asciiDigit = regexCheck(/\d/);
const asciiHexDigit = regexCheck(/[\dA-Fa-f]/);
const asciiPunctuation = regexCheck(/[!-/:-@[-`{-~]/);
function markdownLineEnding(code) {
  return code !== null && code < -2
}
function markdownLineEndingOrSpace(code) {
  return code !== null && (code < 0 || code === 32)
}
function markdownSpace(code) {
  return code === -2 || code === -1 || code === 32
}
function unicodePunctuation(code) {
  return asciiPunctuation(code) || unicodePunctuationInternal(code)
}
const unicodeWhitespace = regexCheck(/\s/);
function regexCheck(regex) {
  return check
  function check(code) {
    return code !== null && code > -1 && regex.test(String.fromCharCode(code))
  }
}

function factorySpace(effects, ok, type, max) {
  const limit = max ? max - 1 : Number.POSITIVE_INFINITY;
  let size = 0;
  return start
  function start(code) {
    if (markdownSpace(code)) {
      effects.enter(type);
      return prefix(code)
    }
    return ok(code)
  }
  function prefix(code) {
    if (markdownSpace(code) && size++ < limit) {
      effects.consume(code);
      return prefix
    }
    effects.exit(type);
    return ok(code)
  }
}

const content$1 = {
  tokenize: initializeContent
};
function initializeContent(effects) {
  const contentStart = effects.attempt(
    this.parser.constructs.contentInitial,
    afterContentStartConstruct,
    paragraphInitial
  );
  let previous;
  return contentStart
  function afterContentStartConstruct(code) {
    if (code === null) {
      effects.consume(code);
      return
    }
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return factorySpace(effects, contentStart, 'linePrefix')
  }
  function paragraphInitial(code) {
    effects.enter('paragraph');
    return lineStart(code)
  }
  function lineStart(code) {
    const token = effects.enter('chunkText', {
      contentType: 'text',
      previous
    });
    if (previous) {
      previous.next = token;
    }
    previous = token;
    return data(code)
  }
  function data(code) {
    if (code === null) {
      effects.exit('chunkText');
      effects.exit('paragraph');
      effects.consume(code);
      return
    }
    if (markdownLineEnding(code)) {
      effects.consume(code);
      effects.exit('chunkText');
      return lineStart
    }
    effects.consume(code);
    return data
  }
}

const document$1 = {
  tokenize: initializeDocument
};
const containerConstruct = {
  tokenize: tokenizeContainer
};
function initializeDocument(effects) {
  const self = this;
  const stack = [];
  let continued = 0;
  let childFlow;
  let childToken;
  let lineStartOffset;
  return start
  function start(code) {
    if (continued < stack.length) {
      const item = stack[continued];
      self.containerState = item[1];
      return effects.attempt(
        item[0].continuation,
        documentContinue,
        checkNewContainers
      )(code)
    }
    return checkNewContainers(code)
  }
  function documentContinue(code) {
    continued++;
    if (self.containerState._closeFlow) {
      self.containerState._closeFlow = undefined;
      if (childFlow) {
        closeFlow();
      }
      const indexBeforeExits = self.events.length;
      let indexBeforeFlow = indexBeforeExits;
      let point;
      while (indexBeforeFlow--) {
        if (
          self.events[indexBeforeFlow][0] === 'exit' &&
          self.events[indexBeforeFlow][1].type === 'chunkFlow'
        ) {
          point = self.events[indexBeforeFlow][1].end;
          break
        }
      }
      exitContainers(continued);
      let index = indexBeforeExits;
      while (index < self.events.length) {
        self.events[index][1].end = Object.assign({}, point);
        index++;
      }
      splice(
        self.events,
        indexBeforeFlow + 1,
        0,
        self.events.slice(indexBeforeExits)
      );
      self.events.length = index;
      return checkNewContainers(code)
    }
    return start(code)
  }
  function checkNewContainers(code) {
    if (continued === stack.length) {
      if (!childFlow) {
        return documentContinued(code)
      }
      if (childFlow.currentConstruct && childFlow.currentConstruct.concrete) {
        return flowStart(code)
      }
      self.interrupt = Boolean(
        childFlow.currentConstruct && !childFlow._gfmTableDynamicInterruptHack
      );
    }
    self.containerState = {};
    return effects.check(
      containerConstruct,
      thereIsANewContainer,
      thereIsNoNewContainer
    )(code)
  }
  function thereIsANewContainer(code) {
    if (childFlow) closeFlow();
    exitContainers(continued);
    return documentContinued(code)
  }
  function thereIsNoNewContainer(code) {
    self.parser.lazy[self.now().line] = continued !== stack.length;
    lineStartOffset = self.now().offset;
    return flowStart(code)
  }
  function documentContinued(code) {
    self.containerState = {};
    return effects.attempt(
      containerConstruct,
      containerContinue,
      flowStart
    )(code)
  }
  function containerContinue(code) {
    continued++;
    stack.push([self.currentConstruct, self.containerState]);
    return documentContinued(code)
  }
  function flowStart(code) {
    if (code === null) {
      if (childFlow) closeFlow();
      exitContainers(0);
      effects.consume(code);
      return
    }
    childFlow = childFlow || self.parser.flow(self.now());
    effects.enter('chunkFlow', {
      contentType: 'flow',
      previous: childToken,
      _tokenizer: childFlow
    });
    return flowContinue(code)
  }
  function flowContinue(code) {
    if (code === null) {
      writeToChild(effects.exit('chunkFlow'), true);
      exitContainers(0);
      effects.consume(code);
      return
    }
    if (markdownLineEnding(code)) {
      effects.consume(code);
      writeToChild(effects.exit('chunkFlow'));
      continued = 0;
      self.interrupt = undefined;
      return start
    }
    effects.consume(code);
    return flowContinue
  }
  function writeToChild(token, eof) {
    const stream = self.sliceStream(token);
    if (eof) stream.push(null);
    token.previous = childToken;
    if (childToken) childToken.next = token;
    childToken = token;
    childFlow.defineSkip(token.start);
    childFlow.write(stream);
    if (self.parser.lazy[token.start.line]) {
      let index = childFlow.events.length;
      while (index--) {
        if (
          childFlow.events[index][1].start.offset < lineStartOffset &&
          (!childFlow.events[index][1].end ||
            childFlow.events[index][1].end.offset > lineStartOffset)
        ) {
          return
        }
      }
      const indexBeforeExits = self.events.length;
      let indexBeforeFlow = indexBeforeExits;
      let seen;
      let point;
      while (indexBeforeFlow--) {
        if (
          self.events[indexBeforeFlow][0] === 'exit' &&
          self.events[indexBeforeFlow][1].type === 'chunkFlow'
        ) {
          if (seen) {
            point = self.events[indexBeforeFlow][1].end;
            break
          }
          seen = true;
        }
      }
      exitContainers(continued);
      index = indexBeforeExits;
      while (index < self.events.length) {
        self.events[index][1].end = Object.assign({}, point);
        index++;
      }
      splice(
        self.events,
        indexBeforeFlow + 1,
        0,
        self.events.slice(indexBeforeExits)
      );
      self.events.length = index;
    }
  }
  function exitContainers(size) {
    let index = stack.length;
    while (index-- > size) {
      const entry = stack[index];
      self.containerState = entry[1];
      entry[0].exit.call(self, effects);
    }
    stack.length = size;
  }
  function closeFlow() {
    childFlow.write([null]);
    childToken = undefined;
    childFlow = undefined;
    self.containerState._closeFlow = undefined;
  }
}
function tokenizeContainer(effects, ok, nok) {
  return factorySpace(
    effects,
    effects.attempt(this.parser.constructs.document, ok, nok),
    'linePrefix',
    this.parser.constructs.disable.null.includes('codeIndented') ? undefined : 4
  )
}

function classifyCharacter(code) {
  if (
    code === null ||
    markdownLineEndingOrSpace(code) ||
    unicodeWhitespace(code)
  ) {
    return 1
  }
  if (unicodePunctuation(code)) {
    return 2
  }
}

function resolveAll(constructs, events, context) {
  const called = [];
  let index = -1;
  while (++index < constructs.length) {
    const resolve = constructs[index].resolveAll;
    if (resolve && !called.includes(resolve)) {
      events = resolve(events, context);
      called.push(resolve);
    }
  }
  return events
}

const attention = {
  name: 'attention',
  tokenize: tokenizeAttention,
  resolveAll: resolveAllAttention
};
function resolveAllAttention(events, context) {
  let index = -1;
  let open;
  let group;
  let text;
  let openingSequence;
  let closingSequence;
  let use;
  let nextEvents;
  let offset;
  while (++index < events.length) {
    if (
      events[index][0] === 'enter' &&
      events[index][1].type === 'attentionSequence' &&
      events[index][1]._close
    ) {
      open = index;
      while (open--) {
        if (
          events[open][0] === 'exit' &&
          events[open][1].type === 'attentionSequence' &&
          events[open][1]._open &&
          context.sliceSerialize(events[open][1]).charCodeAt(0) ===
            context.sliceSerialize(events[index][1]).charCodeAt(0)
        ) {
          if (
            (events[open][1]._close || events[index][1]._open) &&
            (events[index][1].end.offset - events[index][1].start.offset) % 3 &&
            !(
              (events[open][1].end.offset -
                events[open][1].start.offset +
                events[index][1].end.offset -
                events[index][1].start.offset) %
              3
            )
          ) {
            continue
          }
          use =
            events[open][1].end.offset - events[open][1].start.offset > 1 &&
            events[index][1].end.offset - events[index][1].start.offset > 1
              ? 2
              : 1;
          const start = Object.assign({}, events[open][1].end);
          const end = Object.assign({}, events[index][1].start);
          movePoint(start, -use);
          movePoint(end, use);
          openingSequence = {
            type: use > 1 ? 'strongSequence' : 'emphasisSequence',
            start,
            end: Object.assign({}, events[open][1].end)
          };
          closingSequence = {
            type: use > 1 ? 'strongSequence' : 'emphasisSequence',
            start: Object.assign({}, events[index][1].start),
            end
          };
          text = {
            type: use > 1 ? 'strongText' : 'emphasisText',
            start: Object.assign({}, events[open][1].end),
            end: Object.assign({}, events[index][1].start)
          };
          group = {
            type: use > 1 ? 'strong' : 'emphasis',
            start: Object.assign({}, openingSequence.start),
            end: Object.assign({}, closingSequence.end)
          };
          events[open][1].end = Object.assign({}, openingSequence.start);
          events[index][1].start = Object.assign({}, closingSequence.end);
          nextEvents = [];
          if (events[open][1].end.offset - events[open][1].start.offset) {
            nextEvents = push(nextEvents, [
              ['enter', events[open][1], context],
              ['exit', events[open][1], context]
            ]);
          }
          nextEvents = push(nextEvents, [
            ['enter', group, context],
            ['enter', openingSequence, context],
            ['exit', openingSequence, context],
            ['enter', text, context]
          ]);
          nextEvents = push(
            nextEvents,
            resolveAll(
              context.parser.constructs.insideSpan.null,
              events.slice(open + 1, index),
              context
            )
          );
          nextEvents = push(nextEvents, [
            ['exit', text, context],
            ['enter', closingSequence, context],
            ['exit', closingSequence, context],
            ['exit', group, context]
          ]);
          if (events[index][1].end.offset - events[index][1].start.offset) {
            offset = 2;
            nextEvents = push(nextEvents, [
              ['enter', events[index][1], context],
              ['exit', events[index][1], context]
            ]);
          } else {
            offset = 0;
          }
          splice(events, open - 1, index - open + 3, nextEvents);
          index = open + nextEvents.length - offset - 2;
          break
        }
      }
    }
  }
  index = -1;
  while (++index < events.length) {
    if (events[index][1].type === 'attentionSequence') {
      events[index][1].type = 'data';
    }
  }
  return events
}
function tokenizeAttention(effects, ok) {
  const attentionMarkers = this.parser.constructs.attentionMarkers.null;
  const previous = this.previous;
  const before = classifyCharacter(previous);
  let marker;
  return start
  function start(code) {
    marker = code;
    effects.enter('attentionSequence');
    return inside(code)
  }
  function inside(code) {
    if (code === marker) {
      effects.consume(code);
      return inside
    }
    const token = effects.exit('attentionSequence');
    const after = classifyCharacter(code);
    const open =
      !after || (after === 2 && before) || attentionMarkers.includes(code);
    const close =
      !before || (before === 2 && after) || attentionMarkers.includes(previous);
    token._open = Boolean(marker === 42 ? open : open && (before || !close));
    token._close = Boolean(marker === 42 ? close : close && (after || !open));
    return ok(code)
  }
}
function movePoint(point, offset) {
  point.column += offset;
  point.offset += offset;
  point._bufferIndex += offset;
}

const autolink = {
  name: 'autolink',
  tokenize: tokenizeAutolink
};
function tokenizeAutolink(effects, ok, nok) {
  let size = 0;
  return start
  function start(code) {
    effects.enter('autolink');
    effects.enter('autolinkMarker');
    effects.consume(code);
    effects.exit('autolinkMarker');
    effects.enter('autolinkProtocol');
    return open
  }
  function open(code) {
    if (asciiAlpha(code)) {
      effects.consume(code);
      return schemeOrEmailAtext
    }
    return emailAtext(code)
  }
  function schemeOrEmailAtext(code) {
    if (code === 43 || code === 45 || code === 46 || asciiAlphanumeric(code)) {
      size = 1;
      return schemeInsideOrEmailAtext(code)
    }
    return emailAtext(code)
  }
  function schemeInsideOrEmailAtext(code) {
    if (code === 58) {
      effects.consume(code);
      size = 0;
      return urlInside
    }
    if (
      (code === 43 || code === 45 || code === 46 || asciiAlphanumeric(code)) &&
      size++ < 32
    ) {
      effects.consume(code);
      return schemeInsideOrEmailAtext
    }
    size = 0;
    return emailAtext(code)
  }
  function urlInside(code) {
    if (code === 62) {
      effects.exit('autolinkProtocol');
      effects.enter('autolinkMarker');
      effects.consume(code);
      effects.exit('autolinkMarker');
      effects.exit('autolink');
      return ok
    }
    if (code === null || code === 32 || code === 60 || asciiControl(code)) {
      return nok(code)
    }
    effects.consume(code);
    return urlInside
  }
  function emailAtext(code) {
    if (code === 64) {
      effects.consume(code);
      return emailAtSignOrDot
    }
    if (asciiAtext(code)) {
      effects.consume(code);
      return emailAtext
    }
    return nok(code)
  }
  function emailAtSignOrDot(code) {
    return asciiAlphanumeric(code) ? emailLabel(code) : nok(code)
  }
  function emailLabel(code) {
    if (code === 46) {
      effects.consume(code);
      size = 0;
      return emailAtSignOrDot
    }
    if (code === 62) {
      effects.exit('autolinkProtocol').type = 'autolinkEmail';
      effects.enter('autolinkMarker');
      effects.consume(code);
      effects.exit('autolinkMarker');
      effects.exit('autolink');
      return ok
    }
    return emailValue(code)
  }
  function emailValue(code) {
    if ((code === 45 || asciiAlphanumeric(code)) && size++ < 63) {
      const next = code === 45 ? emailValue : emailLabel;
      effects.consume(code);
      return next
    }
    return nok(code)
  }
}

const blankLine = {
  tokenize: tokenizeBlankLine,
  partial: true
};
function tokenizeBlankLine(effects, ok, nok) {
  return start
  function start(code) {
    return markdownSpace(code)
      ? factorySpace(effects, after, 'linePrefix')(code)
      : after(code)
  }
  function after(code) {
    return code === null || markdownLineEnding(code) ? ok(code) : nok(code)
  }
}

const blockQuote = {
  name: 'blockQuote',
  tokenize: tokenizeBlockQuoteStart,
  continuation: {
    tokenize: tokenizeBlockQuoteContinuation
  },
  exit: exit$1
};
function tokenizeBlockQuoteStart(effects, ok, nok) {
  const self = this;
  return start
  function start(code) {
    if (code === 62) {
      const state = self.containerState;
      if (!state.open) {
        effects.enter('blockQuote', {
          _container: true
        });
        state.open = true;
      }
      effects.enter('blockQuotePrefix');
      effects.enter('blockQuoteMarker');
      effects.consume(code);
      effects.exit('blockQuoteMarker');
      return after
    }
    return nok(code)
  }
  function after(code) {
    if (markdownSpace(code)) {
      effects.enter('blockQuotePrefixWhitespace');
      effects.consume(code);
      effects.exit('blockQuotePrefixWhitespace');
      effects.exit('blockQuotePrefix');
      return ok
    }
    effects.exit('blockQuotePrefix');
    return ok(code)
  }
}
function tokenizeBlockQuoteContinuation(effects, ok, nok) {
  const self = this;
  return contStart
  function contStart(code) {
    if (markdownSpace(code)) {
      return factorySpace(
        effects,
        contBefore,
        'linePrefix',
        self.parser.constructs.disable.null.includes('codeIndented')
          ? undefined
          : 4
      )(code)
    }
    return contBefore(code)
  }
  function contBefore(code) {
    return effects.attempt(blockQuote, ok, nok)(code)
  }
}
function exit$1(effects) {
  effects.exit('blockQuote');
}

const characterEscape = {
  name: 'characterEscape',
  tokenize: tokenizeCharacterEscape
};
function tokenizeCharacterEscape(effects, ok, nok) {
  return start
  function start(code) {
    effects.enter('characterEscape');
    effects.enter('escapeMarker');
    effects.consume(code);
    effects.exit('escapeMarker');
    return inside
  }
  function inside(code) {
    if (asciiPunctuation(code)) {
      effects.enter('characterEscapeValue');
      effects.consume(code);
      effects.exit('characterEscapeValue');
      effects.exit('characterEscape');
      return ok
    }
    return nok(code)
  }
}

const characterReference = {
  name: 'characterReference',
  tokenize: tokenizeCharacterReference
};
function tokenizeCharacterReference(effects, ok, nok) {
  const self = this;
  let size = 0;
  let max;
  let test;
  return start
  function start(code) {
    effects.enter('characterReference');
    effects.enter('characterReferenceMarker');
    effects.consume(code);
    effects.exit('characterReferenceMarker');
    return open
  }
  function open(code) {
    if (code === 35) {
      effects.enter('characterReferenceMarkerNumeric');
      effects.consume(code);
      effects.exit('characterReferenceMarkerNumeric');
      return numeric
    }
    effects.enter('characterReferenceValue');
    max = 31;
    test = asciiAlphanumeric;
    return value(code)
  }
  function numeric(code) {
    if (code === 88 || code === 120) {
      effects.enter('characterReferenceMarkerHexadecimal');
      effects.consume(code);
      effects.exit('characterReferenceMarkerHexadecimal');
      effects.enter('characterReferenceValue');
      max = 6;
      test = asciiHexDigit;
      return value
    }
    effects.enter('characterReferenceValue');
    max = 7;
    test = asciiDigit;
    return value(code)
  }
  function value(code) {
    if (code === 59 && size) {
      const token = effects.exit('characterReferenceValue');
      if (
        test === asciiAlphanumeric &&
        !decodeNamedCharacterReference(self.sliceSerialize(token))
      ) {
        return nok(code)
      }
      effects.enter('characterReferenceMarker');
      effects.consume(code);
      effects.exit('characterReferenceMarker');
      effects.exit('characterReference');
      return ok
    }
    if (test(code) && size++ < max) {
      effects.consume(code);
      return value
    }
    return nok(code)
  }
}

const nonLazyContinuation = {
  tokenize: tokenizeNonLazyContinuation,
  partial: true
};
const codeFenced = {
  name: 'codeFenced',
  tokenize: tokenizeCodeFenced,
  concrete: true
};
function tokenizeCodeFenced(effects, ok, nok) {
  const self = this;
  const closeStart = {
    tokenize: tokenizeCloseStart,
    partial: true
  };
  let initialPrefix = 0;
  let sizeOpen = 0;
  let marker;
  return start
  function start(code) {
    return beforeSequenceOpen(code)
  }
  function beforeSequenceOpen(code) {
    const tail = self.events[self.events.length - 1];
    initialPrefix =
      tail && tail[1].type === 'linePrefix'
        ? tail[2].sliceSerialize(tail[1], true).length
        : 0;
    marker = code;
    effects.enter('codeFenced');
    effects.enter('codeFencedFence');
    effects.enter('codeFencedFenceSequence');
    return sequenceOpen(code)
  }
  function sequenceOpen(code) {
    if (code === marker) {
      sizeOpen++;
      effects.consume(code);
      return sequenceOpen
    }
    if (sizeOpen < 3) {
      return nok(code)
    }
    effects.exit('codeFencedFenceSequence');
    return markdownSpace(code)
      ? factorySpace(effects, infoBefore, 'whitespace')(code)
      : infoBefore(code)
  }
  function infoBefore(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('codeFencedFence');
      return self.interrupt
        ? ok(code)
        : effects.check(nonLazyContinuation, atNonLazyBreak, after)(code)
    }
    effects.enter('codeFencedFenceInfo');
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return info(code)
  }
  function info(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('chunkString');
      effects.exit('codeFencedFenceInfo');
      return infoBefore(code)
    }
    if (markdownSpace(code)) {
      effects.exit('chunkString');
      effects.exit('codeFencedFenceInfo');
      return factorySpace(effects, metaBefore, 'whitespace')(code)
    }
    if (code === 96 && code === marker) {
      return nok(code)
    }
    effects.consume(code);
    return info
  }
  function metaBefore(code) {
    if (code === null || markdownLineEnding(code)) {
      return infoBefore(code)
    }
    effects.enter('codeFencedFenceMeta');
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return meta(code)
  }
  function meta(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('chunkString');
      effects.exit('codeFencedFenceMeta');
      return infoBefore(code)
    }
    if (code === 96 && code === marker) {
      return nok(code)
    }
    effects.consume(code);
    return meta
  }
  function atNonLazyBreak(code) {
    return effects.attempt(closeStart, after, contentBefore)(code)
  }
  function contentBefore(code) {
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return contentStart
  }
  function contentStart(code) {
    return initialPrefix > 0 && markdownSpace(code)
      ? factorySpace(
          effects,
          beforeContentChunk,
          'linePrefix',
          initialPrefix + 1
        )(code)
      : beforeContentChunk(code)
  }
  function beforeContentChunk(code) {
    if (code === null || markdownLineEnding(code)) {
      return effects.check(nonLazyContinuation, atNonLazyBreak, after)(code)
    }
    effects.enter('codeFlowValue');
    return contentChunk(code)
  }
  function contentChunk(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('codeFlowValue');
      return beforeContentChunk(code)
    }
    effects.consume(code);
    return contentChunk
  }
  function after(code) {
    effects.exit('codeFenced');
    return ok(code)
  }
  function tokenizeCloseStart(effects, ok, nok) {
    let size = 0;
    return startBefore
    function startBefore(code) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return start
    }
    function start(code) {
      effects.enter('codeFencedFence');
      return markdownSpace(code)
        ? factorySpace(
            effects,
            beforeSequenceClose,
            'linePrefix',
            self.parser.constructs.disable.null.includes('codeIndented')
              ? undefined
              : 4
          )(code)
        : beforeSequenceClose(code)
    }
    function beforeSequenceClose(code) {
      if (code === marker) {
        effects.enter('codeFencedFenceSequence');
        return sequenceClose(code)
      }
      return nok(code)
    }
    function sequenceClose(code) {
      if (code === marker) {
        size++;
        effects.consume(code);
        return sequenceClose
      }
      if (size >= sizeOpen) {
        effects.exit('codeFencedFenceSequence');
        return markdownSpace(code)
          ? factorySpace(effects, sequenceCloseAfter, 'whitespace')(code)
          : sequenceCloseAfter(code)
      }
      return nok(code)
    }
    function sequenceCloseAfter(code) {
      if (code === null || markdownLineEnding(code)) {
        effects.exit('codeFencedFence');
        return ok(code)
      }
      return nok(code)
    }
  }
}
function tokenizeNonLazyContinuation(effects, ok, nok) {
  const self = this;
  return start
  function start(code) {
    if (code === null) {
      return nok(code)
    }
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return lineStart
  }
  function lineStart(code) {
    return self.parser.lazy[self.now().line] ? nok(code) : ok(code)
  }
}

const codeIndented = {
  name: 'codeIndented',
  tokenize: tokenizeCodeIndented
};
const furtherStart = {
  tokenize: tokenizeFurtherStart,
  partial: true
};
function tokenizeCodeIndented(effects, ok, nok) {
  const self = this;
  return start
  function start(code) {
    effects.enter('codeIndented');
    return factorySpace(effects, afterPrefix, 'linePrefix', 4 + 1)(code)
  }
  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'linePrefix' &&
      tail[2].sliceSerialize(tail[1], true).length >= 4
      ? atBreak(code)
      : nok(code)
  }
  function atBreak(code) {
    if (code === null) {
      return after(code)
    }
    if (markdownLineEnding(code)) {
      return effects.attempt(furtherStart, atBreak, after)(code)
    }
    effects.enter('codeFlowValue');
    return inside(code)
  }
  function inside(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('codeFlowValue');
      return atBreak(code)
    }
    effects.consume(code);
    return inside
  }
  function after(code) {
    effects.exit('codeIndented');
    return ok(code)
  }
}
function tokenizeFurtherStart(effects, ok, nok) {
  const self = this;
  return furtherStart
  function furtherStart(code) {
    if (self.parser.lazy[self.now().line]) {
      return nok(code)
    }
    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return furtherStart
    }
    return factorySpace(effects, afterPrefix, 'linePrefix', 4 + 1)(code)
  }
  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'linePrefix' &&
      tail[2].sliceSerialize(tail[1], true).length >= 4
      ? ok(code)
      : markdownLineEnding(code)
      ? furtherStart(code)
      : nok(code)
  }
}

const codeText = {
  name: 'codeText',
  tokenize: tokenizeCodeText,
  resolve: resolveCodeText,
  previous: previous$1
};
function resolveCodeText(events) {
  let tailExitIndex = events.length - 4;
  let headEnterIndex = 3;
  let index;
  let enter;
  if (
    (events[headEnterIndex][1].type === 'lineEnding' ||
      events[headEnterIndex][1].type === 'space') &&
    (events[tailExitIndex][1].type === 'lineEnding' ||
      events[tailExitIndex][1].type === 'space')
  ) {
    index = headEnterIndex;
    while (++index < tailExitIndex) {
      if (events[index][1].type === 'codeTextData') {
        events[headEnterIndex][1].type = 'codeTextPadding';
        events[tailExitIndex][1].type = 'codeTextPadding';
        headEnterIndex += 2;
        tailExitIndex -= 2;
        break
      }
    }
  }
  index = headEnterIndex - 1;
  tailExitIndex++;
  while (++index <= tailExitIndex) {
    if (enter === undefined) {
      if (index !== tailExitIndex && events[index][1].type !== 'lineEnding') {
        enter = index;
      }
    } else if (
      index === tailExitIndex ||
      events[index][1].type === 'lineEnding'
    ) {
      events[enter][1].type = 'codeTextData';
      if (index !== enter + 2) {
        events[enter][1].end = events[index - 1][1].end;
        events.splice(enter + 2, index - enter - 2);
        tailExitIndex -= index - enter - 2;
        index = enter + 2;
      }
      enter = undefined;
    }
  }
  return events
}
function previous$1(code) {
  return (
    code !== 96 ||
    this.events[this.events.length - 1][1].type === 'characterEscape'
  )
}
function tokenizeCodeText(effects, ok, nok) {
  let sizeOpen = 0;
  let size;
  let token;
  return start
  function start(code) {
    effects.enter('codeText');
    effects.enter('codeTextSequence');
    return sequenceOpen(code)
  }
  function sequenceOpen(code) {
    if (code === 96) {
      effects.consume(code);
      sizeOpen++;
      return sequenceOpen
    }
    effects.exit('codeTextSequence');
    return between(code)
  }
  function between(code) {
    if (code === null) {
      return nok(code)
    }
    if (code === 32) {
      effects.enter('space');
      effects.consume(code);
      effects.exit('space');
      return between
    }
    if (code === 96) {
      token = effects.enter('codeTextSequence');
      size = 0;
      return sequenceClose(code)
    }
    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return between
    }
    effects.enter('codeTextData');
    return data(code)
  }
  function data(code) {
    if (
      code === null ||
      code === 32 ||
      code === 96 ||
      markdownLineEnding(code)
    ) {
      effects.exit('codeTextData');
      return between(code)
    }
    effects.consume(code);
    return data
  }
  function sequenceClose(code) {
    if (code === 96) {
      effects.consume(code);
      size++;
      return sequenceClose
    }
    if (size === sizeOpen) {
      effects.exit('codeTextSequence');
      effects.exit('codeText');
      return ok(code)
    }
    token.type = 'codeTextData';
    return data(code)
  }
}

function subtokenize(events) {
  const jumps = {};
  let index = -1;
  let event;
  let lineIndex;
  let otherIndex;
  let otherEvent;
  let parameters;
  let subevents;
  let more;
  while (++index < events.length) {
    while (index in jumps) {
      index = jumps[index];
    }
    event = events[index];
    if (
      index &&
      event[1].type === 'chunkFlow' &&
      events[index - 1][1].type === 'listItemPrefix'
    ) {
      subevents = event[1]._tokenizer.events;
      otherIndex = 0;
      if (
        otherIndex < subevents.length &&
        subevents[otherIndex][1].type === 'lineEndingBlank'
      ) {
        otherIndex += 2;
      }
      if (
        otherIndex < subevents.length &&
        subevents[otherIndex][1].type === 'content'
      ) {
        while (++otherIndex < subevents.length) {
          if (subevents[otherIndex][1].type === 'content') {
            break
          }
          if (subevents[otherIndex][1].type === 'chunkText') {
            subevents[otherIndex][1]._isInFirstContentOfListItem = true;
            otherIndex++;
          }
        }
      }
    }
    if (event[0] === 'enter') {
      if (event[1].contentType) {
        Object.assign(jumps, subcontent(events, index));
        index = jumps[index];
        more = true;
      }
    }
    else if (event[1]._container) {
      otherIndex = index;
      lineIndex = undefined;
      while (otherIndex--) {
        otherEvent = events[otherIndex];
        if (
          otherEvent[1].type === 'lineEnding' ||
          otherEvent[1].type === 'lineEndingBlank'
        ) {
          if (otherEvent[0] === 'enter') {
            if (lineIndex) {
              events[lineIndex][1].type = 'lineEndingBlank';
            }
            otherEvent[1].type = 'lineEnding';
            lineIndex = otherIndex;
          }
        } else {
          break
        }
      }
      if (lineIndex) {
        event[1].end = Object.assign({}, events[lineIndex][1].start);
        parameters = events.slice(lineIndex, index);
        parameters.unshift(event);
        splice(events, lineIndex, index - lineIndex + 1, parameters);
      }
    }
  }
  return !more
}
function subcontent(events, eventIndex) {
  const token = events[eventIndex][1];
  const context = events[eventIndex][2];
  let startPosition = eventIndex - 1;
  const startPositions = [];
  const tokenizer =
    token._tokenizer || context.parser[token.contentType](token.start);
  const childEvents = tokenizer.events;
  const jumps = [];
  const gaps = {};
  let stream;
  let previous;
  let index = -1;
  let current = token;
  let adjust = 0;
  let start = 0;
  const breaks = [start];
  while (current) {
    while (events[++startPosition][1] !== current) {
    }
    startPositions.push(startPosition);
    if (!current._tokenizer) {
      stream = context.sliceStream(current);
      if (!current.next) {
        stream.push(null);
      }
      if (previous) {
        tokenizer.defineSkip(current.start);
      }
      if (current._isInFirstContentOfListItem) {
        tokenizer._gfmTasklistFirstContentOfListItem = true;
      }
      tokenizer.write(stream);
      if (current._isInFirstContentOfListItem) {
        tokenizer._gfmTasklistFirstContentOfListItem = undefined;
      }
    }
    previous = current;
    current = current.next;
  }
  current = token;
  while (++index < childEvents.length) {
    if (
      childEvents[index][0] === 'exit' &&
      childEvents[index - 1][0] === 'enter' &&
      childEvents[index][1].type === childEvents[index - 1][1].type &&
      childEvents[index][1].start.line !== childEvents[index][1].end.line
    ) {
      start = index + 1;
      breaks.push(start);
      current._tokenizer = undefined;
      current.previous = undefined;
      current = current.next;
    }
  }
  tokenizer.events = [];
  if (current) {
    current._tokenizer = undefined;
    current.previous = undefined;
  } else {
    breaks.pop();
  }
  index = breaks.length;
  while (index--) {
    const slice = childEvents.slice(breaks[index], breaks[index + 1]);
    const start = startPositions.pop();
    jumps.unshift([start, start + slice.length - 1]);
    splice(events, start, 2, slice);
  }
  index = -1;
  while (++index < jumps.length) {
    gaps[adjust + jumps[index][0]] = adjust + jumps[index][1];
    adjust += jumps[index][1] - jumps[index][0] - 1;
  }
  return gaps
}

const content = {
  tokenize: tokenizeContent,
  resolve: resolveContent
};
const continuationConstruct = {
  tokenize: tokenizeContinuation,
  partial: true
};
function resolveContent(events) {
  subtokenize(events);
  return events
}
function tokenizeContent(effects, ok) {
  let previous;
  return chunkStart
  function chunkStart(code) {
    effects.enter('content');
    previous = effects.enter('chunkContent', {
      contentType: 'content'
    });
    return chunkInside(code)
  }
  function chunkInside(code) {
    if (code === null) {
      return contentEnd(code)
    }
    if (markdownLineEnding(code)) {
      return effects.check(
        continuationConstruct,
        contentContinue,
        contentEnd
      )(code)
    }
    effects.consume(code);
    return chunkInside
  }
  function contentEnd(code) {
    effects.exit('chunkContent');
    effects.exit('content');
    return ok(code)
  }
  function contentContinue(code) {
    effects.consume(code);
    effects.exit('chunkContent');
    previous.next = effects.enter('chunkContent', {
      contentType: 'content',
      previous
    });
    previous = previous.next;
    return chunkInside
  }
}
function tokenizeContinuation(effects, ok, nok) {
  const self = this;
  return startLookahead
  function startLookahead(code) {
    effects.exit('chunkContent');
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return factorySpace(effects, prefixed, 'linePrefix')
  }
  function prefixed(code) {
    if (code === null || markdownLineEnding(code)) {
      return nok(code)
    }
    const tail = self.events[self.events.length - 1];
    if (
      !self.parser.constructs.disable.null.includes('codeIndented') &&
      tail &&
      tail[1].type === 'linePrefix' &&
      tail[2].sliceSerialize(tail[1], true).length >= 4
    ) {
      return ok(code)
    }
    return effects.interrupt(self.parser.constructs.flow, nok, ok)(code)
  }
}

function factoryDestination(
  effects,
  ok,
  nok,
  type,
  literalType,
  literalMarkerType,
  rawType,
  stringType,
  max
) {
  const limit = max || Number.POSITIVE_INFINITY;
  let balance = 0;
  return start
  function start(code) {
    if (code === 60) {
      effects.enter(type);
      effects.enter(literalType);
      effects.enter(literalMarkerType);
      effects.consume(code);
      effects.exit(literalMarkerType);
      return enclosedBefore
    }
    if (code === null || code === 32 || code === 41 || asciiControl(code)) {
      return nok(code)
    }
    effects.enter(type);
    effects.enter(rawType);
    effects.enter(stringType);
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return raw(code)
  }
  function enclosedBefore(code) {
    if (code === 62) {
      effects.enter(literalMarkerType);
      effects.consume(code);
      effects.exit(literalMarkerType);
      effects.exit(literalType);
      effects.exit(type);
      return ok
    }
    effects.enter(stringType);
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return enclosed(code)
  }
  function enclosed(code) {
    if (code === 62) {
      effects.exit('chunkString');
      effects.exit(stringType);
      return enclosedBefore(code)
    }
    if (code === null || code === 60 || markdownLineEnding(code)) {
      return nok(code)
    }
    effects.consume(code);
    return code === 92 ? enclosedEscape : enclosed
  }
  function enclosedEscape(code) {
    if (code === 60 || code === 62 || code === 92) {
      effects.consume(code);
      return enclosed
    }
    return enclosed(code)
  }
  function raw(code) {
    if (
      !balance &&
      (code === null || code === 41 || markdownLineEndingOrSpace(code))
    ) {
      effects.exit('chunkString');
      effects.exit(stringType);
      effects.exit(rawType);
      effects.exit(type);
      return ok(code)
    }
    if (balance < limit && code === 40) {
      effects.consume(code);
      balance++;
      return raw
    }
    if (code === 41) {
      effects.consume(code);
      balance--;
      return raw
    }
    if (code === null || code === 32 || code === 40 || asciiControl(code)) {
      return nok(code)
    }
    effects.consume(code);
    return code === 92 ? rawEscape : raw
  }
  function rawEscape(code) {
    if (code === 40 || code === 41 || code === 92) {
      effects.consume(code);
      return raw
    }
    return raw(code)
  }
}

function factoryLabel(effects, ok, nok, type, markerType, stringType) {
  const self = this;
  let size = 0;
  let seen;
  return start
  function start(code) {
    effects.enter(type);
    effects.enter(markerType);
    effects.consume(code);
    effects.exit(markerType);
    effects.enter(stringType);
    return atBreak
  }
  function atBreak(code) {
    if (
      size > 999 ||
      code === null ||
      code === 91 ||
      (code === 93 && !seen) ||
      (code === 94 &&
        !size &&
        '_hiddenFootnoteSupport' in self.parser.constructs)
    ) {
      return nok(code)
    }
    if (code === 93) {
      effects.exit(stringType);
      effects.enter(markerType);
      effects.consume(code);
      effects.exit(markerType);
      effects.exit(type);
      return ok
    }
    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return atBreak
    }
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return labelInside(code)
  }
  function labelInside(code) {
    if (
      code === null ||
      code === 91 ||
      code === 93 ||
      markdownLineEnding(code) ||
      size++ > 999
    ) {
      effects.exit('chunkString');
      return atBreak(code)
    }
    effects.consume(code);
    if (!seen) seen = !markdownSpace(code);
    return code === 92 ? labelEscape : labelInside
  }
  function labelEscape(code) {
    if (code === 91 || code === 92 || code === 93) {
      effects.consume(code);
      size++;
      return labelInside
    }
    return labelInside(code)
  }
}

function factoryTitle(effects, ok, nok, type, markerType, stringType) {
  let marker;
  return start
  function start(code) {
    if (code === 34 || code === 39 || code === 40) {
      effects.enter(type);
      effects.enter(markerType);
      effects.consume(code);
      effects.exit(markerType);
      marker = code === 40 ? 41 : code;
      return begin
    }
    return nok(code)
  }
  function begin(code) {
    if (code === marker) {
      effects.enter(markerType);
      effects.consume(code);
      effects.exit(markerType);
      effects.exit(type);
      return ok
    }
    effects.enter(stringType);
    return atBreak(code)
  }
  function atBreak(code) {
    if (code === marker) {
      effects.exit(stringType);
      return begin(marker)
    }
    if (code === null) {
      return nok(code)
    }
    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return factorySpace(effects, atBreak, 'linePrefix')
    }
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return inside(code)
  }
  function inside(code) {
    if (code === marker || code === null || markdownLineEnding(code)) {
      effects.exit('chunkString');
      return atBreak(code)
    }
    effects.consume(code);
    return code === 92 ? escape : inside
  }
  function escape(code) {
    if (code === marker || code === 92) {
      effects.consume(code);
      return inside
    }
    return inside(code)
  }
}

function factoryWhitespace(effects, ok) {
  let seen;
  return start
  function start(code) {
    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      seen = true;
      return start
    }
    if (markdownSpace(code)) {
      return factorySpace(
        effects,
        start,
        seen ? 'linePrefix' : 'lineSuffix'
      )(code)
    }
    return ok(code)
  }
}

const definition$1 = {
  name: 'definition',
  tokenize: tokenizeDefinition
};
const titleBefore = {
  tokenize: tokenizeTitleBefore,
  partial: true
};
function tokenizeDefinition(effects, ok, nok) {
  const self = this;
  let identifier;
  return start
  function start(code) {
    effects.enter('definition');
    return before(code)
  }
  function before(code) {
    return factoryLabel.call(
      self,
      effects,
      labelAfter,
      nok,
      'definitionLabel',
      'definitionLabelMarker',
      'definitionLabelString'
    )(code)
  }
  function labelAfter(code) {
    identifier = normalizeIdentifier$1(
      self.sliceSerialize(self.events[self.events.length - 1][1]).slice(1, -1)
    );
    if (code === 58) {
      effects.enter('definitionMarker');
      effects.consume(code);
      effects.exit('definitionMarker');
      return markerAfter
    }
    return nok(code)
  }
  function markerAfter(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, destinationBefore)(code)
      : destinationBefore(code)
  }
  function destinationBefore(code) {
    return factoryDestination(
      effects,
      destinationAfter,
      nok,
      'definitionDestination',
      'definitionDestinationLiteral',
      'definitionDestinationLiteralMarker',
      'definitionDestinationRaw',
      'definitionDestinationString'
    )(code)
  }
  function destinationAfter(code) {
    return effects.attempt(titleBefore, after, after)(code)
  }
  function after(code) {
    return markdownSpace(code)
      ? factorySpace(effects, afterWhitespace, 'whitespace')(code)
      : afterWhitespace(code)
  }
  function afterWhitespace(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('definition');
      self.parser.defined.push(identifier);
      return ok(code)
    }
    return nok(code)
  }
}
function tokenizeTitleBefore(effects, ok, nok) {
  return titleBefore
  function titleBefore(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, beforeMarker)(code)
      : nok(code)
  }
  function beforeMarker(code) {
    return factoryTitle(
      effects,
      titleAfter,
      nok,
      'definitionTitle',
      'definitionTitleMarker',
      'definitionTitleString'
    )(code)
  }
  function titleAfter(code) {
    return markdownSpace(code)
      ? factorySpace(effects, titleAfterOptionalWhitespace, 'whitespace')(code)
      : titleAfterOptionalWhitespace(code)
  }
  function titleAfterOptionalWhitespace(code) {
    return code === null || markdownLineEnding(code) ? ok(code) : nok(code)
  }
}

const hardBreakEscape = {
  name: 'hardBreakEscape',
  tokenize: tokenizeHardBreakEscape
};
function tokenizeHardBreakEscape(effects, ok, nok) {
  return start
  function start(code) {
    effects.enter('hardBreakEscape');
    effects.consume(code);
    return after
  }
  function after(code) {
    if (markdownLineEnding(code)) {
      effects.exit('hardBreakEscape');
      return ok(code)
    }
    return nok(code)
  }
}

const headingAtx = {
  name: 'headingAtx',
  tokenize: tokenizeHeadingAtx,
  resolve: resolveHeadingAtx
};
function resolveHeadingAtx(events, context) {
  let contentEnd = events.length - 2;
  let contentStart = 3;
  let content;
  let text;
  if (events[contentStart][1].type === 'whitespace') {
    contentStart += 2;
  }
  if (
    contentEnd - 2 > contentStart &&
    events[contentEnd][1].type === 'whitespace'
  ) {
    contentEnd -= 2;
  }
  if (
    events[contentEnd][1].type === 'atxHeadingSequence' &&
    (contentStart === contentEnd - 1 ||
      (contentEnd - 4 > contentStart &&
        events[contentEnd - 2][1].type === 'whitespace'))
  ) {
    contentEnd -= contentStart + 1 === contentEnd ? 2 : 4;
  }
  if (contentEnd > contentStart) {
    content = {
      type: 'atxHeadingText',
      start: events[contentStart][1].start,
      end: events[contentEnd][1].end
    };
    text = {
      type: 'chunkText',
      start: events[contentStart][1].start,
      end: events[contentEnd][1].end,
      contentType: 'text'
    };
    splice(events, contentStart, contentEnd - contentStart + 1, [
      ['enter', content, context],
      ['enter', text, context],
      ['exit', text, context],
      ['exit', content, context]
    ]);
  }
  return events
}
function tokenizeHeadingAtx(effects, ok, nok) {
  let size = 0;
  return start
  function start(code) {
    effects.enter('atxHeading');
    return before(code)
  }
  function before(code) {
    effects.enter('atxHeadingSequence');
    return sequenceOpen(code)
  }
  function sequenceOpen(code) {
    if (code === 35 && size++ < 6) {
      effects.consume(code);
      return sequenceOpen
    }
    if (code === null || markdownLineEndingOrSpace(code)) {
      effects.exit('atxHeadingSequence');
      return atBreak(code)
    }
    return nok(code)
  }
  function atBreak(code) {
    if (code === 35) {
      effects.enter('atxHeadingSequence');
      return sequenceFurther(code)
    }
    if (code === null || markdownLineEnding(code)) {
      effects.exit('atxHeading');
      return ok(code)
    }
    if (markdownSpace(code)) {
      return factorySpace(effects, atBreak, 'whitespace')(code)
    }
    effects.enter('atxHeadingText');
    return data(code)
  }
  function sequenceFurther(code) {
    if (code === 35) {
      effects.consume(code);
      return sequenceFurther
    }
    effects.exit('atxHeadingSequence');
    return atBreak(code)
  }
  function data(code) {
    if (code === null || code === 35 || markdownLineEndingOrSpace(code)) {
      effects.exit('atxHeadingText');
      return atBreak(code)
    }
    effects.consume(code);
    return data
  }
}

const htmlBlockNames = [
  'address',
  'article',
  'aside',
  'base',
  'basefont',
  'blockquote',
  'body',
  'caption',
  'center',
  'col',
  'colgroup',
  'dd',
  'details',
  'dialog',
  'dir',
  'div',
  'dl',
  'dt',
  'fieldset',
  'figcaption',
  'figure',
  'footer',
  'form',
  'frame',
  'frameset',
  'h1',
  'h2',
  'h3',
  'h4',
  'h5',
  'h6',
  'head',
  'header',
  'hr',
  'html',
  'iframe',
  'legend',
  'li',
  'link',
  'main',
  'menu',
  'menuitem',
  'nav',
  'noframes',
  'ol',
  'optgroup',
  'option',
  'p',
  'param',
  'search',
  'section',
  'summary',
  'table',
  'tbody',
  'td',
  'tfoot',
  'th',
  'thead',
  'title',
  'tr',
  'track',
  'ul'
];
const htmlRawNames = ['pre', 'script', 'style', 'textarea'];

const htmlFlow = {
  name: 'htmlFlow',
  tokenize: tokenizeHtmlFlow,
  resolveTo: resolveToHtmlFlow,
  concrete: true
};
const blankLineBefore = {
  tokenize: tokenizeBlankLineBefore,
  partial: true
};
const nonLazyContinuationStart = {
  tokenize: tokenizeNonLazyContinuationStart,
  partial: true
};
function resolveToHtmlFlow(events) {
  let index = events.length;
  while (index--) {
    if (events[index][0] === 'enter' && events[index][1].type === 'htmlFlow') {
      break
    }
  }
  if (index > 1 && events[index - 2][1].type === 'linePrefix') {
    events[index][1].start = events[index - 2][1].start;
    events[index + 1][1].start = events[index - 2][1].start;
    events.splice(index - 2, 2);
  }
  return events
}
function tokenizeHtmlFlow(effects, ok, nok) {
  const self = this;
  let marker;
  let closingTag;
  let buffer;
  let index;
  let markerB;
  return start
  function start(code) {
    return before(code)
  }
  function before(code) {
    effects.enter('htmlFlow');
    effects.enter('htmlFlowData');
    effects.consume(code);
    return open
  }
  function open(code) {
    if (code === 33) {
      effects.consume(code);
      return declarationOpen
    }
    if (code === 47) {
      effects.consume(code);
      closingTag = true;
      return tagCloseStart
    }
    if (code === 63) {
      effects.consume(code);
      marker = 3;
      return self.interrupt ? ok : continuationDeclarationInside
    }
    if (asciiAlpha(code)) {
      effects.consume(code);
      buffer = String.fromCharCode(code);
      return tagName
    }
    return nok(code)
  }
  function declarationOpen(code) {
    if (code === 45) {
      effects.consume(code);
      marker = 2;
      return commentOpenInside
    }
    if (code === 91) {
      effects.consume(code);
      marker = 5;
      index = 0;
      return cdataOpenInside
    }
    if (asciiAlpha(code)) {
      effects.consume(code);
      marker = 4;
      return self.interrupt ? ok : continuationDeclarationInside
    }
    return nok(code)
  }
  function commentOpenInside(code) {
    if (code === 45) {
      effects.consume(code);
      return self.interrupt ? ok : continuationDeclarationInside
    }
    return nok(code)
  }
  function cdataOpenInside(code) {
    const value = 'CDATA[';
    if (code === value.charCodeAt(index++)) {
      effects.consume(code);
      if (index === value.length) {
        return self.interrupt ? ok : continuation
      }
      return cdataOpenInside
    }
    return nok(code)
  }
  function tagCloseStart(code) {
    if (asciiAlpha(code)) {
      effects.consume(code);
      buffer = String.fromCharCode(code);
      return tagName
    }
    return nok(code)
  }
  function tagName(code) {
    if (
      code === null ||
      code === 47 ||
      code === 62 ||
      markdownLineEndingOrSpace(code)
    ) {
      const slash = code === 47;
      const name = buffer.toLowerCase();
      if (!slash && !closingTag && htmlRawNames.includes(name)) {
        marker = 1;
        return self.interrupt ? ok(code) : continuation(code)
      }
      if (htmlBlockNames.includes(buffer.toLowerCase())) {
        marker = 6;
        if (slash) {
          effects.consume(code);
          return basicSelfClosing
        }
        return self.interrupt ? ok(code) : continuation(code)
      }
      marker = 7;
      return self.interrupt && !self.parser.lazy[self.now().line]
        ? nok(code)
        : closingTag
        ? completeClosingTagAfter(code)
        : completeAttributeNameBefore(code)
    }
    if (code === 45 || asciiAlphanumeric(code)) {
      effects.consume(code);
      buffer += String.fromCharCode(code);
      return tagName
    }
    return nok(code)
  }
  function basicSelfClosing(code) {
    if (code === 62) {
      effects.consume(code);
      return self.interrupt ? ok : continuation
    }
    return nok(code)
  }
  function completeClosingTagAfter(code) {
    if (markdownSpace(code)) {
      effects.consume(code);
      return completeClosingTagAfter
    }
    return completeEnd(code)
  }
  function completeAttributeNameBefore(code) {
    if (code === 47) {
      effects.consume(code);
      return completeEnd
    }
    if (code === 58 || code === 95 || asciiAlpha(code)) {
      effects.consume(code);
      return completeAttributeName
    }
    if (markdownSpace(code)) {
      effects.consume(code);
      return completeAttributeNameBefore
    }
    return completeEnd(code)
  }
  function completeAttributeName(code) {
    if (
      code === 45 ||
      code === 46 ||
      code === 58 ||
      code === 95 ||
      asciiAlphanumeric(code)
    ) {
      effects.consume(code);
      return completeAttributeName
    }
    return completeAttributeNameAfter(code)
  }
  function completeAttributeNameAfter(code) {
    if (code === 61) {
      effects.consume(code);
      return completeAttributeValueBefore
    }
    if (markdownSpace(code)) {
      effects.consume(code);
      return completeAttributeNameAfter
    }
    return completeAttributeNameBefore(code)
  }
  function completeAttributeValueBefore(code) {
    if (
      code === null ||
      code === 60 ||
      code === 61 ||
      code === 62 ||
      code === 96
    ) {
      return nok(code)
    }
    if (code === 34 || code === 39) {
      effects.consume(code);
      markerB = code;
      return completeAttributeValueQuoted
    }
    if (markdownSpace(code)) {
      effects.consume(code);
      return completeAttributeValueBefore
    }
    return completeAttributeValueUnquoted(code)
  }
  function completeAttributeValueQuoted(code) {
    if (code === markerB) {
      effects.consume(code);
      markerB = null;
      return completeAttributeValueQuotedAfter
    }
    if (code === null || markdownLineEnding(code)) {
      return nok(code)
    }
    effects.consume(code);
    return completeAttributeValueQuoted
  }
  function completeAttributeValueUnquoted(code) {
    if (
      code === null ||
      code === 34 ||
      code === 39 ||
      code === 47 ||
      code === 60 ||
      code === 61 ||
      code === 62 ||
      code === 96 ||
      markdownLineEndingOrSpace(code)
    ) {
      return completeAttributeNameAfter(code)
    }
    effects.consume(code);
    return completeAttributeValueUnquoted
  }
  function completeAttributeValueQuotedAfter(code) {
    if (code === 47 || code === 62 || markdownSpace(code)) {
      return completeAttributeNameBefore(code)
    }
    return nok(code)
  }
  function completeEnd(code) {
    if (code === 62) {
      effects.consume(code);
      return completeAfter
    }
    return nok(code)
  }
  function completeAfter(code) {
    if (code === null || markdownLineEnding(code)) {
      return continuation(code)
    }
    if (markdownSpace(code)) {
      effects.consume(code);
      return completeAfter
    }
    return nok(code)
  }
  function continuation(code) {
    if (code === 45 && marker === 2) {
      effects.consume(code);
      return continuationCommentInside
    }
    if (code === 60 && marker === 1) {
      effects.consume(code);
      return continuationRawTagOpen
    }
    if (code === 62 && marker === 4) {
      effects.consume(code);
      return continuationClose
    }
    if (code === 63 && marker === 3) {
      effects.consume(code);
      return continuationDeclarationInside
    }
    if (code === 93 && marker === 5) {
      effects.consume(code);
      return continuationCdataInside
    }
    if (markdownLineEnding(code) && (marker === 6 || marker === 7)) {
      effects.exit('htmlFlowData');
      return effects.check(
        blankLineBefore,
        continuationAfter,
        continuationStart
      )(code)
    }
    if (code === null || markdownLineEnding(code)) {
      effects.exit('htmlFlowData');
      return continuationStart(code)
    }
    effects.consume(code);
    return continuation
  }
  function continuationStart(code) {
    return effects.check(
      nonLazyContinuationStart,
      continuationStartNonLazy,
      continuationAfter
    )(code)
  }
  function continuationStartNonLazy(code) {
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return continuationBefore
  }
  function continuationBefore(code) {
    if (code === null || markdownLineEnding(code)) {
      return continuationStart(code)
    }
    effects.enter('htmlFlowData');
    return continuation(code)
  }
  function continuationCommentInside(code) {
    if (code === 45) {
      effects.consume(code);
      return continuationDeclarationInside
    }
    return continuation(code)
  }
  function continuationRawTagOpen(code) {
    if (code === 47) {
      effects.consume(code);
      buffer = '';
      return continuationRawEndTag
    }
    return continuation(code)
  }
  function continuationRawEndTag(code) {
    if (code === 62) {
      const name = buffer.toLowerCase();
      if (htmlRawNames.includes(name)) {
        effects.consume(code);
        return continuationClose
      }
      return continuation(code)
    }
    if (asciiAlpha(code) && buffer.length < 8) {
      effects.consume(code);
      buffer += String.fromCharCode(code);
      return continuationRawEndTag
    }
    return continuation(code)
  }
  function continuationCdataInside(code) {
    if (code === 93) {
      effects.consume(code);
      return continuationDeclarationInside
    }
    return continuation(code)
  }
  function continuationDeclarationInside(code) {
    if (code === 62) {
      effects.consume(code);
      return continuationClose
    }
    if (code === 45 && marker === 2) {
      effects.consume(code);
      return continuationDeclarationInside
    }
    return continuation(code)
  }
  function continuationClose(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('htmlFlowData');
      return continuationAfter(code)
    }
    effects.consume(code);
    return continuationClose
  }
  function continuationAfter(code) {
    effects.exit('htmlFlow');
    return ok(code)
  }
}
function tokenizeNonLazyContinuationStart(effects, ok, nok) {
  const self = this;
  return start
  function start(code) {
    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return after
    }
    return nok(code)
  }
  function after(code) {
    return self.parser.lazy[self.now().line] ? nok(code) : ok(code)
  }
}
function tokenizeBlankLineBefore(effects, ok, nok) {
  return start
  function start(code) {
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return effects.attempt(blankLine, ok, nok)
  }
}

const htmlText = {
  name: 'htmlText',
  tokenize: tokenizeHtmlText
};
function tokenizeHtmlText(effects, ok, nok) {
  const self = this;
  let marker;
  let index;
  let returnState;
  return start
  function start(code) {
    effects.enter('htmlText');
    effects.enter('htmlTextData');
    effects.consume(code);
    return open
  }
  function open(code) {
    if (code === 33) {
      effects.consume(code);
      return declarationOpen
    }
    if (code === 47) {
      effects.consume(code);
      return tagCloseStart
    }
    if (code === 63) {
      effects.consume(code);
      return instruction
    }
    if (asciiAlpha(code)) {
      effects.consume(code);
      return tagOpen
    }
    return nok(code)
  }
  function declarationOpen(code) {
    if (code === 45) {
      effects.consume(code);
      return commentOpenInside
    }
    if (code === 91) {
      effects.consume(code);
      index = 0;
      return cdataOpenInside
    }
    if (asciiAlpha(code)) {
      effects.consume(code);
      return declaration
    }
    return nok(code)
  }
  function commentOpenInside(code) {
    if (code === 45) {
      effects.consume(code);
      return commentEnd
    }
    return nok(code)
  }
  function comment(code) {
    if (code === null) {
      return nok(code)
    }
    if (code === 45) {
      effects.consume(code);
      return commentClose
    }
    if (markdownLineEnding(code)) {
      returnState = comment;
      return lineEndingBefore(code)
    }
    effects.consume(code);
    return comment
  }
  function commentClose(code) {
    if (code === 45) {
      effects.consume(code);
      return commentEnd
    }
    return comment(code)
  }
  function commentEnd(code) {
    return code === 62
      ? end(code)
      : code === 45
      ? commentClose(code)
      : comment(code)
  }
  function cdataOpenInside(code) {
    const value = 'CDATA[';
    if (code === value.charCodeAt(index++)) {
      effects.consume(code);
      return index === value.length ? cdata : cdataOpenInside
    }
    return nok(code)
  }
  function cdata(code) {
    if (code === null) {
      return nok(code)
    }
    if (code === 93) {
      effects.consume(code);
      return cdataClose
    }
    if (markdownLineEnding(code)) {
      returnState = cdata;
      return lineEndingBefore(code)
    }
    effects.consume(code);
    return cdata
  }
  function cdataClose(code) {
    if (code === 93) {
      effects.consume(code);
      return cdataEnd
    }
    return cdata(code)
  }
  function cdataEnd(code) {
    if (code === 62) {
      return end(code)
    }
    if (code === 93) {
      effects.consume(code);
      return cdataEnd
    }
    return cdata(code)
  }
  function declaration(code) {
    if (code === null || code === 62) {
      return end(code)
    }
    if (markdownLineEnding(code)) {
      returnState = declaration;
      return lineEndingBefore(code)
    }
    effects.consume(code);
    return declaration
  }
  function instruction(code) {
    if (code === null) {
      return nok(code)
    }
    if (code === 63) {
      effects.consume(code);
      return instructionClose
    }
    if (markdownLineEnding(code)) {
      returnState = instruction;
      return lineEndingBefore(code)
    }
    effects.consume(code);
    return instruction
  }
  function instructionClose(code) {
    return code === 62 ? end(code) : instruction(code)
  }
  function tagCloseStart(code) {
    if (asciiAlpha(code)) {
      effects.consume(code);
      return tagClose
    }
    return nok(code)
  }
  function tagClose(code) {
    if (code === 45 || asciiAlphanumeric(code)) {
      effects.consume(code);
      return tagClose
    }
    return tagCloseBetween(code)
  }
  function tagCloseBetween(code) {
    if (markdownLineEnding(code)) {
      returnState = tagCloseBetween;
      return lineEndingBefore(code)
    }
    if (markdownSpace(code)) {
      effects.consume(code);
      return tagCloseBetween
    }
    return end(code)
  }
  function tagOpen(code) {
    if (code === 45 || asciiAlphanumeric(code)) {
      effects.consume(code);
      return tagOpen
    }
    if (code === 47 || code === 62 || markdownLineEndingOrSpace(code)) {
      return tagOpenBetween(code)
    }
    return nok(code)
  }
  function tagOpenBetween(code) {
    if (code === 47) {
      effects.consume(code);
      return end
    }
    if (code === 58 || code === 95 || asciiAlpha(code)) {
      effects.consume(code);
      return tagOpenAttributeName
    }
    if (markdownLineEnding(code)) {
      returnState = tagOpenBetween;
      return lineEndingBefore(code)
    }
    if (markdownSpace(code)) {
      effects.consume(code);
      return tagOpenBetween
    }
    return end(code)
  }
  function tagOpenAttributeName(code) {
    if (
      code === 45 ||
      code === 46 ||
      code === 58 ||
      code === 95 ||
      asciiAlphanumeric(code)
    ) {
      effects.consume(code);
      return tagOpenAttributeName
    }
    return tagOpenAttributeNameAfter(code)
  }
  function tagOpenAttributeNameAfter(code) {
    if (code === 61) {
      effects.consume(code);
      return tagOpenAttributeValueBefore
    }
    if (markdownLineEnding(code)) {
      returnState = tagOpenAttributeNameAfter;
      return lineEndingBefore(code)
    }
    if (markdownSpace(code)) {
      effects.consume(code);
      return tagOpenAttributeNameAfter
    }
    return tagOpenBetween(code)
  }
  function tagOpenAttributeValueBefore(code) {
    if (
      code === null ||
      code === 60 ||
      code === 61 ||
      code === 62 ||
      code === 96
    ) {
      return nok(code)
    }
    if (code === 34 || code === 39) {
      effects.consume(code);
      marker = code;
      return tagOpenAttributeValueQuoted
    }
    if (markdownLineEnding(code)) {
      returnState = tagOpenAttributeValueBefore;
      return lineEndingBefore(code)
    }
    if (markdownSpace(code)) {
      effects.consume(code);
      return tagOpenAttributeValueBefore
    }
    effects.consume(code);
    return tagOpenAttributeValueUnquoted
  }
  function tagOpenAttributeValueQuoted(code) {
    if (code === marker) {
      effects.consume(code);
      marker = undefined;
      return tagOpenAttributeValueQuotedAfter
    }
    if (code === null) {
      return nok(code)
    }
    if (markdownLineEnding(code)) {
      returnState = tagOpenAttributeValueQuoted;
      return lineEndingBefore(code)
    }
    effects.consume(code);
    return tagOpenAttributeValueQuoted
  }
  function tagOpenAttributeValueUnquoted(code) {
    if (
      code === null ||
      code === 34 ||
      code === 39 ||
      code === 60 ||
      code === 61 ||
      code === 96
    ) {
      return nok(code)
    }
    if (code === 47 || code === 62 || markdownLineEndingOrSpace(code)) {
      return tagOpenBetween(code)
    }
    effects.consume(code);
    return tagOpenAttributeValueUnquoted
  }
  function tagOpenAttributeValueQuotedAfter(code) {
    if (code === 47 || code === 62 || markdownLineEndingOrSpace(code)) {
      return tagOpenBetween(code)
    }
    return nok(code)
  }
  function end(code) {
    if (code === 62) {
      effects.consume(code);
      effects.exit('htmlTextData');
      effects.exit('htmlText');
      return ok
    }
    return nok(code)
  }
  function lineEndingBefore(code) {
    effects.exit('htmlTextData');
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return lineEndingAfter
  }
  function lineEndingAfter(code) {
    return markdownSpace(code)
      ? factorySpace(
          effects,
          lineEndingAfterPrefix,
          'linePrefix',
          self.parser.constructs.disable.null.includes('codeIndented')
            ? undefined
            : 4
        )(code)
      : lineEndingAfterPrefix(code)
  }
  function lineEndingAfterPrefix(code) {
    effects.enter('htmlTextData');
    return returnState(code)
  }
}

const labelEnd = {
  name: 'labelEnd',
  tokenize: tokenizeLabelEnd,
  resolveTo: resolveToLabelEnd,
  resolveAll: resolveAllLabelEnd
};
const resourceConstruct = {
  tokenize: tokenizeResource
};
const referenceFullConstruct = {
  tokenize: tokenizeReferenceFull
};
const referenceCollapsedConstruct = {
  tokenize: tokenizeReferenceCollapsed
};
function resolveAllLabelEnd(events) {
  let index = -1;
  while (++index < events.length) {
    const token = events[index][1];
    if (
      token.type === 'labelImage' ||
      token.type === 'labelLink' ||
      token.type === 'labelEnd'
    ) {
      events.splice(index + 1, token.type === 'labelImage' ? 4 : 2);
      token.type = 'data';
      index++;
    }
  }
  return events
}
function resolveToLabelEnd(events, context) {
  let index = events.length;
  let offset = 0;
  let token;
  let open;
  let close;
  let media;
  while (index--) {
    token = events[index][1];
    if (open) {
      if (
        token.type === 'link' ||
        (token.type === 'labelLink' && token._inactive)
      ) {
        break
      }
      if (events[index][0] === 'enter' && token.type === 'labelLink') {
        token._inactive = true;
      }
    } else if (close) {
      if (
        events[index][0] === 'enter' &&
        (token.type === 'labelImage' || token.type === 'labelLink') &&
        !token._balanced
      ) {
        open = index;
        if (token.type !== 'labelLink') {
          offset = 2;
          break
        }
      }
    } else if (token.type === 'labelEnd') {
      close = index;
    }
  }
  const group = {
    type: events[open][1].type === 'labelLink' ? 'link' : 'image',
    start: Object.assign({}, events[open][1].start),
    end: Object.assign({}, events[events.length - 1][1].end)
  };
  const label = {
    type: 'label',
    start: Object.assign({}, events[open][1].start),
    end: Object.assign({}, events[close][1].end)
  };
  const text = {
    type: 'labelText',
    start: Object.assign({}, events[open + offset + 2][1].end),
    end: Object.assign({}, events[close - 2][1].start)
  };
  media = [
    ['enter', group, context],
    ['enter', label, context]
  ];
  media = push(media, events.slice(open + 1, open + offset + 3));
  media = push(media, [['enter', text, context]]);
  media = push(
    media,
    resolveAll(
      context.parser.constructs.insideSpan.null,
      events.slice(open + offset + 4, close - 3),
      context
    )
  );
  media = push(media, [
    ['exit', text, context],
    events[close - 2],
    events[close - 1],
    ['exit', label, context]
  ]);
  media = push(media, events.slice(close + 1));
  media = push(media, [['exit', group, context]]);
  splice(events, open, events.length, media);
  return events
}
function tokenizeLabelEnd(effects, ok, nok) {
  const self = this;
  let index = self.events.length;
  let labelStart;
  let defined;
  while (index--) {
    if (
      (self.events[index][1].type === 'labelImage' ||
        self.events[index][1].type === 'labelLink') &&
      !self.events[index][1]._balanced
    ) {
      labelStart = self.events[index][1];
      break
    }
  }
  return start
  function start(code) {
    if (!labelStart) {
      return nok(code)
    }
    if (labelStart._inactive) {
      return labelEndNok(code)
    }
    defined = self.parser.defined.includes(
      normalizeIdentifier$1(
        self.sliceSerialize({
          start: labelStart.end,
          end: self.now()
        })
      )
    );
    effects.enter('labelEnd');
    effects.enter('labelMarker');
    effects.consume(code);
    effects.exit('labelMarker');
    effects.exit('labelEnd');
    return after
  }
  function after(code) {
    if (code === 40) {
      return effects.attempt(
        resourceConstruct,
        labelEndOk,
        defined ? labelEndOk : labelEndNok
      )(code)
    }
    if (code === 91) {
      return effects.attempt(
        referenceFullConstruct,
        labelEndOk,
        defined ? referenceNotFull : labelEndNok
      )(code)
    }
    return defined ? labelEndOk(code) : labelEndNok(code)
  }
  function referenceNotFull(code) {
    return effects.attempt(
      referenceCollapsedConstruct,
      labelEndOk,
      labelEndNok
    )(code)
  }
  function labelEndOk(code) {
    return ok(code)
  }
  function labelEndNok(code) {
    labelStart._balanced = true;
    return nok(code)
  }
}
function tokenizeResource(effects, ok, nok) {
  return resourceStart
  function resourceStart(code) {
    effects.enter('resource');
    effects.enter('resourceMarker');
    effects.consume(code);
    effects.exit('resourceMarker');
    return resourceBefore
  }
  function resourceBefore(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, resourceOpen)(code)
      : resourceOpen(code)
  }
  function resourceOpen(code) {
    if (code === 41) {
      return resourceEnd(code)
    }
    return factoryDestination(
      effects,
      resourceDestinationAfter,
      resourceDestinationMissing,
      'resourceDestination',
      'resourceDestinationLiteral',
      'resourceDestinationLiteralMarker',
      'resourceDestinationRaw',
      'resourceDestinationString',
      32
    )(code)
  }
  function resourceDestinationAfter(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, resourceBetween)(code)
      : resourceEnd(code)
  }
  function resourceDestinationMissing(code) {
    return nok(code)
  }
  function resourceBetween(code) {
    if (code === 34 || code === 39 || code === 40) {
      return factoryTitle(
        effects,
        resourceTitleAfter,
        nok,
        'resourceTitle',
        'resourceTitleMarker',
        'resourceTitleString'
      )(code)
    }
    return resourceEnd(code)
  }
  function resourceTitleAfter(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, resourceEnd)(code)
      : resourceEnd(code)
  }
  function resourceEnd(code) {
    if (code === 41) {
      effects.enter('resourceMarker');
      effects.consume(code);
      effects.exit('resourceMarker');
      effects.exit('resource');
      return ok
    }
    return nok(code)
  }
}
function tokenizeReferenceFull(effects, ok, nok) {
  const self = this;
  return referenceFull
  function referenceFull(code) {
    return factoryLabel.call(
      self,
      effects,
      referenceFullAfter,
      referenceFullMissing,
      'reference',
      'referenceMarker',
      'referenceString'
    )(code)
  }
  function referenceFullAfter(code) {
    return self.parser.defined.includes(
      normalizeIdentifier$1(
        self.sliceSerialize(self.events[self.events.length - 1][1]).slice(1, -1)
      )
    )
      ? ok(code)
      : nok(code)
  }
  function referenceFullMissing(code) {
    return nok(code)
  }
}
function tokenizeReferenceCollapsed(effects, ok, nok) {
  return referenceCollapsedStart
  function referenceCollapsedStart(code) {
    effects.enter('reference');
    effects.enter('referenceMarker');
    effects.consume(code);
    effects.exit('referenceMarker');
    return referenceCollapsedOpen
  }
  function referenceCollapsedOpen(code) {
    if (code === 93) {
      effects.enter('referenceMarker');
      effects.consume(code);
      effects.exit('referenceMarker');
      effects.exit('reference');
      return ok
    }
    return nok(code)
  }
}

const labelStartImage = {
  name: 'labelStartImage',
  tokenize: tokenizeLabelStartImage,
  resolveAll: labelEnd.resolveAll
};
function tokenizeLabelStartImage(effects, ok, nok) {
  const self = this;
  return start
  function start(code) {
    effects.enter('labelImage');
    effects.enter('labelImageMarker');
    effects.consume(code);
    effects.exit('labelImageMarker');
    return open
  }
  function open(code) {
    if (code === 91) {
      effects.enter('labelMarker');
      effects.consume(code);
      effects.exit('labelMarker');
      effects.exit('labelImage');
      return after
    }
    return nok(code)
  }
  function after(code) {
    return code === 94 && '_hiddenFootnoteSupport' in self.parser.constructs
      ? nok(code)
      : ok(code)
  }
}

const labelStartLink = {
  name: 'labelStartLink',
  tokenize: tokenizeLabelStartLink,
  resolveAll: labelEnd.resolveAll
};
function tokenizeLabelStartLink(effects, ok, nok) {
  const self = this;
  return start
  function start(code) {
    effects.enter('labelLink');
    effects.enter('labelMarker');
    effects.consume(code);
    effects.exit('labelMarker');
    effects.exit('labelLink');
    return after
  }
  function after(code) {
    return code === 94 && '_hiddenFootnoteSupport' in self.parser.constructs
      ? nok(code)
      : ok(code)
  }
}

const lineEnding = {
  name: 'lineEnding',
  tokenize: tokenizeLineEnding
};
function tokenizeLineEnding(effects, ok) {
  return start
  function start(code) {
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return factorySpace(effects, ok, 'linePrefix')
  }
}

const thematicBreak$1 = {
  name: 'thematicBreak',
  tokenize: tokenizeThematicBreak
};
function tokenizeThematicBreak(effects, ok, nok) {
  let size = 0;
  let marker;
  return start
  function start(code) {
    effects.enter('thematicBreak');
    return before(code)
  }
  function before(code) {
    marker = code;
    return atBreak(code)
  }
  function atBreak(code) {
    if (code === marker) {
      effects.enter('thematicBreakSequence');
      return sequence(code)
    }
    if (size >= 3 && (code === null || markdownLineEnding(code))) {
      effects.exit('thematicBreak');
      return ok(code)
    }
    return nok(code)
  }
  function sequence(code) {
    if (code === marker) {
      effects.consume(code);
      size++;
      return sequence
    }
    effects.exit('thematicBreakSequence');
    return markdownSpace(code)
      ? factorySpace(effects, atBreak, 'whitespace')(code)
      : atBreak(code)
  }
}

const list$2 = {
  name: 'list',
  tokenize: tokenizeListStart,
  continuation: {
    tokenize: tokenizeListContinuation
  },
  exit: tokenizeListEnd
};
const listItemPrefixWhitespaceConstruct = {
  tokenize: tokenizeListItemPrefixWhitespace,
  partial: true
};
const indentConstruct = {
  tokenize: tokenizeIndent$1,
  partial: true
};
function tokenizeListStart(effects, ok, nok) {
  const self = this;
  const tail = self.events[self.events.length - 1];
  let initialSize =
    tail && tail[1].type === 'linePrefix'
      ? tail[2].sliceSerialize(tail[1], true).length
      : 0;
  let size = 0;
  return start
  function start(code) {
    const kind =
      self.containerState.type ||
      (code === 42 || code === 43 || code === 45
        ? 'listUnordered'
        : 'listOrdered');
    if (
      kind === 'listUnordered'
        ? !self.containerState.marker || code === self.containerState.marker
        : asciiDigit(code)
    ) {
      if (!self.containerState.type) {
        self.containerState.type = kind;
        effects.enter(kind, {
          _container: true
        });
      }
      if (kind === 'listUnordered') {
        effects.enter('listItemPrefix');
        return code === 42 || code === 45
          ? effects.check(thematicBreak$1, nok, atMarker)(code)
          : atMarker(code)
      }
      if (!self.interrupt || code === 49) {
        effects.enter('listItemPrefix');
        effects.enter('listItemValue');
        return inside(code)
      }
    }
    return nok(code)
  }
  function inside(code) {
    if (asciiDigit(code) && ++size < 10) {
      effects.consume(code);
      return inside
    }
    if (
      (!self.interrupt || size < 2) &&
      (self.containerState.marker
        ? code === self.containerState.marker
        : code === 41 || code === 46)
    ) {
      effects.exit('listItemValue');
      return atMarker(code)
    }
    return nok(code)
  }
  function atMarker(code) {
    effects.enter('listItemMarker');
    effects.consume(code);
    effects.exit('listItemMarker');
    self.containerState.marker = self.containerState.marker || code;
    return effects.check(
      blankLine,
      self.interrupt ? nok : onBlank,
      effects.attempt(
        listItemPrefixWhitespaceConstruct,
        endOfPrefix,
        otherPrefix
      )
    )
  }
  function onBlank(code) {
    self.containerState.initialBlankLine = true;
    initialSize++;
    return endOfPrefix(code)
  }
  function otherPrefix(code) {
    if (markdownSpace(code)) {
      effects.enter('listItemPrefixWhitespace');
      effects.consume(code);
      effects.exit('listItemPrefixWhitespace');
      return endOfPrefix
    }
    return nok(code)
  }
  function endOfPrefix(code) {
    self.containerState.size =
      initialSize +
      self.sliceSerialize(effects.exit('listItemPrefix'), true).length;
    return ok(code)
  }
}
function tokenizeListContinuation(effects, ok, nok) {
  const self = this;
  self.containerState._closeFlow = undefined;
  return effects.check(blankLine, onBlank, notBlank)
  function onBlank(code) {
    self.containerState.furtherBlankLines =
      self.containerState.furtherBlankLines ||
      self.containerState.initialBlankLine;
    return factorySpace(
      effects,
      ok,
      'listItemIndent',
      self.containerState.size + 1
    )(code)
  }
  function notBlank(code) {
    if (self.containerState.furtherBlankLines || !markdownSpace(code)) {
      self.containerState.furtherBlankLines = undefined;
      self.containerState.initialBlankLine = undefined;
      return notInCurrentItem(code)
    }
    self.containerState.furtherBlankLines = undefined;
    self.containerState.initialBlankLine = undefined;
    return effects.attempt(indentConstruct, ok, notInCurrentItem)(code)
  }
  function notInCurrentItem(code) {
    self.containerState._closeFlow = true;
    self.interrupt = undefined;
    return factorySpace(
      effects,
      effects.attempt(list$2, ok, nok),
      'linePrefix',
      self.parser.constructs.disable.null.includes('codeIndented')
        ? undefined
        : 4
    )(code)
  }
}
function tokenizeIndent$1(effects, ok, nok) {
  const self = this;
  return factorySpace(
    effects,
    afterPrefix,
    'listItemIndent',
    self.containerState.size + 1
  )
  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'listItemIndent' &&
      tail[2].sliceSerialize(tail[1], true).length === self.containerState.size
      ? ok(code)
      : nok(code)
  }
}
function tokenizeListEnd(effects) {
  effects.exit(this.containerState.type);
}
function tokenizeListItemPrefixWhitespace(effects, ok, nok) {
  const self = this;
  return factorySpace(
    effects,
    afterPrefix,
    'listItemPrefixWhitespace',
    self.parser.constructs.disable.null.includes('codeIndented')
      ? undefined
      : 4 + 1
  )
  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return !markdownSpace(code) &&
      tail &&
      tail[1].type === 'listItemPrefixWhitespace'
      ? ok(code)
      : nok(code)
  }
}

const setextUnderline = {
  name: 'setextUnderline',
  tokenize: tokenizeSetextUnderline,
  resolveTo: resolveToSetextUnderline
};
function resolveToSetextUnderline(events, context) {
  let index = events.length;
  let content;
  let text;
  let definition;
  while (index--) {
    if (events[index][0] === 'enter') {
      if (events[index][1].type === 'content') {
        content = index;
        break
      }
      if (events[index][1].type === 'paragraph') {
        text = index;
      }
    }
    else {
      if (events[index][1].type === 'content') {
        events.splice(index, 1);
      }
      if (!definition && events[index][1].type === 'definition') {
        definition = index;
      }
    }
  }
  const heading = {
    type: 'setextHeading',
    start: Object.assign({}, events[text][1].start),
    end: Object.assign({}, events[events.length - 1][1].end)
  };
  events[text][1].type = 'setextHeadingText';
  if (definition) {
    events.splice(text, 0, ['enter', heading, context]);
    events.splice(definition + 1, 0, ['exit', events[content][1], context]);
    events[content][1].end = Object.assign({}, events[definition][1].end);
  } else {
    events[content][1] = heading;
  }
  events.push(['exit', heading, context]);
  return events
}
function tokenizeSetextUnderline(effects, ok, nok) {
  const self = this;
  let marker;
  return start
  function start(code) {
    let index = self.events.length;
    let paragraph;
    while (index--) {
      if (
        self.events[index][1].type !== 'lineEnding' &&
        self.events[index][1].type !== 'linePrefix' &&
        self.events[index][1].type !== 'content'
      ) {
        paragraph = self.events[index][1].type === 'paragraph';
        break
      }
    }
    if (!self.parser.lazy[self.now().line] && (self.interrupt || paragraph)) {
      effects.enter('setextHeadingLine');
      marker = code;
      return before(code)
    }
    return nok(code)
  }
  function before(code) {
    effects.enter('setextHeadingLineSequence');
    return inside(code)
  }
  function inside(code) {
    if (code === marker) {
      effects.consume(code);
      return inside
    }
    effects.exit('setextHeadingLineSequence');
    return markdownSpace(code)
      ? factorySpace(effects, after, 'lineSuffix')(code)
      : after(code)
  }
  function after(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('setextHeadingLine');
      return ok(code)
    }
    return nok(code)
  }
}

const flow$1 = {
  tokenize: initializeFlow
};
function initializeFlow(effects) {
  const self = this;
  const initial = effects.attempt(
    blankLine,
    atBlankEnding,
    effects.attempt(
      this.parser.constructs.flowInitial,
      afterConstruct,
      factorySpace(
        effects,
        effects.attempt(
          this.parser.constructs.flow,
          afterConstruct,
          effects.attempt(content, afterConstruct)
        ),
        'linePrefix'
      )
    )
  );
  return initial
  function atBlankEnding(code) {
    if (code === null) {
      effects.consume(code);
      return
    }
    effects.enter('lineEndingBlank');
    effects.consume(code);
    effects.exit('lineEndingBlank');
    self.currentConstruct = undefined;
    return initial
  }
  function afterConstruct(code) {
    if (code === null) {
      effects.consume(code);
      return
    }
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    self.currentConstruct = undefined;
    return initial
  }
}

const resolver = {
  resolveAll: createResolver()
};
const string$1 = initializeFactory('string');
const text$3 = initializeFactory('text');
function initializeFactory(field) {
  return {
    tokenize: initializeText,
    resolveAll: createResolver(
      field === 'text' ? resolveAllLineSuffixes : undefined
    )
  }
  function initializeText(effects) {
    const self = this;
    const constructs = this.parser.constructs[field];
    const text = effects.attempt(constructs, start, notText);
    return start
    function start(code) {
      return atBreak(code) ? text(code) : notText(code)
    }
    function notText(code) {
      if (code === null) {
        effects.consume(code);
        return
      }
      effects.enter('data');
      effects.consume(code);
      return data
    }
    function data(code) {
      if (atBreak(code)) {
        effects.exit('data');
        return text(code)
      }
      effects.consume(code);
      return data
    }
    function atBreak(code) {
      if (code === null) {
        return true
      }
      const list = constructs[code];
      let index = -1;
      if (list) {
        while (++index < list.length) {
          const item = list[index];
          if (!item.previous || item.previous.call(self, self.previous)) {
            return true
          }
        }
      }
      return false
    }
  }
}
function createResolver(extraResolver) {
  return resolveAllText
  function resolveAllText(events, context) {
    let index = -1;
    let enter;
    while (++index <= events.length) {
      if (enter === undefined) {
        if (events[index] && events[index][1].type === 'data') {
          enter = index;
          index++;
        }
      } else if (!events[index] || events[index][1].type !== 'data') {
        if (index !== enter + 2) {
          events[enter][1].end = events[index - 1][1].end;
          events.splice(enter + 2, index - enter - 2);
          index = enter + 2;
        }
        enter = undefined;
      }
    }
    return extraResolver ? extraResolver(events, context) : events
  }
}
function resolveAllLineSuffixes(events, context) {
  let eventIndex = 0;
  while (++eventIndex <= events.length) {
    if (
      (eventIndex === events.length ||
        events[eventIndex][1].type === 'lineEnding') &&
      events[eventIndex - 1][1].type === 'data'
    ) {
      const data = events[eventIndex - 1][1];
      const chunks = context.sliceStream(data);
      let index = chunks.length;
      let bufferIndex = -1;
      let size = 0;
      let tabs;
      while (index--) {
        const chunk = chunks[index];
        if (typeof chunk === 'string') {
          bufferIndex = chunk.length;
          while (chunk.charCodeAt(bufferIndex - 1) === 32) {
            size++;
            bufferIndex--;
          }
          if (bufferIndex) break
          bufferIndex = -1;
        }
        else if (chunk === -2) {
          tabs = true;
          size++;
        } else if (chunk === -1) ; else {
          index++;
          break
        }
      }
      if (size) {
        const token = {
          type:
            eventIndex === events.length || tabs || size < 2
              ? 'lineSuffix'
              : 'hardBreakTrailing',
          start: {
            line: data.end.line,
            column: data.end.column - size,
            offset: data.end.offset - size,
            _index: data.start._index + index,
            _bufferIndex: index
              ? bufferIndex
              : data.start._bufferIndex + bufferIndex
          },
          end: Object.assign({}, data.end)
        };
        data.end = Object.assign({}, token.start);
        if (data.start.offset === data.end.offset) {
          Object.assign(data, token);
        } else {
          events.splice(
            eventIndex,
            0,
            ['enter', token, context],
            ['exit', token, context]
          );
          eventIndex += 2;
        }
      }
      eventIndex++;
    }
  }
  return events
}

function createTokenizer(parser, initialize, from) {
  let point = Object.assign(
    from
      ? Object.assign({}, from)
      : {
          line: 1,
          column: 1,
          offset: 0
        },
    {
      _index: 0,
      _bufferIndex: -1
    }
  );
  const columnStart = {};
  const resolveAllConstructs = [];
  let chunks = [];
  let stack = [];
  const effects = {
    consume,
    enter,
    exit,
    attempt: constructFactory(onsuccessfulconstruct),
    check: constructFactory(onsuccessfulcheck),
    interrupt: constructFactory(onsuccessfulcheck, {
      interrupt: true
    })
  };
  const context = {
    previous: null,
    code: null,
    containerState: {},
    events: [],
    parser,
    sliceStream,
    sliceSerialize,
    now,
    defineSkip,
    write
  };
  let state = initialize.tokenize.call(context, effects);
  if (initialize.resolveAll) {
    resolveAllConstructs.push(initialize);
  }
  return context
  function write(slice) {
    chunks = push(chunks, slice);
    main();
    if (chunks[chunks.length - 1] !== null) {
      return []
    }
    addResult(initialize, 0);
    context.events = resolveAll(resolveAllConstructs, context.events, context);
    return context.events
  }
  function sliceSerialize(token, expandTabs) {
    return serializeChunks(sliceStream(token), expandTabs)
  }
  function sliceStream(token) {
    return sliceChunks(chunks, token)
  }
  function now() {
    const {line, column, offset, _index, _bufferIndex} = point;
    return {
      line,
      column,
      offset,
      _index,
      _bufferIndex
    }
  }
  function defineSkip(value) {
    columnStart[value.line] = value.column;
    accountForPotentialSkip();
  }
  function main() {
    let chunkIndex;
    while (point._index < chunks.length) {
      const chunk = chunks[point._index];
      if (typeof chunk === 'string') {
        chunkIndex = point._index;
        if (point._bufferIndex < 0) {
          point._bufferIndex = 0;
        }
        while (
          point._index === chunkIndex &&
          point._bufferIndex < chunk.length
        ) {
          go(chunk.charCodeAt(point._bufferIndex));
        }
      } else {
        go(chunk);
      }
    }
  }
  function go(code) {
    state = state(code);
  }
  function consume(code) {
    if (markdownLineEnding(code)) {
      point.line++;
      point.column = 1;
      point.offset += code === -3 ? 2 : 1;
      accountForPotentialSkip();
    } else if (code !== -1) {
      point.column++;
      point.offset++;
    }
    if (point._bufferIndex < 0) {
      point._index++;
    } else {
      point._bufferIndex++;
      if (point._bufferIndex === chunks[point._index].length) {
        point._bufferIndex = -1;
        point._index++;
      }
    }
    context.previous = code;
  }
  function enter(type, fields) {
    const token = fields || {};
    token.type = type;
    token.start = now();
    context.events.push(['enter', token, context]);
    stack.push(token);
    return token
  }
  function exit(type) {
    const token = stack.pop();
    token.end = now();
    context.events.push(['exit', token, context]);
    return token
  }
  function onsuccessfulconstruct(construct, info) {
    addResult(construct, info.from);
  }
  function onsuccessfulcheck(_, info) {
    info.restore();
  }
  function constructFactory(onreturn, fields) {
    return hook
    function hook(constructs, returnState, bogusState) {
      let listOfConstructs;
      let constructIndex;
      let currentConstruct;
      let info;
      return Array.isArray(constructs)
        ? handleListOfConstructs(constructs)
        : 'tokenize' in constructs
        ?
          handleListOfConstructs([constructs])
        : handleMapOfConstructs(constructs)
      function handleMapOfConstructs(map) {
        return start
        function start(code) {
          const def = code !== null && map[code];
          const all = code !== null && map.null;
          const list = [
            ...(Array.isArray(def) ? def : def ? [def] : []),
            ...(Array.isArray(all) ? all : all ? [all] : [])
          ];
          return handleListOfConstructs(list)(code)
        }
      }
      function handleListOfConstructs(list) {
        listOfConstructs = list;
        constructIndex = 0;
        if (list.length === 0) {
          return bogusState
        }
        return handleConstruct(list[constructIndex])
      }
      function handleConstruct(construct) {
        return start
        function start(code) {
          info = store();
          currentConstruct = construct;
          if (!construct.partial) {
            context.currentConstruct = construct;
          }
          if (
            construct.name &&
            context.parser.constructs.disable.null.includes(construct.name)
          ) {
            return nok()
          }
          return construct.tokenize.call(
            fields ? Object.assign(Object.create(context), fields) : context,
            effects,
            ok,
            nok
          )(code)
        }
      }
      function ok(code) {
        onreturn(currentConstruct, info);
        return returnState
      }
      function nok(code) {
        info.restore();
        if (++constructIndex < listOfConstructs.length) {
          return handleConstruct(listOfConstructs[constructIndex])
        }
        return bogusState
      }
    }
  }
  function addResult(construct, from) {
    if (construct.resolveAll && !resolveAllConstructs.includes(construct)) {
      resolveAllConstructs.push(construct);
    }
    if (construct.resolve) {
      splice(
        context.events,
        from,
        context.events.length - from,
        construct.resolve(context.events.slice(from), context)
      );
    }
    if (construct.resolveTo) {
      context.events = construct.resolveTo(context.events, context);
    }
  }
  function store() {
    const startPoint = now();
    const startPrevious = context.previous;
    const startCurrentConstruct = context.currentConstruct;
    const startEventsIndex = context.events.length;
    const startStack = Array.from(stack);
    return {
      restore,
      from: startEventsIndex
    }
    function restore() {
      point = startPoint;
      context.previous = startPrevious;
      context.currentConstruct = startCurrentConstruct;
      context.events.length = startEventsIndex;
      stack = startStack;
      accountForPotentialSkip();
    }
  }
  function accountForPotentialSkip() {
    if (point.line in columnStart && point.column < 2) {
      point.column = columnStart[point.line];
      point.offset += columnStart[point.line] - 1;
    }
  }
}
function sliceChunks(chunks, token) {
  const startIndex = token.start._index;
  const startBufferIndex = token.start._bufferIndex;
  const endIndex = token.end._index;
  const endBufferIndex = token.end._bufferIndex;
  let view;
  if (startIndex === endIndex) {
    view = [chunks[startIndex].slice(startBufferIndex, endBufferIndex)];
  } else {
    view = chunks.slice(startIndex, endIndex);
    if (startBufferIndex > -1) {
      const head = view[0];
      if (typeof head === 'string') {
        view[0] = head.slice(startBufferIndex);
      } else {
        view.shift();
      }
    }
    if (endBufferIndex > 0) {
      view.push(chunks[endIndex].slice(0, endBufferIndex));
    }
  }
  return view
}
function serializeChunks(chunks, expandTabs) {
  let index = -1;
  const result = [];
  let atTab;
  while (++index < chunks.length) {
    const chunk = chunks[index];
    let value;
    if (typeof chunk === 'string') {
      value = chunk;
    } else
      switch (chunk) {
        case -5: {
          value = '\r';
          break
        }
        case -4: {
          value = '\n';
          break
        }
        case -3: {
          value = '\r' + '\n';
          break
        }
        case -2: {
          value = expandTabs ? ' ' : '\t';
          break
        }
        case -1: {
          if (!expandTabs && atTab) continue
          value = ' ';
          break
        }
        default: {
          value = String.fromCharCode(chunk);
        }
      }
    atTab = chunk === -2;
    result.push(value);
  }
  return result.join('')
}

const document = {
  [42]: list$2,
  [43]: list$2,
  [45]: list$2,
  [48]: list$2,
  [49]: list$2,
  [50]: list$2,
  [51]: list$2,
  [52]: list$2,
  [53]: list$2,
  [54]: list$2,
  [55]: list$2,
  [56]: list$2,
  [57]: list$2,
  [62]: blockQuote
};
const contentInitial = {
  [91]: definition$1
};
const flowInitial = {
  [-2]: codeIndented,
  [-1]: codeIndented,
  [32]: codeIndented
};
const flow = {
  [35]: headingAtx,
  [42]: thematicBreak$1,
  [45]: [setextUnderline, thematicBreak$1],
  [60]: htmlFlow,
  [61]: setextUnderline,
  [95]: thematicBreak$1,
  [96]: codeFenced,
  [126]: codeFenced
};
const string = {
  [38]: characterReference,
  [92]: characterEscape
};
const text$2 = {
  [-5]: lineEnding,
  [-4]: lineEnding,
  [-3]: lineEnding,
  [33]: labelStartImage,
  [38]: characterReference,
  [42]: attention,
  [60]: [autolink, htmlText],
  [91]: labelStartLink,
  [92]: [hardBreakEscape, characterEscape],
  [93]: labelEnd,
  [95]: attention,
  [96]: codeText
};
const insideSpan = {
  null: [attention, resolver]
};
const attentionMarkers = {
  null: [42, 95]
};
const disable = {
  null: []
};

var defaultConstructs = /*#__PURE__*/Object.freeze({
  __proto__: null,
  attentionMarkers: attentionMarkers,
  contentInitial: contentInitial,
  disable: disable,
  document: document,
  flow: flow,
  flowInitial: flowInitial,
  insideSpan: insideSpan,
  string: string,
  text: text$2
});

function parse$1(options) {
  const settings = options || {};
  const constructs =
    combineExtensions([defaultConstructs, ...(settings.extensions || [])]);
  const parser = {
    defined: [],
    lazy: {},
    constructs,
    content: create(content$1),
    document: create(document$1),
    flow: create(flow$1),
    string: create(string$1),
    text: create(text$3)
  };
  return parser
  function create(initial) {
    return creator
    function creator(from) {
      return createTokenizer(parser, initial, from)
    }
  }
}

function postprocess(events) {
  while (!subtokenize(events)) {
  }
  return events
}

const search = /[\0\t\n\r]/g;
function preprocess() {
  let column = 1;
  let buffer = '';
  let start = true;
  let atCarriageReturn;
  return preprocessor
  function preprocessor(value, encoding, end) {
    const chunks = [];
    let match;
    let next;
    let startPosition;
    let endPosition;
    let code;
    value =
      buffer +
      (typeof value === 'string'
        ? value.toString()
        : new TextDecoder(encoding || undefined).decode(value));
    startPosition = 0;
    buffer = '';
    if (start) {
      if (value.charCodeAt(0) === 65279) {
        startPosition++;
      }
      start = undefined;
    }
    while (startPosition < value.length) {
      search.lastIndex = startPosition;
      match = search.exec(value);
      endPosition =
        match && match.index !== undefined ? match.index : value.length;
      code = value.charCodeAt(endPosition);
      if (!match) {
        buffer = value.slice(startPosition);
        break
      }
      if (code === 10 && startPosition === endPosition && atCarriageReturn) {
        chunks.push(-3);
        atCarriageReturn = undefined;
      } else {
        if (atCarriageReturn) {
          chunks.push(-5);
          atCarriageReturn = undefined;
        }
        if (startPosition < endPosition) {
          chunks.push(value.slice(startPosition, endPosition));
          column += endPosition - startPosition;
        }
        switch (code) {
          case 0: {
            chunks.push(65533);
            column++;
            break
          }
          case 9: {
            next = Math.ceil(column / 4) * 4;
            chunks.push(-2);
            while (column++ < next) chunks.push(-1);
            break
          }
          case 10: {
            chunks.push(-4);
            column = 1;
            break
          }
          default: {
            atCarriageReturn = true;
            column = 1;
          }
        }
      }
      startPosition = endPosition + 1;
    }
    if (end) {
      if (atCarriageReturn) chunks.push(-5);
      if (buffer) chunks.push(buffer);
      chunks.push(null);
    }
    return chunks
  }
}

const characterEscapeOrReference =
  /\\([!-/:-@[-`{-~])|&(#(?:\d{1,7}|x[\da-f]{1,6})|[\da-z]{1,31});/gi;
function decodeString(value) {
  return value.replace(characterEscapeOrReference, decode)
}
function decode($0, $1, $2) {
  if ($1) {
    return $1
  }
  const head = $2.charCodeAt(0);
  if (head === 35) {
    const head = $2.charCodeAt(1);
    const hex = head === 120 || head === 88;
    return decodeNumericCharacterReference($2.slice(hex ? 2 : 1), hex ? 16 : 10)
  }
  return decodeNamedCharacterReference($2) || $0
}

const own$4 = {}.hasOwnProperty;
function fromMarkdown(value, encoding, options) {
  if (typeof encoding !== 'string') {
    options = encoding;
    encoding = undefined;
  }
  return compiler(options)(
    postprocess(
      parse$1(options).document().write(preprocess()(value, encoding, true))
    )
  )
}
function compiler(options) {
  const config = {
    transforms: [],
    canContainEols: ['emphasis', 'fragment', 'heading', 'paragraph', 'strong'],
    enter: {
      autolink: opener(link),
      autolinkProtocol: onenterdata,
      autolinkEmail: onenterdata,
      atxHeading: opener(heading),
      blockQuote: opener(blockQuote),
      characterEscape: onenterdata,
      characterReference: onenterdata,
      codeFenced: opener(codeFlow),
      codeFencedFenceInfo: buffer,
      codeFencedFenceMeta: buffer,
      codeIndented: opener(codeFlow, buffer),
      codeText: opener(codeText, buffer),
      codeTextData: onenterdata,
      data: onenterdata,
      codeFlowValue: onenterdata,
      definition: opener(definition),
      definitionDestinationString: buffer,
      definitionLabelString: buffer,
      definitionTitleString: buffer,
      emphasis: opener(emphasis),
      hardBreakEscape: opener(hardBreak),
      hardBreakTrailing: opener(hardBreak),
      htmlFlow: opener(html, buffer),
      htmlFlowData: onenterdata,
      htmlText: opener(html, buffer),
      htmlTextData: onenterdata,
      image: opener(image),
      label: buffer,
      link: opener(link),
      listItem: opener(listItem),
      listItemValue: onenterlistitemvalue,
      listOrdered: opener(list, onenterlistordered),
      listUnordered: opener(list),
      paragraph: opener(paragraph),
      reference: onenterreference,
      referenceString: buffer,
      resourceDestinationString: buffer,
      resourceTitleString: buffer,
      setextHeading: opener(heading),
      strong: opener(strong),
      thematicBreak: opener(thematicBreak)
    },
    exit: {
      atxHeading: closer(),
      atxHeadingSequence: onexitatxheadingsequence,
      autolink: closer(),
      autolinkEmail: onexitautolinkemail,
      autolinkProtocol: onexitautolinkprotocol,
      blockQuote: closer(),
      characterEscapeValue: onexitdata,
      characterReferenceMarkerHexadecimal: onexitcharacterreferencemarker,
      characterReferenceMarkerNumeric: onexitcharacterreferencemarker,
      characterReferenceValue: onexitcharacterreferencevalue,
      codeFenced: closer(onexitcodefenced),
      codeFencedFence: onexitcodefencedfence,
      codeFencedFenceInfo: onexitcodefencedfenceinfo,
      codeFencedFenceMeta: onexitcodefencedfencemeta,
      codeFlowValue: onexitdata,
      codeIndented: closer(onexitcodeindented),
      codeText: closer(onexitcodetext),
      codeTextData: onexitdata,
      data: onexitdata,
      definition: closer(),
      definitionDestinationString: onexitdefinitiondestinationstring,
      definitionLabelString: onexitdefinitionlabelstring,
      definitionTitleString: onexitdefinitiontitlestring,
      emphasis: closer(),
      hardBreakEscape: closer(onexithardbreak),
      hardBreakTrailing: closer(onexithardbreak),
      htmlFlow: closer(onexithtmlflow),
      htmlFlowData: onexitdata,
      htmlText: closer(onexithtmltext),
      htmlTextData: onexitdata,
      image: closer(onexitimage),
      label: onexitlabel,
      labelText: onexitlabeltext,
      lineEnding: onexitlineending,
      link: closer(onexitlink),
      listItem: closer(),
      listOrdered: closer(),
      listUnordered: closer(),
      paragraph: closer(),
      referenceString: onexitreferencestring,
      resourceDestinationString: onexitresourcedestinationstring,
      resourceTitleString: onexitresourcetitlestring,
      resource: onexitresource,
      setextHeading: closer(onexitsetextheading),
      setextHeadingLineSequence: onexitsetextheadinglinesequence,
      setextHeadingText: onexitsetextheadingtext,
      strong: closer(),
      thematicBreak: closer()
    }
  };
  configure$1(config, (options || {}).mdastExtensions || []);
  const data = {};
  return compile
  function compile(events) {
    let tree = {
      type: 'root',
      children: []
    };
    const context = {
      stack: [tree],
      tokenStack: [],
      config,
      enter,
      exit,
      buffer,
      resume,
      data
    };
    const listStack = [];
    let index = -1;
    while (++index < events.length) {
      if (
        events[index][1].type === 'listOrdered' ||
        events[index][1].type === 'listUnordered'
      ) {
        if (events[index][0] === 'enter') {
          listStack.push(index);
        } else {
          const tail = listStack.pop();
          index = prepareList(events, tail, index);
        }
      }
    }
    index = -1;
    while (++index < events.length) {
      const handler = config[events[index][0]];
      if (own$4.call(handler, events[index][1].type)) {
        handler[events[index][1].type].call(
          Object.assign(
            {
              sliceSerialize: events[index][2].sliceSerialize
            },
            context
          ),
          events[index][1]
        );
      }
    }
    if (context.tokenStack.length > 0) {
      const tail = context.tokenStack[context.tokenStack.length - 1];
      const handler = tail[1] || defaultOnError;
      handler.call(context, undefined, tail[0]);
    }
    tree.position = {
      start: point$3(
        events.length > 0
          ? events[0][1].start
          : {
              line: 1,
              column: 1,
              offset: 0
            }
      ),
      end: point$3(
        events.length > 0
          ? events[events.length - 2][1].end
          : {
              line: 1,
              column: 1,
              offset: 0
            }
      )
    };
    index = -1;
    while (++index < config.transforms.length) {
      tree = config.transforms[index](tree) || tree;
    }
    return tree
  }
  function prepareList(events, start, length) {
    let index = start - 1;
    let containerBalance = -1;
    let listSpread = false;
    let listItem;
    let lineIndex;
    let firstBlankLineIndex;
    let atMarker;
    while (++index <= length) {
      const event = events[index];
      switch (event[1].type) {
        case 'listUnordered':
        case 'listOrdered':
        case 'blockQuote': {
          if (event[0] === 'enter') {
            containerBalance++;
          } else {
            containerBalance--;
          }
          atMarker = undefined;
          break
        }
        case 'lineEndingBlank': {
          if (event[0] === 'enter') {
            if (
              listItem &&
              !atMarker &&
              !containerBalance &&
              !firstBlankLineIndex
            ) {
              firstBlankLineIndex = index;
            }
            atMarker = undefined;
          }
          break
        }
        case 'linePrefix':
        case 'listItemValue':
        case 'listItemMarker':
        case 'listItemPrefix':
        case 'listItemPrefixWhitespace': {
          break
        }
        default: {
          atMarker = undefined;
        }
      }
      if (
        (!containerBalance &&
          event[0] === 'enter' &&
          event[1].type === 'listItemPrefix') ||
        (containerBalance === -1 &&
          event[0] === 'exit' &&
          (event[1].type === 'listUnordered' ||
            event[1].type === 'listOrdered'))
      ) {
        if (listItem) {
          let tailIndex = index;
          lineIndex = undefined;
          while (tailIndex--) {
            const tailEvent = events[tailIndex];
            if (
              tailEvent[1].type === 'lineEnding' ||
              tailEvent[1].type === 'lineEndingBlank'
            ) {
              if (tailEvent[0] === 'exit') continue
              if (lineIndex) {
                events[lineIndex][1].type = 'lineEndingBlank';
                listSpread = true;
              }
              tailEvent[1].type = 'lineEnding';
              lineIndex = tailIndex;
            } else if (
              tailEvent[1].type === 'linePrefix' ||
              tailEvent[1].type === 'blockQuotePrefix' ||
              tailEvent[1].type === 'blockQuotePrefixWhitespace' ||
              tailEvent[1].type === 'blockQuoteMarker' ||
              tailEvent[1].type === 'listItemIndent'
            ) ; else {
              break
            }
          }
          if (
            firstBlankLineIndex &&
            (!lineIndex || firstBlankLineIndex < lineIndex)
          ) {
            listItem._spread = true;
          }
          listItem.end = Object.assign(
            {},
            lineIndex ? events[lineIndex][1].start : event[1].end
          );
          events.splice(lineIndex || index, 0, ['exit', listItem, event[2]]);
          index++;
          length++;
        }
        if (event[1].type === 'listItemPrefix') {
          const item = {
            type: 'listItem',
            _spread: false,
            start: Object.assign({}, event[1].start),
            end: undefined
          };
          listItem = item;
          events.splice(index, 0, ['enter', item, event[2]]);
          index++;
          length++;
          firstBlankLineIndex = undefined;
          atMarker = true;
        }
      }
    }
    events[start][1]._spread = listSpread;
    return length
  }
  function opener(create, and) {
    return open
    function open(token) {
      enter.call(this, create(token), token);
      if (and) and.call(this, token);
    }
  }
  function buffer() {
    this.stack.push({
      type: 'fragment',
      children: []
    });
  }
  function enter(node, token, errorHandler) {
    const parent = this.stack[this.stack.length - 1];
    const siblings = parent.children;
    siblings.push(node);
    this.stack.push(node);
    this.tokenStack.push([token, errorHandler]);
    node.position = {
      start: point$3(token.start),
      end: undefined
    };
  }
  function closer(and) {
    return close
    function close(token) {
      if (and) and.call(this, token);
      exit.call(this, token);
    }
  }
  function exit(token, onExitError) {
    const node = this.stack.pop();
    const open = this.tokenStack.pop();
    if (!open) {
      throw new Error(
        'Cannot close `' +
          token.type +
          '` (' +
          stringifyPosition$2({
            start: token.start,
            end: token.end
          }) +
          '): it’s not open'
      )
    } else if (open[0].type !== token.type) {
      if (onExitError) {
        onExitError.call(this, token, open[0]);
      } else {
        const handler = open[1] || defaultOnError;
        handler.call(this, token, open[0]);
      }
    }
    node.position.end = point$3(token.end);
  }
  function resume() {
    return toString$2(this.stack.pop())
  }
  function onenterlistordered() {
    this.data.expectingFirstListItemValue = true;
  }
  function onenterlistitemvalue(token) {
    if (this.data.expectingFirstListItemValue) {
      const ancestor = this.stack[this.stack.length - 2];
      ancestor.start = Number.parseInt(this.sliceSerialize(token), 10);
      this.data.expectingFirstListItemValue = undefined;
    }
  }
  function onexitcodefencedfenceinfo() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.lang = data;
  }
  function onexitcodefencedfencemeta() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.meta = data;
  }
  function onexitcodefencedfence() {
    if (this.data.flowCodeInside) return
    this.buffer();
    this.data.flowCodeInside = true;
  }
  function onexitcodefenced() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.value = data.replace(/^(\r?\n|\r)|(\r?\n|\r)$/g, '');
    this.data.flowCodeInside = undefined;
  }
  function onexitcodeindented() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.value = data.replace(/(\r?\n|\r)$/g, '');
  }
  function onexitdefinitionlabelstring(token) {
    const label = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.label = label;
    node.identifier = normalizeIdentifier$1(
      this.sliceSerialize(token)
    ).toLowerCase();
  }
  function onexitdefinitiontitlestring() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.title = data;
  }
  function onexitdefinitiondestinationstring() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.url = data;
  }
  function onexitatxheadingsequence(token) {
    const node = this.stack[this.stack.length - 1];
    if (!node.depth) {
      const depth = this.sliceSerialize(token).length;
      node.depth = depth;
    }
  }
  function onexitsetextheadingtext() {
    this.data.setextHeadingSlurpLineEnding = true;
  }
  function onexitsetextheadinglinesequence(token) {
    const node = this.stack[this.stack.length - 1];
    node.depth = this.sliceSerialize(token).codePointAt(0) === 61 ? 1 : 2;
  }
  function onexitsetextheading() {
    this.data.setextHeadingSlurpLineEnding = undefined;
  }
  function onenterdata(token) {
    const node = this.stack[this.stack.length - 1];
    const siblings = node.children;
    let tail = siblings[siblings.length - 1];
    if (!tail || tail.type !== 'text') {
      tail = text();
      tail.position = {
        start: point$3(token.start),
        end: undefined
      };
      siblings.push(tail);
    }
    this.stack.push(tail);
  }
  function onexitdata(token) {
    const tail = this.stack.pop();
    tail.value += this.sliceSerialize(token);
    tail.position.end = point$3(token.end);
  }
  function onexitlineending(token) {
    const context = this.stack[this.stack.length - 1];
    if (this.data.atHardBreak) {
      const tail = context.children[context.children.length - 1];
      tail.position.end = point$3(token.end);
      this.data.atHardBreak = undefined;
      return
    }
    if (
      !this.data.setextHeadingSlurpLineEnding &&
      config.canContainEols.includes(context.type)
    ) {
      onenterdata.call(this, token);
      onexitdata.call(this, token);
    }
  }
  function onexithardbreak() {
    this.data.atHardBreak = true;
  }
  function onexithtmlflow() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.value = data;
  }
  function onexithtmltext() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.value = data;
  }
  function onexitcodetext() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.value = data;
  }
  function onexitlink() {
    const node = this.stack[this.stack.length - 1];
    if (this.data.inReference) {
      const referenceType = this.data.referenceType || 'shortcut';
      node.type += 'Reference';
      node.referenceType = referenceType;
      delete node.url;
      delete node.title;
    } else {
      delete node.identifier;
      delete node.label;
    }
    this.data.referenceType = undefined;
  }
  function onexitimage() {
    const node = this.stack[this.stack.length - 1];
    if (this.data.inReference) {
      const referenceType = this.data.referenceType || 'shortcut';
      node.type += 'Reference';
      node.referenceType = referenceType;
      delete node.url;
      delete node.title;
    } else {
      delete node.identifier;
      delete node.label;
    }
    this.data.referenceType = undefined;
  }
  function onexitlabeltext(token) {
    const string = this.sliceSerialize(token);
    const ancestor = this.stack[this.stack.length - 2];
    ancestor.label = decodeString(string);
    ancestor.identifier = normalizeIdentifier$1(string).toLowerCase();
  }
  function onexitlabel() {
    const fragment = this.stack[this.stack.length - 1];
    const value = this.resume();
    const node = this.stack[this.stack.length - 1];
    this.data.inReference = true;
    if (node.type === 'link') {
      const children = fragment.children;
      node.children = children;
    } else {
      node.alt = value;
    }
  }
  function onexitresourcedestinationstring() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.url = data;
  }
  function onexitresourcetitlestring() {
    const data = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.title = data;
  }
  function onexitresource() {
    this.data.inReference = undefined;
  }
  function onenterreference() {
    this.data.referenceType = 'collapsed';
  }
  function onexitreferencestring(token) {
    const label = this.resume();
    const node = this.stack[this.stack.length - 1];
    node.label = label;
    node.identifier = normalizeIdentifier$1(
      this.sliceSerialize(token)
    ).toLowerCase();
    this.data.referenceType = 'full';
  }
  function onexitcharacterreferencemarker(token) {
    this.data.characterReferenceType = token.type;
  }
  function onexitcharacterreferencevalue(token) {
    const data = this.sliceSerialize(token);
    const type = this.data.characterReferenceType;
    let value;
    if (type) {
      value = decodeNumericCharacterReference(
        data,
        type === 'characterReferenceMarkerNumeric' ? 10 : 16
      );
      this.data.characterReferenceType = undefined;
    } else {
      const result = decodeNamedCharacterReference(data);
      value = result;
    }
    const tail = this.stack.pop();
    tail.value += value;
    tail.position.end = point$3(token.end);
  }
  function onexitautolinkprotocol(token) {
    onexitdata.call(this, token);
    const node = this.stack[this.stack.length - 1];
    node.url = this.sliceSerialize(token);
  }
  function onexitautolinkemail(token) {
    onexitdata.call(this, token);
    const node = this.stack[this.stack.length - 1];
    node.url = 'mailto:' + this.sliceSerialize(token);
  }
  function blockQuote() {
    return {
      type: 'blockquote',
      children: []
    }
  }
  function codeFlow() {
    return {
      type: 'code',
      lang: null,
      meta: null,
      value: ''
    }
  }
  function codeText() {
    return {
      type: 'inlineCode',
      value: ''
    }
  }
  function definition() {
    return {
      type: 'definition',
      identifier: '',
      label: null,
      title: null,
      url: ''
    }
  }
  function emphasis() {
    return {
      type: 'emphasis',
      children: []
    }
  }
  function heading() {
    return {
      type: 'heading',
      depth: 0,
      children: []
    }
  }
  function hardBreak() {
    return {
      type: 'break'
    }
  }
  function html() {
    return {
      type: 'html',
      value: ''
    }
  }
  function image() {
    return {
      type: 'image',
      title: null,
      url: '',
      alt: null
    }
  }
  function link() {
    return {
      type: 'link',
      title: null,
      url: '',
      children: []
    }
  }
  function list(token) {
    return {
      type: 'list',
      ordered: token.type === 'listOrdered',
      start: null,
      spread: token._spread,
      children: []
    }
  }
  function listItem(token) {
    return {
      type: 'listItem',
      spread: token._spread,
      checked: null,
      children: []
    }
  }
  function paragraph() {
    return {
      type: 'paragraph',
      children: []
    }
  }
  function strong() {
    return {
      type: 'strong',
      children: []
    }
  }
  function text() {
    return {
      type: 'text',
      value: ''
    }
  }
  function thematicBreak() {
    return {
      type: 'thematicBreak'
    }
  }
}
function point$3(d) {
  return {
    line: d.line,
    column: d.column,
    offset: d.offset
  }
}
function configure$1(combined, extensions) {
  let index = -1;
  while (++index < extensions.length) {
    const value = extensions[index];
    if (Array.isArray(value)) {
      configure$1(combined, value);
    } else {
      extension(combined, value);
    }
  }
}
function extension(combined, extension) {
  let key;
  for (key in extension) {
    if (own$4.call(extension, key)) {
      switch (key) {
        case 'canContainEols': {
          const right = extension[key];
          if (right) {
            combined[key].push(...right);
          }
          break
        }
        case 'transforms': {
          const right = extension[key];
          if (right) {
            combined[key].push(...right);
          }
          break
        }
        case 'enter':
        case 'exit': {
          const right = extension[key];
          if (right) {
            Object.assign(combined[key], right);
          }
          break
        }
      }
    }
  }
}
function defaultOnError(left, right) {
  if (left) {
    throw new Error(
      'Cannot close `' +
        left.type +
        '` (' +
        stringifyPosition$2({
          start: left.start,
          end: left.end
        }) +
        '): a different token (`' +
        right.type +
        '`, ' +
        stringifyPosition$2({
          start: right.start,
          end: right.end
        }) +
        ') is open'
    )
  } else {
    throw new Error(
      'Cannot close document, a token (`' +
        right.type +
        '`, ' +
        stringifyPosition$2({
          start: right.start,
          end: right.end
        }) +
        ') is still open'
    )
  }
}

function remarkParse(options) {
  const self = this;
  self.parser = parser;
  function parser(doc) {
    return fromMarkdown(doc, {
      ...self.data('settings'),
      ...options,
      extensions: self.data('micromarkExtensions') || [],
      mdastExtensions: self.data('fromMarkdownExtensions') || []
    })
  }
}

const own$3 = {}.hasOwnProperty;
function zwitch(key, options) {
  const settings = options || {};
  function one(value, ...parameters) {
    let fn = one.invalid;
    const handlers = one.handlers;
    if (value && own$3.call(value, key)) {
      const id = String(value[key]);
      fn = own$3.call(handlers, id) ? handlers[id] : one.unknown;
    }
    if (fn) {
      return fn.call(this, value, ...parameters)
    }
  }
  one.handlers = settings.handlers || {};
  one.invalid = settings.invalid;
  one.unknown = settings.unknown;
  return one
}

const own$2 = {}.hasOwnProperty;
function configure(base, extension) {
  let index = -1;
  let key;
  if (extension.extensions) {
    while (++index < extension.extensions.length) {
      configure(base, extension.extensions[index]);
    }
  }
  for (key in extension) {
    if (own$2.call(extension, key)) {
      switch (key) {
        case 'extensions': {
          break
        }
        case 'unsafe': {
          list$1(base[key], extension[key]);
          break
        }
        case 'join': {
          list$1(base[key], extension[key]);
          break
        }
        case 'handlers': {
          map$4(base[key], extension[key]);
          break
        }
        default: {
          base.options[key] = extension[key];
        }
      }
    }
  }
  return base
}
function list$1(left, right) {
  if (right) {
    left.push(...right);
  }
}
function map$4(left, right) {
  if (right) {
    Object.assign(left, right);
  }
}

function blockquote(node, _, state, info) {
  const exit = state.enter('blockquote');
  const tracker = state.createTracker(info);
  tracker.move('> ');
  tracker.shift(2);
  const value = state.indentLines(
    state.containerFlow(node, tracker.current()),
    map$3
  );
  exit();
  return value
}
function map$3(line, _, blank) {
  return '>' + (blank ? '' : ' ') + line
}

function patternInScope(stack, pattern) {
  return (
    listInScope(stack, pattern.inConstruct, true) &&
    !listInScope(stack, pattern.notInConstruct, false)
  )
}
function listInScope(stack, list, none) {
  if (typeof list === 'string') {
    list = [list];
  }
  if (!list || list.length === 0) {
    return none
  }
  let index = -1;
  while (++index < list.length) {
    if (stack.includes(list[index])) {
      return true
    }
  }
  return false
}

function hardBreak(_, _1, state, info) {
  let index = -1;
  while (++index < state.unsafe.length) {
    if (
      state.unsafe[index].character === '\n' &&
      patternInScope(state.stack, state.unsafe[index])
    ) {
      return /[ \t]/.test(info.before) ? '' : ' '
    }
  }
  return '\\\n'
}

function longestStreak(value, substring) {
  const source = String(value);
  let index = source.indexOf(substring);
  let expected = index;
  let count = 0;
  let max = 0;
  if (typeof substring !== 'string') {
    throw new TypeError('Expected substring')
  }
  while (index !== -1) {
    if (index === expected) {
      if (++count > max) {
        max = count;
      }
    } else {
      count = 1;
    }
    expected = index + substring.length;
    index = source.indexOf(substring, expected);
  }
  return max
}

function formatCodeAsIndented(node, state) {
  return Boolean(
    state.options.fences === false &&
      node.value &&
      !node.lang &&
      /[^ \r\n]/.test(node.value) &&
      !/^[\t ]*(?:[\r\n]|$)|(?:^|[\r\n])[\t ]*$/.test(node.value)
  )
}

function checkFence(state) {
  const marker = state.options.fence || '`';
  if (marker !== '`' && marker !== '~') {
    throw new Error(
      'Cannot serialize code with `' +
        marker +
        '` for `options.fence`, expected `` ` `` or `~`'
    )
  }
  return marker
}

function code$1(node, _, state, info) {
  const marker = checkFence(state);
  const raw = node.value || '';
  const suffix = marker === '`' ? 'GraveAccent' : 'Tilde';
  if (formatCodeAsIndented(node, state)) {
    const exit = state.enter('codeIndented');
    const value = state.indentLines(raw, map$2);
    exit();
    return value
  }
  const tracker = state.createTracker(info);
  const sequence = marker.repeat(Math.max(longestStreak(raw, marker) + 1, 3));
  const exit = state.enter('codeFenced');
  let value = tracker.move(sequence);
  if (node.lang) {
    const subexit = state.enter(`codeFencedLang${suffix}`);
    value += tracker.move(
      state.safe(node.lang, {
        before: value,
        after: ' ',
        encode: ['`'],
        ...tracker.current()
      })
    );
    subexit();
  }
  if (node.lang && node.meta) {
    const subexit = state.enter(`codeFencedMeta${suffix}`);
    value += tracker.move(' ');
    value += tracker.move(
      state.safe(node.meta, {
        before: value,
        after: '\n',
        encode: ['`'],
        ...tracker.current()
      })
    );
    subexit();
  }
  value += tracker.move('\n');
  if (raw) {
    value += tracker.move(raw + '\n');
  }
  value += tracker.move(sequence);
  exit();
  return value
}
function map$2(line, _, blank) {
  return (blank ? '' : '    ') + line
}

function checkQuote(state) {
  const marker = state.options.quote || '"';
  if (marker !== '"' && marker !== "'") {
    throw new Error(
      'Cannot serialize title with `' +
        marker +
        '` for `options.quote`, expected `"`, or `\'`'
    )
  }
  return marker
}

function definition(node, _, state, info) {
  const quote = checkQuote(state);
  const suffix = quote === '"' ? 'Quote' : 'Apostrophe';
  const exit = state.enter('definition');
  let subexit = state.enter('label');
  const tracker = state.createTracker(info);
  let value = tracker.move('[');
  value += tracker.move(
    state.safe(state.associationId(node), {
      before: value,
      after: ']',
      ...tracker.current()
    })
  );
  value += tracker.move(']: ');
  subexit();
  if (
    !node.url ||
    /[\0- \u007F]/.test(node.url)
  ) {
    subexit = state.enter('destinationLiteral');
    value += tracker.move('<');
    value += tracker.move(
      state.safe(node.url, {before: value, after: '>', ...tracker.current()})
    );
    value += tracker.move('>');
  } else {
    subexit = state.enter('destinationRaw');
    value += tracker.move(
      state.safe(node.url, {
        before: value,
        after: node.title ? ' ' : '\n',
        ...tracker.current()
      })
    );
  }
  subexit();
  if (node.title) {
    subexit = state.enter(`title${suffix}`);
    value += tracker.move(' ' + quote);
    value += tracker.move(
      state.safe(node.title, {
        before: value,
        after: quote,
        ...tracker.current()
      })
    );
    value += tracker.move(quote);
    subexit();
  }
  exit();
  return value
}

function checkEmphasis(state) {
  const marker = state.options.emphasis || '*';
  if (marker !== '*' && marker !== '_') {
    throw new Error(
      'Cannot serialize emphasis with `' +
        marker +
        '` for `options.emphasis`, expected `*`, or `_`'
    )
  }
  return marker
}

emphasis.peek = emphasisPeek;
function emphasis(node, _, state, info) {
  const marker = checkEmphasis(state);
  const exit = state.enter('emphasis');
  const tracker = state.createTracker(info);
  let value = tracker.move(marker);
  value += tracker.move(
    state.containerPhrasing(node, {
      before: value,
      after: marker,
      ...tracker.current()
    })
  );
  value += tracker.move(marker);
  exit();
  return value
}
function emphasisPeek(_, _1, state) {
  return state.options.emphasis || '*'
}

const convert$A =
  (
    function (test) {
      if (test === null || test === undefined) {
        return ok$A
      }
      if (typeof test === 'function') {
        return castFactory$A(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$A(test) : propsFactory$A(test)
      }
      if (typeof test === 'string') {
        return typeFactory$A(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$A(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$A(tests[index]);
  }
  return castFactory$A(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].apply(this, parameters)) return true
    }
    return false
  }
}
function propsFactory$A(check) {
  const checkAsRecord =  (check);
  return castFactory$A(all)
  function all(node) {
    const nodeAsRecord =  (
       (node)
    );
    let key;
    for (key in check) {
      if (nodeAsRecord[key] !== checkAsRecord[key]) return false
    }
    return true
  }
}
function typeFactory$A(check) {
  return castFactory$A(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$A(testFunction) {
  return check
  function check(value, index, parent) {
    return Boolean(
      looksLikeANode(value) &&
        testFunction.call(
          this,
          value,
          typeof index === 'number' ? index : undefined,
          parent || undefined
        )
    )
  }
}
function ok$A() {
  return true
}
function looksLikeANode(value) {
  return value !== null && typeof value === 'object' && 'type' in value
}

function color$B(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const empty = [];
const CONTINUE$A = true;
const EXIT$A = false;
const SKIP$A = 'skip';
function visitParents$A(tree, test, visitor, reverse) {
  let check;
  if (typeof test === 'function' && typeof visitor !== 'function') {
    reverse = visitor;
    visitor = test;
  } else {
    check = test;
  }
  const is = convert$A(check);
  const step = reverse ? -1 : 1;
  factory(tree, undefined, [])();
  function factory(node, index, parents) {
    const value =  (
      node && typeof node === 'object' ? node : {}
    );
    if (typeof value.type === 'string') {
      const name =
        typeof value.tagName === 'string'
          ? value.tagName
          :
          typeof value.name === 'string'
          ? value.name
          : undefined;
      Object.defineProperty(visit, 'name', {
        value:
          'node (' + color$B(node.type + (name ? '<' + name + '>' : '')) + ')'
      });
    }
    return visit
    function visit() {
      let result = empty;
      let subresult;
      let offset;
      let grandparents;
      if (!test || is(node, index, parents[parents.length - 1] || undefined)) {
        result = toResult$A(visitor(node, parents));
        if (result[0] === EXIT$A) {
          return result
        }
      }
      if ('children' in node && node.children) {
        const nodeAsParent =  (node);
        if (nodeAsParent.children && result[0] !== SKIP$A) {
          offset = (reverse ? nodeAsParent.children.length : -1) + step;
          grandparents = parents.concat(nodeAsParent);
          while (offset > -1 && offset < nodeAsParent.children.length) {
            const child = nodeAsParent.children[offset];
            subresult = factory(child, offset, grandparents)();
            if (subresult[0] === EXIT$A) {
              return subresult
            }
            offset =
              typeof subresult[1] === 'number' ? subresult[1] : offset + step;
          }
        }
      }
      return result
    }
  }
}
function toResult$A(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$A, value]
  }
  return value === null || value === undefined ? empty : [value]
}

function visit$A(tree, testOrVisitor, visitorOrReverse, maybeReverse) {
  let reverse;
  let test;
  let visitor;
  if (
    typeof testOrVisitor === 'function' &&
    typeof visitorOrReverse !== 'function'
  ) {
    test = undefined;
    visitor = testOrVisitor;
    reverse = visitorOrReverse;
  } else {
    test = testOrVisitor;
    visitor = visitorOrReverse;
    reverse = maybeReverse;
  }
  visitParents$A(tree, test, overload, reverse);
  function overload(node, parents) {
    const parent = parents[parents.length - 1];
    const index = parent ? parent.children.indexOf(node) : undefined;
    return visitor(node, index, parent)
  }
}

function formatHeadingAsSetext(node, state) {
  let literalWithBreak = false;
  visit$A(node, function (node) {
    if (
      ('value' in node && /\r?\n|\r/.test(node.value)) ||
      node.type === 'break'
    ) {
      literalWithBreak = true;
      return EXIT$A
    }
  });
  return Boolean(
    (!node.depth || node.depth < 3) &&
      toString$2(node) &&
      (state.options.setext || literalWithBreak)
  )
}

function heading(node, _, state, info) {
  const rank = Math.max(Math.min(6, node.depth || 1), 1);
  const tracker = state.createTracker(info);
  if (formatHeadingAsSetext(node, state)) {
    const exit = state.enter('headingSetext');
    const subexit = state.enter('phrasing');
    const value = state.containerPhrasing(node, {
      ...tracker.current(),
      before: '\n',
      after: '\n'
    });
    subexit();
    exit();
    return (
      value +
      '\n' +
      (rank === 1 ? '=' : '-').repeat(
        value.length -
          (Math.max(value.lastIndexOf('\r'), value.lastIndexOf('\n')) + 1)
      )
    )
  }
  const sequence = '#'.repeat(rank);
  const exit = state.enter('headingAtx');
  const subexit = state.enter('phrasing');
  tracker.move(sequence + ' ');
  let value = state.containerPhrasing(node, {
    before: '# ',
    after: '\n',
    ...tracker.current()
  });
  if (/^[\t ]/.test(value)) {
    value =
      '&#x' +
      value.charCodeAt(0).toString(16).toUpperCase() +
      ';' +
      value.slice(1);
  }
  value = value ? sequence + ' ' + value : sequence;
  if (state.options.closeAtx) {
    value += ' ' + sequence;
  }
  subexit();
  exit();
  return value
}

html.peek = htmlPeek;
function html(node) {
  return node.value || ''
}
function htmlPeek() {
  return '<'
}

image.peek = imagePeek;
function image(node, _, state, info) {
  const quote = checkQuote(state);
  const suffix = quote === '"' ? 'Quote' : 'Apostrophe';
  const exit = state.enter('image');
  let subexit = state.enter('label');
  const tracker = state.createTracker(info);
  let value = tracker.move('![');
  value += tracker.move(
    state.safe(node.alt, {before: value, after: ']', ...tracker.current()})
  );
  value += tracker.move('](');
  subexit();
  if (
    (!node.url && node.title) ||
    /[\0- \u007F]/.test(node.url)
  ) {
    subexit = state.enter('destinationLiteral');
    value += tracker.move('<');
    value += tracker.move(
      state.safe(node.url, {before: value, after: '>', ...tracker.current()})
    );
    value += tracker.move('>');
  } else {
    subexit = state.enter('destinationRaw');
    value += tracker.move(
      state.safe(node.url, {
        before: value,
        after: node.title ? ' ' : ')',
        ...tracker.current()
      })
    );
  }
  subexit();
  if (node.title) {
    subexit = state.enter(`title${suffix}`);
    value += tracker.move(' ' + quote);
    value += tracker.move(
      state.safe(node.title, {
        before: value,
        after: quote,
        ...tracker.current()
      })
    );
    value += tracker.move(quote);
    subexit();
  }
  value += tracker.move(')');
  exit();
  return value
}
function imagePeek() {
  return '!'
}

imageReference.peek = imageReferencePeek;
function imageReference(node, _, state, info) {
  const type = node.referenceType;
  const exit = state.enter('imageReference');
  let subexit = state.enter('label');
  const tracker = state.createTracker(info);
  let value = tracker.move('![');
  const alt = state.safe(node.alt, {
    before: value,
    after: ']',
    ...tracker.current()
  });
  value += tracker.move(alt + '][');
  subexit();
  const stack = state.stack;
  state.stack = [];
  subexit = state.enter('reference');
  const reference = state.safe(state.associationId(node), {
    before: value,
    after: ']',
    ...tracker.current()
  });
  subexit();
  state.stack = stack;
  exit();
  if (type === 'full' || !alt || alt !== reference) {
    value += tracker.move(reference + ']');
  } else if (type === 'shortcut') {
    value = value.slice(0, -1);
  } else {
    value += tracker.move(']');
  }
  return value
}
function imageReferencePeek() {
  return '!'
}

inlineCode.peek = inlineCodePeek;
function inlineCode(node, _, state) {
  let value = node.value || '';
  let sequence = '`';
  let index = -1;
  while (new RegExp('(^|[^`])' + sequence + '([^`]|$)').test(value)) {
    sequence += '`';
  }
  if (
    /[^ \r\n]/.test(value) &&
    ((/^[ \r\n]/.test(value) && /[ \r\n]$/.test(value)) || /^`|`$/.test(value))
  ) {
    value = ' ' + value + ' ';
  }
  while (++index < state.unsafe.length) {
    const pattern = state.unsafe[index];
    const expression = state.compilePattern(pattern);
    let match;
    if (!pattern.atBreak) continue
    while ((match = expression.exec(value))) {
      let position = match.index;
      if (
        value.charCodeAt(position) === 10  &&
        value.charCodeAt(position - 1) === 13
      ) {
        position--;
      }
      value = value.slice(0, position) + ' ' + value.slice(match.index + 1);
    }
  }
  return sequence + value + sequence
}
function inlineCodePeek() {
  return '`'
}

function formatLinkAsAutolink(node, state) {
  const raw = toString$2(node);
  return Boolean(
    !state.options.resourceLink &&
      node.url &&
      !node.title &&
      node.children &&
      node.children.length === 1 &&
      node.children[0].type === 'text' &&
      (raw === node.url || 'mailto:' + raw === node.url) &&
      /^[a-z][a-z+.-]+:/i.test(node.url) &&
      !/[\0- <>\u007F]/.test(node.url)
  )
}

link.peek = linkPeek;
function link(node, _, state, info) {
  const quote = checkQuote(state);
  const suffix = quote === '"' ? 'Quote' : 'Apostrophe';
  const tracker = state.createTracker(info);
  let exit;
  let subexit;
  if (formatLinkAsAutolink(node, state)) {
    const stack = state.stack;
    state.stack = [];
    exit = state.enter('autolink');
    let value = tracker.move('<');
    value += tracker.move(
      state.containerPhrasing(node, {
        before: value,
        after: '>',
        ...tracker.current()
      })
    );
    value += tracker.move('>');
    exit();
    state.stack = stack;
    return value
  }
  exit = state.enter('link');
  subexit = state.enter('label');
  let value = tracker.move('[');
  value += tracker.move(
    state.containerPhrasing(node, {
      before: value,
      after: '](',
      ...tracker.current()
    })
  );
  value += tracker.move('](');
  subexit();
  if (
    (!node.url && node.title) ||
    /[\0- \u007F]/.test(node.url)
  ) {
    subexit = state.enter('destinationLiteral');
    value += tracker.move('<');
    value += tracker.move(
      state.safe(node.url, {before: value, after: '>', ...tracker.current()})
    );
    value += tracker.move('>');
  } else {
    subexit = state.enter('destinationRaw');
    value += tracker.move(
      state.safe(node.url, {
        before: value,
        after: node.title ? ' ' : ')',
        ...tracker.current()
      })
    );
  }
  subexit();
  if (node.title) {
    subexit = state.enter(`title${suffix}`);
    value += tracker.move(' ' + quote);
    value += tracker.move(
      state.safe(node.title, {
        before: value,
        after: quote,
        ...tracker.current()
      })
    );
    value += tracker.move(quote);
    subexit();
  }
  value += tracker.move(')');
  exit();
  return value
}
function linkPeek(node, _, state) {
  return formatLinkAsAutolink(node, state) ? '<' : '['
}

linkReference.peek = linkReferencePeek;
function linkReference(node, _, state, info) {
  const type = node.referenceType;
  const exit = state.enter('linkReference');
  let subexit = state.enter('label');
  const tracker = state.createTracker(info);
  let value = tracker.move('[');
  const text = state.containerPhrasing(node, {
    before: value,
    after: ']',
    ...tracker.current()
  });
  value += tracker.move(text + '][');
  subexit();
  const stack = state.stack;
  state.stack = [];
  subexit = state.enter('reference');
  const reference = state.safe(state.associationId(node), {
    before: value,
    after: ']',
    ...tracker.current()
  });
  subexit();
  state.stack = stack;
  exit();
  if (type === 'full' || !text || text !== reference) {
    value += tracker.move(reference + ']');
  } else if (type === 'shortcut') {
    value = value.slice(0, -1);
  } else {
    value += tracker.move(']');
  }
  return value
}
function linkReferencePeek() {
  return '['
}

function checkBullet(state) {
  const marker = state.options.bullet || '*';
  if (marker !== '*' && marker !== '+' && marker !== '-') {
    throw new Error(
      'Cannot serialize items with `' +
        marker +
        '` for `options.bullet`, expected `*`, `+`, or `-`'
    )
  }
  return marker
}

function checkBulletOther(state) {
  const bullet = checkBullet(state);
  const bulletOther = state.options.bulletOther;
  if (!bulletOther) {
    return bullet === '*' ? '-' : '*'
  }
  if (bulletOther !== '*' && bulletOther !== '+' && bulletOther !== '-') {
    throw new Error(
      'Cannot serialize items with `' +
        bulletOther +
        '` for `options.bulletOther`, expected `*`, `+`, or `-`'
    )
  }
  if (bulletOther === bullet) {
    throw new Error(
      'Expected `bullet` (`' +
        bullet +
        '`) and `bulletOther` (`' +
        bulletOther +
        '`) to be different'
    )
  }
  return bulletOther
}

function checkBulletOrdered(state) {
  const marker = state.options.bulletOrdered || '.';
  if (marker !== '.' && marker !== ')') {
    throw new Error(
      'Cannot serialize items with `' +
        marker +
        '` for `options.bulletOrdered`, expected `.` or `)`'
    )
  }
  return marker
}

function checkRule(state) {
  const marker = state.options.rule || '*';
  if (marker !== '*' && marker !== '-' && marker !== '_') {
    throw new Error(
      'Cannot serialize rules with `' +
        marker +
        '` for `options.rule`, expected `*`, `-`, or `_`'
    )
  }
  return marker
}

function list(node, parent, state, info) {
  const exit = state.enter('list');
  const bulletCurrent = state.bulletCurrent;
  let bullet = node.ordered ? checkBulletOrdered(state) : checkBullet(state);
  const bulletOther = node.ordered
    ? bullet === '.'
      ? ')'
      : '.'
    : checkBulletOther(state);
  let useDifferentMarker =
    parent && state.bulletLastUsed ? bullet === state.bulletLastUsed : false;
  if (!node.ordered) {
    const firstListItem = node.children ? node.children[0] : undefined;
    if (
      (bullet === '*' || bullet === '-') &&
      firstListItem &&
      (!firstListItem.children || !firstListItem.children[0]) &&
      state.stack[state.stack.length - 1] === 'list' &&
      state.stack[state.stack.length - 2] === 'listItem' &&
      state.stack[state.stack.length - 3] === 'list' &&
      state.stack[state.stack.length - 4] === 'listItem' &&
      state.indexStack[state.indexStack.length - 1] === 0 &&
      state.indexStack[state.indexStack.length - 2] === 0 &&
      state.indexStack[state.indexStack.length - 3] === 0
    ) {
      useDifferentMarker = true;
    }
    if (checkRule(state) === bullet && firstListItem) {
      let index = -1;
      while (++index < node.children.length) {
        const item = node.children[index];
        if (
          item &&
          item.type === 'listItem' &&
          item.children &&
          item.children[0] &&
          item.children[0].type === 'thematicBreak'
        ) {
          useDifferentMarker = true;
          break
        }
      }
    }
  }
  if (useDifferentMarker) {
    bullet = bulletOther;
  }
  state.bulletCurrent = bullet;
  const value = state.containerFlow(node, info);
  state.bulletLastUsed = bullet;
  state.bulletCurrent = bulletCurrent;
  exit();
  return value
}

function checkListItemIndent(state) {
  const style = state.options.listItemIndent || 'one';
  if (style !== 'tab' && style !== 'one' && style !== 'mixed') {
    throw new Error(
      'Cannot serialize items with `' +
        style +
        '` for `options.listItemIndent`, expected `tab`, `one`, or `mixed`'
    )
  }
  return style
}

function listItem(node, parent, state, info) {
  const listItemIndent = checkListItemIndent(state);
  let bullet = state.bulletCurrent || checkBullet(state);
  if (parent && parent.type === 'list' && parent.ordered) {
    bullet =
      (typeof parent.start === 'number' && parent.start > -1
        ? parent.start
        : 1) +
      (state.options.incrementListMarker === false
        ? 0
        : parent.children.indexOf(node)) +
      bullet;
  }
  let size = bullet.length + 1;
  if (
    listItemIndent === 'tab' ||
    (listItemIndent === 'mixed' &&
      ((parent && parent.type === 'list' && parent.spread) || node.spread))
  ) {
    size = Math.ceil(size / 4) * 4;
  }
  const tracker = state.createTracker(info);
  tracker.move(bullet + ' '.repeat(size - bullet.length));
  tracker.shift(size);
  const exit = state.enter('listItem');
  const value = state.indentLines(
    state.containerFlow(node, tracker.current()),
    map
  );
  exit();
  return value
  function map(line, index, blank) {
    if (index) {
      return (blank ? '' : ' '.repeat(size)) + line
    }
    return (blank ? bullet : bullet + ' '.repeat(size - bullet.length)) + line
  }
}

function paragraph(node, _, state, info) {
  const exit = state.enter('paragraph');
  const subexit = state.enter('phrasing');
  const value = state.containerPhrasing(node, info);
  subexit();
  exit();
  return value
}

const phrasing =
  (
    convert$A([
      'break',
      'delete',
      'emphasis',
      'footnote',
      'footnoteReference',
      'image',
      'imageReference',
      'inlineCode',
      'link',
      'linkReference',
      'strong',
      'text'
    ])
  );

function root(node, _, state, info) {
  const hasPhrasing = node.children.some(function (d) {
    return phrasing(d)
  });
  const fn = hasPhrasing ? state.containerPhrasing : state.containerFlow;
  return fn.call(state, node, info)
}

function checkStrong(state) {
  const marker = state.options.strong || '*';
  if (marker !== '*' && marker !== '_') {
    throw new Error(
      'Cannot serialize strong with `' +
        marker +
        '` for `options.strong`, expected `*`, or `_`'
    )
  }
  return marker
}

strong.peek = strongPeek;
function strong(node, _, state, info) {
  const marker = checkStrong(state);
  const exit = state.enter('strong');
  const tracker = state.createTracker(info);
  let value = tracker.move(marker + marker);
  value += tracker.move(
    state.containerPhrasing(node, {
      before: value,
      after: marker,
      ...tracker.current()
    })
  );
  value += tracker.move(marker + marker);
  exit();
  return value
}
function strongPeek(_, _1, state) {
  return state.options.strong || '*'
}

function text$1(node, _, state, info) {
  return state.safe(node.value, info)
}

function checkRuleRepetition(state) {
  const repetition = state.options.ruleRepetition || 3;
  if (repetition < 3) {
    throw new Error(
      'Cannot serialize rules with repetition `' +
        repetition +
        '` for `options.ruleRepetition`, expected `3` or more'
    )
  }
  return repetition
}

function thematicBreak(_, _1, state) {
  const value = (
    checkRule(state) + (state.options.ruleSpaces ? ' ' : '')
  ).repeat(checkRuleRepetition(state));
  return state.options.ruleSpaces ? value.slice(0, -1) : value
}

const handle = {
  blockquote,
  break: hardBreak,
  code: code$1,
  definition,
  emphasis,
  hardBreak,
  heading,
  html,
  image,
  imageReference,
  inlineCode,
  link,
  linkReference,
  list,
  listItem,
  paragraph,
  root,
  strong,
  text: text$1,
  thematicBreak
};

const join = [joinDefaults];
function joinDefaults(left, right, parent, state) {
  if (
    right.type === 'code' &&
    formatCodeAsIndented(right, state) &&
    (left.type === 'list' ||
      (left.type === right.type && formatCodeAsIndented(left, state)))
  ) {
    return false
  }
  if ('spread' in parent && typeof parent.spread === 'boolean') {
    if (
      left.type === 'paragraph' &&
      (left.type === right.type ||
        right.type === 'definition' ||
        (right.type === 'heading' && formatHeadingAsSetext(right, state)))
    ) {
      return
    }
    return parent.spread ? 1 : 0
  }
}

const fullPhrasingSpans = [
  'autolink',
  'destinationLiteral',
  'destinationRaw',
  'reference',
  'titleQuote',
  'titleApostrophe'
];
const unsafe = [
  {character: '\t', after: '[\\r\\n]', inConstruct: 'phrasing'},
  {character: '\t', before: '[\\r\\n]', inConstruct: 'phrasing'},
  {
    character: '\t',
    inConstruct: ['codeFencedLangGraveAccent', 'codeFencedLangTilde']
  },
  {
    character: '\r',
    inConstruct: [
      'codeFencedLangGraveAccent',
      'codeFencedLangTilde',
      'codeFencedMetaGraveAccent',
      'codeFencedMetaTilde',
      'destinationLiteral',
      'headingAtx'
    ]
  },
  {
    character: '\n',
    inConstruct: [
      'codeFencedLangGraveAccent',
      'codeFencedLangTilde',
      'codeFencedMetaGraveAccent',
      'codeFencedMetaTilde',
      'destinationLiteral',
      'headingAtx'
    ]
  },
  {character: ' ', after: '[\\r\\n]', inConstruct: 'phrasing'},
  {character: ' ', before: '[\\r\\n]', inConstruct: 'phrasing'},
  {
    character: ' ',
    inConstruct: ['codeFencedLangGraveAccent', 'codeFencedLangTilde']
  },
  {
    character: '!',
    after: '\\[',
    inConstruct: 'phrasing',
    notInConstruct: fullPhrasingSpans
  },
  {character: '"', inConstruct: 'titleQuote'},
  {atBreak: true, character: '#'},
  {character: '#', inConstruct: 'headingAtx', after: '(?:[\r\n]|$)'},
  {character: '&', after: '[#A-Za-z]', inConstruct: 'phrasing'},
  {character: "'", inConstruct: 'titleApostrophe'},
  {character: '(', inConstruct: 'destinationRaw'},
  {
    before: '\\]',
    character: '(',
    inConstruct: 'phrasing',
    notInConstruct: fullPhrasingSpans
  },
  {atBreak: true, before: '\\d+', character: ')'},
  {character: ')', inConstruct: 'destinationRaw'},
  {atBreak: true, character: '*', after: '(?:[ \t\r\n*])'},
  {character: '*', inConstruct: 'phrasing', notInConstruct: fullPhrasingSpans},
  {atBreak: true, character: '+', after: '(?:[ \t\r\n])'},
  {atBreak: true, character: '-', after: '(?:[ \t\r\n-])'},
  {atBreak: true, before: '\\d+', character: '.', after: '(?:[ \t\r\n]|$)'},
  {atBreak: true, character: '<', after: '[!/?A-Za-z]'},
  {
    character: '<',
    after: '[!/?A-Za-z]',
    inConstruct: 'phrasing',
    notInConstruct: fullPhrasingSpans
  },
  {character: '<', inConstruct: 'destinationLiteral'},
  {atBreak: true, character: '='},
  {atBreak: true, character: '>'},
  {character: '>', inConstruct: 'destinationLiteral'},
  {atBreak: true, character: '['},
  {character: '[', inConstruct: 'phrasing', notInConstruct: fullPhrasingSpans},
  {character: '[', inConstruct: ['label', 'reference']},
  {character: '\\', after: '[\\r\\n]', inConstruct: 'phrasing'},
  {character: ']', inConstruct: ['label', 'reference']},
  {atBreak: true, character: '_'},
  {character: '_', inConstruct: 'phrasing', notInConstruct: fullPhrasingSpans},
  {atBreak: true, character: '`'},
  {
    character: '`',
    inConstruct: ['codeFencedLangGraveAccent', 'codeFencedMetaGraveAccent']
  },
  {character: '`', inConstruct: 'phrasing', notInConstruct: fullPhrasingSpans},
  {atBreak: true, character: '~'}
];

function association(node) {
  if (node.label || !node.identifier) {
    return node.label || ''
  }
  return decodeString(node.identifier)
}

function compilePattern(pattern) {
  if (!pattern._compiled) {
    const before =
      (pattern.atBreak ? '[\\r\\n][\\t ]*' : '') +
      (pattern.before ? '(?:' + pattern.before + ')' : '');
    pattern._compiled = new RegExp(
      (before ? '(' + before + ')' : '') +
        (/[|\\{}()[\]^$+*?.-]/.test(pattern.character) ? '\\' : '') +
        pattern.character +
        (pattern.after ? '(?:' + pattern.after + ')' : ''),
      'g'
    );
  }
  return pattern._compiled
}

function containerPhrasing(parent, state, info) {
  const indexStack = state.indexStack;
  const children = parent.children || [];
  const results = [];
  let index = -1;
  let before = info.before;
  indexStack.push(-1);
  let tracker = state.createTracker(info);
  while (++index < children.length) {
    const child = children[index];
    let after;
    indexStack[indexStack.length - 1] = index;
    if (index + 1 < children.length) {
      let handle = state.handle.handlers[children[index + 1].type];
      if (handle && handle.peek) handle = handle.peek;
      after = handle
        ? handle(children[index + 1], parent, state, {
            before: '',
            after: '',
            ...tracker.current()
          }).charAt(0)
        : '';
    } else {
      after = info.after;
    }
    if (
      results.length > 0 &&
      (before === '\r' || before === '\n') &&
      child.type === 'html'
    ) {
      results[results.length - 1] = results[results.length - 1].replace(
        /(\r?\n|\r)$/,
        ' '
      );
      before = ' ';
      tracker = state.createTracker(info);
      tracker.move(results.join(''));
    }
    results.push(
      tracker.move(
        state.handle(child, parent, state, {
          ...tracker.current(),
          before,
          after
        })
      )
    );
    before = results[results.length - 1].slice(-1);
  }
  indexStack.pop();
  return results.join('')
}

function containerFlow(parent, state, info) {
  const indexStack = state.indexStack;
  const children = parent.children || [];
  const tracker = state.createTracker(info);
  const results = [];
  let index = -1;
  indexStack.push(-1);
  while (++index < children.length) {
    const child = children[index];
    indexStack[indexStack.length - 1] = index;
    results.push(
      tracker.move(
        state.handle(child, parent, state, {
          before: '\n',
          after: '\n',
          ...tracker.current()
        })
      )
    );
    if (child.type !== 'list') {
      state.bulletLastUsed = undefined;
    }
    if (index < children.length - 1) {
      results.push(
        tracker.move(between(child, children[index + 1], parent, state))
      );
    }
  }
  indexStack.pop();
  return results.join('')
}
function between(left, right, parent, state) {
  let index = state.join.length;
  while (index--) {
    const result = state.join[index](left, right, parent, state);
    if (result === true || result === 1) {
      break
    }
    if (typeof result === 'number') {
      return '\n'.repeat(1 + result)
    }
    if (result === false) {
      return '\n\n<!---->\n\n'
    }
  }
  return '\n\n'
}

const eol$1 = /\r?\n|\r/g;
function indentLines(value, map) {
  const result = [];
  let start = 0;
  let line = 0;
  let match;
  while ((match = eol$1.exec(value))) {
    one(value.slice(start, match.index));
    result.push(match[0]);
    start = match.index + match[0].length;
    line++;
  }
  one(value.slice(start));
  return result.join('')
  function one(value) {
    result.push(map(value, line, !value));
  }
}

function safe(state, input, config) {
  const value = (config.before || '') + (input || '') + (config.after || '');
  const positions = [];
  const result = [];
  const infos = {};
  let index = -1;
  while (++index < state.unsafe.length) {
    const pattern = state.unsafe[index];
    if (!patternInScope(state.stack, pattern)) {
      continue
    }
    const expression = state.compilePattern(pattern);
    let match;
    while ((match = expression.exec(value))) {
      const before = 'before' in pattern || Boolean(pattern.atBreak);
      const after = 'after' in pattern;
      const position = match.index + (before ? match[1].length : 0);
      if (positions.includes(position)) {
        if (infos[position].before && !before) {
          infos[position].before = false;
        }
        if (infos[position].after && !after) {
          infos[position].after = false;
        }
      } else {
        positions.push(position);
        infos[position] = {before, after};
      }
    }
  }
  positions.sort(numerical);
  let start = config.before ? config.before.length : 0;
  const end = value.length - (config.after ? config.after.length : 0);
  index = -1;
  while (++index < positions.length) {
    const position = positions[index];
    if (position < start || position >= end) {
      continue
    }
    if (
      (position + 1 < end &&
        positions[index + 1] === position + 1 &&
        infos[position].after &&
        !infos[position + 1].before &&
        !infos[position + 1].after) ||
      (positions[index - 1] === position - 1 &&
        infos[position].before &&
        !infos[position - 1].before &&
        !infos[position - 1].after)
    ) {
      continue
    }
    if (start !== position) {
      result.push(escapeBackslashes(value.slice(start, position), '\\'));
    }
    start = position;
    if (
      /[!-/:-@[-`{-~]/.test(value.charAt(position)) &&
      (!config.encode || !config.encode.includes(value.charAt(position)))
    ) {
      result.push('\\');
    } else {
      result.push(
        '&#x' + value.charCodeAt(position).toString(16).toUpperCase() + ';'
      );
      start++;
    }
  }
  result.push(escapeBackslashes(value.slice(start, end), config.after));
  return result.join('')
}
function numerical(a, b) {
  return a - b
}
function escapeBackslashes(value, after) {
  const expression = /\\(?=[!-/:-@[-`{-~])/g;
  const positions = [];
  const results = [];
  const whole = value + after;
  let index = -1;
  let start = 0;
  let match;
  while ((match = expression.exec(whole))) {
    positions.push(match.index);
  }
  while (++index < positions.length) {
    if (start !== positions[index]) {
      results.push(value.slice(start, positions[index]));
    }
    results.push('\\');
    start = positions[index];
  }
  results.push(value.slice(start));
  return results.join('')
}

function track(config) {
  const options = config || {};
  const now = options.now || {};
  let lineShift = options.lineShift || 0;
  let line = now.line || 1;
  let column = now.column || 1;
  return {move, current, shift}
  function current() {
    return {now: {line, column}, lineShift}
  }
  function shift(value) {
    lineShift += value;
  }
  function move(input) {
    const value = input || '';
    const chunks = value.split(/\r?\n|\r/g);
    const tail = chunks[chunks.length - 1];
    line += chunks.length - 1;
    column =
      chunks.length === 1 ? column + tail.length : 1 + tail.length + lineShift;
    return value
  }
}

function toMarkdown(tree, options = {}) {
  const state = {
    enter,
    indentLines,
    associationId: association,
    containerPhrasing: containerPhrasingBound,
    containerFlow: containerFlowBound,
    createTracker: track,
    compilePattern,
    safe: safeBound,
    stack: [],
    unsafe: [...unsafe],
    join: [...join],
    handlers: {...handle},
    options: {},
    indexStack: [],
    handle: undefined
  };
  configure(state, options);
  if (state.options.tightDefinitions) {
    state.join.push(joinDefinition);
  }
  state.handle = zwitch('type', {
    invalid,
    unknown,
    handlers: state.handlers
  });
  let result = state.handle(tree, undefined, state, {
    before: '\n',
    after: '\n',
    now: {line: 1, column: 1},
    lineShift: 0
  });
  if (
    result &&
    result.charCodeAt(result.length - 1) !== 10 &&
    result.charCodeAt(result.length - 1) !== 13
  ) {
    result += '\n';
  }
  return result
  function enter(name) {
    state.stack.push(name);
    return exit
    function exit() {
      state.stack.pop();
    }
  }
}
function invalid(value) {
  throw new Error('Cannot handle value `' + value + '`, expected node')
}
function unknown(value) {
  const node =  (value);
  throw new Error('Cannot handle unknown node `' + node.type + '`')
}
function joinDefinition(left, right) {
  if (left.type === 'definition' && left.type === right.type) {
    return 0
  }
}
function containerPhrasingBound(parent, info) {
  return containerPhrasing(parent, this, info)
}
function containerFlowBound(parent, info) {
  return containerFlow(parent, this, info)
}
function safeBound(value, config) {
  return safe(this, value, config)
}

function remarkStringify(options) {
  const self = this;
  self.compiler = compiler;
  function compiler(tree) {
    return toMarkdown(tree, {
      ...self.data('settings'),
      ...options,
      extensions: self.data('toMarkdownExtensions') || []
    })
  }
}

function ccount(value, character) {
  const source = String(value);
  if (typeof character !== 'string') {
    throw new TypeError('Expected character')
  }
  let count = 0;
  let index = source.indexOf(character);
  while (index !== -1) {
    count++;
    index = source.indexOf(character, index + character.length);
  }
  return count
}

function escapeStringRegexp(string) {
	if (typeof string !== 'string') {
		throw new TypeError('Expected a string');
	}
	return string
		.replace(/[|\\{}()[\]^$+*?.]/g, '\\$&')
		.replace(/-/g, '\\x2d');
}

function findAndReplace(tree, list, options) {
  const settings = options || {};
  const ignored = convert$A(settings.ignore || []);
  const pairs = toPairs(list);
  let pairIndex = -1;
  while (++pairIndex < pairs.length) {
    visitParents$A(tree, 'text', visitor);
  }
  function visitor(node, parents) {
    let index = -1;
    let grandparent;
    while (++index < parents.length) {
      const parent = parents[index];
      const siblings = grandparent ? grandparent.children : undefined;
      if (
        ignored(
          parent,
          siblings ? siblings.indexOf(parent) : undefined,
          grandparent
        )
      ) {
        return
      }
      grandparent = parent;
    }
    if (grandparent) {
      return handler(node, parents)
    }
  }
  function handler(node, parents) {
    const parent = parents[parents.length - 1];
    const find = pairs[pairIndex][0];
    const replace = pairs[pairIndex][1];
    let start = 0;
    const siblings = parent.children;
    const index = siblings.indexOf(node);
    let change = false;
    let nodes = [];
    find.lastIndex = 0;
    let match = find.exec(node.value);
    while (match) {
      const position = match.index;
      const matchObject = {
        index: match.index,
        input: match.input,
        stack: [...parents, node]
      };
      let value = replace(...match, matchObject);
      if (typeof value === 'string') {
        value = value.length > 0 ? {type: 'text', value} : undefined;
      }
      if (value === false) {
        find.lastIndex = position + 1;
      } else {
        if (start !== position) {
          nodes.push({
            type: 'text',
            value: node.value.slice(start, position)
          });
        }
        if (Array.isArray(value)) {
          nodes.push(...value);
        } else if (value) {
          nodes.push(value);
        }
        start = position + match[0].length;
        change = true;
      }
      if (!find.global) {
        break
      }
      match = find.exec(node.value);
    }
    if (change) {
      if (start < node.value.length) {
        nodes.push({type: 'text', value: node.value.slice(start)});
      }
      parent.children.splice(index, 1, ...nodes);
    } else {
      nodes = [node];
    }
    return index + nodes.length
  }
}
function toPairs(tupleOrList) {
  const result = [];
  if (!Array.isArray(tupleOrList)) {
    throw new TypeError('Expected find and replace tuple or list of tuples')
  }
  const list =
    !tupleOrList[0] || Array.isArray(tupleOrList[0])
      ? tupleOrList
      : [tupleOrList];
  let index = -1;
  while (++index < list.length) {
    const tuple = list[index];
    result.push([toExpression(tuple[0]), toFunction(tuple[1])]);
  }
  return result
}
function toExpression(find) {
  return typeof find === 'string' ? new RegExp(escapeStringRegexp(find), 'g') : find
}
function toFunction(replace) {
  return typeof replace === 'function'
    ? replace
    : function () {
        return replace
      }
}

const inConstruct = 'phrasing';
const notInConstruct = ['autolink', 'link', 'image', 'label'];
function gfmAutolinkLiteralFromMarkdown() {
  return {
    transforms: [transformGfmAutolinkLiterals],
    enter: {
      literalAutolink: enterLiteralAutolink,
      literalAutolinkEmail: enterLiteralAutolinkValue,
      literalAutolinkHttp: enterLiteralAutolinkValue,
      literalAutolinkWww: enterLiteralAutolinkValue
    },
    exit: {
      literalAutolink: exitLiteralAutolink,
      literalAutolinkEmail: exitLiteralAutolinkEmail,
      literalAutolinkHttp: exitLiteralAutolinkHttp,
      literalAutolinkWww: exitLiteralAutolinkWww
    }
  }
}
function gfmAutolinkLiteralToMarkdown() {
  return {
    unsafe: [
      {
        character: '@',
        before: '[+\\-.\\w]',
        after: '[\\-.\\w]',
        inConstruct,
        notInConstruct
      },
      {
        character: '.',
        before: '[Ww]',
        after: '[\\-.\\w]',
        inConstruct,
        notInConstruct
      },
      {
        character: ':',
        before: '[ps]',
        after: '\\/',
        inConstruct,
        notInConstruct
      }
    ]
  }
}
function enterLiteralAutolink(token) {
  this.enter({type: 'link', title: null, url: '', children: []}, token);
}
function enterLiteralAutolinkValue(token) {
  this.config.enter.autolinkProtocol.call(this, token);
}
function exitLiteralAutolinkHttp(token) {
  this.config.exit.autolinkProtocol.call(this, token);
}
function exitLiteralAutolinkWww(token) {
  this.config.exit.data.call(this, token);
  const node = this.stack[this.stack.length - 1];
  ok$B(node.type === 'link');
  node.url = 'http://' + this.sliceSerialize(token);
}
function exitLiteralAutolinkEmail(token) {
  this.config.exit.autolinkEmail.call(this, token);
}
function exitLiteralAutolink(token) {
  this.exit(token);
}
function transformGfmAutolinkLiterals(tree) {
  findAndReplace(
    tree,
    [
      [/(https?:\/\/|www(?=\.))([-.\w]+)([^ \t\r\n]*)/gi, findUrl],
      [/([-.\w+]+)@([-\w]+(?:\.[-\w]+)+)/g, findEmail]
    ],
    {ignore: ['link', 'linkReference']}
  );
}
function findUrl(_, protocol, domain, path, match) {
  let prefix = '';
  if (!previous(match)) {
    return false
  }
  if (/^w/i.test(protocol)) {
    domain = protocol + domain;
    protocol = '';
    prefix = 'http://';
  }
  if (!isCorrectDomain(domain)) {
    return false
  }
  const parts = splitUrl(domain + path);
  if (!parts[0]) return false
  const result = {
    type: 'link',
    title: null,
    url: prefix + protocol + parts[0],
    children: [{type: 'text', value: protocol + parts[0]}]
  };
  if (parts[1]) {
    return [result, {type: 'text', value: parts[1]}]
  }
  return result
}
function findEmail(_, atext, label, match) {
  if (
    !previous(match, true) ||
    /[-\d_]$/.test(label)
  ) {
    return false
  }
  return {
    type: 'link',
    title: null,
    url: 'mailto:' + atext + '@' + label,
    children: [{type: 'text', value: atext + '@' + label}]
  }
}
function isCorrectDomain(domain) {
  const parts = domain.split('.');
  if (
    parts.length < 2 ||
    (parts[parts.length - 1] &&
      (/_/.test(parts[parts.length - 1]) ||
        !/[a-zA-Z\d]/.test(parts[parts.length - 1]))) ||
    (parts[parts.length - 2] &&
      (/_/.test(parts[parts.length - 2]) ||
        !/[a-zA-Z\d]/.test(parts[parts.length - 2])))
  ) {
    return false
  }
  return true
}
function splitUrl(url) {
  const trailExec = /[!"&'),.:;<>?\]}]+$/.exec(url);
  if (!trailExec) {
    return [url, undefined]
  }
  url = url.slice(0, trailExec.index);
  let trail = trailExec[0];
  let closingParenIndex = trail.indexOf(')');
  const openingParens = ccount(url, '(');
  let closingParens = ccount(url, ')');
  while (closingParenIndex !== -1 && openingParens > closingParens) {
    url += trail.slice(0, closingParenIndex + 1);
    trail = trail.slice(closingParenIndex + 1);
    closingParenIndex = trail.indexOf(')');
    closingParens++;
  }
  return [url, trail]
}
function previous(match, email) {
  const code = match.input.charCodeAt(match.index - 1);
  return (
    (match.index === 0 ||
      unicodeWhitespace(code) ||
      unicodePunctuation(code)) &&
    (!email || code !== 47)
  )
}

footnoteReference.peek = footnoteReferencePeek;
function gfmFootnoteFromMarkdown() {
  return {
    enter: {
      gfmFootnoteDefinition: enterFootnoteDefinition,
      gfmFootnoteDefinitionLabelString: enterFootnoteDefinitionLabelString,
      gfmFootnoteCall: enterFootnoteCall,
      gfmFootnoteCallString: enterFootnoteCallString
    },
    exit: {
      gfmFootnoteDefinition: exitFootnoteDefinition,
      gfmFootnoteDefinitionLabelString: exitFootnoteDefinitionLabelString,
      gfmFootnoteCall: exitFootnoteCall,
      gfmFootnoteCallString: exitFootnoteCallString
    }
  }
}
function gfmFootnoteToMarkdown() {
  return {
    unsafe: [{character: '[', inConstruct: ['phrasing', 'label', 'reference']}],
    handlers: {footnoteDefinition, footnoteReference}
  }
}
function enterFootnoteDefinition(token) {
  this.enter(
    {type: 'footnoteDefinition', identifier: '', label: '', children: []},
    token
  );
}
function enterFootnoteDefinitionLabelString() {
  this.buffer();
}
function exitFootnoteDefinitionLabelString(token) {
  const label = this.resume();
  const node = this.stack[this.stack.length - 1];
  ok$B(node.type === 'footnoteDefinition');
  node.label = label;
  node.identifier = normalizeIdentifier$1(
    this.sliceSerialize(token)
  ).toLowerCase();
}
function exitFootnoteDefinition(token) {
  this.exit(token);
}
function enterFootnoteCall(token) {
  this.enter({type: 'footnoteReference', identifier: '', label: ''}, token);
}
function enterFootnoteCallString() {
  this.buffer();
}
function exitFootnoteCallString(token) {
  const label = this.resume();
  const node = this.stack[this.stack.length - 1];
  ok$B(node.type === 'footnoteReference');
  node.label = label;
  node.identifier = normalizeIdentifier$1(
    this.sliceSerialize(token)
  ).toLowerCase();
}
function exitFootnoteCall(token) {
  this.exit(token);
}
function footnoteReference(node, _, state, info) {
  const tracker = state.createTracker(info);
  let value = tracker.move('[^');
  const exit = state.enter('footnoteReference');
  const subexit = state.enter('reference');
  value += tracker.move(
    state.safe(state.associationId(node), {
      ...tracker.current(),
      before: value,
      after: ']'
    })
  );
  subexit();
  exit();
  value += tracker.move(']');
  return value
}
function footnoteReferencePeek() {
  return '['
}
function footnoteDefinition(node, _, state, info) {
  const tracker = state.createTracker(info);
  let value = tracker.move('[^');
  const exit = state.enter('footnoteDefinition');
  const subexit = state.enter('label');
  value += tracker.move(
    state.safe(state.associationId(node), {
      ...tracker.current(),
      before: value,
      after: ']'
    })
  );
  subexit();
  value += tracker.move(
    ']:' + (node.children && node.children.length > 0 ? ' ' : '')
  );
  tracker.shift(4);
  value += tracker.move(
    state.indentLines(state.containerFlow(node, tracker.current()), map$1)
  );
  exit();
  return value
}
function map$1(line, index, blank) {
  if (index === 0) {
    return line
  }
  return (blank ? '' : '    ') + line
}

const constructsWithoutStrikethrough = [
  'autolink',
  'destinationLiteral',
  'destinationRaw',
  'reference',
  'titleQuote',
  'titleApostrophe'
];
handleDelete.peek = peekDelete;
function gfmStrikethroughFromMarkdown() {
  return {
    canContainEols: ['delete'],
    enter: {strikethrough: enterStrikethrough},
    exit: {strikethrough: exitStrikethrough}
  }
}
function gfmStrikethroughToMarkdown() {
  return {
    unsafe: [
      {
        character: '~',
        inConstruct: 'phrasing',
        notInConstruct: constructsWithoutStrikethrough
      }
    ],
    handlers: {delete: handleDelete}
  }
}
function enterStrikethrough(token) {
  this.enter({type: 'delete', children: []}, token);
}
function exitStrikethrough(token) {
  this.exit(token);
}
function handleDelete(node, _, state, info) {
  const tracker = state.createTracker(info);
  const exit = state.enter('strikethrough');
  let value = tracker.move('~~');
  value += state.containerPhrasing(node, {
    ...tracker.current(),
    before: value,
    after: '~'
  });
  value += tracker.move('~~');
  exit();
  return value
}
function peekDelete() {
  return '~'
}

function markdownTable(table, options = {}) {
  const align = (options.align || []).concat();
  const stringLength = options.stringLength || defaultStringLength;
  const alignments = [];
  const cellMatrix = [];
  const sizeMatrix = [];
  const longestCellByColumn = [];
  let mostCellsPerRow = 0;
  let rowIndex = -1;
  while (++rowIndex < table.length) {
    const row = [];
    const sizes = [];
    let columnIndex = -1;
    if (table[rowIndex].length > mostCellsPerRow) {
      mostCellsPerRow = table[rowIndex].length;
    }
    while (++columnIndex < table[rowIndex].length) {
      const cell = serialize(table[rowIndex][columnIndex]);
      if (options.alignDelimiters !== false) {
        const size = stringLength(cell);
        sizes[columnIndex] = size;
        if (
          longestCellByColumn[columnIndex] === undefined ||
          size > longestCellByColumn[columnIndex]
        ) {
          longestCellByColumn[columnIndex] = size;
        }
      }
      row.push(cell);
    }
    cellMatrix[rowIndex] = row;
    sizeMatrix[rowIndex] = sizes;
  }
  let columnIndex = -1;
  if (typeof align === 'object' && 'length' in align) {
    while (++columnIndex < mostCellsPerRow) {
      alignments[columnIndex] = toAlignment(align[columnIndex]);
    }
  } else {
    const code = toAlignment(align);
    while (++columnIndex < mostCellsPerRow) {
      alignments[columnIndex] = code;
    }
  }
  columnIndex = -1;
  const row = [];
  const sizes = [];
  while (++columnIndex < mostCellsPerRow) {
    const code = alignments[columnIndex];
    let before = '';
    let after = '';
    if (code === 99 ) {
      before = ':';
      after = ':';
    } else if (code === 108 ) {
      before = ':';
    } else if (code === 114 ) {
      after = ':';
    }
    let size =
      options.alignDelimiters === false
        ? 1
        : Math.max(
            1,
            longestCellByColumn[columnIndex] - before.length - after.length
          );
    const cell = before + '-'.repeat(size) + after;
    if (options.alignDelimiters !== false) {
      size = before.length + size + after.length;
      if (size > longestCellByColumn[columnIndex]) {
        longestCellByColumn[columnIndex] = size;
      }
      sizes[columnIndex] = size;
    }
    row[columnIndex] = cell;
  }
  cellMatrix.splice(1, 0, row);
  sizeMatrix.splice(1, 0, sizes);
  rowIndex = -1;
  const lines = [];
  while (++rowIndex < cellMatrix.length) {
    const row = cellMatrix[rowIndex];
    const sizes = sizeMatrix[rowIndex];
    columnIndex = -1;
    const line = [];
    while (++columnIndex < mostCellsPerRow) {
      const cell = row[columnIndex] || '';
      let before = '';
      let after = '';
      if (options.alignDelimiters !== false) {
        const size =
          longestCellByColumn[columnIndex] - (sizes[columnIndex] || 0);
        const code = alignments[columnIndex];
        if (code === 114 ) {
          before = ' '.repeat(size);
        } else if (code === 99 ) {
          if (size % 2) {
            before = ' '.repeat(size / 2 + 0.5);
            after = ' '.repeat(size / 2 - 0.5);
          } else {
            before = ' '.repeat(size / 2);
            after = before;
          }
        } else {
          after = ' '.repeat(size);
        }
      }
      if (options.delimiterStart !== false && !columnIndex) {
        line.push('|');
      }
      if (
        options.padding !== false &&
        !(options.alignDelimiters === false && cell === '') &&
        (options.delimiterStart !== false || columnIndex)
      ) {
        line.push(' ');
      }
      if (options.alignDelimiters !== false) {
        line.push(before);
      }
      line.push(cell);
      if (options.alignDelimiters !== false) {
        line.push(after);
      }
      if (options.padding !== false) {
        line.push(' ');
      }
      if (
        options.delimiterEnd !== false ||
        columnIndex !== mostCellsPerRow - 1
      ) {
        line.push('|');
      }
    }
    lines.push(
      options.delimiterEnd === false
        ? line.join('').replace(/ +$/, '')
        : line.join('')
    );
  }
  return lines.join('\n')
}
function serialize(value) {
  return value === null || value === undefined ? '' : String(value)
}
function defaultStringLength(value) {
  return value.length
}
function toAlignment(value) {
  const code = typeof value === 'string' ? value.codePointAt(0) : 0;
  return code === 67  || code === 99
    ? 99
    : code === 76  || code === 108
    ? 108
    : code === 82  || code === 114
    ? 114
    : 0
}

function gfmTableFromMarkdown() {
  return {
    enter: {
      table: enterTable,
      tableData: enterCell,
      tableHeader: enterCell,
      tableRow: enterRow
    },
    exit: {
      codeText: exitCodeText,
      table: exitTable,
      tableData: exit,
      tableHeader: exit,
      tableRow: exit
    }
  }
}
function enterTable(token) {
  const align = token._align;
  this.enter(
    {
      type: 'table',
      align: align.map(function (d) {
        return d === 'none' ? null : d
      }),
      children: []
    },
    token
  );
  this.data.inTable = true;
}
function exitTable(token) {
  this.exit(token);
  this.data.inTable = undefined;
}
function enterRow(token) {
  this.enter({type: 'tableRow', children: []}, token);
}
function exit(token) {
  this.exit(token);
}
function enterCell(token) {
  this.enter({type: 'tableCell', children: []}, token);
}
function exitCodeText(token) {
  let value = this.resume();
  if (this.data.inTable) {
    value = value.replace(/\\([\\|])/g, replace);
  }
  const node = this.stack[this.stack.length - 1];
  ok$B(node.type === 'inlineCode');
  node.value = value;
  this.exit(token);
}
function replace($0, $1) {
  return $1 === '|' ? $1 : $0
}
function gfmTableToMarkdown(options) {
  const settings = options || {};
  const padding = settings.tableCellPadding;
  const alignDelimiters = settings.tablePipeAlign;
  const stringLength = settings.stringLength;
  const around = padding ? ' ' : '|';
  return {
    unsafe: [
      {character: '\r', inConstruct: 'tableCell'},
      {character: '\n', inConstruct: 'tableCell'},
      {atBreak: true, character: '|', after: '[\t :-]'},
      {character: '|', inConstruct: 'tableCell'},
      {atBreak: true, character: ':', after: '-'},
      {atBreak: true, character: '-', after: '[:|-]'}
    ],
    handlers: {
      inlineCode: inlineCodeWithTable,
      table: handleTable,
      tableCell: handleTableCell,
      tableRow: handleTableRow
    }
  }
  function handleTable(node, _, state, info) {
    return serializeData(handleTableAsData(node, state, info), node.align)
  }
  function handleTableRow(node, _, state, info) {
    const row = handleTableRowAsData(node, state, info);
    const value = serializeData([row]);
    return value.slice(0, value.indexOf('\n'))
  }
  function handleTableCell(node, _, state, info) {
    const exit = state.enter('tableCell');
    const subexit = state.enter('phrasing');
    const value = state.containerPhrasing(node, {
      ...info,
      before: around,
      after: around
    });
    subexit();
    exit();
    return value
  }
  function serializeData(matrix, align) {
    return markdownTable(matrix, {
      align,
      alignDelimiters,
      padding,
      stringLength
    })
  }
  function handleTableAsData(node, state, info) {
    const children = node.children;
    let index = -1;
    const result = [];
    const subexit = state.enter('table');
    while (++index < children.length) {
      result[index] = handleTableRowAsData(children[index], state, info);
    }
    subexit();
    return result
  }
  function handleTableRowAsData(node, state, info) {
    const children = node.children;
    let index = -1;
    const result = [];
    const subexit = state.enter('tableRow');
    while (++index < children.length) {
      result[index] = handleTableCell(children[index], node, state, info);
    }
    subexit();
    return result
  }
  function inlineCodeWithTable(node, parent, state) {
    let value = handle.inlineCode(node, parent, state);
    if (state.stack.includes('tableCell')) {
      value = value.replace(/\|/g, '\\$&');
    }
    return value
  }
}

function gfmTaskListItemFromMarkdown() {
  return {
    exit: {
      taskListCheckValueChecked: exitCheck,
      taskListCheckValueUnchecked: exitCheck,
      paragraph: exitParagraphWithTaskListItem
    }
  }
}
function gfmTaskListItemToMarkdown() {
  return {
    unsafe: [{atBreak: true, character: '-', after: '[:|-]'}],
    handlers: {listItem: listItemWithTaskListItem}
  }
}
function exitCheck(token) {
  const node = this.stack[this.stack.length - 2];
  ok$B(node.type === 'listItem');
  node.checked = token.type === 'taskListCheckValueChecked';
}
function exitParagraphWithTaskListItem(token) {
  const parent = this.stack[this.stack.length - 2];
  if (
    parent &&
    parent.type === 'listItem' &&
    typeof parent.checked === 'boolean'
  ) {
    const node = this.stack[this.stack.length - 1];
    ok$B(node.type === 'paragraph');
    const head = node.children[0];
    if (head && head.type === 'text') {
      const siblings = parent.children;
      let index = -1;
      let firstParaghraph;
      while (++index < siblings.length) {
        const sibling = siblings[index];
        if (sibling.type === 'paragraph') {
          firstParaghraph = sibling;
          break
        }
      }
      if (firstParaghraph === node) {
        head.value = head.value.slice(1);
        if (head.value.length === 0) {
          node.children.shift();
        } else if (
          node.position &&
          head.position &&
          typeof head.position.start.offset === 'number'
        ) {
          head.position.start.column++;
          head.position.start.offset++;
          node.position.start = Object.assign({}, head.position.start);
        }
      }
    }
  }
  this.exit(token);
}
function listItemWithTaskListItem(node, parent, state, info) {
  const head = node.children[0];
  const checkable =
    typeof node.checked === 'boolean' && head && head.type === 'paragraph';
  const checkbox = '[' + (node.checked ? 'x' : ' ') + '] ';
  const tracker = state.createTracker(info);
  if (checkable) {
    tracker.move(checkbox);
  }
  let value = handle.listItem(node, parent, state, {
    ...info,
    ...tracker.current()
  });
  if (checkable) {
    value = value.replace(/^(?:[*+-]|\d+\.)([\r\n]| {1,3})/, check);
  }
  return value
  function check($0) {
    return $0 + checkbox
  }
}

function gfmFromMarkdown() {
  return [
    gfmAutolinkLiteralFromMarkdown(),
    gfmFootnoteFromMarkdown(),
    gfmStrikethroughFromMarkdown(),
    gfmTableFromMarkdown(),
    gfmTaskListItemFromMarkdown()
  ]
}
function gfmToMarkdown(options) {
  return {
    extensions: [
      gfmAutolinkLiteralToMarkdown(),
      gfmFootnoteToMarkdown(),
      gfmStrikethroughToMarkdown(),
      gfmTableToMarkdown(options),
      gfmTaskListItemToMarkdown()
    ]
  }
}

const wwwPrefix = {
  tokenize: tokenizeWwwPrefix,
  partial: true
};
const domain = {
  tokenize: tokenizeDomain,
  partial: true
};
const path = {
  tokenize: tokenizePath,
  partial: true
};
const trail = {
  tokenize: tokenizeTrail,
  partial: true
};
const emailDomainDotTrail = {
  tokenize: tokenizeEmailDomainDotTrail,
  partial: true
};
const wwwAutolink = {
  tokenize: tokenizeWwwAutolink,
  previous: previousWww
};
const protocolAutolink = {
  tokenize: tokenizeProtocolAutolink,
  previous: previousProtocol
};
const emailAutolink = {
  tokenize: tokenizeEmailAutolink,
  previous: previousEmail
};
const text = {};
function gfmAutolinkLiteral() {
  return {
    text
  }
}
let code = 48;
while (code < 123) {
  text[code] = emailAutolink;
  code++;
  if (code === 58) code = 65;
  else if (code === 91) code = 97;
}
text[43] = emailAutolink;
text[45] = emailAutolink;
text[46] = emailAutolink;
text[95] = emailAutolink;
text[72] = [emailAutolink, protocolAutolink];
text[104] = [emailAutolink, protocolAutolink];
text[87] = [emailAutolink, wwwAutolink];
text[119] = [emailAutolink, wwwAutolink];
function tokenizeEmailAutolink(effects, ok, nok) {
  const self = this;
  let dot;
  let data;
  return start
  function start(code) {
    if (
      !gfmAtext(code) ||
      !previousEmail.call(self, self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }
    effects.enter('literalAutolink');
    effects.enter('literalAutolinkEmail');
    return atext(code)
  }
  function atext(code) {
    if (gfmAtext(code)) {
      effects.consume(code);
      return atext
    }
    if (code === 64) {
      effects.consume(code);
      return emailDomain
    }
    return nok(code)
  }
  function emailDomain(code) {
    if (code === 46) {
      return effects.check(
        emailDomainDotTrail,
        emailDomainAfter,
        emailDomainDot
      )(code)
    }
    if (code === 45 || code === 95 || asciiAlphanumeric(code)) {
      data = true;
      effects.consume(code);
      return emailDomain
    }
    return emailDomainAfter(code)
  }
  function emailDomainDot(code) {
    effects.consume(code);
    dot = true;
    return emailDomain
  }
  function emailDomainAfter(code) {
    if (data && dot && asciiAlpha(self.previous)) {
      effects.exit('literalAutolinkEmail');
      effects.exit('literalAutolink');
      return ok(code)
    }
    return nok(code)
  }
}
function tokenizeWwwAutolink(effects, ok, nok) {
  const self = this;
  return wwwStart
  function wwwStart(code) {
    if (
      (code !== 87 && code !== 119) ||
      !previousWww.call(self, self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }
    effects.enter('literalAutolink');
    effects.enter('literalAutolinkWww');
    return effects.check(
      wwwPrefix,
      effects.attempt(domain, effects.attempt(path, wwwAfter), nok),
      nok
    )(code)
  }
  function wwwAfter(code) {
    effects.exit('literalAutolinkWww');
    effects.exit('literalAutolink');
    return ok(code)
  }
}
function tokenizeProtocolAutolink(effects, ok, nok) {
  const self = this;
  let buffer = '';
  let seen = false;
  return protocolStart
  function protocolStart(code) {
    if (
      (code === 72 || code === 104) &&
      previousProtocol.call(self, self.previous) &&
      !previousUnbalanced(self.events)
    ) {
      effects.enter('literalAutolink');
      effects.enter('literalAutolinkHttp');
      buffer += String.fromCodePoint(code);
      effects.consume(code);
      return protocolPrefixInside
    }
    return nok(code)
  }
  function protocolPrefixInside(code) {
    if (asciiAlpha(code) && buffer.length < 5) {
      buffer += String.fromCodePoint(code);
      effects.consume(code);
      return protocolPrefixInside
    }
    if (code === 58) {
      const protocol = buffer.toLowerCase();
      if (protocol === 'http' || protocol === 'https') {
        effects.consume(code);
        return protocolSlashesInside
      }
    }
    return nok(code)
  }
  function protocolSlashesInside(code) {
    if (code === 47) {
      effects.consume(code);
      if (seen) {
        return afterProtocol
      }
      seen = true;
      return protocolSlashesInside
    }
    return nok(code)
  }
  function afterProtocol(code) {
    return code === null ||
      asciiControl(code) ||
      markdownLineEndingOrSpace(code) ||
      unicodeWhitespace(code) ||
      unicodePunctuation(code)
      ? nok(code)
      : effects.attempt(domain, effects.attempt(path, protocolAfter), nok)(code)
  }
  function protocolAfter(code) {
    effects.exit('literalAutolinkHttp');
    effects.exit('literalAutolink');
    return ok(code)
  }
}
function tokenizeWwwPrefix(effects, ok, nok) {
  let size = 0;
  return wwwPrefixInside
  function wwwPrefixInside(code) {
    if ((code === 87 || code === 119) && size < 3) {
      size++;
      effects.consume(code);
      return wwwPrefixInside
    }
    if (code === 46 && size === 3) {
      effects.consume(code);
      return wwwPrefixAfter
    }
    return nok(code)
  }
  function wwwPrefixAfter(code) {
    return code === null ? nok(code) : ok(code)
  }
}
function tokenizeDomain(effects, ok, nok) {
  let underscoreInLastSegment;
  let underscoreInLastLastSegment;
  let seen;
  return domainInside
  function domainInside(code) {
    if (code === 46 || code === 95) {
      return effects.check(trail, domainAfter, domainAtPunctuation)(code)
    }
    if (
      code === null ||
      markdownLineEndingOrSpace(code) ||
      unicodeWhitespace(code) ||
      (code !== 45 && unicodePunctuation(code))
    ) {
      return domainAfter(code)
    }
    seen = true;
    effects.consume(code);
    return domainInside
  }
  function domainAtPunctuation(code) {
    if (code === 95) {
      underscoreInLastSegment = true;
    }
    else {
      underscoreInLastLastSegment = underscoreInLastSegment;
      underscoreInLastSegment = undefined;
    }
    effects.consume(code);
    return domainInside
  }
  function domainAfter(code) {
    if (underscoreInLastLastSegment || underscoreInLastSegment || !seen) {
      return nok(code)
    }
    return ok(code)
  }
}
function tokenizePath(effects, ok) {
  let sizeOpen = 0;
  let sizeClose = 0;
  return pathInside
  function pathInside(code) {
    if (code === 40) {
      sizeOpen++;
      effects.consume(code);
      return pathInside
    }
    if (code === 41 && sizeClose < sizeOpen) {
      return pathAtPunctuation(code)
    }
    if (
      code === 33 ||
      code === 34 ||
      code === 38 ||
      code === 39 ||
      code === 41 ||
      code === 42 ||
      code === 44 ||
      code === 46 ||
      code === 58 ||
      code === 59 ||
      code === 60 ||
      code === 63 ||
      code === 93 ||
      code === 95 ||
      code === 126
    ) {
      return effects.check(trail, ok, pathAtPunctuation)(code)
    }
    if (
      code === null ||
      markdownLineEndingOrSpace(code) ||
      unicodeWhitespace(code)
    ) {
      return ok(code)
    }
    effects.consume(code);
    return pathInside
  }
  function pathAtPunctuation(code) {
    if (code === 41) {
      sizeClose++;
    }
    effects.consume(code);
    return pathInside
  }
}
function tokenizeTrail(effects, ok, nok) {
  return trail
  function trail(code) {
    if (
      code === 33 ||
      code === 34 ||
      code === 39 ||
      code === 41 ||
      code === 42 ||
      code === 44 ||
      code === 46 ||
      code === 58 ||
      code === 59 ||
      code === 63 ||
      code === 95 ||
      code === 126
    ) {
      effects.consume(code);
      return trail
    }
    if (code === 38) {
      effects.consume(code);
      return trailCharRefStart
    }
    if (code === 93) {
      effects.consume(code);
      return trailBracketAfter
    }
    if (
      code === 60 ||
      code === null ||
      markdownLineEndingOrSpace(code) ||
      unicodeWhitespace(code)
    ) {
      return ok(code)
    }
    return nok(code)
  }
  function trailBracketAfter(code) {
    if (
      code === null ||
      code === 40 ||
      code === 91 ||
      markdownLineEndingOrSpace(code) ||
      unicodeWhitespace(code)
    ) {
      return ok(code)
    }
    return trail(code)
  }
  function trailCharRefStart(code) {
    return asciiAlpha(code) ? trailCharRefInside(code) : nok(code)
  }
  function trailCharRefInside(code) {
    if (code === 59) {
      effects.consume(code);
      return trail
    }
    if (asciiAlpha(code)) {
      effects.consume(code);
      return trailCharRefInside
    }
    return nok(code)
  }
}
function tokenizeEmailDomainDotTrail(effects, ok, nok) {
  return start
  function start(code) {
    effects.consume(code);
    return after
  }
  function after(code) {
    return asciiAlphanumeric(code) ? nok(code) : ok(code)
  }
}
function previousWww(code) {
  return (
    code === null ||
    code === 40 ||
    code === 42 ||
    code === 95 ||
    code === 91 ||
    code === 93 ||
    code === 126 ||
    markdownLineEndingOrSpace(code)
  )
}
function previousProtocol(code) {
  return !asciiAlpha(code)
}
function previousEmail(code) {
  return !(code === 47 || gfmAtext(code))
}
function gfmAtext(code) {
  return (
    code === 43 ||
    code === 45 ||
    code === 46 ||
    code === 95 ||
    asciiAlphanumeric(code)
  )
}
function previousUnbalanced(events) {
  let index = events.length;
  let result = false;
  while (index--) {
    const token = events[index][1];
    if (
      (token.type === 'labelLink' || token.type === 'labelImage') &&
      !token._balanced
    ) {
      result = true;
      break
    }
    if (token._gfmAutolinkLiteralWalkedInto) {
      result = false;
      break
    }
  }
  if (events.length > 0 && !result) {
    events[events.length - 1][1]._gfmAutolinkLiteralWalkedInto = true;
  }
  return result
}

const indent = {
  tokenize: tokenizeIndent,
  partial: true
};
function gfmFootnote() {
  return {
    document: {
      [91]: {
        tokenize: tokenizeDefinitionStart,
        continuation: {
          tokenize: tokenizeDefinitionContinuation
        },
        exit: gfmFootnoteDefinitionEnd
      }
    },
    text: {
      [91]: {
        tokenize: tokenizeGfmFootnoteCall
      },
      [93]: {
        add: 'after',
        tokenize: tokenizePotentialGfmFootnoteCall,
        resolveTo: resolveToPotentialGfmFootnoteCall
      }
    }
  }
}
function tokenizePotentialGfmFootnoteCall(effects, ok, nok) {
  const self = this;
  let index = self.events.length;
  const defined = self.parser.gfmFootnotes || (self.parser.gfmFootnotes = []);
  let labelStart;
  while (index--) {
    const token = self.events[index][1];
    if (token.type === 'labelImage') {
      labelStart = token;
      break
    }
    if (
      token.type === 'gfmFootnoteCall' ||
      token.type === 'labelLink' ||
      token.type === 'label' ||
      token.type === 'image' ||
      token.type === 'link'
    ) {
      break
    }
  }
  return start
  function start(code) {
    if (!labelStart || !labelStart._balanced) {
      return nok(code)
    }
    const id = normalizeIdentifier$1(
      self.sliceSerialize({
        start: labelStart.end,
        end: self.now()
      })
    );
    if (id.codePointAt(0) !== 94 || !defined.includes(id.slice(1))) {
      return nok(code)
    }
    effects.enter('gfmFootnoteCallLabelMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteCallLabelMarker');
    return ok(code)
  }
}
function resolveToPotentialGfmFootnoteCall(events, context) {
  let index = events.length;
  while (index--) {
    if (
      events[index][1].type === 'labelImage' &&
      events[index][0] === 'enter'
    ) {
      events[index][1];
      break
    }
  }
  events[index + 1][1].type = 'data';
  events[index + 3][1].type = 'gfmFootnoteCallLabelMarker';
  const call = {
    type: 'gfmFootnoteCall',
    start: Object.assign({}, events[index + 3][1].start),
    end: Object.assign({}, events[events.length - 1][1].end)
  };
  const marker = {
    type: 'gfmFootnoteCallMarker',
    start: Object.assign({}, events[index + 3][1].end),
    end: Object.assign({}, events[index + 3][1].end)
  };
  marker.end.column++;
  marker.end.offset++;
  marker.end._bufferIndex++;
  const string = {
    type: 'gfmFootnoteCallString',
    start: Object.assign({}, marker.end),
    end: Object.assign({}, events[events.length - 1][1].start)
  };
  const chunk = {
    type: 'chunkString',
    contentType: 'string',
    start: Object.assign({}, string.start),
    end: Object.assign({}, string.end)
  };
  const replacement = [
    events[index + 1],
    events[index + 2],
    ['enter', call, context],
    events[index + 3],
    events[index + 4],
    ['enter', marker, context],
    ['exit', marker, context],
    ['enter', string, context],
    ['enter', chunk, context],
    ['exit', chunk, context],
    ['exit', string, context],
    events[events.length - 2],
    events[events.length - 1],
    ['exit', call, context]
  ];
  events.splice(index, events.length - index + 1, ...replacement);
  return events
}
function tokenizeGfmFootnoteCall(effects, ok, nok) {
  const self = this;
  const defined = self.parser.gfmFootnotes || (self.parser.gfmFootnotes = []);
  let size = 0;
  let data;
  return start
  function start(code) {
    effects.enter('gfmFootnoteCall');
    effects.enter('gfmFootnoteCallLabelMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteCallLabelMarker');
    return callStart
  }
  function callStart(code) {
    if (code !== 94) return nok(code)
    effects.enter('gfmFootnoteCallMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteCallMarker');
    effects.enter('gfmFootnoteCallString');
    effects.enter('chunkString').contentType = 'string';
    return callData
  }
  function callData(code) {
    if (
      size > 999 ||
      (code === 93 && !data) ||
      code === null ||
      code === 91 ||
      markdownLineEndingOrSpace(code)
    ) {
      return nok(code)
    }
    if (code === 93) {
      effects.exit('chunkString');
      const token = effects.exit('gfmFootnoteCallString');
      if (!defined.includes(normalizeIdentifier$1(self.sliceSerialize(token)))) {
        return nok(code)
      }
      effects.enter('gfmFootnoteCallLabelMarker');
      effects.consume(code);
      effects.exit('gfmFootnoteCallLabelMarker');
      effects.exit('gfmFootnoteCall');
      return ok
    }
    if (!markdownLineEndingOrSpace(code)) {
      data = true;
    }
    size++;
    effects.consume(code);
    return code === 92 ? callEscape : callData
  }
  function callEscape(code) {
    if (code === 91 || code === 92 || code === 93) {
      effects.consume(code);
      size++;
      return callData
    }
    return callData(code)
  }
}
function tokenizeDefinitionStart(effects, ok, nok) {
  const self = this;
  const defined = self.parser.gfmFootnotes || (self.parser.gfmFootnotes = []);
  let identifier;
  let size = 0;
  let data;
  return start
  function start(code) {
    effects.enter('gfmFootnoteDefinition')._container = true;
    effects.enter('gfmFootnoteDefinitionLabel');
    effects.enter('gfmFootnoteDefinitionLabelMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteDefinitionLabelMarker');
    return labelAtMarker
  }
  function labelAtMarker(code) {
    if (code === 94) {
      effects.enter('gfmFootnoteDefinitionMarker');
      effects.consume(code);
      effects.exit('gfmFootnoteDefinitionMarker');
      effects.enter('gfmFootnoteDefinitionLabelString');
      effects.enter('chunkString').contentType = 'string';
      return labelInside
    }
    return nok(code)
  }
  function labelInside(code) {
    if (
      size > 999 ||
      (code === 93 && !data) ||
      code === null ||
      code === 91 ||
      markdownLineEndingOrSpace(code)
    ) {
      return nok(code)
    }
    if (code === 93) {
      effects.exit('chunkString');
      const token = effects.exit('gfmFootnoteDefinitionLabelString');
      identifier = normalizeIdentifier$1(self.sliceSerialize(token));
      effects.enter('gfmFootnoteDefinitionLabelMarker');
      effects.consume(code);
      effects.exit('gfmFootnoteDefinitionLabelMarker');
      effects.exit('gfmFootnoteDefinitionLabel');
      return labelAfter
    }
    if (!markdownLineEndingOrSpace(code)) {
      data = true;
    }
    size++;
    effects.consume(code);
    return code === 92 ? labelEscape : labelInside
  }
  function labelEscape(code) {
    if (code === 91 || code === 92 || code === 93) {
      effects.consume(code);
      size++;
      return labelInside
    }
    return labelInside(code)
  }
  function labelAfter(code) {
    if (code === 58) {
      effects.enter('definitionMarker');
      effects.consume(code);
      effects.exit('definitionMarker');
      if (!defined.includes(identifier)) {
        defined.push(identifier);
      }
      return factorySpace(
        effects,
        whitespaceAfter,
        'gfmFootnoteDefinitionWhitespace'
      )
    }
    return nok(code)
  }
  function whitespaceAfter(code) {
    return ok(code)
  }
}
function tokenizeDefinitionContinuation(effects, ok, nok) {
  return effects.check(blankLine, ok, effects.attempt(indent, ok, nok))
}
function gfmFootnoteDefinitionEnd(effects) {
  effects.exit('gfmFootnoteDefinition');
}
function tokenizeIndent(effects, ok, nok) {
  const self = this;
  return factorySpace(
    effects,
    afterPrefix,
    'gfmFootnoteDefinitionIndent',
    4 + 1
  )
  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'gfmFootnoteDefinitionIndent' &&
      tail[2].sliceSerialize(tail[1], true).length === 4
      ? ok(code)
      : nok(code)
  }
}

function gfmStrikethrough(options) {
  const options_ = options || {};
  let single = options_.singleTilde;
  const tokenizer = {
    tokenize: tokenizeStrikethrough,
    resolveAll: resolveAllStrikethrough
  };
  if (single === null || single === undefined) {
    single = true;
  }
  return {
    text: {
      [126]: tokenizer
    },
    insideSpan: {
      null: [tokenizer]
    },
    attentionMarkers: {
      null: [126]
    }
  }
  function resolveAllStrikethrough(events, context) {
    let index = -1;
    while (++index < events.length) {
      if (
        events[index][0] === 'enter' &&
        events[index][1].type === 'strikethroughSequenceTemporary' &&
        events[index][1]._close
      ) {
        let open = index;
        while (open--) {
          if (
            events[open][0] === 'exit' &&
            events[open][1].type === 'strikethroughSequenceTemporary' &&
            events[open][1]._open &&
            events[index][1].end.offset - events[index][1].start.offset ===
              events[open][1].end.offset - events[open][1].start.offset
          ) {
            events[index][1].type = 'strikethroughSequence';
            events[open][1].type = 'strikethroughSequence';
            const strikethrough = {
              type: 'strikethrough',
              start: Object.assign({}, events[open][1].start),
              end: Object.assign({}, events[index][1].end)
            };
            const text = {
              type: 'strikethroughText',
              start: Object.assign({}, events[open][1].end),
              end: Object.assign({}, events[index][1].start)
            };
            const nextEvents = [
              ['enter', strikethrough, context],
              ['enter', events[open][1], context],
              ['exit', events[open][1], context],
              ['enter', text, context]
            ];
            const insideSpan = context.parser.constructs.insideSpan.null;
            if (insideSpan) {
              splice(
                nextEvents,
                nextEvents.length,
                0,
                resolveAll(insideSpan, events.slice(open + 1, index), context)
              );
            }
            splice(nextEvents, nextEvents.length, 0, [
              ['exit', text, context],
              ['enter', events[index][1], context],
              ['exit', events[index][1], context],
              ['exit', strikethrough, context]
            ]);
            splice(events, open - 1, index - open + 3, nextEvents);
            index = open + nextEvents.length - 2;
            break
          }
        }
      }
    }
    index = -1;
    while (++index < events.length) {
      if (events[index][1].type === 'strikethroughSequenceTemporary') {
        events[index][1].type = 'data';
      }
    }
    return events
  }
  function tokenizeStrikethrough(effects, ok, nok) {
    const previous = this.previous;
    const events = this.events;
    let size = 0;
    return start
    function start(code) {
      if (
        previous === 126 &&
        events[events.length - 1][1].type !== 'characterEscape'
      ) {
        return nok(code)
      }
      effects.enter('strikethroughSequenceTemporary');
      return more(code)
    }
    function more(code) {
      const before = classifyCharacter(previous);
      if (code === 126) {
        if (size > 1) return nok(code)
        effects.consume(code);
        size++;
        return more
      }
      if (size < 2 && !single) return nok(code)
      const token = effects.exit('strikethroughSequenceTemporary');
      const after = classifyCharacter(code);
      token._open = !after || (after === 2 && Boolean(before));
      token._close = !before || (before === 2 && Boolean(after));
      return ok(code)
    }
  }
}

class EditMap {
  constructor() {
    this.map = [];
  }
  add(index, remove, add) {
    addImpl(this, index, remove, add);
  }
  consume(events) {
    this.map.sort(function (a, b) {
      return a[0] - b[0]
    });
    if (this.map.length === 0) {
      return
    }
    let index = this.map.length;
    const vecs = [];
    while (index > 0) {
      index -= 1;
      vecs.push(
        events.slice(this.map[index][0] + this.map[index][1]),
        this.map[index][2]
      );
      events.length = this.map[index][0];
    }
    vecs.push([...events]);
    events.length = 0;
    let slice = vecs.pop();
    while (slice) {
      events.push(...slice);
      slice = vecs.pop();
    }
    this.map.length = 0;
  }
}
function addImpl(editMap, at, remove, add) {
  let index = 0;
  if (remove === 0 && add.length === 0) {
    return
  }
  while (index < editMap.map.length) {
    if (editMap.map[index][0] === at) {
      editMap.map[index][1] += remove;
      editMap.map[index][2].push(...add);
      return
    }
    index += 1;
  }
  editMap.map.push([at, remove, add]);
}

function gfmTableAlign(events, index) {
  let inDelimiterRow = false;
  const align = [];
  while (index < events.length) {
    const event = events[index];
    if (inDelimiterRow) {
      if (event[0] === 'enter') {
        if (event[1].type === 'tableContent') {
          align.push(
            events[index + 1][1].type === 'tableDelimiterMarker'
              ? 'left'
              : 'none'
          );
        }
      }
      else if (event[1].type === 'tableContent') {
        if (events[index - 1][1].type === 'tableDelimiterMarker') {
          const alignIndex = align.length - 1;
          align[alignIndex] = align[alignIndex] === 'left' ? 'center' : 'right';
        }
      }
      else if (event[1].type === 'tableDelimiterRow') {
        break
      }
    } else if (event[0] === 'enter' && event[1].type === 'tableDelimiterRow') {
      inDelimiterRow = true;
    }
    index += 1;
  }
  return align
}

function gfmTable() {
  return {
    flow: {
      null: {
        tokenize: tokenizeTable,
        resolveAll: resolveTable
      }
    }
  }
}
function tokenizeTable(effects, ok, nok) {
  const self = this;
  let size = 0;
  let sizeB = 0;
  let seen;
  return start
  function start(code) {
    let index = self.events.length - 1;
    while (index > -1) {
      const type = self.events[index][1].type;
      if (
        type === 'lineEnding' ||
        type === 'linePrefix'
      )
        index--;
      else break
    }
    const tail = index > -1 ? self.events[index][1].type : null;
    const next =
      tail === 'tableHead' || tail === 'tableRow' ? bodyRowStart : headRowBefore;
    if (next === bodyRowStart && self.parser.lazy[self.now().line]) {
      return nok(code)
    }
    return next(code)
  }
  function headRowBefore(code) {
    effects.enter('tableHead');
    effects.enter('tableRow');
    return headRowStart(code)
  }
  function headRowStart(code) {
    if (code === 124) {
      return headRowBreak(code)
    }
    seen = true;
    sizeB += 1;
    return headRowBreak(code)
  }
  function headRowBreak(code) {
    if (code === null) {
      return nok(code)
    }
    if (markdownLineEnding(code)) {
      if (sizeB > 1) {
        sizeB = 0;
        self.interrupt = true;
        effects.exit('tableRow');
        effects.enter('lineEnding');
        effects.consume(code);
        effects.exit('lineEnding');
        return headDelimiterStart
      }
      return nok(code)
    }
    if (markdownSpace(code)) {
      return factorySpace(effects, headRowBreak, 'whitespace')(code)
    }
    sizeB += 1;
    if (seen) {
      seen = false;
      size += 1;
    }
    if (code === 124) {
      effects.enter('tableCellDivider');
      effects.consume(code);
      effects.exit('tableCellDivider');
      seen = true;
      return headRowBreak
    }
    effects.enter('data');
    return headRowData(code)
  }
  function headRowData(code) {
    if (code === null || code === 124 || markdownLineEndingOrSpace(code)) {
      effects.exit('data');
      return headRowBreak(code)
    }
    effects.consume(code);
    return code === 92 ? headRowEscape : headRowData
  }
  function headRowEscape(code) {
    if (code === 92 || code === 124) {
      effects.consume(code);
      return headRowData
    }
    return headRowData(code)
  }
  function headDelimiterStart(code) {
    self.interrupt = false;
    if (self.parser.lazy[self.now().line]) {
      return nok(code)
    }
    effects.enter('tableDelimiterRow');
    seen = false;
    if (markdownSpace(code)) {
      return factorySpace(
        effects,
        headDelimiterBefore,
        'linePrefix',
        self.parser.constructs.disable.null.includes('codeIndented')
          ? undefined
          : 4
      )(code)
    }
    return headDelimiterBefore(code)
  }
  function headDelimiterBefore(code) {
    if (code === 45 || code === 58) {
      return headDelimiterValueBefore(code)
    }
    if (code === 124) {
      seen = true;
      effects.enter('tableCellDivider');
      effects.consume(code);
      effects.exit('tableCellDivider');
      return headDelimiterCellBefore
    }
    return headDelimiterNok(code)
  }
  function headDelimiterCellBefore(code) {
    if (markdownSpace(code)) {
      return factorySpace(effects, headDelimiterValueBefore, 'whitespace')(code)
    }
    return headDelimiterValueBefore(code)
  }
  function headDelimiterValueBefore(code) {
    if (code === 58) {
      sizeB += 1;
      seen = true;
      effects.enter('tableDelimiterMarker');
      effects.consume(code);
      effects.exit('tableDelimiterMarker');
      return headDelimiterLeftAlignmentAfter
    }
    if (code === 45) {
      sizeB += 1;
      return headDelimiterLeftAlignmentAfter(code)
    }
    if (code === null || markdownLineEnding(code)) {
      return headDelimiterCellAfter(code)
    }
    return headDelimiterNok(code)
  }
  function headDelimiterLeftAlignmentAfter(code) {
    if (code === 45) {
      effects.enter('tableDelimiterFiller');
      return headDelimiterFiller(code)
    }
    return headDelimiterNok(code)
  }
  function headDelimiterFiller(code) {
    if (code === 45) {
      effects.consume(code);
      return headDelimiterFiller
    }
    if (code === 58) {
      seen = true;
      effects.exit('tableDelimiterFiller');
      effects.enter('tableDelimiterMarker');
      effects.consume(code);
      effects.exit('tableDelimiterMarker');
      return headDelimiterRightAlignmentAfter
    }
    effects.exit('tableDelimiterFiller');
    return headDelimiterRightAlignmentAfter(code)
  }
  function headDelimiterRightAlignmentAfter(code) {
    if (markdownSpace(code)) {
      return factorySpace(effects, headDelimiterCellAfter, 'whitespace')(code)
    }
    return headDelimiterCellAfter(code)
  }
  function headDelimiterCellAfter(code) {
    if (code === 124) {
      return headDelimiterBefore(code)
    }
    if (code === null || markdownLineEnding(code)) {
      if (!seen || size !== sizeB) {
        return headDelimiterNok(code)
      }
      effects.exit('tableDelimiterRow');
      effects.exit('tableHead');
      return ok(code)
    }
    return headDelimiterNok(code)
  }
  function headDelimiterNok(code) {
    return nok(code)
  }
  function bodyRowStart(code) {
    effects.enter('tableRow');
    return bodyRowBreak(code)
  }
  function bodyRowBreak(code) {
    if (code === 124) {
      effects.enter('tableCellDivider');
      effects.consume(code);
      effects.exit('tableCellDivider');
      return bodyRowBreak
    }
    if (code === null || markdownLineEnding(code)) {
      effects.exit('tableRow');
      return ok(code)
    }
    if (markdownSpace(code)) {
      return factorySpace(effects, bodyRowBreak, 'whitespace')(code)
    }
    effects.enter('data');
    return bodyRowData(code)
  }
  function bodyRowData(code) {
    if (code === null || code === 124 || markdownLineEndingOrSpace(code)) {
      effects.exit('data');
      return bodyRowBreak(code)
    }
    effects.consume(code);
    return code === 92 ? bodyRowEscape : bodyRowData
  }
  function bodyRowEscape(code) {
    if (code === 92 || code === 124) {
      effects.consume(code);
      return bodyRowData
    }
    return bodyRowData(code)
  }
}
function resolveTable(events, context) {
  let index = -1;
  let inFirstCellAwaitingPipe = true;
  let rowKind = 0;
  let lastCell = [0, 0, 0, 0];
  let cell = [0, 0, 0, 0];
  let afterHeadAwaitingFirstBodyRow = false;
  let lastTableEnd = 0;
  let currentTable;
  let currentBody;
  let currentCell;
  const map = new EditMap();
  while (++index < events.length) {
    const event = events[index];
    const token = event[1];
    if (event[0] === 'enter') {
      if (token.type === 'tableHead') {
        afterHeadAwaitingFirstBodyRow = false;
        if (lastTableEnd !== 0) {
          flushTableEnd(map, context, lastTableEnd, currentTable, currentBody);
          currentBody = undefined;
          lastTableEnd = 0;
        }
        currentTable = {
          type: 'table',
          start: Object.assign({}, token.start),
          end: Object.assign({}, token.end)
        };
        map.add(index, 0, [['enter', currentTable, context]]);
      } else if (
        token.type === 'tableRow' ||
        token.type === 'tableDelimiterRow'
      ) {
        inFirstCellAwaitingPipe = true;
        currentCell = undefined;
        lastCell = [0, 0, 0, 0];
        cell = [0, index + 1, 0, 0];
        if (afterHeadAwaitingFirstBodyRow) {
          afterHeadAwaitingFirstBodyRow = false;
          currentBody = {
            type: 'tableBody',
            start: Object.assign({}, token.start),
            end: Object.assign({}, token.end)
          };
          map.add(index, 0, [['enter', currentBody, context]]);
        }
        rowKind = token.type === 'tableDelimiterRow' ? 2 : currentBody ? 3 : 1;
      }
      else if (
        rowKind &&
        (token.type === 'data' ||
          token.type === 'tableDelimiterMarker' ||
          token.type === 'tableDelimiterFiller')
      ) {
        inFirstCellAwaitingPipe = false;
        if (cell[2] === 0) {
          if (lastCell[1] !== 0) {
            cell[0] = cell[1];
            currentCell = flushCell(
              map,
              context,
              lastCell,
              rowKind,
              undefined,
              currentCell
            );
            lastCell = [0, 0, 0, 0];
          }
          cell[2] = index;
        }
      } else if (token.type === 'tableCellDivider') {
        if (inFirstCellAwaitingPipe) {
          inFirstCellAwaitingPipe = false;
        } else {
          if (lastCell[1] !== 0) {
            cell[0] = cell[1];
            currentCell = flushCell(
              map,
              context,
              lastCell,
              rowKind,
              undefined,
              currentCell
            );
          }
          lastCell = cell;
          cell = [lastCell[1], index, 0, 0];
        }
      }
    }
    else if (token.type === 'tableHead') {
      afterHeadAwaitingFirstBodyRow = true;
      lastTableEnd = index;
    } else if (
      token.type === 'tableRow' ||
      token.type === 'tableDelimiterRow'
    ) {
      lastTableEnd = index;
      if (lastCell[1] !== 0) {
        cell[0] = cell[1];
        currentCell = flushCell(
          map,
          context,
          lastCell,
          rowKind,
          index,
          currentCell
        );
      } else if (cell[1] !== 0) {
        currentCell = flushCell(map, context, cell, rowKind, index, currentCell);
      }
      rowKind = 0;
    } else if (
      rowKind &&
      (token.type === 'data' ||
        token.type === 'tableDelimiterMarker' ||
        token.type === 'tableDelimiterFiller')
    ) {
      cell[3] = index;
    }
  }
  if (lastTableEnd !== 0) {
    flushTableEnd(map, context, lastTableEnd, currentTable, currentBody);
  }
  map.consume(context.events);
  index = -1;
  while (++index < context.events.length) {
    const event = context.events[index];
    if (event[0] === 'enter' && event[1].type === 'table') {
      event[1]._align = gfmTableAlign(context.events, index);
    }
  }
  return events
}
function flushCell(map, context, range, rowKind, rowEnd, previousCell) {
  const groupName =
    rowKind === 1
      ? 'tableHeader'
      : rowKind === 2
      ? 'tableDelimiter'
      : 'tableData';
  const valueName = 'tableContent';
  if (range[0] !== 0) {
    previousCell.end = Object.assign({}, getPoint(context.events, range[0]));
    map.add(range[0], 0, [['exit', previousCell, context]]);
  }
  const now = getPoint(context.events, range[1]);
  previousCell = {
    type: groupName,
    start: Object.assign({}, now),
    end: Object.assign({}, now)
  };
  map.add(range[1], 0, [['enter', previousCell, context]]);
  if (range[2] !== 0) {
    const relatedStart = getPoint(context.events, range[2]);
    const relatedEnd = getPoint(context.events, range[3]);
    const valueToken = {
      type: valueName,
      start: Object.assign({}, relatedStart),
      end: Object.assign({}, relatedEnd)
    };
    map.add(range[2], 0, [['enter', valueToken, context]]);
    if (rowKind !== 2) {
      const start = context.events[range[2]];
      const end = context.events[range[3]];
      start[1].end = Object.assign({}, end[1].end);
      start[1].type = 'chunkText';
      start[1].contentType = 'text';
      if (range[3] > range[2] + 1) {
        const a = range[2] + 1;
        const b = range[3] - range[2] - 1;
        map.add(a, b, []);
      }
    }
    map.add(range[3] + 1, 0, [['exit', valueToken, context]]);
  }
  if (rowEnd !== undefined) {
    previousCell.end = Object.assign({}, getPoint(context.events, rowEnd));
    map.add(rowEnd, 0, [['exit', previousCell, context]]);
    previousCell = undefined;
  }
  return previousCell
}
function flushTableEnd(map, context, index, table, tableBody) {
  const exits = [];
  const related = getPoint(context.events, index);
  if (tableBody) {
    tableBody.end = Object.assign({}, related);
    exits.push(['exit', tableBody, context]);
  }
  table.end = Object.assign({}, related);
  exits.push(['exit', table, context]);
  map.add(index + 1, 0, exits);
}
function getPoint(events, index) {
  const event = events[index];
  const side = event[0] === 'enter' ? 'start' : 'end';
  return event[1][side]
}

const tasklistCheck = {
  tokenize: tokenizeTasklistCheck
};
function gfmTaskListItem() {
  return {
    text: {
      [91]: tasklistCheck
    }
  }
}
function tokenizeTasklistCheck(effects, ok, nok) {
  const self = this;
  return open
  function open(code) {
    if (
      self.previous !== null ||
      !self._gfmTasklistFirstContentOfListItem
    ) {
      return nok(code)
    }
    effects.enter('taskListCheck');
    effects.enter('taskListCheckMarker');
    effects.consume(code);
    effects.exit('taskListCheckMarker');
    return inside
  }
  function inside(code) {
    if (markdownLineEndingOrSpace(code)) {
      effects.enter('taskListCheckValueUnchecked');
      effects.consume(code);
      effects.exit('taskListCheckValueUnchecked');
      return close
    }
    if (code === 88 || code === 120) {
      effects.enter('taskListCheckValueChecked');
      effects.consume(code);
      effects.exit('taskListCheckValueChecked');
      return close
    }
    return nok(code)
  }
  function close(code) {
    if (code === 93) {
      effects.enter('taskListCheckMarker');
      effects.consume(code);
      effects.exit('taskListCheckMarker');
      effects.exit('taskListCheck');
      return after
    }
    return nok(code)
  }
  function after(code) {
    if (markdownLineEnding(code)) {
      return ok(code)
    }
    if (markdownSpace(code)) {
      return effects.check(
        {
          tokenize: spaceThenNonSpace
        },
        ok,
        nok
      )(code)
    }
    return nok(code)
  }
}
function spaceThenNonSpace(effects, ok, nok) {
  return factorySpace(effects, after, 'whitespace')
  function after(code) {
    return code === null ? nok(code) : ok(code)
  }
}

function gfm(options) {
  return combineExtensions([
    gfmAutolinkLiteral(),
    gfmFootnote(),
    gfmStrikethrough(options),
    gfmTable(),
    gfmTaskListItem()
  ])
}

const emptyOptions$2 = {};
function remarkGfm(options) {
  const self =  (this);
  const settings = options || emptyOptions$2;
  const data = self.data();
  const micromarkExtensions =
    data.micromarkExtensions || (data.micromarkExtensions = []);
  const fromMarkdownExtensions =
    data.fromMarkdownExtensions || (data.fromMarkdownExtensions = []);
  const toMarkdownExtensions =
    data.toMarkdownExtensions || (data.toMarkdownExtensions = []);
  micromarkExtensions.push(gfm(settings));
  fromMarkdownExtensions.push(gfmFromMarkdown());
  toMarkdownExtensions.push(gfmToMarkdown(settings));
}

function location(file) {
  const value = String(file);
  const indices = [];
  const search = /\r?\n|\r/g;
  while (search.test(value)) {
    indices.push(search.lastIndex);
  }
  indices.push(value.length + 1);
  return {toPoint, toOffset}
  function toPoint(offset) {
    let index = -1;
    if (
      typeof offset === 'number' &&
      offset > -1 &&
      offset < indices[indices.length - 1]
    ) {
      while (++index < indices.length) {
        if (indices[index] > offset) {
          return {
            line: index + 1,
            column: offset - (index > 0 ? indices[index - 1] : 0) + 1,
            offset
          }
        }
      }
    }
    return {line: undefined, column: undefined, offset: undefined}
  }
  function toOffset(point) {
    const line = point && point.line;
    const column = point && point.column;
    if (
      typeof line === 'number' &&
      typeof column === 'number' &&
      !Number.isNaN(line) &&
      !Number.isNaN(column) &&
      line - 1 in indices
    ) {
      const offset = (indices[line - 2] || 0) + column - 1 || 0;
      if (offset > -1 && offset < indices[indices.length - 1]) {
        return offset
      }
    }
    return -1
  }
}

const convert$z =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$z
      }
      if (typeof test === 'string') {
        return typeFactory$z(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$z(test) : propsFactory$z(test)
      }
      if (typeof test === 'function') {
        return castFactory$z(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$z(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$z(tests[index]);
  }
  return castFactory$z(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$z(check) {
  return castFactory$z(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$z(check) {
  return castFactory$z(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$z(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$z() {
  return true
}

function color$A(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$z = true;
const SKIP$z = 'skip';
const EXIT$z = false;
const visitParents$z =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      var is = convert$z(test);
      var step = reverse ? -1 : 1;
      factory(tree, null, [])();
      function factory(node, index, parents) {
        var value = typeof node === 'object' && node !== null ? node : {};
        var name;
        if (typeof value.type === 'string') {
          name =
            typeof value.tagName === 'string'
              ? value.tagName
              : typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' +
              color$A(value.type + (name ? '<' + name + '>' : '')) +
              ')'
          });
        }
        return visit
        function visit() {
          var result = [];
          var subresult;
          var offset;
          var grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$z(visitor(node, parents));
            if (result[0] === EXIT$z) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$z) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$z) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$z(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$z, value]
  }
  return [value]
}

const visit$z =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$z(tree, test, overload, reverse);
      function overload(node, parents) {
        var parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

const own$1 = {}.hasOwnProperty;
function messageControl(options) {
  if (!options || typeof options !== 'object' || !options.name) {
    throw new Error(
      'Expected `name` in `options`, got `' + (options || {}).name + '`'
    )
  }
  if (!options.marker) {
    throw new Error(
      'Expected `marker` in `options`, got `' + options.marker + '`'
    )
  }
  const enable = 'enable' in options && options.enable ? options.enable : [];
  const disable = 'disable' in options && options.disable ? options.disable : [];
  let reset = options.reset;
  const sources =
    typeof options.source === 'string'
      ? [options.source]
      : options.source || [options.name];
  return transformer
  function transformer(tree, file) {
    const toOffset = location(file).toOffset;
    const initial = !reset;
    const gaps = detectGaps(tree, file);
    const scope = {};
    const globals = [];
    visit$z(tree, options.test, visitor);
    file.messages = file.messages.filter((m) => filter(m));
    function visitor(node, position, parent) {
      const mark = options.marker(node);
      if (!mark || mark.name !== options.name) {
        return
      }
      const ruleIds = mark.attributes.split(/\s/g);
      const point = mark.node.position && mark.node.position.start;
      const next =
        (parent && position !== null && parent.children[position + 1]) ||
        undefined;
      const tail = (next && next.position && next.position.end) || undefined;
      let index = -1;
      const verb = ruleIds.shift();
      if (verb !== 'enable' && verb !== 'disable' && verb !== 'ignore') {
        file.fail(
          'Unknown keyword `' +
            verb +
            '`: expected ' +
            "`'enable'`, `'disable'`, or `'ignore'`",
          mark.node
        );
      }
      if (ruleIds.length > 0) {
        while (++index < ruleIds.length) {
          const ruleId = ruleIds[index];
          if (isKnown(ruleId, verb, mark.node)) {
            toggle(point, verb === 'enable', ruleId);
            if (verb === 'ignore') {
              toggle(tail, true, ruleId);
            }
          }
        }
      } else if (verb === 'ignore') {
        toggle(point, false);
        toggle(tail, true);
      } else {
        toggle(point, verb === 'enable');
        reset = verb !== 'enable';
      }
    }
    function filter(message) {
      let gapIndex = gaps.length;
      if (!message.source || !sources.includes(message.source)) {
        return true
      }
      if (!message.line) {
        message.line = 1;
      }
      if (!message.column) {
        message.column = 1;
      }
      const offset = toOffset(message);
      while (gapIndex--) {
        if (gaps[gapIndex][0] <= offset && gaps[gapIndex][1] > offset) {
          return false
        }
      }
      return (
        (!message.ruleId ||
          check(message, scope[message.ruleId], message.ruleId)) &&
        check(message, globals)
      )
    }
    function isKnown(ruleId, verb, node) {
      const result = options.known ? options.known.includes(ruleId) : true;
      if (!result) {
        file.message(
          'Unknown rule: cannot ' + verb + " `'" + ruleId + "'`",
          node
        );
      }
      return result
    }
    function getState(ruleId) {
      const ranges = ruleId ? scope[ruleId] : globals;
      if (ranges && ranges.length > 0) {
        return ranges[ranges.length - 1].state
      }
      if (!ruleId) {
        return !reset
      }
      return reset ? enable.includes(ruleId) : !disable.includes(ruleId)
    }
    function toggle(point, state, ruleId) {
      let markers = ruleId ? scope[ruleId] : globals;
      if (!markers) {
        markers = [];
        scope[String(ruleId)] = markers;
      }
      const previousState = getState(ruleId);
      if (state !== previousState) {
        markers.push({state, point});
      }
      if (!ruleId) {
        for (ruleId in scope) {
          if (own$1.call(scope, ruleId)) {
            toggle(point, state, ruleId);
          }
        }
      }
    }
    function check(message, ranges, ruleId) {
      if (ranges && ranges.length > 0) {
        let index = ranges.length;
        while (index--) {
          const range = ranges[index];
          if (
            message.line &&
            message.column &&
            range.point &&
            range.point.line &&
            range.point.column &&
            (range.point.line < message.line ||
              (range.point.line === message.line &&
                range.point.column <= message.column))
          ) {
            return range.state === true
          }
        }
      }
      if (!ruleId) {
        return Boolean(initial || reset)
      }
      return reset ? enable.includes(ruleId) : !disable.includes(ruleId)
    }
  }
}
function detectGaps(tree, file) {
  const children = tree.children || [];
  const lastNode = children[children.length - 1];
  const gaps = [];
  let offset = 0;
  let gap;
  visit$z(tree, one);
  if (
    lastNode &&
    lastNode.position &&
    lastNode.position.end &&
    offset === lastNode.position.end.offset &&
    file.toString().slice(offset).trim() !== ''
  ) {
    update();
    update(
      tree &&
        tree.position &&
        tree.position.end &&
        tree.position.end.offset &&
        tree.position.end.offset - 1
    );
  }
  return gaps
  function one(node) {
    update(node.position && node.position.start && node.position.start.offset);
    if (!('children' in node)) {
      update(node.position && node.position.end && node.position.end.offset);
    }
  }
  function update(latest) {
    if (latest === null || latest === undefined) {
      gap = true;
    } else if (offset < latest) {
      if (gap) {
        gaps.push([offset, latest]);
        gap = undefined;
      }
      offset = latest;
    }
  }
}

const commentExpression = /\s*([a-zA-Z\d-]+)(\s+([\s\S]*))?\s*/;
const esCommentExpression = new RegExp(
  '(\\s*\\/\\*' + commentExpression.source + '\\*\\/\\s*)'
);
const markerExpression = new RegExp(
  '(\\s*<!--' + commentExpression.source + '-->\\s*)'
);
function commentMarker(value) {
  if (
    isNode(value) &&
    (value.type === 'html' ||
      value.type === 'comment' ||
      value.type === 'mdxFlowExpression' ||
      value.type === 'mdxTextExpression')
  ) {
    let offset = 2;
    let match;
    if (value.type === 'comment') {
      match = value.value.match(commentExpression);
      offset = 1;
    } else if (value.type === 'html') {
      match = value.value.match(markerExpression);
    } else if (
      value.type === 'mdxFlowExpression' ||
      value.type === 'mdxTextExpression'
    ) {
      match = value.value.match(esCommentExpression);
    }
    if (match && match[0].length === value.value.length) {
      const parameters = parseParameters(match[offset + 1] || '');
      if (parameters) {
        return {
          name: match[offset],
          attributes: (match[offset + 2] || '').trim(),
          parameters,
          node: value
        }
      }
    }
  }
  return null
}
function parseParameters(value) {
  const parameters = {};
  return value
    .replace(
      /\s+([-\w]+)(?:=(?:"((?:\\[\s\S]|[^"])*)"|'((?:\\[\s\S]|[^'])*)'|((?:\\[\s\S]|[^"'\s])+)))?/gi,
      replacer
    )
    .replace(/\s+/g, '')
    ? null
    : parameters
  function replacer(_, $1, $2, $3, $4) {
    let value = $2 === undefined ? ($3 === undefined ? $4 : $3) : $2;
    const number = Number(value);
    if (value === 'true' || value === undefined) {
      value = true;
    } else if (value === 'false') {
      value = false;
    } else if (value.trim() && !Number.isNaN(number)) {
      value = number;
    }
    parameters[$1] = value;
    return ''
  }
}
function isNode(value) {
  return Boolean(value && typeof value === 'object' && 'type' in value)
}

const test = [
  'html',
  'comment',
  'mdxFlowExpression',
  'mdxTextExpression'
];
function remarkMessageControl(options) {
  return messageControl(
    Object.assign({marker: commentMarker, test}, options)
  )
}

function remarkLint() {
  this.use(lintMessageControl);
}
function lintMessageControl() {
  return remarkMessageControl({name: 'lint', source: 'remark-lint'})
}

function lintRule(meta, rule) {
  const id = typeof meta === 'string' ? meta : meta.origin;
  const url = typeof meta === 'string' ? undefined : meta.url;
  const parts = id.split(':');
  const source = parts[1] ? parts[0] : undefined;
  const ruleId = parts[1];
  Object.defineProperty(plugin, 'name', {value: id});
  return plugin
  function plugin(config) {
    const [severity, options] = coerce$1(ruleId, config);
    if (!severity) return
    const fatal = severity === 2;
    return (tree, file, next) => {
      let index = file.messages.length - 1;
      wrap(rule, (error) => {
        const messages = file.messages;
        if (error && !messages.includes(error)) {
          try {
            file.fail(error);
          } catch {}
        }
        while (++index < messages.length) {
          Object.assign(messages[index], {ruleId, source, fatal, url});
        }
        next();
      })(tree, file, options);
    }
  }
}
function coerce$1(name, config) {
  if (!Array.isArray(config)) return [1, config]
  const [severity, ...options] = config;
  switch (severity) {
    case false:
    case 'off':
    case 0: {
      return [0, ...options]
    }
    case true:
    case 'on':
    case 'warn':
    case 1: {
      return [1, ...options]
    }
    case 'error':
    case 2: {
      return [2, ...options]
    }
    default: {
      if (typeof severity !== 'number') return [1, config]
      throw new Error(
        'Incorrect severity `' +
          severity +
          '` for `' +
          name +
          '`, ' +
          'expected 0, 1, or 2'
      )
    }
  }
}

/**
 * ## When should I use this?
 *
 * You can use this package to check that fenced code markers are consistent.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Turn this rule on.
 * See [StackExchange](https://unix.stackexchange.com/questions/18743) for more
 * info.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * always adds final line endings.
 *
 * ## Example
 *
 * ##### `ok.md`
 *
 * ###### In
 *
 * > 👉 **Note**: `␊` represents a line feed (`\n`).
 *
 * ```markdown
 * Alpha␊
 * ```
 *
 * ###### Out
 *
 * No messages.
 *
 * ##### `not-ok.md`
 *
 * ###### In
 *
 * > 👉 **Note**: `␀` represents the end of the file.
 *
 * ```markdown
 * Bravo␀
 * ```
 *
 * ###### Out
 *
 * ```text
 * 1:1: Missing newline character at end of file
 * ```
 *
 * @module final-newline
 * @summary
 *   remark-lint rule to warn when files don’t end in a newline.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 */
const remarkLintFinalNewline = lintRule(
  {
    origin: 'remark-lint:final-newline',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-final-newline#readme'
  },
  (_, file) => {
    const value = String(file);
    const last = value.length - 1;
    if (last > -1 && value.charAt(last) !== '\n') {
      file.message('Missing newline character at end of file');
    }
  }
);
var remarkLintFinalNewline$1 = remarkLintFinalNewline;

function commonjsRequire(path) {
	throw new Error('Could not dynamically require "' + path + '". Please configure the dynamicRequireTargets or/and ignoreDynamicRequires option of @rollup/plugin-commonjs appropriately for this require call to work.');
}

var pluralize = {exports: {}};

(function (module, exports) {
	(function (root, pluralize) {
	  if (typeof commonjsRequire === 'function' && 'object' === 'object' && 'object' === 'object') {
	    module.exports = pluralize();
	  } else {
	    root.pluralize = pluralize();
	  }
	})(commonjsGlobal, function () {
	  var pluralRules = [];
	  var singularRules = [];
	  var uncountables = {};
	  var irregularPlurals = {};
	  var irregularSingles = {};
	  function sanitizeRule (rule) {
	    if (typeof rule === 'string') {
	      return new RegExp('^' + rule + '$', 'i');
	    }
	    return rule;
	  }
	  function restoreCase (word, token) {
	    if (word === token) return token;
	    if (word === word.toLowerCase()) return token.toLowerCase();
	    if (word === word.toUpperCase()) return token.toUpperCase();
	    if (word[0] === word[0].toUpperCase()) {
	      return token.charAt(0).toUpperCase() + token.substr(1).toLowerCase();
	    }
	    return token.toLowerCase();
	  }
	  function interpolate (str, args) {
	    return str.replace(/\$(\d{1,2})/g, function (match, index) {
	      return args[index] || '';
	    });
	  }
	  function replace (word, rule) {
	    return word.replace(rule[0], function (match, index) {
	      var result = interpolate(rule[1], arguments);
	      if (match === '') {
	        return restoreCase(word[index - 1], result);
	      }
	      return restoreCase(match, result);
	    });
	  }
	  function sanitizeWord (token, word, rules) {
	    if (!token.length || uncountables.hasOwnProperty(token)) {
	      return word;
	    }
	    var len = rules.length;
	    while (len--) {
	      var rule = rules[len];
	      if (rule[0].test(word)) return replace(word, rule);
	    }
	    return word;
	  }
	  function replaceWord (replaceMap, keepMap, rules) {
	    return function (word) {
	      var token = word.toLowerCase();
	      if (keepMap.hasOwnProperty(token)) {
	        return restoreCase(word, token);
	      }
	      if (replaceMap.hasOwnProperty(token)) {
	        return restoreCase(word, replaceMap[token]);
	      }
	      return sanitizeWord(token, word, rules);
	    };
	  }
	  function checkWord (replaceMap, keepMap, rules, bool) {
	    return function (word) {
	      var token = word.toLowerCase();
	      if (keepMap.hasOwnProperty(token)) return true;
	      if (replaceMap.hasOwnProperty(token)) return false;
	      return sanitizeWord(token, token, rules) === token;
	    };
	  }
	  function pluralize (word, count, inclusive) {
	    var pluralized = count === 1
	      ? pluralize.singular(word) : pluralize.plural(word);
	    return (inclusive ? count + ' ' : '') + pluralized;
	  }
	  pluralize.plural = replaceWord(
	    irregularSingles, irregularPlurals, pluralRules
	  );
	  pluralize.isPlural = checkWord(
	    irregularSingles, irregularPlurals, pluralRules
	  );
	  pluralize.singular = replaceWord(
	    irregularPlurals, irregularSingles, singularRules
	  );
	  pluralize.isSingular = checkWord(
	    irregularPlurals, irregularSingles, singularRules
	  );
	  pluralize.addPluralRule = function (rule, replacement) {
	    pluralRules.push([sanitizeRule(rule), replacement]);
	  };
	  pluralize.addSingularRule = function (rule, replacement) {
	    singularRules.push([sanitizeRule(rule), replacement]);
	  };
	  pluralize.addUncountableRule = function (word) {
	    if (typeof word === 'string') {
	      uncountables[word.toLowerCase()] = true;
	      return;
	    }
	    pluralize.addPluralRule(word, '$0');
	    pluralize.addSingularRule(word, '$0');
	  };
	  pluralize.addIrregularRule = function (single, plural) {
	    plural = plural.toLowerCase();
	    single = single.toLowerCase();
	    irregularSingles[single] = plural;
	    irregularPlurals[plural] = single;
	  };
	  [
	    ['I', 'we'],
	    ['me', 'us'],
	    ['he', 'they'],
	    ['she', 'they'],
	    ['them', 'them'],
	    ['myself', 'ourselves'],
	    ['yourself', 'yourselves'],
	    ['itself', 'themselves'],
	    ['herself', 'themselves'],
	    ['himself', 'themselves'],
	    ['themself', 'themselves'],
	    ['is', 'are'],
	    ['was', 'were'],
	    ['has', 'have'],
	    ['this', 'these'],
	    ['that', 'those'],
	    ['echo', 'echoes'],
	    ['dingo', 'dingoes'],
	    ['volcano', 'volcanoes'],
	    ['tornado', 'tornadoes'],
	    ['torpedo', 'torpedoes'],
	    ['genus', 'genera'],
	    ['viscus', 'viscera'],
	    ['stigma', 'stigmata'],
	    ['stoma', 'stomata'],
	    ['dogma', 'dogmata'],
	    ['lemma', 'lemmata'],
	    ['schema', 'schemata'],
	    ['anathema', 'anathemata'],
	    ['ox', 'oxen'],
	    ['axe', 'axes'],
	    ['die', 'dice'],
	    ['yes', 'yeses'],
	    ['foot', 'feet'],
	    ['eave', 'eaves'],
	    ['goose', 'geese'],
	    ['tooth', 'teeth'],
	    ['quiz', 'quizzes'],
	    ['human', 'humans'],
	    ['proof', 'proofs'],
	    ['carve', 'carves'],
	    ['valve', 'valves'],
	    ['looey', 'looies'],
	    ['thief', 'thieves'],
	    ['groove', 'grooves'],
	    ['pickaxe', 'pickaxes'],
	    ['passerby', 'passersby']
	  ].forEach(function (rule) {
	    return pluralize.addIrregularRule(rule[0], rule[1]);
	  });
	  [
	    [/s?$/i, 's'],
	    [/[^\u0000-\u007F]$/i, '$0'],
	    [/([^aeiou]ese)$/i, '$1'],
	    [/(ax|test)is$/i, '$1es'],
	    [/(alias|[^aou]us|t[lm]as|gas|ris)$/i, '$1es'],
	    [/(e[mn]u)s?$/i, '$1s'],
	    [/([^l]ias|[aeiou]las|[ejzr]as|[iu]am)$/i, '$1'],
	    [/(alumn|syllab|vir|radi|nucle|fung|cact|stimul|termin|bacill|foc|uter|loc|strat)(?:us|i)$/i, '$1i'],
	    [/(alumn|alg|vertebr)(?:a|ae)$/i, '$1ae'],
	    [/(seraph|cherub)(?:im)?$/i, '$1im'],
	    [/(her|at|gr)o$/i, '$1oes'],
	    [/(agend|addend|millenni|dat|extrem|bacteri|desiderat|strat|candelabr|errat|ov|symposi|curricul|automat|quor)(?:a|um)$/i, '$1a'],
	    [/(apheli|hyperbat|periheli|asyndet|noumen|phenomen|criteri|organ|prolegomen|hedr|automat)(?:a|on)$/i, '$1a'],
	    [/sis$/i, 'ses'],
	    [/(?:(kni|wi|li)fe|(ar|l|ea|eo|oa|hoo)f)$/i, '$1$2ves'],
	    [/([^aeiouy]|qu)y$/i, '$1ies'],
	    [/([^ch][ieo][ln])ey$/i, '$1ies'],
	    [/(x|ch|ss|sh|zz)$/i, '$1es'],
	    [/(matr|cod|mur|sil|vert|ind|append)(?:ix|ex)$/i, '$1ices'],
	    [/\b((?:tit)?m|l)(?:ice|ouse)$/i, '$1ice'],
	    [/(pe)(?:rson|ople)$/i, '$1ople'],
	    [/(child)(?:ren)?$/i, '$1ren'],
	    [/eaux$/i, '$0'],
	    [/m[ae]n$/i, 'men'],
	    ['thou', 'you']
	  ].forEach(function (rule) {
	    return pluralize.addPluralRule(rule[0], rule[1]);
	  });
	  [
	    [/s$/i, ''],
	    [/(ss)$/i, '$1'],
	    [/(wi|kni|(?:after|half|high|low|mid|non|night|[^\w]|^)li)ves$/i, '$1fe'],
	    [/(ar|(?:wo|[ae])l|[eo][ao])ves$/i, '$1f'],
	    [/ies$/i, 'y'],
	    [/\b([pl]|zomb|(?:neck|cross)?t|coll|faer|food|gen|goon|group|lass|talk|goal|cut)ies$/i, '$1ie'],
	    [/\b(mon|smil)ies$/i, '$1ey'],
	    [/\b((?:tit)?m|l)ice$/i, '$1ouse'],
	    [/(seraph|cherub)im$/i, '$1'],
	    [/(x|ch|ss|sh|zz|tto|go|cho|alias|[^aou]us|t[lm]as|gas|(?:her|at|gr)o|[aeiou]ris)(?:es)?$/i, '$1'],
	    [/(analy|diagno|parenthe|progno|synop|the|empha|cri|ne)(?:sis|ses)$/i, '$1sis'],
	    [/(movie|twelve|abuse|e[mn]u)s$/i, '$1'],
	    [/(test)(?:is|es)$/i, '$1is'],
	    [/(alumn|syllab|vir|radi|nucle|fung|cact|stimul|termin|bacill|foc|uter|loc|strat)(?:us|i)$/i, '$1us'],
	    [/(agend|addend|millenni|dat|extrem|bacteri|desiderat|strat|candelabr|errat|ov|symposi|curricul|quor)a$/i, '$1um'],
	    [/(apheli|hyperbat|periheli|asyndet|noumen|phenomen|criteri|organ|prolegomen|hedr|automat)a$/i, '$1on'],
	    [/(alumn|alg|vertebr)ae$/i, '$1a'],
	    [/(cod|mur|sil|vert|ind)ices$/i, '$1ex'],
	    [/(matr|append)ices$/i, '$1ix'],
	    [/(pe)(rson|ople)$/i, '$1rson'],
	    [/(child)ren$/i, '$1'],
	    [/(eau)x?$/i, '$1'],
	    [/men$/i, 'man']
	  ].forEach(function (rule) {
	    return pluralize.addSingularRule(rule[0], rule[1]);
	  });
	  [
	    'adulthood',
	    'advice',
	    'agenda',
	    'aid',
	    'aircraft',
	    'alcohol',
	    'ammo',
	    'analytics',
	    'anime',
	    'athletics',
	    'audio',
	    'bison',
	    'blood',
	    'bream',
	    'buffalo',
	    'butter',
	    'carp',
	    'cash',
	    'chassis',
	    'chess',
	    'clothing',
	    'cod',
	    'commerce',
	    'cooperation',
	    'corps',
	    'debris',
	    'diabetes',
	    'digestion',
	    'elk',
	    'energy',
	    'equipment',
	    'excretion',
	    'expertise',
	    'firmware',
	    'flounder',
	    'fun',
	    'gallows',
	    'garbage',
	    'graffiti',
	    'hardware',
	    'headquarters',
	    'health',
	    'herpes',
	    'highjinks',
	    'homework',
	    'housework',
	    'information',
	    'jeans',
	    'justice',
	    'kudos',
	    'labour',
	    'literature',
	    'machinery',
	    'mackerel',
	    'mail',
	    'media',
	    'mews',
	    'moose',
	    'music',
	    'mud',
	    'manga',
	    'news',
	    'only',
	    'personnel',
	    'pike',
	    'plankton',
	    'pliers',
	    'police',
	    'pollution',
	    'premises',
	    'rain',
	    'research',
	    'rice',
	    'salmon',
	    'scissors',
	    'series',
	    'sewage',
	    'shambles',
	    'shrimp',
	    'software',
	    'species',
	    'staff',
	    'swine',
	    'tennis',
	    'traffic',
	    'transportation',
	    'trout',
	    'tuna',
	    'wealth',
	    'welfare',
	    'whiting',
	    'wildebeest',
	    'wildlife',
	    'you',
	    /pok[eé]mon$/i,
	    /[^aeiou]ese$/i,
	    /deer$/i,
	    /fish$/i,
	    /measles$/i,
	    /o[iu]s$/i,
	    /pox$/i,
	    /sheep$/i
	  ].forEach(pluralize.addUncountableRule);
	  return pluralize;
	});
} (pluralize));
var pluralizeExports = pluralize.exports;
var plural = getDefaultExportFromCjs(pluralizeExports);

const convert$y =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$y
      }
      if (typeof test === 'string') {
        return typeFactory$y(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$y(test) : propsFactory$y(test)
      }
      if (typeof test === 'function') {
        return castFactory$y(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$y(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$y(tests[index]);
  }
  return castFactory$y(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$y(check) {
  return castFactory$y(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$y(check) {
  return castFactory$y(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$y(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$y() {
  return true
}

function color$z(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$y = true;
const EXIT$y = false;
const SKIP$y = 'skip';
const visitParents$y =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$y(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$z(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$y(visitor(node, parents));
            if (result[0] === EXIT$y) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$y) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$y) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$y(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$y, value]
  }
  return [value]
}

const visit$y =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$y(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that list items are not indented.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * There is no specific handling of indented list items (or anything else) in
 * markdown.
 * While it is possible to use an indent to align ordered lists on their marker:
 *
 * ```markdown
 *   1. One
 *  10. Ten
 * 100. Hundred
 * ```
 *
 * …such a style is uncommon and a bit hard to maintain: adding a 10th item
 * means 9 other items have to change (more arduous, while unlikely, would be
 * the 100th item).
 * Hence, it’s recommended to not indent items and to turn this rule on.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats all items without indent.
 *
 * @module list-item-bullet-indent
 * @summary
 *   remark-lint rule to warn when list items are indented.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   Paragraph.
 *
 *   * List item
 *   * List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Paragraph.
 *
 *   ·* List item
 *   ·* List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   3:2: Incorrect indentation before bullet: remove 1 space
 *   4:2: Incorrect indentation before bullet: remove 1 space
 */
const remarkLintListItemBulletIndent = lintRule(
  {
    origin: 'remark-lint:list-item-bullet-indent',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-list-item-bullet-indent#readme'
  },
  (tree, file) => {
    visit$y(tree, 'list', (list, _, grandparent) => {
      let index = -1;
      while (++index < list.children.length) {
        const item = list.children[index];
        if (
          grandparent &&
          grandparent.type === 'root' &&
          grandparent.position &&
          typeof grandparent.position.start.column === 'number' &&
          item.position &&
          typeof item.position.start.column === 'number'
        ) {
          const indent =
            item.position.start.column - grandparent.position.start.column;
          if (indent) {
            file.message(
              'Incorrect indentation before bullet: remove ' +
                indent +
                ' ' +
                plural('space', indent),
              item.position.start
            );
          }
        }
      }
    });
  }
);
var remarkLintListItemBulletIndent$1 = remarkLintListItemBulletIndent;

const convert$x =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$x
      }
      if (typeof test === 'string') {
        return typeFactory$x(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$x(test) : propsFactory$x(test)
      }
      if (typeof test === 'function') {
        return castFactory$x(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$x(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$x(tests[index]);
  }
  return castFactory$x(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$x(check) {
  return castFactory$x(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$x(check) {
  return castFactory$x(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$x(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$x() {
  return true
}

function color$y(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$x = true;
const EXIT$x = false;
const SKIP$x = 'skip';
const visitParents$x =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$x(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$y(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$x(visitor(node, parents));
            if (result[0] === EXIT$x) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$x) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$x) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$x(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$x, value]
  }
  return [value]
}

const visit$x =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$x(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

const pointStart = point$2('start');
const pointEnd = point$2('end');
function point$2(type) {
  return point
  function point(node) {
    const point = (node && node.position && node.position[type]) || {};
    return {
      line: point.line || null,
      column: point.column || null,
      offset: point.offset > -1 ? point.offset : null
    }
  }
}

function generated(node) {
  return (
    !node ||
    !node.position ||
    !node.position.start ||
    !node.position.start.line ||
    !node.position.start.column ||
    !node.position.end ||
    !node.position.end.line ||
    !node.position.end.column
  )
}

/**
 * ## When should I use this?
 *
 * You can use this package to check that the spacing between list item markers
 * and content is inconsistent.
 *
 * ## API
 *
 * The following options (default: `'tab-size'`) are accepted:
 *
 * *   `'space'`
 *     — prefer a single space
 * *   `'tab-size'`
 *     — prefer spaces the size of the next tab stop
 * *   `'mixed'`
 *     — prefer `'space'` for tight lists and `'tab-size'` for loose lists
 *
 * ## Recommendation
 *
 * First, some background.
 * The number of spaces that occur after list markers (`*`, `-`, and `+` for
 * unordered lists, or `.` and `)` for unordered lists) and before the content
 * on the first line, defines how much indentation can be used for further
 * lines.
 * At least one space is required and up to 4 spaces are allowed (if there is no
 * further content after the marker then it’s a blank line which is handled as
 * if there was one space; if there are 5 or more spaces and then content, it’s
 * also seen as one space and the rest is seen as indented code).
 *
 * There are two types of lists in markdown (other than ordered and unordered):
 * tight and loose lists.
 * Lists are tight by default but if there is a blank line between two list
 * items or between two blocks inside an item, that turns the whole list into a
 * loose list.
 * When turning markdown into HTML, paragraphs in tight lists are not wrapped
 * in `<p>` tags.
 *
 * Historically, how indentation of lists works in markdown has been a mess,
 * especially with how they interact with indented code.
 * CommonMark made that a *lot* better, but there remain (documented but
 * complex) edge cases and some behavior intuitive.
 * Due to this, the default of this list is `'tab-size'`, which worked the best
 * in most markdown parsers.
 * Currently, the situation between markdown parsers is better, so choosing
 * `'space'` (which seems to be the most common style used by authors) should
 * be okay.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * uses `'tab-size'` (named `'tab'` there) by default.
 * [`listItemIndent: '1'` (for `'space'`) or `listItemIndent: 'mixed'`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionslistitemindent)
 * is supported.
 *
 * @module list-item-indent
 * @summary
 *   remark-lint rule to warn when spacing between list item markers and
 *   content is inconsistent.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   *···List
 *   ····item.
 *
 *   Paragraph.
 *
 *   11.·List
 *   ····item.
 *
 *   Paragraph.
 *
 *   *···List
 *   ····item.
 *
 *   *···List
 *   ····item.
 *
 * @example
 *   {"name": "ok.md", "config": "mixed"}
 *
 *   *·List item.
 *
 *   Paragraph.
 *
 *   11.·List item
 *
 *   Paragraph.
 *
 *   *···List
 *   ····item.
 *
 *   *···List
 *   ····item.
 *
 * @example
 *   {"name": "ok.md", "config": "space"}
 *
 *   *·List item.
 *
 *   Paragraph.
 *
 *   11.·List item
 *
 *   Paragraph.
 *
 *   *·List
 *   ··item.
 *
 *   *·List
 *   ··item.
 *
 * @example
 *   {"name": "not-ok.md", "config": "space", "label": "input"}
 *
 *   *···List
 *   ····item.
 *
 * @example
 *   {"name": "not-ok.md", "config": "space", "label": "output"}
 *
 *    1:5: Incorrect list-item indent: remove 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "config": "tab-size", "label": "input"}
 *
 *   *·List
 *   ··item.
 *
 * @example
 *   {"name": "not-ok.md", "config": "tab-size", "label": "output"}
 *
 *    1:3: Incorrect list-item indent: add 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "config": "mixed", "label": "input"}
 *
 *   *···List item.
 *
 * @example
 *   {"name": "not-ok.md", "config": "mixed", "label": "output"}
 *
 *    1:5: Incorrect list-item indent: remove 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "config": "💩", "label": "output", "positionless": true}
 *
 *    1:1: Incorrect list-item indent style `💩`: use either `'tab-size'`, `'space'`, or `'mixed'`
 */
const remarkLintListItemIndent = lintRule(
  {
    origin: 'remark-lint:list-item-indent',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-list-item-indent#readme'
  },
  (tree, file, option = 'tab-size') => {
    const value = String(file);
    if (option !== 'tab-size' && option !== 'space' && option !== 'mixed') {
      file.fail(
        'Incorrect list-item indent style `' +
          option +
          "`: use either `'tab-size'`, `'space'`, or `'mixed'`"
      );
    }
    visit$x(tree, 'list', (node) => {
      if (generated(node)) return
      const spread = node.spread;
      let index = -1;
      while (++index < node.children.length) {
        const item = node.children[index];
        const head = item.children[0];
        const final = pointStart(head);
        const marker = value
          .slice(pointStart(item).offset, final.offset)
          .replace(/\[[x ]?]\s*$/i, '');
        const bulletSize = marker.replace(/\s+$/, '').length;
        const style =
          option === 'tab-size' || (option === 'mixed' && spread)
            ? Math.ceil(bulletSize / 4) * 4
            : bulletSize + 1;
        if (marker.length !== style) {
          const diff = style - marker.length;
          const abs = Math.abs(diff);
          file.message(
            'Incorrect list-item indent: ' +
              (diff > 0 ? 'add' : 'remove') +
              ' ' +
              abs +
              ' ' +
              plural('space', abs),
            final
          );
        }
      }
    });
  }
);
var remarkLintListItemIndent$1 = remarkLintListItemIndent;

const convert$w =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$w
      }
      if (typeof test === 'string') {
        return typeFactory$w(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$w(test) : propsFactory$w(test)
      }
      if (typeof test === 'function') {
        return castFactory$w(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$w(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$w(tests[index]);
  }
  return castFactory$w(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$w(check) {
  return castFactory$w(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$w(check) {
  return castFactory$w(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$w(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$w() {
  return true
}

function color$x(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$w = true;
const EXIT$w = false;
const SKIP$w = 'skip';
const visitParents$w =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$w(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$x(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$w(visitor(node, parents));
            if (result[0] === EXIT$w) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$w) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$w) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$w(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$w, value]
  }
  return [value]
}

const visit$w =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$w(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that lines in block quotes start with `>`.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Rules around “lazy” lines are not straightforward and visually confusing,
 * so it’s recommended to start each line with a `>`.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * adds `>` markers to every line in a block quote.
 *
 * @module no-blockquote-without-marker
 * @summary
 *   remark-lint rule to warn when lines in block quotes start without `>`.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   > Foo…
 *   > …bar…
 *   > …baz.
 *
 * @example
 *   {"name": "ok-tabs.md"}
 *
 *   >»Foo…
 *   >»…bar…
 *   >»…baz.
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   > Foo…
 *   …bar…
 *   > …baz.
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   2:1: Missing marker in block quote
 *
 * @example
 *   {"name": "not-ok-tabs.md", "label": "input"}
 *
 *   >»Foo…
 *   »…bar…
 *   …baz.
 *
 * @example
 *   {"name": "not-ok-tabs.md", "label": "output"}
 *
 *   2:1: Missing marker in block quote
 *   3:1: Missing marker in block quote
 */
const remarkLintNoBlockquoteWithoutMarker = lintRule(
  {
    origin: 'remark-lint:no-blockquote-without-marker',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-blockquote-without-marker#readme'
  },
  (tree, file) => {
    const value = String(file);
    const loc = location(file);
    visit$w(tree, 'blockquote', (node) => {
      let index = -1;
      while (++index < node.children.length) {
        const child = node.children[index];
        if (child.type === 'paragraph' && !generated(child)) {
          const end = pointEnd(child).line;
          const column = pointStart(child).column;
          let line = pointStart(child).line;
          while (++line <= end) {
            const offset = loc.toOffset({line, column});
            if (/>[\t ]+$/.test(value.slice(offset - 5, offset))) {
              continue
            }
            file.message('Missing marker in block quote', {
              line,
              column: column - 2
            });
          }
        }
      }
    });
  }
);
var remarkLintNoBlockquoteWithoutMarker$1 = remarkLintNoBlockquoteWithoutMarker;

const convert$v =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$v
      }
      if (typeof test === 'string') {
        return typeFactory$v(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$v(test) : propsFactory$v(test)
      }
      if (typeof test === 'function') {
        return castFactory$v(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$v(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$v(tests[index]);
  }
  return castFactory$v(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$v(check) {
  return castFactory$v(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$v(check) {
  return castFactory$v(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$v(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$v() {
  return true
}

function color$w(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$v = true;
const EXIT$v = false;
const SKIP$v = 'skip';
const visitParents$v =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$v(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$w(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$v(visitor(node, parents));
            if (result[0] === EXIT$v) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$v) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$v) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$v(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$v, value]
  }
  return [value]
}

const visit$v =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$v(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

const emptyOptions$1 = {};
function toString$1(value, options) {
  const settings = options || emptyOptions$1;
  const includeImageAlt =
    typeof settings.includeImageAlt === 'boolean'
      ? settings.includeImageAlt
      : true;
  const includeHtml =
    typeof settings.includeHtml === 'boolean' ? settings.includeHtml : true;
  return one$1(value, includeImageAlt, includeHtml)
}
function one$1(value, includeImageAlt, includeHtml) {
  if (node$1(value)) {
    if ('value' in value) {
      return value.type === 'html' && !includeHtml ? '' : value.value
    }
    if (includeImageAlt && 'alt' in value && value.alt) {
      return value.alt
    }
    if ('children' in value) {
      return all$1(value.children, includeImageAlt, includeHtml)
    }
  }
  if (Array.isArray(value)) {
    return all$1(value, includeImageAlt, includeHtml)
  }
  return ''
}
function all$1(values, includeImageAlt, includeHtml) {
  const result = [];
  let index = -1;
  while (++index < values.length) {
    result[index] = one$1(values[index], includeImageAlt, includeHtml);
  }
  return result.join('')
}
function node$1(value) {
  return Boolean(value && typeof value === 'object')
}

/**
 * ## When should I use this?
 *
 * You can use this package to check that autolink literal URLs are not used.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Autolink literal URLs (just a URL) are a feature enabled by GFM.
 * They don’t work everywhere.
 * Due to this, it’s recommended to instead use normal autolinks
 * (`<https://url>`) or links (`[text](url)`).
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * never creates autolink literals and always uses normal autolinks (`<url>`).
 *
 * @module no-literal-urls
 * @summary
 *   remark-lint rule to warn for autolink literals.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   <http://foo.bar/baz>
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "gfm": true}
 *
 *   http://foo.bar/baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "gfm": true}
 *
 *   1:1-1:19: Don’t use literal URLs without angle brackets
 */
const remarkLintNoLiteralUrls = lintRule(
  {
    origin: 'remark-lint:no-literal-urls',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-literal-urls#readme'
  },
  (tree, file) => {
    visit$v(tree, 'link', (node) => {
      const value = toString$1(node);
      if (
        !generated(node) &&
        pointStart(node).column === pointStart(node.children[0]).column &&
        pointEnd(node).column ===
          pointEnd(node.children[node.children.length - 1]).column &&
        (node.url === 'mailto:' + value || node.url === value)
      ) {
        file.message('Don’t use literal URLs without angle brackets', node);
      }
    });
  }
);
var remarkLintNoLiteralUrls$1 = remarkLintNoLiteralUrls;

const convert$u =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$u
      }
      if (typeof test === 'string') {
        return typeFactory$u(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$u(test) : propsFactory$u(test)
      }
      if (typeof test === 'function') {
        return castFactory$u(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$u(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$u(tests[index]);
  }
  return castFactory$u(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$u(check) {
  return castFactory$u(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$u(check) {
  return castFactory$u(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$u(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$u() {
  return true
}

function color$v(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$u = true;
const EXIT$u = false;
const SKIP$u = 'skip';
const visitParents$u =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$u(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$v(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$u(visitor(node, parents));
            if (result[0] === EXIT$u) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$u) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$u) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$u(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$u, value]
  }
  return [value]
}

const visit$u =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$u(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that ordered list markers are consistent.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `'.'`
 *     — prefer dots
 * *   `')'`
 *     — prefer parens
 * *   `'consistent'`
 *     — detect the first used style and warn when further markers differ
 *
 * ## Recommendation
 *
 * Parens for list markers were not supported in markdown before CommonMark.
 * While they should work in most places now, not all markdown parsers follow
 * CommonMark.
 * Due to this, it’s recommended to prefer dots.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats ordered lists with dots by default.
 * Pass
 * [`bulletOrdered: ')'`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionsbulletordered)
 * to always use parens.
 *
 * @module ordered-list-marker-style
 * @summary
 *   remark-lint rule to warn when ordered list markers are inconsistent.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   1.  Foo
 *
 *
 *   1.  Bar
 *
 *   Unordered lists are not affected by this rule.
 *
 *   * Foo
 *
 * @example
 *   {"name": "ok.md", "config": "."}
 *
 *   1.  Foo
 *
 *   2.  Bar
 *
 * @example
 *   {"name": "ok.md", "config": ")"}
 *
 *   1)  Foo
 *
 *   2)  Bar
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   1.  Foo
 *
 *   2)  Bar
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   3:1-3:8: Marker style should be `.`
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "config": "💩", "positionless": true}
 *
 *   1:1: Incorrect ordered list item marker style `💩`: use either `'.'` or `')'`
 */
const remarkLintOrderedListMarkerStyle = lintRule(
  {
    origin: 'remark-lint:ordered-list-marker-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-ordered-list-marker-style#readme'
  },
  (tree, file, option = 'consistent') => {
    const value = String(file);
    if (option !== 'consistent' && option !== '.' && option !== ')') {
      file.fail(
        'Incorrect ordered list item marker style `' +
          option +
          "`: use either `'.'` or `')'`"
      );
    }
    visit$u(tree, 'list', (node) => {
      let index = -1;
      if (!node.ordered) return
      while (++index < node.children.length) {
        const child = node.children[index];
        if (!generated(child)) {
          const marker =  (
            value
              .slice(
                pointStart(child).offset,
                pointStart(child.children[0]).offset
              )
              .replace(/\s|\d/g, '')
              .replace(/\[[x ]?]\s*$/i, '')
          );
          if (option === 'consistent') {
            option = marker;
          } else if (marker !== option) {
            file.message('Marker style should be `' + option + '`', child);
          }
        }
      }
    });
  }
);
var remarkLintOrderedListMarkerStyle$1 = remarkLintOrderedListMarkerStyle;

const convert$t =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$t
      }
      if (typeof test === 'string') {
        return typeFactory$t(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$t(test) : propsFactory$t(test)
      }
      if (typeof test === 'function') {
        return castFactory$t(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$t(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$t(tests[index]);
  }
  return castFactory$t(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$t(check) {
  return castFactory$t(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$t(check) {
  return castFactory$t(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$t(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$t() {
  return true
}

function color$u(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$t = true;
const EXIT$t = false;
const SKIP$t = 'skip';
const visitParents$t =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$t(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$u(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$t(visitor(node, parents));
            if (result[0] === EXIT$t) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$t) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$t) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$t(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$t, value]
  }
  return [value]
}

const visit$t =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$t(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that hard breaks use two spaces and
 * not more.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Less than two spaces do not create a hard breaks and more than two spaces
 * have no effect.
 * Due to this, it’s recommended to turn this rule on.
 *
 * @module hard-break-spaces
 * @summary
 *   remark-lint rule to warn when more spaces are used than needed
 *   for hard breaks.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   Lorem ipsum··
 *   dolor sit amet
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Lorem ipsum···
 *   dolor sit amet.
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:12-2:1: Use two spaces for hard line breaks
 */
const remarkLintHardBreakSpaces = lintRule(
  {
    origin: 'remark-lint:hard-break-spaces',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-hard-break-spaces#readme'
  },
  (tree, file) => {
    const value = String(file);
    visit$t(tree, 'break', (node) => {
      if (!generated(node)) {
        const slice = value
          .slice(pointStart(node).offset, pointEnd(node).offset)
          .split('\n', 1)[0]
          .replace(/\r$/, '');
        if (slice.length > 2) {
          file.message('Use two spaces for hard line breaks', node);
        }
      }
    });
  }
);
var remarkLintHardBreakSpaces$1 = remarkLintHardBreakSpaces;

function stringifyPosition$1(value) {
  if (!value || typeof value !== 'object') {
    return ''
  }
  if ('position' in value || 'type' in value) {
    return position$1(value.position)
  }
  if ('start' in value || 'end' in value) {
    return position$1(value)
  }
  if ('line' in value || 'column' in value) {
    return point$1(value)
  }
  return ''
}
function point$1(point) {
  return index$1(point && point.line) + ':' + index$1(point && point.column)
}
function position$1(pos) {
  return point$1(pos && pos.start) + '-' + point$1(pos && pos.end)
}
function index$1(value) {
  return value && typeof value === 'number' ? value : 1
}

const convert$s =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$s
      }
      if (typeof test === 'string') {
        return typeFactory$s(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$s(test) : propsFactory$s(test)
      }
      if (typeof test === 'function') {
        return castFactory$s(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$s(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$s(tests[index]);
  }
  return castFactory$s(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$s(check) {
  return castFactory$s(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$s(check) {
  return castFactory$s(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$s(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$s() {
  return true
}

function color$t(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$s = true;
const EXIT$s = false;
const SKIP$s = 'skip';
const visitParents$s =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$s(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$t(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$s(visitor(node, parents));
            if (result[0] === EXIT$s) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$s) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$s) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$s(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$s, value]
  }
  return [value]
}

const visit$s =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$s(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that identifiers are defined once.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * It’s a mistake when the same identifier is defined multiple times.
 *
 * @module no-duplicate-definitions
 * @summary
 *   remark-lint rule to warn when identifiers are defined multiple times.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   [foo]: bar
 *   [baz]: qux
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [foo]: bar
 *   [foo]: qux
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   2:1-2:11: Do not use definitions with the same identifier (1:1)
 */
const remarkLintNoDuplicateDefinitions = lintRule(
  {
    origin: 'remark-lint:no-duplicate-definitions',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-duplicate-definitions#readme'
  },
  (tree, file) => {
    const map = Object.create(null);
    visit$s(tree, (node) => {
      if (
        (node.type === 'definition' || node.type === 'footnoteDefinition') &&
        !generated(node)
      ) {
        const identifier = node.identifier;
        const duplicate = map[identifier];
        if (duplicate) {
          file.message(
            'Do not use definitions with the same identifier (' +
              duplicate +
              ')',
            node
          );
        }
        map[identifier] = stringifyPosition$1(pointStart(node));
      }
    });
  }
);
var remarkLintNoDuplicateDefinitions$1 = remarkLintNoDuplicateDefinitions;

const convert$r =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$r
      }
      if (typeof test === 'string') {
        return typeFactory$r(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$r(test) : propsFactory$r(test)
      }
      if (typeof test === 'function') {
        return castFactory$r(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$r(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$r(tests[index]);
  }
  return castFactory$r(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$r(check) {
  return castFactory$r(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$r(check) {
  return castFactory$r(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$r(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$r() {
  return true
}

function color$s(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$r = true;
const EXIT$r = false;
const SKIP$r = 'skip';
const visitParents$r =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$r(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$s(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$r(visitor(node, parents));
            if (result[0] === EXIT$r) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$r) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$r) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$r(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$r, value]
  }
  return [value]
}

const visit$r =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$r(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

function headingStyle(node, relative) {
  const last = node.children[node.children.length - 1];
  const depth = node.depth;
  const pos = node.position && node.position.end;
  const final = last && last.position && last.position.end;
  if (!pos) {
    return null
  }
  if (!last) {
    if (pos.column - 1 <= depth * 2) {
      return consolidate(depth, relative)
    }
    return 'atx-closed'
  }
  if (final && final.line + 1 === pos.line) {
    return 'setext'
  }
  if (final && final.column + depth < pos.column) {
    return 'atx-closed'
  }
  return consolidate(depth, relative)
}
function consolidate(depth, relative) {
  return depth < 3
    ? 'atx'
    : relative === 'atx' || relative === 'setext'
    ? relative
    : null
}

/**
 * ## When should I use this?
 *
 * You can use this package to check that there is on space between `#`
 * characters and the content in headings.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * One space is required and more than one space has no effect.
 * Due to this, it’s recommended to turn this rule on.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats headings with exactly one space.
 *
 * @module no-heading-content-indent
 * @summary
 *   remark-lint rule to warn when there are too many spaces between
 *   hashes and content in headings.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   #·Foo
 *
 *   ## Bar·##
 *
 *     ##·Baz
 *
 *   Setext headings are not affected.
 *
 *   Baz
 *   ===
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   #··Foo
 *
 *   ## Bar··##
 *
 *     ##··Baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:4: Remove 1 space before this heading’s content
 *   3:7: Remove 1 space after this heading’s content
 *   5:7: Remove 1 space before this heading’s content
 *
 * @example
 *   {"name": "empty-heading.md"}
 *
 *   #··
 */
const remarkLintNoHeadingContentIndent = lintRule(
  {
    origin: 'remark-lint:no-heading-content-indent',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-heading-content-indent#readme'
  },
  (tree, file) => {
    visit$r(tree, 'heading', (node) => {
      if (generated(node)) {
        return
      }
      const type = headingStyle(node, 'atx');
      if (type === 'atx' || type === 'atx-closed') {
        const head = pointStart(node.children[0]).column;
        if (!head) {
          return
        }
        const diff = head - pointStart(node).column - 1 - node.depth;
        if (diff) {
          file.message(
            'Remove ' +
              Math.abs(diff) +
              ' ' +
              plural('space', Math.abs(diff)) +
              ' before this heading’s content',
            pointStart(node.children[0])
          );
        }
      }
      if (type === 'atx-closed') {
        const final = pointEnd(node.children[node.children.length - 1]);
        const diff = pointEnd(node).column - final.column - 1 - node.depth;
        if (diff) {
          file.message(
            'Remove ' +
              diff +
              ' ' +
              plural('space', diff) +
              ' after this heading’s content',
            final
          );
        }
      }
    });
  }
);
var remarkLintNoHeadingContentIndent$1 = remarkLintNoHeadingContentIndent;

const convert$q =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$q
      }
      if (typeof test === 'string') {
        return typeFactory$q(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$q(test) : propsFactory$q(test)
      }
      if (typeof test === 'function') {
        return castFactory$q(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$q(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$q(tests[index]);
  }
  return castFactory$q(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$q(check) {
  return castFactory$q(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$q(check) {
  return castFactory$q(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$q(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$q() {
  return true
}

function color$r(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$q = true;
const EXIT$q = false;
const SKIP$q = 'skip';
const visitParents$q =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$q(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$r(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$q(visitor(node, parents));
            if (result[0] === EXIT$q) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$q) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$q) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$q(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$q, value]
  }
  return [value]
}

const visit$q =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$q(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

const emptyOptions = {};
function toString(value, options) {
  const settings = options || emptyOptions;
  const includeImageAlt =
    typeof settings.includeImageAlt === 'boolean'
      ? settings.includeImageAlt
      : true;
  const includeHtml =
    typeof settings.includeHtml === 'boolean' ? settings.includeHtml : true;
  return one(value, includeImageAlt, includeHtml)
}
function one(value, includeImageAlt, includeHtml) {
  if (node(value)) {
    if ('value' in value) {
      return value.type === 'html' && !includeHtml ? '' : value.value
    }
    if (includeImageAlt && 'alt' in value && value.alt) {
      return value.alt
    }
    if ('children' in value) {
      return all(value.children, includeImageAlt, includeHtml)
    }
  }
  if (Array.isArray(value)) {
    return all(value, includeImageAlt, includeHtml)
  }
  return ''
}
function all(values, includeImageAlt, includeHtml) {
  const result = [];
  let index = -1;
  while (++index < values.length) {
    result[index] = one(values[index], includeImageAlt, includeHtml);
  }
  return result.join('')
}
function node(value) {
  return Boolean(value && typeof value === 'object')
}

/**
 * ## When should I use this?
 *
 * You can use this package to check that inline constructs (links) are
 * not padded.
 * Historically, it was possible to pad emphasis, strong, and strikethrough
 * too, but this was removed in CommonMark, making this rule much less useful.
 *
 * ## API
 *
 * There are no options.
 *
 * @module no-inline-padding
 * @summary
 *   remark-lint rule to warn when inline constructs are padded.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   Alpha [bravo](http://echo.fox/trot)
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Alpha [ bravo ](http://echo.fox/trot)
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:7-1:38: Don’t pad `link` with inner spaces
 */
const remarkLintNoInlinePadding = lintRule(
  {
    origin: 'remark-lint:no-inline-padding',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-inline-padding#readme'
  },
  (tree, file) => {
    visit$q(tree, (node) => {
      if (
        (node.type === 'link' || node.type === 'linkReference') &&
        !generated(node)
      ) {
        const value = toString(node);
        if (value.charAt(0) === ' ' || value.charAt(value.length - 1) === ' ') {
          file.message('Don’t pad `' + node.type + '` with inner spaces', node);
        }
      }
    });
  }
);
var remarkLintNoInlinePadding$1 = remarkLintNoInlinePadding;

const convert$p =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$p
      }
      if (typeof test === 'string') {
        return typeFactory$p(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$p(test) : propsFactory$p(test)
      }
      if (typeof test === 'function') {
        return castFactory$p(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$p(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$p(tests[index]);
  }
  return castFactory$p(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$p(check) {
  return castFactory$p(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$p(check) {
  return castFactory$p(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$p(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$p() {
  return true
}

function color$q(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$p = true;
const EXIT$p = false;
const SKIP$p = 'skip';
const visitParents$p =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$p(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$q(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$p(visitor(node, parents));
            if (result[0] === EXIT$p) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$p) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$p) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$p(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$p, value]
  }
  return [value]
}

const visit$p =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$p(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that collapsed or full reference images
 * are used.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Shortcut references use an implicit style that looks a lot like something
 * that could occur as plain text instead of syntax.
 * In some cases, plain text is intended instead of an image.
 * Due to this, it’s recommended to use collapsed (or full) references
 * instead.
 *
 * @module no-shortcut-reference-image
 * @summary
 *   remark-lint rule to warn when shortcut reference images are used.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   ![foo][]
 *
 *   [foo]: http://foo.bar/baz.png
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ![foo]
 *
 *   [foo]: http://foo.bar/baz.png
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:7: Use the trailing [] on reference images
 */
const remarkLintNoShortcutReferenceImage = lintRule(
  {
    origin: 'remark-lint:no-shortcut-reference-image',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-shortcut-reference-image#readme'
  },
  (tree, file) => {
    visit$p(tree, 'imageReference', (node) => {
      if (!generated(node) && node.referenceType === 'shortcut') {
        file.message('Use the trailing [] on reference images', node);
      }
    });
  }
);
var remarkLintNoShortcutReferenceImage$1 = remarkLintNoShortcutReferenceImage;

const convert$o =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$o
      }
      if (typeof test === 'string') {
        return typeFactory$o(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$o(test) : propsFactory$o(test)
      }
      if (typeof test === 'function') {
        return castFactory$o(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$o(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$o(tests[index]);
  }
  return castFactory$o(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$o(check) {
  return castFactory$o(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$o(check) {
  return castFactory$o(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$o(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$o() {
  return true
}

function color$p(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$o = true;
const EXIT$o = false;
const SKIP$o = 'skip';
const visitParents$o =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$o(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$p(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$o(visitor(node, parents));
            if (result[0] === EXIT$o) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$o) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$o) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$o(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$o, value]
  }
  return [value]
}

const visit$o =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$o(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that collapsed or full reference links
 * are used.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Shortcut references use an implicit style that looks a lot like something
 * that could occur as plain text instead of syntax.
 * In some cases, plain text is intended instead of a link.
 * Due to this, it’s recommended to use collapsed (or full) references
 * instead.
 *
 * @module no-shortcut-reference-link
 * @summary
 *   remark-lint rule to warn when shortcut reference links are used.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   [foo][]
 *
 *   [foo]: http://foo.bar/baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [foo]
 *
 *   [foo]: http://foo.bar/baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:6: Use the trailing `[]` on reference links
 */
const remarkLintNoShortcutReferenceLink = lintRule(
  {
    origin: 'remark-lint:no-shortcut-reference-link',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-shortcut-reference-link#readme'
  },
  (tree, file) => {
    visit$o(tree, 'linkReference', (node) => {
      if (!generated(node) && node.referenceType === 'shortcut') {
        file.message('Use the trailing `[]` on reference links', node);
      }
    });
  }
);
var remarkLintNoShortcutReferenceLink$1 = remarkLintNoShortcutReferenceLink;

function normalizeIdentifier(value) {
  return (
    value
      .replace(/[\t\n\r ]+/g, ' ')
      .replace(/^ | $/g, '')
      .toLowerCase()
      .toUpperCase()
  )
}

const convert$n =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$n
      }
      if (typeof test === 'string') {
        return typeFactory$n(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$n(test) : propsFactory$n(test)
      }
      if (typeof test === 'function') {
        return castFactory$n(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$n(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$n(tests[index]);
  }
  return castFactory$n(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$n(check) {
  return castFactory$n(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$n(check) {
  return castFactory$n(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$n(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$n() {
  return true
}

function color$o(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$n = true;
const EXIT$n = false;
const SKIP$n = 'skip';
const visitParents$n =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$n(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$o(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$n(visitor(node, parents));
            if (result[0] === EXIT$n) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$n) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$n) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$n(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$n, value]
  }
  return [value]
}

const visit$n =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$n(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that referenced definitions are defined.
 *
 * ## API
 *
 * The following options (default: `undefined`) are accepted:
 *
 * *   `Object` with the following fields:
 *     *   `allow` (`Array<string | RegExp | { source: string }>`,
 *         default: `[]`)
 *         — text or regex that you want to be allowed between `[` and `]`
 *         even though it’s undefined; regex is provided via a `RegExp` object
 *         or via a `{source: string}` object where `source` is the source
 *         text of a case-insensitive regex
 *
 * ## Recommendation
 *
 * Shortcut references use an implicit syntax that could also occur as plain
 * text.
 * For example, it is reasonable to expect an author adding `[…]` to abbreviate
 * some text somewhere in a document:
 *
 * ```markdown
 * > Some […] quote.
 * ```
 *
 * This isn’t a problem, but it might become one when an author later adds a
 * definition:
 *
 * ```markdown
 * Some text. […][]
 *
 * […] #read-more "Read more"
 * ```
 *
 * The second author might expect only their newly added text to form a link,
 * but their changes also result in a link for the first author’s text.
 *
 * @module no-undefined-references
 * @summary
 *   remark-lint rule to warn when undefined definitions are referenced.
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   [foo][]
 *
 *   Just a [ bracket.
 *
 *   Typically, you’d want to use escapes (with a backslash: \\) to escape what
 *   could turn into a \[reference otherwise].
 *
 *   Just two braces can’t link: [].
 *
 *   [foo]: https://example.com
 *
 * @example
 *   {"name": "ok-allow.md", "config": {"allow": ["...", "…"]}}
 *
 *   > Eliding a portion of a quoted passage […] is acceptable.
 *
 * @example
 *   {"name": "ok-allow.md", "config": {"allow": ["a", {"source": "^b\\."}]}}
 *
 *   [foo][b.c]
 *
 *   [bar][a]
 *
 *   Matching is case-insensitive: [bar][B.C]
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [bar]
 *
 *   [baz][]
 *
 *   [text][qux]
 *
 *   Spread [over
 *   lines][]
 *
 *   > in [a
 *   > block quote][]
 *
 *   [asd][a
 *
 *   Can include [*emphasis*].
 *
 *   Multiple pairs: [a][b][c].
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:6: Found reference to undefined definition
 *   3:1-3:8: Found reference to undefined definition
 *   5:1-5:12: Found reference to undefined definition
 *   7:8-8:9: Found reference to undefined definition
 *   10:6-11:17: Found reference to undefined definition
 *   13:1-13:6: Found reference to undefined definition
 *   15:13-15:25: Found reference to undefined definition
 *   17:17-17:23: Found reference to undefined definition
 *   17:23-17:26: Found reference to undefined definition
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "config": {"allow": ["a", {"source": "^b\\."}]}}
 *
 *   [foo][a.c]
 *
 *   [bar][b]
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "config": {"allow": ["a", {"source": "^b\\."}]}}
 *
 *   1:1-1:11: Found reference to undefined definition
 *   3:1-3:9: Found reference to undefined definition
 */
const remarkLintNoUndefinedReferences = lintRule(
  {
    origin: 'remark-lint:no-undefined-references',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-undefined-references#readme'
  },
  (tree, file, option = {}) => {
    const contents = String(file);
    const loc = location(file);
    const lineEnding = /(\r?\n|\r)[\t ]*(>[\t ]*)*/g;
    const map = Object.create(null);
    const allow = option.allow || [];
    const regexes = [];
    const strings = new Set();
    let index = -1;
    while (++index < allow.length) {
      const value = allow[index];
      if (typeof value === 'string') {
        strings.add(normalizeIdentifier(value));
      } else if (value instanceof RegExp) {
        regexes.push(value);
      } else {
        regexes.push(new RegExp(value.source, 'i'));
      }
    }
    visit$n(tree, (node) => {
      if (
        (node.type === 'definition' || node.type === 'footnoteDefinition') &&
        !generated(node)
      ) {
        map[normalizeIdentifier(node.identifier)] = true;
      }
    });
    visit$n(tree, (node) => {
      if (
        (node.type === 'imageReference' ||
          node.type === 'linkReference' ||
          node.type === 'footnoteReference') &&
        !generated(node) &&
        !(normalizeIdentifier(node.identifier) in map) &&
        !isAllowed(node.identifier)
      ) {
        file.message('Found reference to undefined definition', node);
      }
      if (node.type === 'paragraph' || node.type === 'heading') {
        findInPhrasing(node);
      }
    });
    function findInPhrasing(node) {
      let ranges = [];
      visit$n(node, (child) => {
        if (child === node) return
        if (child.type === 'link' || child.type === 'linkReference') {
          ranges = [];
          return SKIP$n
        }
        if (child.type !== 'text') return
        const start = pointStart(child).offset;
        const end = pointEnd(child).offset;
        if (typeof start !== 'number' || typeof end !== 'number') {
          return EXIT$n
        }
        const source = contents.slice(start, end);
        const lines = [[start, '']];
        let last = 0;
        lineEnding.lastIndex = 0;
        let match = lineEnding.exec(source);
        while (match) {
          const index = match.index;
          lines[lines.length - 1][1] = source.slice(last, index);
          last = index + match[0].length;
          lines.push([start + last, '']);
          match = lineEnding.exec(source);
        }
        lines[lines.length - 1][1] = source.slice(last);
        let lineIndex = -1;
        while (++lineIndex < lines.length) {
          const line = lines[lineIndex][1];
          let index = 0;
          while (index < line.length) {
            const code = line.charCodeAt(index);
            if (code === 92) {
              const next = line.charCodeAt(index + 1);
              index++;
              if (next === 91 || next === 93) {
                index++;
              }
            }
            else if (code === 91) {
              ranges.push([lines[lineIndex][0] + index]);
              index++;
            }
            else if (code === 93) {
              if (ranges.length === 0) {
                index++;
              } else if (line.charCodeAt(index + 1) === 91) {
                index++;
                let range = ranges.pop();
                if (range) {
                  range.push(lines[lineIndex][0] + index);
                  if (range.length === 4) {
                    handleRange(range);
                    range = [];
                  }
                  range.push(lines[lineIndex][0] + index);
                  ranges.push(range);
                  index++;
                }
              } else {
                index++;
                const range = ranges.pop();
                if (range) {
                  range.push(lines[lineIndex][0] + index);
                  handleRange(range);
                }
              }
            }
            else {
              index++;
            }
          }
        }
      });
      let index = -1;
      while (++index < ranges.length) {
        handleRange(ranges[index]);
      }
      return SKIP$n
      function handleRange(range) {
        if (range.length === 1) return
        if (range.length === 3) range.length = 2;
        if (range.length === 2 && range[0] + 2 === range[1]) return
        const offset = range.length === 4 && range[2] + 2 !== range[3] ? 2 : 0;
        const id = contents
          .slice(range[0 + offset] + 1, range[1 + offset] - 1)
          .replace(lineEnding, ' ');
        const pos = {
          start: loc.toPoint(range[0]),
          end: loc.toPoint(range[range.length - 1])
        };
        if (
          !generated({position: pos}) &&
          !(normalizeIdentifier(id) in map) &&
          !isAllowed(id)
        ) {
          file.message('Found reference to undefined definition', pos);
        }
      }
    }
    function isAllowed(id) {
      const normalized = normalizeIdentifier(id);
      return (
        strings.has(normalized) ||
        regexes.some((regex) => regex.test(normalized))
      )
    }
  }
);
var remarkLintNoUndefinedReferences$1 = remarkLintNoUndefinedReferences;

const convert$m =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$m
      }
      if (typeof test === 'string') {
        return typeFactory$m(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$m(test) : propsFactory$m(test)
      }
      if (typeof test === 'function') {
        return castFactory$m(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$m(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$m(tests[index]);
  }
  return castFactory$m(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$m(check) {
  return castFactory$m(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$m(check) {
  return castFactory$m(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$m(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$m() {
  return true
}

function color$n(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$m = true;
const EXIT$m = false;
const SKIP$m = 'skip';
const visitParents$m =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$m(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$n(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$m(visitor(node, parents));
            if (result[0] === EXIT$m) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$m) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$m) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$m(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$m, value]
  }
  return [value]
}

const visit$m =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$m(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check definitions are referenced.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Unused definitions do not contribute anything, so they can be removed.
 *
 * @module no-unused-definitions
 * @summary
 *   remark-lint rule to warn when unreferenced definitions are used.
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   [foo][]
 *
 *   [foo]: https://example.com
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [bar]: https://example.com
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:27: Found unused definition
 */
const own = {}.hasOwnProperty;
const remarkLintNoUnusedDefinitions = lintRule(
  {
    origin: 'remark-lint:no-unused-definitions',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-unused-definitions#readme'
  },
  (tree, file) => {
    const map = Object.create(null);
    visit$m(tree, (node) => {
      if (
        (node.type === 'definition' || node.type === 'footnoteDefinition') &&
        !generated(node)
      ) {
        map[node.identifier.toUpperCase()] = {node, used: false};
      }
    });
    visit$m(tree, (node) => {
      if (
        node.type === 'imageReference' ||
        node.type === 'linkReference' ||
        node.type === 'footnoteReference'
      ) {
        const info = map[node.identifier.toUpperCase()];
        if (!generated(node) && info) {
          info.used = true;
        }
      }
    });
    let identifier;
    for (identifier in map) {
      if (own.call(map, identifier)) {
        const entry = map[identifier];
        if (!entry.used) {
          file.message('Found unused definition', entry.node);
        }
      }
    }
  }
);
var remarkLintNoUnusedDefinitions$1 = remarkLintNoUnusedDefinitions;

const remarkPresetLintRecommended = {
  plugins: [
    remarkLint,
    remarkLintFinalNewline$1,
    remarkLintListItemBulletIndent$1,
    [remarkLintListItemIndent$1, 'tab-size'],
    remarkLintNoBlockquoteWithoutMarker$1,
    remarkLintNoLiteralUrls$1,
    [remarkLintOrderedListMarkerStyle$1, '.'],
    remarkLintHardBreakSpaces$1,
    remarkLintNoDuplicateDefinitions$1,
    remarkLintNoHeadingContentIndent$1,
    remarkLintNoInlinePadding$1,
    remarkLintNoShortcutReferenceImage$1,
    remarkLintNoShortcutReferenceLink$1,
    remarkLintNoUndefinedReferences$1,
    remarkLintNoUnusedDefinitions$1
  ]
};
var remarkPresetLintRecommended$1 = remarkPresetLintRecommended;

const convert$l =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$l
      }
      if (typeof test === 'string') {
        return typeFactory$l(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$l(test) : propsFactory$l(test)
      }
      if (typeof test === 'function') {
        return castFactory$l(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$l(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$l(tests[index]);
  }
  return castFactory$l(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$l(check) {
  return castFactory$l(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$l(check) {
  return castFactory$l(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$l(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$l() {
  return true
}

function color$m(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$l = true;
const EXIT$l = false;
const SKIP$l = 'skip';
const visitParents$l =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$l(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$m(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$l(visitor(node, parents));
            if (result[0] === EXIT$l) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$l) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$l) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$l(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$l, value]
  }
  return [value]
}

const visit$l =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$l(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that the “indent” of block quotes is
 * consistent.
 * Indent here is the `>` (greater than) marker and the spaces before content.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `number` (example: `2`)
 *     — preferred indent of `>` and spaces before content
 * *   `'consistent'`
 *     — detect the first used style and warn when further block quotes differ
 *
 * ## Recommendation
 *
 * CommonMark specifies that when block quotes are used the `>` markers can be
 * followed by an optional space.
 * No space at all arguably looks rather ugly:
 *
 * ```markdown
 * >Mars and
 * >Venus.
 * ```
 *
 * There is no specific handling of more that one space, so if 5 spaces were
 * used after `>`, then indented code kicks in:
 *
 * ```markdown
 * >     neptune()
 * ```
 *
 * Due to this, it’s recommended to configure this rule with `2`.
 *
 * @module blockquote-indentation
 * @summary
 *   remark-lint rule to warn when block quotes are indented too much or
 *   too little.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "config": 4}
 *
 *   >   Hello
 *
 *   Paragraph.
 *
 *   >   World
 * @example
 *   {"name": "ok.md", "config": 2}
 *
 *   > Hello
 *
 *   Paragraph.
 *
 *   > World
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   >  Hello
 *
 *   Paragraph.
 *
 *   >   World
 *
 *   Paragraph.
 *
 *   > World
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   5:5: Remove 1 space between block quote and content
 *   9:3: Add 1 space between block quote and content
 */
const remarkLintBlockquoteIndentation = lintRule(
  {
    origin: 'remark-lint:blockquote-indentation',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-blockquote-indentation#readme'
  },
  (tree, file, option = 'consistent') => {
    visit$l(tree, 'blockquote', (node) => {
      if (generated(node) || node.children.length === 0) {
        return
      }
      if (option === 'consistent') {
        option = check(node);
      } else {
        const diff = option - check(node);
        if (diff !== 0) {
          const abs = Math.abs(diff);
          file.message(
            (diff > 0 ? 'Add' : 'Remove') +
              ' ' +
              abs +
              ' ' +
              plural('space', abs) +
              ' between block quote and content',
            pointStart(node.children[0])
          );
        }
      }
    });
  }
);
var remarkLintBlockquoteIndentation$1 = remarkLintBlockquoteIndentation;
function check(node) {
  return pointStart(node.children[0]).column - pointStart(node).column
}

const convert$k =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$k
      }
      if (typeof test === 'string') {
        return typeFactory$k(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$k(test) : propsFactory$k(test)
      }
      if (typeof test === 'function') {
        return castFactory$k(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$k(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$k(tests[index]);
  }
  return castFactory$k(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$k(check) {
  return castFactory$k(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$k(check) {
  return castFactory$k(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$k(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$k() {
  return true
}

function color$l(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$k = true;
const EXIT$k = false;
const SKIP$k = 'skip';
const visitParents$k =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$k(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$l(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$k(visitor(node, parents));
            if (result[0] === EXIT$k) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$k) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$k) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$k(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$k, value]
  }
  return [value]
}

const visit$k =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$k(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that the style of GFM tasklists is
 * consistent.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `Object` with the following fields:
 *     *   `checked` (`'x'`, `'X'`, or `'consistent'`, default: `'consistent'`)
 *         — preferred character to use for checked checkboxes
 *     *   `unchecked` (`'·'` (a space), `'»'` (a tab), or `'consistent'`,
 *         default: `'consistent'`)
 *         — preferred character to use for unchecked checkboxes
 * *   `'consistent'`
 *     — detect the first used styles and warn when further checkboxes differ
 *
 * ## Recommendation
 *
 * It’s recommended to set `options.checked` to `'x'` (a lowercase X) as it
 * prevents an extra keyboard press and `options.unchecked` to `'·'` (a space)
 * to make all checkboxes align.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats checked checkboxes using `'x'` (lowercase X) and unchecked checkboxes
 * using `'·'` (a space).
 *
 * @module checkbox-character-style
 * @summary
 *   remark-lint rule to warn when list item checkboxes violate a given
 *   style.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "config": {"checked": "x"}, "gfm": true}
 *
 *   - [x] List item
 *   - [x] List item
 *
 * @example
 *   {"name": "ok.md", "config": {"checked": "X"}, "gfm": true}
 *
 *   - [X] List item
 *   - [X] List item
 *
 * @example
 *   {"name": "ok.md", "config": {"unchecked": " "}, "gfm": true}
 *
 *   - [ ] List item
 *   - [ ] List item
 *   - [ ]··
 *   - [ ]
 *
 * @example
 *   {"name": "ok.md", "config": {"unchecked": "\t"}, "gfm": true}
 *
 *   - [»] List item
 *   - [»] List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "gfm": true}
 *
 *   - [x] List item
 *   - [X] List item
 *   - [ ] List item
 *   - [»] List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "gfm": true}
 *
 *   2:5: Checked checkboxes should use `x` as a marker
 *   4:5: Unchecked checkboxes should use ` ` as a marker
 *
 * @example
 *   {"config": {"unchecked": "💩"}, "name": "not-ok.md", "label": "output", "positionless": true, "gfm": true}
 *
 *   1:1: Incorrect unchecked checkbox marker `💩`: use either `'\t'`, or `' '`
 *
 * @example
 *   {"config": {"checked": "💩"}, "name": "not-ok.md", "label": "output", "positionless": true, "gfm": true}
 *
 *   1:1: Incorrect checked checkbox marker `💩`: use either `'x'`, or `'X'`
 */
const remarkLintCheckboxCharacterStyle = lintRule(
  {
    origin: 'remark-lint:checkbox-character-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-checkbox-character-style#readme'
  },
  (tree, file, option = 'consistent') => {
    const value = String(file);
    let checked = 'consistent';
    let unchecked = 'consistent';
    if (typeof option === 'object') {
      checked = option.checked || 'consistent';
      unchecked = option.unchecked || 'consistent';
    }
    if (unchecked !== 'consistent' && unchecked !== ' ' && unchecked !== '\t') {
      file.fail(
        'Incorrect unchecked checkbox marker `' +
          unchecked +
          "`: use either `'\\t'`, or `' '`"
      );
    }
    if (checked !== 'consistent' && checked !== 'x' && checked !== 'X') {
      file.fail(
        'Incorrect checked checkbox marker `' +
          checked +
          "`: use either `'x'`, or `'X'`"
      );
    }
    visit$k(tree, 'listItem', (node) => {
      const head = node.children[0];
      const point = pointStart(head);
      if (
        typeof node.checked !== 'boolean' ||
        !head ||
        typeof point.offset !== 'number'
      ) {
        return
      }
      point.offset -= 2;
      point.column -= 2;
      const match = /\[([\t Xx])]/.exec(
        value.slice(point.offset - 2, point.offset + 1)
      );
      if (!match) return
      const style = node.checked ? checked : unchecked;
      if (style === 'consistent') {
        if (node.checked) {
          checked = match[1];
        } else {
          unchecked = match[1];
        }
      } else if (match[1] !== style) {
        file.message(
          (node.checked ? 'Checked' : 'Unchecked') +
            ' checkboxes should use `' +
            style +
            '` as a marker',
          point
        );
      }
    });
  }
);
var remarkLintCheckboxCharacterStyle$1 = remarkLintCheckboxCharacterStyle;

const convert$j =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$j
      }
      if (typeof test === 'string') {
        return typeFactory$j(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$j(test) : propsFactory$j(test)
      }
      if (typeof test === 'function') {
        return castFactory$j(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$j(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$j(tests[index]);
  }
  return castFactory$j(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$j(check) {
  return castFactory$j(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$j(check) {
  return castFactory$j(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$j(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$j() {
  return true
}

function color$k(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$j = true;
const EXIT$j = false;
const SKIP$j = 'skip';
const visitParents$j =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$j(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$k(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$j(visitor(node, parents));
            if (result[0] === EXIT$j) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$j) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$j) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$j(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$j, value]
  }
  return [value]
}

const visit$j =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$j(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that the “indent” after a GFM tasklist
 * checkbox is a single space.
 *
 * ## API
 *
 * There are no accepted options.
 *
 * ## Recommendation
 *
 * GFM allows zero or more spaces and tabs after checkboxes.
 * No space at all arguably looks rather ugly:
 *
 * ```markdown
 * * [x]Pluto
 * ```
 *
 * More that one space is superfluous:
 *
 * ```markdown
 * * [x]   Jupiter
 * ```
 *
 * Due to this, it’s recommended to turn this rule on.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats checkboxes and the content after them with a single space between.
 *
 * @module checkbox-content-indent
 * @summary
 *   remark-lint rule to warn when GFM tasklist checkboxes are followed by
 *   more than one space.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "gfm": true}
 *
 *   - [ ] List item
 *   +  [x] List Item
 *   *   [X] List item
 *   -    [ ] List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "gfm": true}
 *
 *   - [ ] List item
 *   + [x]  List item
 *   * [X]   List item
 *   - [ ]    List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "gfm": true}
 *
 *   2:7-2:8: Checkboxes should be followed by a single character
 *   3:7-3:9: Checkboxes should be followed by a single character
 *   4:7-4:10: Checkboxes should be followed by a single character
 */
const remarkLintCheckboxContentIndent = lintRule(
  {
    origin: 'remark-lint:checkbox-content-indent',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-checkbox-content-indent#readme'
  },
  (tree, file) => {
    const value = String(file);
    const loc = location(file);
    visit$j(tree, 'listItem', (node) => {
      const head = node.children[0];
      const point = pointStart(head);
      if (
        typeof node.checked !== 'boolean' ||
        !head ||
        typeof point.offset !== 'number'
      ) {
        return
      }
      const match = /\[([\t xX])]/.exec(
        value.slice(point.offset - 4, point.offset + 1)
      );
      if (!match) return
      const initial = point.offset;
      let final = initial;
      while (/[\t ]/.test(value.charAt(final))) final++;
      if (final - initial > 0) {
        file.message('Checkboxes should be followed by a single character', {
          start: loc.toPoint(initial),
          end: loc.toPoint(final)
        });
      }
    });
  }
);
var remarkLintCheckboxContentIndent$1 = remarkLintCheckboxContentIndent;

const convert$i =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$i
      }
      if (typeof test === 'string') {
        return typeFactory$i(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$i(test) : propsFactory$i(test)
      }
      if (typeof test === 'function') {
        return castFactory$i(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$i(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$i(tests[index]);
  }
  return castFactory$i(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$i(check) {
  return castFactory$i(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$i(check) {
  return castFactory$i(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$i(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$i() {
  return true
}

function color$j(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$i = true;
const EXIT$i = false;
const SKIP$i = 'skip';
const visitParents$i =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$i(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$j(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$i(visitor(node, parents));
            if (result[0] === EXIT$i) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$i) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$i) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$i(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$i, value]
  }
  return [value]
}

const visit$i =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$i(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that code blocks are consistent.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `'fenced'`
 *     — prefer fenced code blocks:
 *     ````markdown
 *     ```js
 *     code()
 *     ```
 *     ````
 * *   `'indented'`
 *     — prefer indented code blocks:
 *     ```markdown
 *         code()
 *     ```
 * *   `'consistent'`
 *     — detect the first used style and warn when further code blocks differ
 *
 * ## Recommendation
 *
 * Indentation in markdown is complex, especially because lists and indented
 * code can interfere in unexpected ways.
 * Fenced code has more features than indented code: importantly, specifying a
 * programming language.
 * Since CommonMark took the idea of fenced code from GFM, fenced code became
 * widely supported.
 * Due to this, it’s recommended to configure this rule with `'fenced'`.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats code blocks as fenced code when they have a language flag and as
 * indented code otherwise.
 * Pass
 * [`fences: true`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionsfences)
 * to always use fenced code.
 *
 * @module code-block-style
 * @summary
 *   remark-lint rule to warn when code blocks violate a given style.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 *
 * @example
 *   {"config": "indented", "name": "ok.md"}
 *
 *       alpha()
 *
 *   Paragraph.
 *
 *       bravo()
 *
 * @example
 *   {"config": "indented", "name": "not-ok.md", "label": "input"}
 *
 *   ```
 *   alpha()
 *   ```
 *
 *   Paragraph.
 *
 *   ```
 *   bravo()
 *   ```
 *
 * @example
 *   {"config": "indented", "name": "not-ok.md", "label": "output"}
 *
 *   1:1-3:4: Code blocks should be indented
 *   7:1-9:4: Code blocks should be indented
 *
 * @example
 *   {"config": "fenced", "name": "ok.md"}
 *
 *   ```
 *   alpha()
 *   ```
 *
 *   Paragraph.
 *
 *   ```
 *   bravo()
 *   ```
 *
 * @example
 *   {"config": "fenced", "name": "not-ok-fenced.md", "label": "input"}
 *
 *       alpha()
 *
 *   Paragraph.
 *
 *       bravo()
 *
 * @example
 *   {"config": "fenced", "name": "not-ok-fenced.md", "label": "output"}
 *
 *   1:1-1:12: Code blocks should be fenced
 *   5:1-5:12: Code blocks should be fenced
 *
 * @example
 *   {"name": "not-ok-consistent.md", "label": "input"}
 *
 *       alpha()
 *
 *   Paragraph.
 *
 *   ```
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok-consistent.md", "label": "output"}
 *
 *   5:1-7:4: Code blocks should be indented
 *
 * @example
 *   {"config": "💩", "name": "not-ok-incorrect.md", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect code block style `💩`: use either `'consistent'`, `'fenced'`, or `'indented'`
 */
const remarkLintCodeBlockStyle = lintRule(
  {
    origin: 'remark-lint:code-block-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-code-block-style#readme'
  },
  (tree, file, option = 'consistent') => {
    const value = String(file);
    if (
      option !== 'consistent' &&
      option !== 'fenced' &&
      option !== 'indented'
    ) {
      file.fail(
        'Incorrect code block style `' +
          option +
          "`: use either `'consistent'`, `'fenced'`, or `'indented'`"
      );
    }
    visit$i(tree, 'code', (node) => {
      if (generated(node)) {
        return
      }
      const initial = pointStart(node).offset;
      const final = pointEnd(node).offset;
      const current =
        node.lang || /^\s*([~`])\1{2,}/.test(value.slice(initial, final))
          ? 'fenced'
          : 'indented';
      if (option === 'consistent') {
        option = current;
      } else if (option !== current) {
        file.message('Code blocks should be ' + option, node);
      }
    });
  }
);
var remarkLintCodeBlockStyle$1 = remarkLintCodeBlockStyle;

const convert$h =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$h
      }
      if (typeof test === 'string') {
        return typeFactory$h(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$h(test) : propsFactory$h(test)
      }
      if (typeof test === 'function') {
        return castFactory$h(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$h(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$h(tests[index]);
  }
  return castFactory$h(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$h(check) {
  return castFactory$h(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$h(check) {
  return castFactory$h(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$h(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$h() {
  return true
}

function color$i(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$h = true;
const EXIT$h = false;
const SKIP$h = 'skip';
const visitParents$h =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$h(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$i(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$h(visitor(node, parents));
            if (result[0] === EXIT$h) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$h) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$h) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$h(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$h, value]
  }
  return [value]
}

const visit$h =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$h(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that the labels used in definitions
 * do not use meaningless white space.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Definitions and references are matched together by collapsing white space.
 * Using more white space in labels might incorrectly indicate that they are of
 * importance.
 * Due to this, it’s recommended to use one space (or a line ending if needed)
 * and turn this rule on.
 *
 * @module definition-spacing
 * @summary
 *   remark-lint rule to warn when consecutive whitespace is used in
 *   a definition label.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   [example domain]: http://example.com "Example Domain"
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [example····domain]: http://example.com "Example Domain"
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:57: Do not use consecutive whitespace in definition labels
 */
const label = /^\s*\[((?:\\[\s\S]|[^[\]])+)]/;
const remarkLintDefinitionSpacing = lintRule(
  {
    origin: 'remark-lint:definition-spacing',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-definition-spacing#readme'
  },
  (tree, file) => {
    const value = String(file);
    visit$h(tree, (node) => {
      if (node.type === 'definition' || node.type === 'footnoteDefinition') {
        const start = pointStart(node).offset;
        const end = pointEnd(node).offset;
        if (typeof start === 'number' && typeof end === 'number') {
          const match = value.slice(start, end).match(label);
          if (match && /[ \t\n]{2,}/.test(match[1])) {
            file.message(
              'Do not use consecutive whitespace in definition labels',
              node
            );
          }
        }
      }
    });
  }
);
var remarkLintDefinitionSpacing$1 = remarkLintDefinitionSpacing;

const convert$g =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$g
      }
      if (typeof test === 'string') {
        return typeFactory$g(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$g(test) : propsFactory$g(test)
      }
      if (typeof test === 'function') {
        return castFactory$g(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$g(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$g(tests[index]);
  }
  return castFactory$g(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$g(check) {
  return castFactory$g(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$g(check) {
  return castFactory$g(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$g(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$g() {
  return true
}

function color$h(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$g = true;
const EXIT$g = false;
const SKIP$g = 'skip';
const visitParents$g =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$g(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$h(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$g(visitor(node, parents));
            if (result[0] === EXIT$g) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$g) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$g) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$g(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$g, value]
  }
  return [value]
}

const visit$g =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$g(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that language flags of fenced code
 * are used and consistent.
 *
 * ## API
 *
 * The following options (default: `undefined`) are accepted:
 *
 * *   `Array<string>`
 *     — as if passing `{flags: options}`
 * *   `Object` with the following fields:
 *     *   `allowEmpty` (`boolean`, default: `false`)
 *         — allow language flags to be omitted
 *     *   `flags` (`Array<string>` default: `[]`)
 *         — specific flags to allow (other flags will result in a warning)
 *
 * ## Recommendation
 *
 * While omitting the language flag is perfectly fine to signal that the code is
 * plain text, it *could* point to a mistake.
 * It’s recommended to instead use a certain flag for plain text (such as `txt`)
 * and to turn this rule on.
 *
 * @module fenced-code-flag
 * @summary
 *   remark-lint rule to check that language flags of fenced code are used.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ```
 *   alpha()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-3:4: Missing code language flag
 *
 * @example
 *   {"name": "ok.md", "config": {"allowEmpty": true}}
 *
 *   ```
 *   alpha()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "config": {"allowEmpty": false}, "label": "input"}
 *
 *   ```
 *   alpha()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "config": {"allowEmpty": false}, "label": "output"}
 *
 *   1:1-3:4: Missing code language flag
 *
 * @example
 *   {"name": "ok.md", "config": ["alpha"]}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "ok.md", "config": {"flags":["alpha"]}}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "config": ["charlie"], "label": "input"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "config": ["charlie"], "label": "output"}
 *
 *   1:1-3:4: Incorrect code language flag
 */
const fence = /^ {0,3}([~`])\1{2,}/;
const remarkLintFencedCodeFlag = lintRule(
  {
    origin: 'remark-lint:fenced-code-flag',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-fenced-code-flag#readme'
  },
  (tree, file, option) => {
    const value = String(file);
    let allowEmpty = false;
    let allowed = [];
    if (typeof option === 'object') {
      if (Array.isArray(option)) {
        allowed = option;
      } else {
        allowEmpty = Boolean(option.allowEmpty);
        if (option.flags) {
          allowed = option.flags;
        }
      }
    }
    visit$g(tree, 'code', (node) => {
      if (!generated(node)) {
        if (node.lang) {
          if (allowed.length > 0 && !allowed.includes(node.lang)) {
            file.message('Incorrect code language flag', node);
          }
        } else {
          const slice = value.slice(
            pointStart(node).offset,
            pointEnd(node).offset
          );
          if (!allowEmpty && fence.test(slice)) {
            file.message('Missing code language flag', node);
          }
        }
      }
    });
  }
);
var remarkLintFencedCodeFlag$1 = remarkLintFencedCodeFlag;

const convert$f =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$f
      }
      if (typeof test === 'string') {
        return typeFactory$f(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$f(test) : propsFactory$f(test)
      }
      if (typeof test === 'function') {
        return castFactory$f(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$f(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$f(tests[index]);
  }
  return castFactory$f(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$f(check) {
  return castFactory$f(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$f(check) {
  return castFactory$f(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$f(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$f() {
  return true
}

function color$g(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$f = true;
const EXIT$f = false;
const SKIP$f = 'skip';
const visitParents$f =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$f(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$g(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$f(visitor(node, parents));
            if (result[0] === EXIT$f) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$f) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$f) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$f(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$f, value]
  }
  return [value]
}

const visit$f =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$f(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that fenced code markers are consistent.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   ``'`'``
 *     — prefer grave accents
 * *   `'~'`
 *     — prefer tildes
 * *   `'consistent'`
 *     — detect the first used style and warn when further fenced code differs
 *
 * ## Recommendation
 *
 * Tildes are extremely uncommon.
 * Due to this, it’s recommended to configure this rule with ``'`'``.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats fenced code with grave accents by default.
 * Pass
 * [`fence: '~'`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionsfence)
 * to always use tildes.
 *
 * @module fenced-code-marker
 * @summary
 *   remark-lint rule to warn when fenced code markers are inconsistent.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   Indented code blocks are not affected by this rule:
 *
 *       bravo()
 *
 * @example
 *   {"name": "ok.md", "config": "`"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 *   ```
 *   charlie()
 *   ```
 *
 * @example
 *   {"name": "ok.md", "config": "~"}
 *
 *   ~~~alpha
 *   bravo()
 *   ~~~
 *
 *   ~~~
 *   charlie()
 *   ~~~
 *
 * @example
 *   {"name": "not-ok-consistent-tick.md", "label": "input"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 *   ~~~
 *   charlie()
 *   ~~~
 *
 * @example
 *   {"name": "not-ok-consistent-tick.md", "label": "output"}
 *
 *   5:1-7:4: Fenced code should use `` ` `` as a marker
 *
 * @example
 *   {"name": "not-ok-consistent-tilde.md", "label": "input"}
 *
 *   ~~~alpha
 *   bravo()
 *   ~~~
 *
 *   ```
 *   charlie()
 *   ```
 *
 * @example
 *   {"name": "not-ok-consistent-tilde.md", "label": "output"}
 *
 *   5:1-7:4: Fenced code should use `~` as a marker
 *
 * @example
 *   {"name": "not-ok-incorrect.md", "config": "💩", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect fenced code marker `💩`: use either `'consistent'`, `` '`' ``, or `'~'`
 */
const remarkLintFencedCodeMarker = lintRule(
  {
    origin: 'remark-lint:fenced-code-marker',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-fenced-code-marker#readme'
  },
  (tree, file, option = 'consistent') => {
    const contents = String(file);
    if (option !== 'consistent' && option !== '~' && option !== '`') {
      file.fail(
        'Incorrect fenced code marker `' +
          option +
          "`: use either `'consistent'`, `` '`' ``, or `'~'`"
      );
    }
    visit$f(tree, 'code', (node) => {
      const start = pointStart(node).offset;
      if (typeof start === 'number') {
        const marker = contents
          .slice(start, start + 4)
          .replace(/^\s+/, '')
          .charAt(0);
        if (marker === '~' || marker === '`') {
          if (option === 'consistent') {
            option = marker;
          } else if (marker !== option) {
            file.message(
              'Fenced code should use `' +
                (option === '~' ? option : '` ` `') +
                '` as a marker',
              node
            );
          }
        }
      }
    });
  }
);
var remarkLintFencedCodeMarker$1 = remarkLintFencedCodeMarker;

/**
 * ## When should I use this?
 *
 * You can use this package to check that file extensions are `md`.
 *
 * ## API
 *
 * The following options (default: `'md'`) are accepted:
 *
 * *   `string` (example `'markdown'`)
 *     — preferred file extension (no dot)
 *
 * > 👉 **Note**: does not warn when files have no file extensions (such as
 * > `AUTHORS` or `LICENSE`).
 *
 * ## Recommendation
 *
 * Use `md` as it’s the most common.
 * Also use `md` when your markdown contains common syntax extensions (such as
 * GFM, frontmatter, or math).
 * Do not use `md` for MDX: use `mdx` instead.
 *
 * @module file-extension
 * @summary
 *   remark-lint rule to check the file extension.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "readme.md"}
 *
 * @example
 *   {"name": "readme"}
 *
 * @example
 *   {"name": "readme.mkd", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect extension: use `md`
 *
 * @example
 *   {"name": "readme.mkd", "config": "mkd"}
 */
const remarkLintFileExtension = lintRule(
  {
    origin: 'remark-lint:file-extension',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-file-extension#readme'
  },
  (_, file, option = 'md') => {
    const ext = file.extname;
    if (ext && ext.slice(1) !== option) {
      file.message('Incorrect extension: use `' + option + '`');
    }
  }
);
var remarkLintFileExtension$1 = remarkLintFileExtension;

const convert$e =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$e
      }
      if (typeof test === 'string') {
        return typeFactory$e(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$e(test) : propsFactory$e(test)
      }
      if (typeof test === 'function') {
        return castFactory$e(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$e(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$e(tests[index]);
  }
  return castFactory$e(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$e(check) {
  return castFactory$e(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$e(check) {
  return castFactory$e(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$e(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$e() {
  return true
}

function color$f(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$e = true;
const EXIT$e = false;
const SKIP$e = 'skip';
const visitParents$e =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$e(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$f(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$e(visitor(node, parents));
            if (result[0] === EXIT$e) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$e) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$e) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$e(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$e, value]
  }
  return [value]
}

const visit$e =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$e(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that definitions are placed at the end of
 * the document.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * There are different strategies for placing definitions.
 * The simplest is perhaps to place them all at the bottem of documents.
 * If you prefer that, turn on this rule.
 *
 * @module final-definition
 * @summary
 *   remark-lint rule to warn when definitions are used *in* the document
 *   instead of at the end.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   Paragraph.
 *
 *   [example]: http://example.com "Example Domain"
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Paragraph.
 *
 *   [example]: http://example.com "Example Domain"
 *
 *   Another paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   3:1-3:47: Move definitions to the end of the file (after the node at line `5`)
 *
 * @example
 *   {"name": "ok-comments.md"}
 *
 *   Paragraph.
 *
 *   [example-1]: http://example.com/one/
 *
 *   <!-- Comments are fine between and after definitions -->
 *
 *   [example-2]: http://example.com/two/
 */
const remarkLintFinalDefinition = lintRule(
  {
    origin: 'remark-lint:final-definition',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-final-definition#readme'
  },
  (tree, file) => {
    let last = 0;
    visit$e(
      tree,
      (node) => {
        if (
          node.type === 'root' ||
          generated(node) ||
          (node.type === 'html' && /^\s*<!--/.test(node.value))
        ) {
          return
        }
        const line = pointStart(node).line;
        if (node.type === 'definition') {
          if (last && last > line) {
            file.message(
              'Move definitions to the end of the file (after the node at line `' +
                last +
                '`)',
              node
            );
          }
        } else if (last === 0) {
          last = line;
        }
      },
      true
    );
  }
);
var remarkLintFinalDefinition$1 = remarkLintFinalDefinition;

const convert$d =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$d
      }
      if (typeof test === 'string') {
        return typeFactory$d(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$d(test) : propsFactory$d(test)
      }
      if (typeof test === 'function') {
        return castFactory$d(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$d(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$d(tests[index]);
  }
  return castFactory$d(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$d(check) {
  return castFactory$d(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$d(check) {
  return castFactory$d(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$d(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$d() {
  return true
}

function color$e(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$d = true;
const EXIT$d = false;
const SKIP$d = 'skip';
const visitParents$d =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$d(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$e(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$d(visitor(node, parents));
            if (result[0] === EXIT$d) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$d) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$d) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$d(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$d, value]
  }
  return [value]
}

const visit$d =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$d(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check the heading rank of the first heading.
 *
 * ## API
 *
 * The following options (default: `1`) are accepted:
 *
 * *   `number` (example `1`)
 *     — expected rank of first heading
 *
 * ## Recommendation
 *
 * In most cases you’d want to first heading in a markdown document to start at
 * rank 1.
 * In some cases a different rank makes more sense, such as when building a blog
 * and generating the primary heading from frontmatter metadata, in which case
 * a value of `2` can be defined here.
 *
 * @module first-heading-level
 * @summary
 *   remark-lint rule to warn when the first heading has an unexpected rank.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   # The default is to expect a level one heading
 *
 * @example
 *   {"name": "ok-html.md"}
 *
 *   <h1>An HTML heading is also seen by this rule.</h1>
 *
 * @example
 *   {"name": "ok-delayed.md"}
 *
 *   You can use markdown content before the heading.
 *
 *   <div>Or non-heading HTML</div>
 *
 *   <h1>So the first heading, be it HTML or markdown, is checked</h1>
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ## Bravo
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:9: First heading level should be `1`
 *
 * @example
 *   {"name": "not-ok-html.md", "label": "input"}
 *
 *   <h2>Charlie</h2>
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok-html.md", "label": "output"}
 *
 *   1:1-1:17: First heading level should be `1`
 *
 * @example
 *   {"name": "ok.md", "config": 2}
 *
 *   ## Delta
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "ok-html.md", "config": 2}
 *
 *   <h2>Echo</h2>
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "config": 2, "label": "input"}
 *
 *   # Foxtrot
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "config": 2, "label": "output"}
 *
 *   1:1-1:10: First heading level should be `2`
 *
 * @example
 *   {"name": "not-ok-html.md", "config": 2, "label": "input"}
 *
 *   <h1>Golf</h1>
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok-html.md", "config": 2, "label": "output"}
 *
 *   1:1-1:14: First heading level should be `2`
 */
const re$2 = /<h([1-6])/;
const remarkLintFirstHeadingLevel = lintRule(
  {
    origin: 'remark-lint:first-heading-level',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-first-heading-level#readme'
  },
  (tree, file, option = 1) => {
    visit$d(tree, (node) => {
      if (!generated(node)) {
        let rank;
        if (node.type === 'heading') {
          rank = node.depth;
        } else if (node.type === 'html') {
          rank = infer(node);
        }
        if (rank !== undefined) {
          if (rank !== option) {
            file.message('First heading level should be `' + option + '`', node);
          }
          return EXIT$d
        }
      }
    });
  }
);
var remarkLintFirstHeadingLevel$1 = remarkLintFirstHeadingLevel;
function infer(node) {
  const results = node.value.match(re$2);
  return results ? Number(results[1]) : undefined
}

const convert$c =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$c
      }
      if (typeof test === 'string') {
        return typeFactory$c(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$c(test) : propsFactory$c(test)
      }
      if (typeof test === 'function') {
        return castFactory$c(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$c(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$c(tests[index]);
  }
  return castFactory$c(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$c(check) {
  return castFactory$c(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$c(check) {
  return castFactory$c(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$c(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$c() {
  return true
}

function color$d(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$c = true;
const EXIT$c = false;
const SKIP$c = 'skip';
const visitParents$c =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$c(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$d(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$c(visitor(node, parents));
            if (result[0] === EXIT$c) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$c) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$c) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$c(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$c, value]
  }
  return [value]
}

const visit$c =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$c(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that headings are consistent.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `'atx'`
 *     — prefer ATX headings:
 *     ```markdown
 *     ## Hello
 *     ```
 * *   `'atx-closed'`
 *     — prefer ATX headings with a closing sequence:
 *     ```markdown
 *     ## Hello ##
 *     ```
 * *   `'setext'`
 *     — prefer setext headings:
 *     ```markdown
 *     Hello
 *     -----
 *     ```
 * *   `'consistent'`
 *     — detect the first used style and warn when further headings differ
 *
 * ## Recommendation
 *
 * Setext headings are limited in that they can only construct headings with a
 * rank of one and two.
 * On the other hand, they do allow multiple lines of content whereas ATX only
 * allows one line.
 * The number of used markers in their underline does not matter, leading to
 * either:
 *
 * *   1 marker (`Hello\n-`), which is the bare minimum, and for rank 2 headings
 *     looks suspiciously like an empty list item
 * *   using as many markers as the content (`Hello\n-----`), which is hard to
 *     maintain
 * *   an arbitrary number (`Hello\n---`), which for rank 2 headings looks
 *     suspiciously like a thematic break
 *
 * Setext headings are also rather uncommon.
 * Using a sequence of hashes at the end of ATX headings is even more uncommon.
 * Due to this, it’s recommended to prefer ATX headings.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats headings as ATX by default.
 * The other styles can be configured with
 * [`setext: true`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionssetext)
 * or
 * [`closeAtx: true`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionscloseatx).
 *
 * @module heading-style
 * @summary
 *   remark-lint rule to warn when headings violate a given style.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "config": "atx"}
 *
 *   # Alpha
 *
 *   ## Bravo
 *
 *   ### Charlie
 *
 * @example
 *   {"name": "ok.md", "config": "atx-closed"}
 *
 *   # Delta ##
 *
 *   ## Echo ##
 *
 *   ### Foxtrot ###
 *
 * @example
 *   {"name": "ok.md", "config": "setext"}
 *
 *   Golf
 *   ====
 *
 *   Hotel
 *   -----
 *
 *   ### India
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Juliett
 *   =======
 *
 *   ## Kilo
 *
 *   ### Lima ###
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   4:1-4:8: Headings should use setext
 *   6:1-6:13: Headings should use setext
 *
 * @example
 *   {"name": "not-ok.md", "config": "💩", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect heading style type `💩`: use either `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`
 */
const remarkLintHeadingStyle = lintRule(
  {
    origin: 'remark-lint:heading-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-heading-style#readme'
  },
  (tree, file, option = 'consistent') => {
    if (
      option !== 'consistent' &&
      option !== 'atx' &&
      option !== 'atx-closed' &&
      option !== 'setext'
    ) {
      file.fail(
        'Incorrect heading style type `' +
          option +
          "`: use either `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`"
      );
    }
    visit$c(tree, 'heading', (node) => {
      if (!generated(node)) {
        if (option === 'consistent') {
          option = headingStyle(node) || 'consistent';
        } else if (headingStyle(node, option) !== option) {
          file.message('Headings should use ' + option, node);
        }
      }
    });
  }
);
var remarkLintHeadingStyle$1 = remarkLintHeadingStyle;

const convert$b =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$b
      }
      if (typeof test === 'string') {
        return typeFactory$b(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$b(test) : propsFactory$b(test)
      }
      if (typeof test === 'function') {
        return castFactory$b(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$b(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$b(tests[index]);
  }
  return castFactory$b(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$b(check) {
  return castFactory$b(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$b(check) {
  return castFactory$b(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$b(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$b() {
  return true
}

function color$c(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$b = true;
const EXIT$b = false;
const SKIP$b = 'skip';
const visitParents$b =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$b(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$c(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$b(visitor(node, parents));
            if (result[0] === EXIT$b) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$b) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$b) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$b(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$b, value]
  }
  return [value]
}

const visit$b =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$b(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that lines do not exceed a certain size.
 *
 * ## API
 *
 * The following options (default: `80`) are accepted:
 *
 * *   `number` (example: `72`)
 *     — max number of characters to accept in heading text
 *
 * Ignores nodes that cannot be wrapped, such as headings, tables, code,
 * definitions, HTML, and JSX.
 * Ignores images, links, and code (inline) if they start before the wrap, end
 * after the wrap, and there’s no white space after them.
 *
 * ## Recommendation
 *
 * Whether to wrap prose or not is a stylistic choice.
 *
 * @module maximum-line-length
 * @summary
 *   remark-lint rule to warn when lines are too long.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "positionless": true, "gfm": true}
 *
 *   This line is simply not toooooooooooooooooooooooooooooooooooooooooooo
 *   long.
 *
 *   This is also fine: <http://this-long-url-with-a-long-domain.co.uk/a-long-path?query=variables>
 *
 *   <http://this-link-is-fine.com>
 *
 *   `alphaBravoCharlieDeltaEchoFoxtrotGolfHotelIndiaJuliettKiloLimaMikeNovemberOscarPapaQuebec.romeo()`
 *
 *   [foo](http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables)
 *
 *   <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>
 *
 *   ![foo](http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables)
 *
 *   | An | exception | is | line | length | in | long | tables | because | those | can’t | just |
 *   | -- | --------- | -- | ---- | ------ | -- | ---- | ------ | ------- | ----- | ----- | ---- |
 *   | be | helped    |    |      |        |    |      |        |         |       |       | .    |
 *
 *   <a><b><i><p><q><s><u>alpha bravo charlie delta echo foxtrot golf</u></s></q></p></i></b></a>
 *
 *   The following is also fine (note the `.`), because there is no whitespace.
 *
 *   <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>.
 *
 *   In addition, definitions are also fine:
 *
 *   [foo]: <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>
 *
 * @example
 *   {"name": "not-ok.md", "config": 80, "label": "input", "positionless": true}
 *
 *   This line is simply not tooooooooooooooooooooooooooooooooooooooooooooooooooooooo
 *   long.
 *
 *   Just like thiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiis one.
 *
 *   And this one is also very wrong: because the link starts aaaaaaafter the column: <http://line.com>
 *
 *   <http://this-long-url-with-a-long-domain-is-not-ok.co.uk/a-long-path?query=variables> and such.
 *
 *   And this one is also very wrong: because the code starts aaaaaaafter the column: `alpha.bravo()`
 *
 *   `alphaBravoCharlieDeltaEchoFoxtrotGolfHotelIndiaJuliettKiloLimaMikeNovemberOscar.papa()` and such.
 *
 * @example
 *   {"name": "not-ok.md", "config": 80, "label": "output", "positionless": true}
 *
 *   4:86: Line must be at most 80 characters
 *   6:99: Line must be at most 80 characters
 *   8:96: Line must be at most 80 characters
 *   10:97: Line must be at most 80 characters
 *   12:99: Line must be at most 80 characters
 *
 * @example
 *   {"name": "ok-mixed-line-endings.md", "config": 10, "positionless": true}
 *
 *   0123456789␍␊
 *   0123456789␊
 *   01234␍␊
 *   01234␊
 *
 * @example
 *   {"name": "not-ok-mixed-line-endings.md", "config": 10, "label": "input", "positionless": true}
 *
 *   012345678901␍␊
 *   012345678901␊
 *   01234567890␍␊
 *   01234567890␊
 *
 * @example
 *   {"name": "not-ok-mixed-line-endings.md", "config": 10, "label": "output", "positionless": true}
 *
 *   1:13: Line must be at most 10 characters
 *   2:13: Line must be at most 10 characters
 *   3:12: Line must be at most 10 characters
 *   4:12: Line must be at most 10 characters
 */
const remarkLintMaximumLineLength = lintRule(
  {
    origin: 'remark-lint:maximum-line-length',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-maximum-line-length#readme'
  },
  (tree, file, option = 80) => {
    const value = String(file);
    const lines = value.split(/\r?\n/);
    visit$b(tree, (node) => {
      if (
        (node.type === 'heading' ||
          node.type === 'table' ||
          node.type === 'code' ||
          node.type === 'definition' ||
          node.type === 'html' ||
          node.type === 'jsx' ||
          node.type === 'mdxFlowExpression' ||
          node.type === 'mdxJsxFlowElement' ||
          node.type === 'mdxJsxTextElement' ||
          node.type === 'mdxTextExpression' ||
          node.type === 'mdxjsEsm' ||
          node.type === 'yaml' ||
          node.type === 'toml') &&
        !generated(node)
      ) {
        allowList(pointStart(node).line - 1, pointEnd(node).line);
      }
    });
    visit$b(tree, (node, pos, parent) => {
      if (
        (node.type === 'link' ||
          node.type === 'image' ||
          node.type === 'inlineCode') &&
        !generated(node) &&
        parent &&
        typeof pos === 'number'
      ) {
        const initial = pointStart(node);
        const final = pointEnd(node);
        if (initial.column > option || final.column < option) {
          return
        }
        const next = parent.children[pos + 1];
        if (
          next &&
          pointStart(next).line === initial.line &&
          (!('value' in next) || /^(.+?[ \t].+?)/.test(next.value))
        ) {
          return
        }
        allowList(initial.line - 1, final.line);
      }
    });
    let index = -1;
    while (++index < lines.length) {
      const lineLength = lines[index].length;
      if (lineLength > option) {
        file.message('Line must be at most ' + option + ' characters', {
          line: index + 1,
          column: lineLength + 1
        });
      }
    }
    function allowList(initial, final) {
      while (initial < final) {
        lines[initial++] = '';
      }
    }
  }
);
var remarkLintMaximumLineLength$1 = remarkLintMaximumLineLength;

const convert$a =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$a
      }
      if (typeof test === 'string') {
        return typeFactory$a(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$a(test) : propsFactory$a(test)
      }
      if (typeof test === 'function') {
        return castFactory$a(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$a(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$a(tests[index]);
  }
  return castFactory$a(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$a(check) {
  return castFactory$a(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$a(check) {
  return castFactory$a(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$a(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$a() {
  return true
}

function color$b(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$a = true;
const EXIT$a = false;
const SKIP$a = 'skip';
const visitParents$a =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$a(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$b(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$a(visitor(node, parents));
            if (result[0] === EXIT$a) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$a) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$a) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$a(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$a, value]
  }
  return [value]
}

const visit$a =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$a(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that no more blank lines than needed
 * are used between blocks.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * More than one blank line has no effect between blocks.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * adds exactly one blank line between any block.
 *
 * @module no-consecutive-blank-lines
 * @summary
 *   remark-lint rule to warn when more blank lines that needed are used
 *   between blocks.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   Foo…
 *   ␊
 *   …Bar.
 *
 * @example
 *   {"name": "empty-document.md"}
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Foo…
 *   ␊
 *   ␊
 *   …Bar
 *   ␊
 *   ␊
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   4:1: Remove 1 line before node
 *   4:5: Remove 2 lines after node
 */
const unknownContainerSize = new Set(['mdxJsxFlowElement', 'mdxJsxTextElement']);
const remarkLintNoConsecutiveBlankLines = lintRule(
  {
    origin: 'remark-lint:no-consecutive-blank-lines',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-consecutive-blank-lines#readme'
  },
  (tree, file) => {
    visit$a(tree, (node) => {
      if (!generated(node) && 'children' in node) {
        const head = node.children[0];
        if (head && !generated(head)) {
          if (!unknownContainerSize.has(node.type)) {
            compare(pointStart(node), pointStart(head), 0);
          }
          let index = -1;
          while (++index < node.children.length) {
            const previous = node.children[index - 1];
            const child = node.children[index];
            if (previous && !generated(previous) && !generated(child)) {
              compare(pointEnd(previous), pointStart(child), 2);
            }
          }
          const tail = node.children[node.children.length - 1];
          if (
            tail !== head &&
            !generated(tail) &&
            !unknownContainerSize.has(node.type)
          ) {
            compare(pointEnd(node), pointEnd(tail), 1);
          }
        }
      }
    });
    function compare(start, end, max) {
      const diff = end.line - start.line;
      const lines = Math.abs(diff) - max;
      if (lines > 0) {
        file.message(
          'Remove ' +
            lines +
            ' ' +
            plural('line', Math.abs(lines)) +
            ' ' +
            (diff > 0 ? 'before' : 'after') +
            ' node',
          end
        );
      }
    }
  }
);
var remarkLintNoConsecutiveBlankLines$1 = remarkLintNoConsecutiveBlankLines;

/**
 * ## When should I use this?
 *
 * You can use this package to check that file names do not start with
 *  articles (`a`, `the`, etc).
 *
 * ## API
 *
 * There are no options.
 *
 * @module no-file-name-articles
 * @summary
 *   remark-lint rule to warn when file names start with articles.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "title.md"}
 *
 * @example
 *   {"name": "a-title.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not start file names with `a`
 *
 * @example
 *   {"name": "the-title.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not start file names with `the`
 *
 * @example
 *   {"name": "teh-title.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not start file names with `teh`
 *
 * @example
 *   {"name": "an-article.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not start file names with `an`
 */
const remarkLintNoFileNameArticles = lintRule(
  {
    origin: 'remark-lint:no-file-name-articles',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-file-name-articles#readme'
  },
  (_, file) => {
    const match = file.stem && file.stem.match(/^(the|teh|an?)\b/i);
    if (match) {
      file.message('Do not start file names with `' + match[0] + '`');
    }
  }
);
var remarkLintNoFileNameArticles$1 = remarkLintNoFileNameArticles;

/**
 * ## When should I use this?
 *
 * You can use this package to check that no consecutive dashes appear in
 * file names.
 *
 * ## API
 *
 * There are no options.
 *
 * @module no-file-name-consecutive-dashes
 * @summary
 *   remark-lint rule to warn when consecutive dashes appear in file names.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "plug-ins.md"}
 *
 * @example
 *   {"name": "plug--ins.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not use consecutive dashes in a file name
 */
const remarkLintNoFileNameConsecutiveDashes = lintRule(
  {
    origin: 'remark-lint:no-file-name-consecutive-dashes',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-file-name-consecutive-dashes#readme'
  },
  (_, file) => {
    if (file.stem && /-{2,}/.test(file.stem)) {
      file.message('Do not use consecutive dashes in a file name');
    }
  }
);
var remarkLintNoFileNameConsecutiveDashes$1 = remarkLintNoFileNameConsecutiveDashes;

/**
 * ## When should I use this?
 *
 * You can use this package to check that no initial or final dashes appear in
 * file names.
 *
 * ## API
 *
 * There are no options.
 *
 * @module no-file-name-outer-dashes
 * @summary
 *   remark-lint rule to warn when initial or final dashes appear in file names.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "readme.md"}
 *
 * @example
 *   {"name": "-readme.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not use initial or final dashes in a file name
 *
 * @example
 *   {"name": "readme-.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not use initial or final dashes in a file name
 */
const remarkLintNofileNameOuterDashes = lintRule(
  {
    origin: 'remark-lint:no-file-name-outer-dashes',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-file-name-outer-dashes#readme'
  },
  (_, file) => {
    if (file.stem && /^-|-$/.test(file.stem)) {
      file.message('Do not use initial or final dashes in a file name');
    }
  }
);
var remarkLintNofileNameOuterDashes$1 = remarkLintNofileNameOuterDashes;

const convert$9 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$9
      }
      if (typeof test === 'string') {
        return typeFactory$9(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$9(test) : propsFactory$9(test)
      }
      if (typeof test === 'function') {
        return castFactory$9(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$9(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$9(tests[index]);
  }
  return castFactory$9(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$9(check) {
  return castFactory$9(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$9(check) {
  return castFactory$9(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$9(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$9() {
  return true
}

function color$a(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$9 = true;
const EXIT$9 = false;
const SKIP$9 = 'skip';
const visitParents$9 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$9(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$a(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$9(visitor(node, parents));
            if (result[0] === EXIT$9) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$9) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$9) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$9(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$9, value]
  }
  return [value]
}

const visit$9 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$9(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that headings are not indented.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * There is no specific handling of indented headings (or anything else) in
 * markdown.
 * While it is possible to use an indent to headings on their text:
 *
 * ```markdown
 *    # One
 *   ## Two
 *  ### Three
 * #### Four
 * ```
 *
 * …such style is uncommon, a bit hard to maintain, and it’s impossible to add a
 * heading with a rank of 5 as it would form indented code instead.
 * Hence, it’s recommended to not indent headings and to turn this rule on.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats all headings without indent.
 *
 * @module no-heading-indent
 * @summary
 *   remark-lint rule to warn when headings are indented.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   #·Hello world
 *
 *   Foo
 *   -----
 *
 *   #·Hello world·#
 *
 *   Bar
 *   =====
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ···# Hello world
 *
 *   ·Foo
 *   -----
 *
 *   ·# Hello world #
 *
 *   ···Bar
 *   =====
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:4: Remove 3 spaces before this heading
 *   3:2: Remove 1 space before this heading
 *   6:2: Remove 1 space before this heading
 *   8:4: Remove 3 spaces before this heading
 */
const remarkLintNoHeadingIndent = lintRule(
  {
    origin: 'remark-lint:no-heading-indent',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-heading-indent#readme'
  },
  (tree, file) => {
    visit$9(tree, 'heading', (node, _, parent) => {
      if (generated(node) || (parent && parent.type !== 'root')) {
        return
      }
      const diff = pointStart(node).column - 1;
      if (diff) {
        file.message(
          'Remove ' +
            diff +
            ' ' +
            plural('space', diff) +
            ' before this heading',
          pointStart(node)
        );
      }
    });
  }
);
var remarkLintNoHeadingIndent$1 = remarkLintNoHeadingIndent;

const convert$8 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$8
      }
      if (typeof test === 'string') {
        return typeFactory$8(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$8(test) : propsFactory$8(test)
      }
      if (typeof test === 'function') {
        return castFactory$8(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$8(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$8(tests[index]);
  }
  return castFactory$8(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$8(check) {
  return castFactory$8(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$8(check) {
  return castFactory$8(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$8(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$8() {
  return true
}

function color$9(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$8 = true;
const EXIT$8 = false;
const SKIP$8 = 'skip';
const visitParents$8 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$8(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$9(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$8(visitor(node, parents));
            if (result[0] === EXIT$8) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$8) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$8) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$8(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$8, value]
  }
  return [value]
}

const visit$8 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$8(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

function stringifyPosition(value) {
  if (!value || typeof value !== 'object') {
    return ''
  }
  if ('position' in value || 'type' in value) {
    return position(value.position)
  }
  if ('start' in value || 'end' in value) {
    return position(value)
  }
  if ('line' in value || 'column' in value) {
    return point(value)
  }
  return ''
}
function point(point) {
  return index(point && point.line) + ':' + index(point && point.column)
}
function position(pos) {
  return point(pos && pos.start) + '-' + point(pos && pos.end)
}
function index(value) {
  return value && typeof value === 'number' ? value : 1
}

/**
 * ## When should I use this?
 *
 * You can use this package to check that no more than one top level heading
 * is used.
 *
 * ## API
 *
 * The following options (default: `1`) are accepted:
 *
 * *   `number` (example: `1`)
 *     — assumed top level heading rank
 *
 * ## Recommendation
 *
 * Documents should almost always have one main heading, which is typically a
 * heading with a rank of `1`.
 *
 * @module no-multiple-toplevel-headings
 * @summary
 *   remark-lint rule to warn when more than one top level heading is used.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "config": 1}
 *
 *   # Foo
 *
 *   ## Bar
 *
 * @example
 *   {"name": "not-ok.md", "config": 1, "label": "input"}
 *
 *   # Foo
 *
 *   # Bar
 *
 * @example
 *   {"name": "not-ok.md", "config": 1, "label": "output"}
 *
 *   3:1-3:6: Don’t use multiple top level headings (1:1)
 */
const remarkLintNoMultipleToplevelHeadings = lintRule(
  {
    origin: 'remark-lint:no-multiple-toplevel-headings',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-multiple-toplevel-headings#readme'
  },
  (tree, file, option = 1) => {
    let duplicate;
    visit$8(tree, 'heading', (node) => {
      if (!generated(node) && node.depth === option) {
        if (duplicate) {
          file.message(
            'Don’t use multiple top level headings (' + duplicate + ')',
            node
          );
        } else {
          duplicate = stringifyPosition(pointStart(node));
        }
      }
    });
  }
);
var remarkLintNoMultipleToplevelHeadings$1 = remarkLintNoMultipleToplevelHeadings;

const convert$7 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$7
      }
      if (typeof test === 'string') {
        return typeFactory$7(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$7(test) : propsFactory$7(test)
      }
      if (typeof test === 'function') {
        return castFactory$7(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$7(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$7(tests[index]);
  }
  return castFactory$7(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$7(check) {
  return castFactory$7(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$7(check) {
  return castFactory$7(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$7(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$7() {
  return true
}

function color$8(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$7 = true;
const EXIT$7 = false;
const SKIP$7 = 'skip';
const visitParents$7 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$7(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$8(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$7(visitor(node, parents));
            if (result[0] === EXIT$7) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$7) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$7) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$7(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$7, value]
  }
  return [value]
}

const visit$7 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$7(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that not all lines in shell code are
 * preceded by dollars (`$`).
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Dollars make copy/pasting hard.
 * Either put both dollars in front of some lines (to indicate shell commands)
 * and don’t put them in front of other lines, or use fenced code to indicate
 * shell commands on their own, followed by another fenced code that contains
 * just the output.
 *
 * @module no-shell-dollars
 * @summary
 *   remark-lint rule to warn every line in shell code is preceded by `$`s.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   ```bash
 *   echo a
 *   ```
 *
 *   ```sh
 *   echo a
 *   echo a > file
 *   ```
 *
 *   ```zsh
 *   $ echo a
 *   a
 *   $ echo a > file
 *   ```
 *
 *   Some empty code:
 *
 *   ```command
 *   ```
 *
 *   It’s fine to use dollars in non-shell code.
 *
 *   ```js
 *   $('div').remove()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ```sh
 *   $ echo a
 *   ```
 *
 *   ```bash
 *   $ echo a
 *   $ echo a > file
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-3:4: Do not use dollar signs before shell commands
 *   5:1-8:4: Do not use dollar signs before shell commands
 */
const flags = new Set([
  'sh',
  'bash',
  'bats',
  'cgi',
  'command',
  'fcgi',
  'ksh',
  'tmux',
  'tool',
  'zsh'
]);
const remarkLintNoShellDollars = lintRule(
  {
    origin: 'remark-lint:no-shell-dollars',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-shell-dollars#readme'
  },
  (tree, file) => {
    visit$7(tree, 'code', (node) => {
      if (!generated(node) && node.lang && flags.has(node.lang)) {
        const lines = node.value
          .split('\n')
          .filter((line) => line.trim().length > 0);
        let index = -1;
        if (lines.length === 0) {
          return
        }
        while (++index < lines.length) {
          const line = lines[index];
          if (line.trim() && !/^\s*\$\s*/.test(line)) {
            return
          }
        }
        file.message('Do not use dollar signs before shell commands', node);
      }
    });
  }
);
var remarkLintNoShellDollars$1 = remarkLintNoShellDollars;

const convert$6 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$6
      }
      if (typeof test === 'string') {
        return typeFactory$6(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$6(test) : propsFactory$6(test)
      }
      if (typeof test === 'function') {
        return castFactory$6(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$6(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$6(tests[index]);
  }
  return castFactory$6(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$6(check) {
  return castFactory$6(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$6(check) {
  return castFactory$6(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$6(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$6() {
  return true
}

function color$7(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$6 = true;
const EXIT$6 = false;
const SKIP$6 = 'skip';
const visitParents$6 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$6(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$7(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$6(visitor(node, parents));
            if (result[0] === EXIT$6) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$6) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$6) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$6(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$6, value]
  }
  return [value]
}

const visit$6 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$6(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that tables are not indented.
 * Tables are a GFM feature enabled with
 * [`remark-gfm`](https://github.com/remarkjs/remark-gfm).
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * There is no specific handling of indented tables (or anything else) in
 * markdown.
 * Hence, it’s recommended to not indent tables and to turn this rule on.
 *
 * ## Fix
 *
 * [`remark-gfm`](https://github.com/remarkjs/remark-gfm)
 * formats all tables without indent.
 *
 * @module no-table-indentation
 * @summary
 *   remark-lint rule to warn when tables are indented.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "gfm": true}
 *
 *   Paragraph.
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "gfm": true}
 *
 *   Paragraph.
 *
 *   ···| A     | B     |
 *   ···| ----- | ----- |
 *   ···| Alpha | Bravo |
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "gfm": true}
 *
 *   3:4: Do not indent table rows
 *   4:4: Do not indent table rows
 *   5:4: Do not indent table rows
 *
 * @example
 *   {"name": "not-ok-blockquote.md", "label": "input", "gfm": true}
 *
 *   >··| A |
 *   >·| - |
 *
 * @example
 *   {"name": "not-ok-blockquote.md", "label": "output", "gfm": true}
 *
 *   1:4: Do not indent table rows
 *
 * @example
 *   {"name": "not-ok-list.md", "label": "input", "gfm": true}
 *
 *   -···paragraph
 *
 *   ·····| A |
 *   ····| - |
 *
 * @example
 *   {"name": "not-ok-list.md", "label": "output", "gfm": true}
 *
 *   3:6: Do not indent table rows
 */
const remarkLintNoTableIndentation = lintRule(
  {
    origin: 'remark-lint:no-table-indentation',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-table-indentation#readme'
  },
  (tree, file) => {
    const value = String(file);
    const loc = location(value);
    visit$6(tree, 'table', (node, _, parent) => {
      const end = pointEnd(node).line;
      let line = pointStart(node).line;
      let column = 0;
      if (parent && parent.type === 'root') {
        column = 1;
      } else if (parent && parent.type === 'blockquote') {
        column = pointStart(parent).column + 2;
      } else if (parent && parent.type === 'listItem') {
        column = pointStart(parent.children[0]).column;
        if (parent.children[0] === node) {
          line++;
        }
      }
      if (!column || !line) {
        return
      }
      while (line <= end) {
        let offset = loc.toOffset({line, column});
        const lineColumn = offset;
        while (/[ \t]/.test(value.charAt(offset - 1))) {
          offset--;
        }
        if (!offset || /[\r\n>]/.test(value.charAt(offset - 1))) {
          offset = lineColumn;
          while (/[ \t]/.test(value.charAt(offset))) {
            offset++;
          }
          if (lineColumn !== offset) {
            file.message('Do not indent table rows', loc.toPoint(offset));
          }
        }
        line++;
      }
      return SKIP$6
    });
  }
);
var remarkLintNoTableIndentation$1 = remarkLintNoTableIndentation;

/**
 * ## When should I use this?
 *
 * You can use this package to check that tabs are not used.
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * Regardless of the debate in other languages of whether to use tabs vs.
 * spaces, when it comes to markdown, tabs do not work as expected.
 * Largely around contains such as block quotes and lists.
 * Take for example block quotes: `>\ta` gives a paragraph with the text `a`
 * in a blockquote, so one might expect that `>\t\ta` results in indented code
 * with the text `a` in a block quote.
 *
 * ```markdown
 * >\ta
 *
 * >\t\ta
 * ```
 *
 * Yields:
 *
 * ```html
 * <blockquote>
 * <p>a</p>
 * </blockquote>
 * <blockquote>
 * <pre><code>  a
 * </code></pre>
 * </blockquote>
 * ```
 *
 * Because markdown uses a hardcoded tab size of 4, the first tab could be
 * represented as 3 spaces (because there’s a `>` before).
 * One of those “spaces” is taken because block quotes allow the `>` to be
 * followed by one space, leaving 2 spaces.
 * The next tab can be represented as 4 spaces, so together we have 6 spaces.
 * The indented code uses 4 spaces, so there are two spaces left, which are
 * shown in the indented code.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * uses spaces exclusively for indentation.
 *
 * @module no-tabs
 * @summary
 *   remark-lint rule to warn when tabs are used.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   Foo Bar
 *
 *   ····Foo
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "positionless": true}
 *
 *   »Here's one before a code block.
 *
 *   Here's a tab:», and here is another:».
 *
 *   And this is in `inline»code`.
 *
 *   >»This is in a block quote.
 *
 *   *»And…
 *
 *   »1.»in a list.
 *
 *   And this is a tab as the last character.»
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1: Use spaces instead of tabs
 *   3:14: Use spaces instead of tabs
 *   3:37: Use spaces instead of tabs
 *   5:23: Use spaces instead of tabs
 *   7:2: Use spaces instead of tabs
 *   9:2: Use spaces instead of tabs
 *   11:1: Use spaces instead of tabs
 *   11:4: Use spaces instead of tabs
 *   13:41: Use spaces instead of tabs
 */
const remarkLintNoTabs = lintRule(
  {
    origin: 'remark-lint:no-tabs',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-tabs#readme'
  },
  (_, file) => {
    const value = String(file);
    const toPoint = location(file).toPoint;
    let index = value.indexOf('\t');
    while (index !== -1) {
      file.message('Use spaces instead of tabs', toPoint(index));
      index = value.indexOf('\t', index + 1);
    }
  }
);
var remarkLintNoTabs$1 = remarkLintNoTabs;

var sliced$1 = function (args, slice, sliceEnd) {
  var ret = [];
  var len = args.length;
  if (0 === len) return ret;
  var start = slice < 0
    ? Math.max(0, slice + len)
    : slice || 0;
  if (sliceEnd !== undefined) {
    len = sliceEnd < 0
      ? sliceEnd + len
      : sliceEnd;
  }
  while (len-- > start) {
    ret[len - start] = args[len];
  }
  return ret;
};
getDefaultExportFromCjs(sliced$1);

var slice = Array.prototype.slice;
var co_1 = co$1;
function co$1(fn) {
  var isGenFun = isGeneratorFunction(fn);
  return function (done) {
    var ctx = this;
    var gen = fn;
    if (isGenFun) {
      var args = slice.call(arguments), len = args.length;
      var hasCallback = len && 'function' == typeof args[len - 1];
      done = hasCallback ? args.pop() : error;
      gen = fn.apply(this, args);
    } else {
      done = done || error;
    }
    next();
    function exit(err, res) {
      setImmediate(function(){
        done.call(ctx, err, res);
      });
    }
    function next(err, res) {
      var ret;
      if (arguments.length > 2) res = slice.call(arguments, 1);
      if (err) {
        try {
          ret = gen.throw(err);
        } catch (e) {
          return exit(e);
        }
      }
      if (!err) {
        try {
          ret = gen.next(res);
        } catch (e) {
          return exit(e);
        }
      }
      if (ret.done) return exit(null, ret.value);
      ret.value = toThunk(ret.value, ctx);
      if ('function' == typeof ret.value) {
        var called = false;
        try {
          ret.value.call(ctx, function(){
            if (called) return;
            called = true;
            next.apply(ctx, arguments);
          });
        } catch (e) {
          setImmediate(function(){
            if (called) return;
            called = true;
            next(e);
          });
        }
        return;
      }
      next(new TypeError('You may only yield a function, promise, generator, array, or object, '
        + 'but the following was passed: "' + String(ret.value) + '"'));
    }
  }
}
function toThunk(obj, ctx) {
  if (isGeneratorFunction(obj)) {
    return co$1(obj.call(ctx));
  }
  if (isGenerator(obj)) {
    return co$1(obj);
  }
  if (isPromise(obj)) {
    return promiseToThunk(obj);
  }
  if ('function' == typeof obj) {
    return obj;
  }
  if (isObject$1(obj) || Array.isArray(obj)) {
    return objectToThunk.call(ctx, obj);
  }
  return obj;
}
function objectToThunk(obj){
  var ctx = this;
  var isArray = Array.isArray(obj);
  return function(done){
    var keys = Object.keys(obj);
    var pending = keys.length;
    var results = isArray
      ? new Array(pending)
      : new obj.constructor();
    var finished;
    if (!pending) {
      setImmediate(function(){
        done(null, results);
      });
      return;
    }
    if (!isArray) {
      for (var i = 0; i < pending; i++) {
        results[keys[i]] = undefined;
      }
    }
    for (var i = 0; i < keys.length; i++) {
      run(obj[keys[i]], keys[i]);
    }
    function run(fn, key) {
      if (finished) return;
      try {
        fn = toThunk(fn, ctx);
        if ('function' != typeof fn) {
          results[key] = fn;
          return --pending || done(null, results);
        }
        fn.call(ctx, function(err, res){
          if (finished) return;
          if (err) {
            finished = true;
            return done(err);
          }
          results[key] = res;
          --pending || done(null, results);
        });
      } catch (err) {
        finished = true;
        done(err);
      }
    }
  }
}
function promiseToThunk(promise) {
  return function(fn){
    promise.then(function(res) {
      fn(null, res);
    }, fn);
  }
}
function isPromise(obj) {
  return obj && 'function' == typeof obj.then;
}
function isGenerator(obj) {
  return obj && 'function' == typeof obj.next && 'function' == typeof obj.throw;
}
function isGeneratorFunction(obj) {
  return obj && obj.constructor && 'GeneratorFunction' == obj.constructor.name;
}
function isObject$1(val) {
  return val && Object == val.constructor;
}
function error(err) {
  if (!err) return;
  setImmediate(function(){
    throw err;
  });
}
getDefaultExportFromCjs(co_1);

var sliced = sliced$1;
var noop = function(){};
var co = co_1;
var wrapped_1 = wrapped$1;
function wrapped$1(fn) {
  function wrap() {
    var args = sliced(arguments);
    var last = args[args.length - 1];
    var ctx = this;
    var done = typeof last == 'function' ? args.pop() : noop;
    if (!fn) {
      return done.apply(ctx, [null].concat(args));
    }
    if (generator(fn)) {
      return co(fn).apply(ctx, args.concat(done));
    }
    if (fn.length > args.length) {
      try {
        return fn.apply(ctx, args.concat(done));
      } catch (e) {
        return done(e);
      }
    }
    return sync(fn, done).apply(ctx, args);
  }
  return wrap;
}
function sync(fn, done) {
  return function () {
    var ret;
    try {
      ret = fn.apply(this, arguments);
    } catch (err) {
      return done(err);
    }
    if (promise(ret)) {
      ret.then(function (value) { done(null, value); }, done);
    } else {
      ret instanceof Error ? done(ret) : done(null, ret);
    }
  }
}
function generator(value) {
  return value
    && value.constructor
    && 'GeneratorFunction' == value.constructor.name;
}
function promise(value) {
  return value && 'function' == typeof value.then;
}
getDefaultExportFromCjs(wrapped_1);

var wrapped = wrapped_1;
var unifiedLintRule = factory;
function factory(id, rule) {
  var parts = id.split(':');
  var source = parts[0];
  var ruleId = parts[1];
  var fn = wrapped(rule);
  if (!ruleId) {
    ruleId = source;
    source = null;
  }
  attacher.displayName = id;
  return attacher
  function attacher(raw) {
    var config = coerce(ruleId, raw);
    var severity = config[0];
    var options = config[1];
    var fatal = severity === 2;
    return severity ? transformer : undefined
    function transformer(tree, file, next) {
      var index = file.messages.length;
      fn(tree, file, options, done);
      function done(err) {
        var messages = file.messages;
        var message;
        if (err && messages.indexOf(err) === -1) {
          try {
            file.fail(err);
          } catch (_) {}
        }
        while (index < messages.length) {
          message = messages[index];
          message.ruleId = ruleId;
          message.source = source;
          message.fatal = fatal;
          index++;
        }
        next();
      }
    }
  }
}
function coerce(name, value) {
  var def = 1;
  var result;
  var level;
  if (typeof value === 'boolean') {
    result = [value];
  } else if (value == null) {
    result = [def];
  } else if (
    typeof value === 'object' &&
    (typeof value[0] === 'number' ||
      typeof value[0] === 'boolean' ||
      typeof value[0] === 'string')
  ) {
    result = value.concat();
  } else {
    result = [1, value];
  }
  level = result[0];
  if (typeof level === 'boolean') {
    level = level ? 1 : 0;
  } else if (typeof level === 'string') {
    if (level === 'off') {
      level = 0;
    } else if (level === 'on' || level === 'warn') {
      level = 1;
    } else if (level === 'error') {
      level = 2;
    } else {
      level = 1;
      result = [level, result];
    }
  }
  if (level < 0 || level > 2) {
    throw new Error(
      'Incorrect severity `' +
        level +
        '` for `' +
        name +
        '`, ' +
        'expected 0, 1, or 2'
    )
  }
  result[0] = level;
  return result
}
getDefaultExportFromCjs(unifiedLintRule);

var rule = unifiedLintRule;
var remarkLintNoTrailingSpaces = rule('remark-lint:no-trailing-spaces', noTrailingSpaces);
function noTrailingSpaces(ast, file) {
  var lines = file.toString().split(/\r?\n/);
  for (var i = 0; i < lines.length; i++) {
    var currentLine = lines[i];
    var lineIndex = i + 1;
    if (/\s$/.test(currentLine)) {
      file.message('Remove trailing whitespace', {
        position: {
          start: { line: lineIndex, column: currentLine.length + 1 },
          end: { line: lineIndex }
        }
      });
    }
  }
}
var remarkLintNoTrailingSpaces$1 = getDefaultExportFromCjs(remarkLintNoTrailingSpaces);

function* getLinksRecursively(node) {
  if (node.url) {
    yield node;
  }
  for (const child of node.children || []) {
    yield* getLinksRecursively(child);
  }
}
function validateLinks(tree, vfile) {
  const currentFileURL = pathToFileURL(path$2.join(vfile.cwd, vfile.path));
  let previousDefinitionLabel;
  for (const node of getLinksRecursively(tree)) {
    if (node.url[0] !== "#") {
      const targetURL = new URL(node.url, currentFileURL);
      if (targetURL.protocol === "file:" && !fs.existsSync(targetURL)) {
        vfile.message("Broken link", node);
      } else if (targetURL.pathname === currentFileURL.pathname) {
        const expected = node.url.includes("#")
          ? node.url.slice(node.url.indexOf("#"))
          : "#";
        vfile.message(
          `Self-reference must start with hash (expected "${expected}", got "${node.url}")`,
          node,
        );
      }
    }
    if (node.type === "definition") {
      if (previousDefinitionLabel && previousDefinitionLabel > node.label) {
        vfile.message(
          `Unordered reference ("${node.label}" should be before "${previousDefinitionLabel}")`,
          node,
        );
      }
      previousDefinitionLabel = node.label;
    }
  }
}
const remarkLintNodejsLinks = lintRule(
  "remark-lint:nodejs-links",
  validateLinks,
);

/*! js-yaml 4.1.0 https://github.com/nodeca/js-yaml @license MIT */
function isNothing(subject) {
  return (typeof subject === 'undefined') || (subject === null);
}
function isObject(subject) {
  return (typeof subject === 'object') && (subject !== null);
}
function toArray(sequence) {
  if (Array.isArray(sequence)) return sequence;
  else if (isNothing(sequence)) return [];
  return [ sequence ];
}
function extend(target, source) {
  var index, length, key, sourceKeys;
  if (source) {
    sourceKeys = Object.keys(source);
    for (index = 0, length = sourceKeys.length; index < length; index += 1) {
      key = sourceKeys[index];
      target[key] = source[key];
    }
  }
  return target;
}
function repeat(string, count) {
  var result = '', cycle;
  for (cycle = 0; cycle < count; cycle += 1) {
    result += string;
  }
  return result;
}
function isNegativeZero(number) {
  return (number === 0) && (Number.NEGATIVE_INFINITY === 1 / number);
}
var isNothing_1      = isNothing;
var isObject_1       = isObject;
var toArray_1        = toArray;
var repeat_1         = repeat;
var isNegativeZero_1 = isNegativeZero;
var extend_1         = extend;
var common = {
	isNothing: isNothing_1,
	isObject: isObject_1,
	toArray: toArray_1,
	repeat: repeat_1,
	isNegativeZero: isNegativeZero_1,
	extend: extend_1
};
function formatError(exception, compact) {
  var where = '', message = exception.reason || '(unknown reason)';
  if (!exception.mark) return message;
  if (exception.mark.name) {
    where += 'in "' + exception.mark.name + '" ';
  }
  where += '(' + (exception.mark.line + 1) + ':' + (exception.mark.column + 1) + ')';
  if (!compact && exception.mark.snippet) {
    where += '\n\n' + exception.mark.snippet;
  }
  return message + ' ' + where;
}
function YAMLException$1(reason, mark) {
  Error.call(this);
  this.name = 'YAMLException';
  this.reason = reason;
  this.mark = mark;
  this.message = formatError(this, false);
  if (Error.captureStackTrace) {
    Error.captureStackTrace(this, this.constructor);
  } else {
    this.stack = (new Error()).stack || '';
  }
}
YAMLException$1.prototype = Object.create(Error.prototype);
YAMLException$1.prototype.constructor = YAMLException$1;
YAMLException$1.prototype.toString = function toString(compact) {
  return this.name + ': ' + formatError(this, compact);
};
var exception = YAMLException$1;
function getLine(buffer, lineStart, lineEnd, position, maxLineLength) {
  var head = '';
  var tail = '';
  var maxHalfLength = Math.floor(maxLineLength / 2) - 1;
  if (position - lineStart > maxHalfLength) {
    head = ' ... ';
    lineStart = position - maxHalfLength + head.length;
  }
  if (lineEnd - position > maxHalfLength) {
    tail = ' ...';
    lineEnd = position + maxHalfLength - tail.length;
  }
  return {
    str: head + buffer.slice(lineStart, lineEnd).replace(/\t/g, '→') + tail,
    pos: position - lineStart + head.length
  };
}
function padStart(string, max) {
  return common.repeat(' ', max - string.length) + string;
}
function makeSnippet(mark, options) {
  options = Object.create(options || null);
  if (!mark.buffer) return null;
  if (!options.maxLength) options.maxLength = 79;
  if (typeof options.indent      !== 'number') options.indent      = 1;
  if (typeof options.linesBefore !== 'number') options.linesBefore = 3;
  if (typeof options.linesAfter  !== 'number') options.linesAfter  = 2;
  var re = /\r?\n|\r|\0/g;
  var lineStarts = [ 0 ];
  var lineEnds = [];
  var match;
  var foundLineNo = -1;
  while ((match = re.exec(mark.buffer))) {
    lineEnds.push(match.index);
    lineStarts.push(match.index + match[0].length);
    if (mark.position <= match.index && foundLineNo < 0) {
      foundLineNo = lineStarts.length - 2;
    }
  }
  if (foundLineNo < 0) foundLineNo = lineStarts.length - 1;
  var result = '', i, line;
  var lineNoLength = Math.min(mark.line + options.linesAfter, lineEnds.length).toString().length;
  var maxLineLength = options.maxLength - (options.indent + lineNoLength + 3);
  for (i = 1; i <= options.linesBefore; i++) {
    if (foundLineNo - i < 0) break;
    line = getLine(
      mark.buffer,
      lineStarts[foundLineNo - i],
      lineEnds[foundLineNo - i],
      mark.position - (lineStarts[foundLineNo] - lineStarts[foundLineNo - i]),
      maxLineLength
    );
    result = common.repeat(' ', options.indent) + padStart((mark.line - i + 1).toString(), lineNoLength) +
      ' | ' + line.str + '\n' + result;
  }
  line = getLine(mark.buffer, lineStarts[foundLineNo], lineEnds[foundLineNo], mark.position, maxLineLength);
  result += common.repeat(' ', options.indent) + padStart((mark.line + 1).toString(), lineNoLength) +
    ' | ' + line.str + '\n';
  result += common.repeat('-', options.indent + lineNoLength + 3 + line.pos) + '^' + '\n';
  for (i = 1; i <= options.linesAfter; i++) {
    if (foundLineNo + i >= lineEnds.length) break;
    line = getLine(
      mark.buffer,
      lineStarts[foundLineNo + i],
      lineEnds[foundLineNo + i],
      mark.position - (lineStarts[foundLineNo] - lineStarts[foundLineNo + i]),
      maxLineLength
    );
    result += common.repeat(' ', options.indent) + padStart((mark.line + i + 1).toString(), lineNoLength) +
      ' | ' + line.str + '\n';
  }
  return result.replace(/\n$/, '');
}
var snippet = makeSnippet;
var TYPE_CONSTRUCTOR_OPTIONS = [
  'kind',
  'multi',
  'resolve',
  'construct',
  'instanceOf',
  'predicate',
  'represent',
  'representName',
  'defaultStyle',
  'styleAliases'
];
var YAML_NODE_KINDS = [
  'scalar',
  'sequence',
  'mapping'
];
function compileStyleAliases(map) {
  var result = {};
  if (map !== null) {
    Object.keys(map).forEach(function (style) {
      map[style].forEach(function (alias) {
        result[String(alias)] = style;
      });
    });
  }
  return result;
}
function Type$1(tag, options) {
  options = options || {};
  Object.keys(options).forEach(function (name) {
    if (TYPE_CONSTRUCTOR_OPTIONS.indexOf(name) === -1) {
      throw new exception('Unknown option "' + name + '" is met in definition of "' + tag + '" YAML type.');
    }
  });
  this.options       = options;
  this.tag           = tag;
  this.kind          = options['kind']          || null;
  this.resolve       = options['resolve']       || function () { return true; };
  this.construct     = options['construct']     || function (data) { return data; };
  this.instanceOf    = options['instanceOf']    || null;
  this.predicate     = options['predicate']     || null;
  this.represent     = options['represent']     || null;
  this.representName = options['representName'] || null;
  this.defaultStyle  = options['defaultStyle']  || null;
  this.multi         = options['multi']         || false;
  this.styleAliases  = compileStyleAliases(options['styleAliases'] || null);
  if (YAML_NODE_KINDS.indexOf(this.kind) === -1) {
    throw new exception('Unknown kind "' + this.kind + '" is specified for "' + tag + '" YAML type.');
  }
}
var type = Type$1;
function compileList(schema, name) {
  var result = [];
  schema[name].forEach(function (currentType) {
    var newIndex = result.length;
    result.forEach(function (previousType, previousIndex) {
      if (previousType.tag === currentType.tag &&
          previousType.kind === currentType.kind &&
          previousType.multi === currentType.multi) {
        newIndex = previousIndex;
      }
    });
    result[newIndex] = currentType;
  });
  return result;
}
function compileMap() {
  var result = {
        scalar: {},
        sequence: {},
        mapping: {},
        fallback: {},
        multi: {
          scalar: [],
          sequence: [],
          mapping: [],
          fallback: []
        }
      }, index, length;
  function collectType(type) {
    if (type.multi) {
      result.multi[type.kind].push(type);
      result.multi['fallback'].push(type);
    } else {
      result[type.kind][type.tag] = result['fallback'][type.tag] = type;
    }
  }
  for (index = 0, length = arguments.length; index < length; index += 1) {
    arguments[index].forEach(collectType);
  }
  return result;
}
function Schema$1(definition) {
  return this.extend(definition);
}
Schema$1.prototype.extend = function extend(definition) {
  var implicit = [];
  var explicit = [];
  if (definition instanceof type) {
    explicit.push(definition);
  } else if (Array.isArray(definition)) {
    explicit = explicit.concat(definition);
  } else if (definition && (Array.isArray(definition.implicit) || Array.isArray(definition.explicit))) {
    if (definition.implicit) implicit = implicit.concat(definition.implicit);
    if (definition.explicit) explicit = explicit.concat(definition.explicit);
  } else {
    throw new exception('Schema.extend argument should be a Type, [ Type ], ' +
      'or a schema definition ({ implicit: [...], explicit: [...] })');
  }
  implicit.forEach(function (type$1) {
    if (!(type$1 instanceof type)) {
      throw new exception('Specified list of YAML types (or a single Type object) contains a non-Type object.');
    }
    if (type$1.loadKind && type$1.loadKind !== 'scalar') {
      throw new exception('There is a non-scalar type in the implicit list of a schema. Implicit resolving of such types is not supported.');
    }
    if (type$1.multi) {
      throw new exception('There is a multi type in the implicit list of a schema. Multi tags can only be listed as explicit.');
    }
  });
  explicit.forEach(function (type$1) {
    if (!(type$1 instanceof type)) {
      throw new exception('Specified list of YAML types (or a single Type object) contains a non-Type object.');
    }
  });
  var result = Object.create(Schema$1.prototype);
  result.implicit = (this.implicit || []).concat(implicit);
  result.explicit = (this.explicit || []).concat(explicit);
  result.compiledImplicit = compileList(result, 'implicit');
  result.compiledExplicit = compileList(result, 'explicit');
  result.compiledTypeMap  = compileMap(result.compiledImplicit, result.compiledExplicit);
  return result;
};
var schema = Schema$1;
var str = new type('tag:yaml.org,2002:str', {
  kind: 'scalar',
  construct: function (data) { return data !== null ? data : ''; }
});
var seq = new type('tag:yaml.org,2002:seq', {
  kind: 'sequence',
  construct: function (data) { return data !== null ? data : []; }
});
var map = new type('tag:yaml.org,2002:map', {
  kind: 'mapping',
  construct: function (data) { return data !== null ? data : {}; }
});
var failsafe = new schema({
  explicit: [
    str,
    seq,
    map
  ]
});
function resolveYamlNull(data) {
  if (data === null) return true;
  var max = data.length;
  return (max === 1 && data === '~') ||
         (max === 4 && (data === 'null' || data === 'Null' || data === 'NULL'));
}
function constructYamlNull() {
  return null;
}
function isNull(object) {
  return object === null;
}
var _null = new type('tag:yaml.org,2002:null', {
  kind: 'scalar',
  resolve: resolveYamlNull,
  construct: constructYamlNull,
  predicate: isNull,
  represent: {
    canonical: function () { return '~';    },
    lowercase: function () { return 'null'; },
    uppercase: function () { return 'NULL'; },
    camelcase: function () { return 'Null'; },
    empty:     function () { return '';     }
  },
  defaultStyle: 'lowercase'
});
function resolveYamlBoolean(data) {
  if (data === null) return false;
  var max = data.length;
  return (max === 4 && (data === 'true' || data === 'True' || data === 'TRUE')) ||
         (max === 5 && (data === 'false' || data === 'False' || data === 'FALSE'));
}
function constructYamlBoolean(data) {
  return data === 'true' ||
         data === 'True' ||
         data === 'TRUE';
}
function isBoolean(object) {
  return Object.prototype.toString.call(object) === '[object Boolean]';
}
var bool = new type('tag:yaml.org,2002:bool', {
  kind: 'scalar',
  resolve: resolveYamlBoolean,
  construct: constructYamlBoolean,
  predicate: isBoolean,
  represent: {
    lowercase: function (object) { return object ? 'true' : 'false'; },
    uppercase: function (object) { return object ? 'TRUE' : 'FALSE'; },
    camelcase: function (object) { return object ? 'True' : 'False'; }
  },
  defaultStyle: 'lowercase'
});
function isHexCode(c) {
  return ((0x30 <= c) && (c <= 0x39)) ||
         ((0x41 <= c) && (c <= 0x46)) ||
         ((0x61 <= c) && (c <= 0x66));
}
function isOctCode(c) {
  return ((0x30 <= c) && (c <= 0x37));
}
function isDecCode(c) {
  return ((0x30 <= c) && (c <= 0x39));
}
function resolveYamlInteger(data) {
  if (data === null) return false;
  var max = data.length,
      index = 0,
      hasDigits = false,
      ch;
  if (!max) return false;
  ch = data[index];
  if (ch === '-' || ch === '+') {
    ch = data[++index];
  }
  if (ch === '0') {
    if (index + 1 === max) return true;
    ch = data[++index];
    if (ch === 'b') {
      index++;
      for (; index < max; index++) {
        ch = data[index];
        if (ch === '_') continue;
        if (ch !== '0' && ch !== '1') return false;
        hasDigits = true;
      }
      return hasDigits && ch !== '_';
    }
    if (ch === 'x') {
      index++;
      for (; index < max; index++) {
        ch = data[index];
        if (ch === '_') continue;
        if (!isHexCode(data.charCodeAt(index))) return false;
        hasDigits = true;
      }
      return hasDigits && ch !== '_';
    }
    if (ch === 'o') {
      index++;
      for (; index < max; index++) {
        ch = data[index];
        if (ch === '_') continue;
        if (!isOctCode(data.charCodeAt(index))) return false;
        hasDigits = true;
      }
      return hasDigits && ch !== '_';
    }
  }
  if (ch === '_') return false;
  for (; index < max; index++) {
    ch = data[index];
    if (ch === '_') continue;
    if (!isDecCode(data.charCodeAt(index))) {
      return false;
    }
    hasDigits = true;
  }
  if (!hasDigits || ch === '_') return false;
  return true;
}
function constructYamlInteger(data) {
  var value = data, sign = 1, ch;
  if (value.indexOf('_') !== -1) {
    value = value.replace(/_/g, '');
  }
  ch = value[0];
  if (ch === '-' || ch === '+') {
    if (ch === '-') sign = -1;
    value = value.slice(1);
    ch = value[0];
  }
  if (value === '0') return 0;
  if (ch === '0') {
    if (value[1] === 'b') return sign * parseInt(value.slice(2), 2);
    if (value[1] === 'x') return sign * parseInt(value.slice(2), 16);
    if (value[1] === 'o') return sign * parseInt(value.slice(2), 8);
  }
  return sign * parseInt(value, 10);
}
function isInteger(object) {
  return (Object.prototype.toString.call(object)) === '[object Number]' &&
         (object % 1 === 0 && !common.isNegativeZero(object));
}
var int = new type('tag:yaml.org,2002:int', {
  kind: 'scalar',
  resolve: resolveYamlInteger,
  construct: constructYamlInteger,
  predicate: isInteger,
  represent: {
    binary:      function (obj) { return obj >= 0 ? '0b' + obj.toString(2) : '-0b' + obj.toString(2).slice(1); },
    octal:       function (obj) { return obj >= 0 ? '0o'  + obj.toString(8) : '-0o'  + obj.toString(8).slice(1); },
    decimal:     function (obj) { return obj.toString(10); },
    hexadecimal: function (obj) { return obj >= 0 ? '0x' + obj.toString(16).toUpperCase() :  '-0x' + obj.toString(16).toUpperCase().slice(1); }
  },
  defaultStyle: 'decimal',
  styleAliases: {
    binary:      [ 2,  'bin' ],
    octal:       [ 8,  'oct' ],
    decimal:     [ 10, 'dec' ],
    hexadecimal: [ 16, 'hex' ]
  }
});
var YAML_FLOAT_PATTERN = new RegExp(
  '^(?:[-+]?(?:[0-9][0-9_]*)(?:\\.[0-9_]*)?(?:[eE][-+]?[0-9]+)?' +
  '|\\.[0-9_]+(?:[eE][-+]?[0-9]+)?' +
  '|[-+]?\\.(?:inf|Inf|INF)' +
  '|\\.(?:nan|NaN|NAN))$');
function resolveYamlFloat(data) {
  if (data === null) return false;
  if (!YAML_FLOAT_PATTERN.test(data) ||
      data[data.length - 1] === '_') {
    return false;
  }
  return true;
}
function constructYamlFloat(data) {
  var value, sign;
  value  = data.replace(/_/g, '').toLowerCase();
  sign   = value[0] === '-' ? -1 : 1;
  if ('+-'.indexOf(value[0]) >= 0) {
    value = value.slice(1);
  }
  if (value === '.inf') {
    return (sign === 1) ? Number.POSITIVE_INFINITY : Number.NEGATIVE_INFINITY;
  } else if (value === '.nan') {
    return NaN;
  }
  return sign * parseFloat(value, 10);
}
var SCIENTIFIC_WITHOUT_DOT = /^[-+]?[0-9]+e/;
function representYamlFloat(object, style) {
  var res;
  if (isNaN(object)) {
    switch (style) {
      case 'lowercase': return '.nan';
      case 'uppercase': return '.NAN';
      case 'camelcase': return '.NaN';
    }
  } else if (Number.POSITIVE_INFINITY === object) {
    switch (style) {
      case 'lowercase': return '.inf';
      case 'uppercase': return '.INF';
      case 'camelcase': return '.Inf';
    }
  } else if (Number.NEGATIVE_INFINITY === object) {
    switch (style) {
      case 'lowercase': return '-.inf';
      case 'uppercase': return '-.INF';
      case 'camelcase': return '-.Inf';
    }
  } else if (common.isNegativeZero(object)) {
    return '-0.0';
  }
  res = object.toString(10);
  return SCIENTIFIC_WITHOUT_DOT.test(res) ? res.replace('e', '.e') : res;
}
function isFloat(object) {
  return (Object.prototype.toString.call(object) === '[object Number]') &&
         (object % 1 !== 0 || common.isNegativeZero(object));
}
var float = new type('tag:yaml.org,2002:float', {
  kind: 'scalar',
  resolve: resolveYamlFloat,
  construct: constructYamlFloat,
  predicate: isFloat,
  represent: representYamlFloat,
  defaultStyle: 'lowercase'
});
var json = failsafe.extend({
  implicit: [
    _null,
    bool,
    int,
    float
  ]
});
var core = json;
var YAML_DATE_REGEXP = new RegExp(
  '^([0-9][0-9][0-9][0-9])'          +
  '-([0-9][0-9])'                    +
  '-([0-9][0-9])$');
var YAML_TIMESTAMP_REGEXP = new RegExp(
  '^([0-9][0-9][0-9][0-9])'          +
  '-([0-9][0-9]?)'                   +
  '-([0-9][0-9]?)'                   +
  '(?:[Tt]|[ \\t]+)'                 +
  '([0-9][0-9]?)'                    +
  ':([0-9][0-9])'                    +
  ':([0-9][0-9])'                    +
  '(?:\\.([0-9]*))?'                 +
  '(?:[ \\t]*(Z|([-+])([0-9][0-9]?)' +
  '(?::([0-9][0-9]))?))?$');
function resolveYamlTimestamp(data) {
  if (data === null) return false;
  if (YAML_DATE_REGEXP.exec(data) !== null) return true;
  if (YAML_TIMESTAMP_REGEXP.exec(data) !== null) return true;
  return false;
}
function constructYamlTimestamp(data) {
  var match, year, month, day, hour, minute, second, fraction = 0,
      delta = null, tz_hour, tz_minute, date;
  match = YAML_DATE_REGEXP.exec(data);
  if (match === null) match = YAML_TIMESTAMP_REGEXP.exec(data);
  if (match === null) throw new Error('Date resolve error');
  year = +(match[1]);
  month = +(match[2]) - 1;
  day = +(match[3]);
  if (!match[4]) {
    return new Date(Date.UTC(year, month, day));
  }
  hour = +(match[4]);
  minute = +(match[5]);
  second = +(match[6]);
  if (match[7]) {
    fraction = match[7].slice(0, 3);
    while (fraction.length < 3) {
      fraction += '0';
    }
    fraction = +fraction;
  }
  if (match[9]) {
    tz_hour = +(match[10]);
    tz_minute = +(match[11] || 0);
    delta = (tz_hour * 60 + tz_minute) * 60000;
    if (match[9] === '-') delta = -delta;
  }
  date = new Date(Date.UTC(year, month, day, hour, minute, second, fraction));
  if (delta) date.setTime(date.getTime() - delta);
  return date;
}
function representYamlTimestamp(object ) {
  return object.toISOString();
}
var timestamp = new type('tag:yaml.org,2002:timestamp', {
  kind: 'scalar',
  resolve: resolveYamlTimestamp,
  construct: constructYamlTimestamp,
  instanceOf: Date,
  represent: representYamlTimestamp
});
function resolveYamlMerge(data) {
  return data === '<<' || data === null;
}
var merge = new type('tag:yaml.org,2002:merge', {
  kind: 'scalar',
  resolve: resolveYamlMerge
});
var BASE64_MAP = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n\r';
function resolveYamlBinary(data) {
  if (data === null) return false;
  var code, idx, bitlen = 0, max = data.length, map = BASE64_MAP;
  for (idx = 0; idx < max; idx++) {
    code = map.indexOf(data.charAt(idx));
    if (code > 64) continue;
    if (code < 0) return false;
    bitlen += 6;
  }
  return (bitlen % 8) === 0;
}
function constructYamlBinary(data) {
  var idx, tailbits,
      input = data.replace(/[\r\n=]/g, ''),
      max = input.length,
      map = BASE64_MAP,
      bits = 0,
      result = [];
  for (idx = 0; idx < max; idx++) {
    if ((idx % 4 === 0) && idx) {
      result.push((bits >> 16) & 0xFF);
      result.push((bits >> 8) & 0xFF);
      result.push(bits & 0xFF);
    }
    bits = (bits << 6) | map.indexOf(input.charAt(idx));
  }
  tailbits = (max % 4) * 6;
  if (tailbits === 0) {
    result.push((bits >> 16) & 0xFF);
    result.push((bits >> 8) & 0xFF);
    result.push(bits & 0xFF);
  } else if (tailbits === 18) {
    result.push((bits >> 10) & 0xFF);
    result.push((bits >> 2) & 0xFF);
  } else if (tailbits === 12) {
    result.push((bits >> 4) & 0xFF);
  }
  return new Uint8Array(result);
}
function representYamlBinary(object ) {
  var result = '', bits = 0, idx, tail,
      max = object.length,
      map = BASE64_MAP;
  for (idx = 0; idx < max; idx++) {
    if ((idx % 3 === 0) && idx) {
      result += map[(bits >> 18) & 0x3F];
      result += map[(bits >> 12) & 0x3F];
      result += map[(bits >> 6) & 0x3F];
      result += map[bits & 0x3F];
    }
    bits = (bits << 8) + object[idx];
  }
  tail = max % 3;
  if (tail === 0) {
    result += map[(bits >> 18) & 0x3F];
    result += map[(bits >> 12) & 0x3F];
    result += map[(bits >> 6) & 0x3F];
    result += map[bits & 0x3F];
  } else if (tail === 2) {
    result += map[(bits >> 10) & 0x3F];
    result += map[(bits >> 4) & 0x3F];
    result += map[(bits << 2) & 0x3F];
    result += map[64];
  } else if (tail === 1) {
    result += map[(bits >> 2) & 0x3F];
    result += map[(bits << 4) & 0x3F];
    result += map[64];
    result += map[64];
  }
  return result;
}
function isBinary(obj) {
  return Object.prototype.toString.call(obj) ===  '[object Uint8Array]';
}
var binary = new type('tag:yaml.org,2002:binary', {
  kind: 'scalar',
  resolve: resolveYamlBinary,
  construct: constructYamlBinary,
  predicate: isBinary,
  represent: representYamlBinary
});
var _hasOwnProperty$3 = Object.prototype.hasOwnProperty;
var _toString$2       = Object.prototype.toString;
function resolveYamlOmap(data) {
  if (data === null) return true;
  var objectKeys = [], index, length, pair, pairKey, pairHasKey,
      object = data;
  for (index = 0, length = object.length; index < length; index += 1) {
    pair = object[index];
    pairHasKey = false;
    if (_toString$2.call(pair) !== '[object Object]') return false;
    for (pairKey in pair) {
      if (_hasOwnProperty$3.call(pair, pairKey)) {
        if (!pairHasKey) pairHasKey = true;
        else return false;
      }
    }
    if (!pairHasKey) return false;
    if (objectKeys.indexOf(pairKey) === -1) objectKeys.push(pairKey);
    else return false;
  }
  return true;
}
function constructYamlOmap(data) {
  return data !== null ? data : [];
}
var omap = new type('tag:yaml.org,2002:omap', {
  kind: 'sequence',
  resolve: resolveYamlOmap,
  construct: constructYamlOmap
});
var _toString$1 = Object.prototype.toString;
function resolveYamlPairs(data) {
  if (data === null) return true;
  var index, length, pair, keys, result,
      object = data;
  result = new Array(object.length);
  for (index = 0, length = object.length; index < length; index += 1) {
    pair = object[index];
    if (_toString$1.call(pair) !== '[object Object]') return false;
    keys = Object.keys(pair);
    if (keys.length !== 1) return false;
    result[index] = [ keys[0], pair[keys[0]] ];
  }
  return true;
}
function constructYamlPairs(data) {
  if (data === null) return [];
  var index, length, pair, keys, result,
      object = data;
  result = new Array(object.length);
  for (index = 0, length = object.length; index < length; index += 1) {
    pair = object[index];
    keys = Object.keys(pair);
    result[index] = [ keys[0], pair[keys[0]] ];
  }
  return result;
}
var pairs = new type('tag:yaml.org,2002:pairs', {
  kind: 'sequence',
  resolve: resolveYamlPairs,
  construct: constructYamlPairs
});
var _hasOwnProperty$2 = Object.prototype.hasOwnProperty;
function resolveYamlSet(data) {
  if (data === null) return true;
  var key, object = data;
  for (key in object) {
    if (_hasOwnProperty$2.call(object, key)) {
      if (object[key] !== null) return false;
    }
  }
  return true;
}
function constructYamlSet(data) {
  return data !== null ? data : {};
}
var set = new type('tag:yaml.org,2002:set', {
  kind: 'mapping',
  resolve: resolveYamlSet,
  construct: constructYamlSet
});
var _default = core.extend({
  implicit: [
    timestamp,
    merge
  ],
  explicit: [
    binary,
    omap,
    pairs,
    set
  ]
});
var _hasOwnProperty$1 = Object.prototype.hasOwnProperty;
var CONTEXT_FLOW_IN   = 1;
var CONTEXT_FLOW_OUT  = 2;
var CONTEXT_BLOCK_IN  = 3;
var CONTEXT_BLOCK_OUT = 4;
var CHOMPING_CLIP  = 1;
var CHOMPING_STRIP = 2;
var CHOMPING_KEEP  = 3;
var PATTERN_NON_PRINTABLE         = /[\x00-\x08\x0B\x0C\x0E-\x1F\x7F-\x84\x86-\x9F\uFFFE\uFFFF]|[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?:[^\uD800-\uDBFF]|^)[\uDC00-\uDFFF]/;
var PATTERN_NON_ASCII_LINE_BREAKS = /[\x85\u2028\u2029]/;
var PATTERN_FLOW_INDICATORS       = /[,\[\]\{\}]/;
var PATTERN_TAG_HANDLE            = /^(?:!|!!|![a-z\-]+!)$/i;
var PATTERN_TAG_URI               = /^(?:!|[^,\[\]\{\}])(?:%[0-9a-f]{2}|[0-9a-z\-#;\/\?:@&=\+\$,_\.!~\*'\(\)\[\]])*$/i;
function _class(obj) { return Object.prototype.toString.call(obj); }
function is_EOL(c) {
  return (c === 0x0A) || (c === 0x0D);
}
function is_WHITE_SPACE(c) {
  return (c === 0x09) || (c === 0x20);
}
function is_WS_OR_EOL(c) {
  return (c === 0x09) ||
         (c === 0x20) ||
         (c === 0x0A) ||
         (c === 0x0D);
}
function is_FLOW_INDICATOR(c) {
  return c === 0x2C ||
         c === 0x5B ||
         c === 0x5D ||
         c === 0x7B ||
         c === 0x7D;
}
function fromHexCode(c) {
  var lc;
  if ((0x30 <= c) && (c <= 0x39)) {
    return c - 0x30;
  }
  lc = c | 0x20;
  if ((0x61 <= lc) && (lc <= 0x66)) {
    return lc - 0x61 + 10;
  }
  return -1;
}
function escapedHexLen(c) {
  if (c === 0x78) { return 2; }
  if (c === 0x75) { return 4; }
  if (c === 0x55) { return 8; }
  return 0;
}
function fromDecimalCode(c) {
  if ((0x30 <= c) && (c <= 0x39)) {
    return c - 0x30;
  }
  return -1;
}
function simpleEscapeSequence(c) {
  return (c === 0x30) ? '\x00' :
        (c === 0x61) ? '\x07' :
        (c === 0x62) ? '\x08' :
        (c === 0x74) ? '\x09' :
        (c === 0x09) ? '\x09' :
        (c === 0x6E) ? '\x0A' :
        (c === 0x76) ? '\x0B' :
        (c === 0x66) ? '\x0C' :
        (c === 0x72) ? '\x0D' :
        (c === 0x65) ? '\x1B' :
        (c === 0x20) ? ' ' :
        (c === 0x22) ? '\x22' :
        (c === 0x2F) ? '/' :
        (c === 0x5C) ? '\x5C' :
        (c === 0x4E) ? '\x85' :
        (c === 0x5F) ? '\xA0' :
        (c === 0x4C) ? '\u2028' :
        (c === 0x50) ? '\u2029' : '';
}
function charFromCodepoint(c) {
  if (c <= 0xFFFF) {
    return String.fromCharCode(c);
  }
  return String.fromCharCode(
    ((c - 0x010000) >> 10) + 0xD800,
    ((c - 0x010000) & 0x03FF) + 0xDC00
  );
}
var simpleEscapeCheck = new Array(256);
var simpleEscapeMap = new Array(256);
for (var i = 0; i < 256; i++) {
  simpleEscapeCheck[i] = simpleEscapeSequence(i) ? 1 : 0;
  simpleEscapeMap[i] = simpleEscapeSequence(i);
}
function State$1(input, options) {
  this.input = input;
  this.filename  = options['filename']  || null;
  this.schema    = options['schema']    || _default;
  this.onWarning = options['onWarning'] || null;
  this.legacy    = options['legacy']    || false;
  this.json      = options['json']      || false;
  this.listener  = options['listener']  || null;
  this.implicitTypes = this.schema.compiledImplicit;
  this.typeMap       = this.schema.compiledTypeMap;
  this.length     = input.length;
  this.position   = 0;
  this.line       = 0;
  this.lineStart  = 0;
  this.lineIndent = 0;
  this.firstTabInLine = -1;
  this.documents = [];
}
function generateError(state, message) {
  var mark = {
    name:     state.filename,
    buffer:   state.input.slice(0, -1),
    position: state.position,
    line:     state.line,
    column:   state.position - state.lineStart
  };
  mark.snippet = snippet(mark);
  return new exception(message, mark);
}
function throwError(state, message) {
  throw generateError(state, message);
}
function throwWarning(state, message) {
  if (state.onWarning) {
    state.onWarning.call(null, generateError(state, message));
  }
}
var directiveHandlers = {
  YAML: function handleYamlDirective(state, name, args) {
    var match, major, minor;
    if (state.version !== null) {
      throwError(state, 'duplication of %YAML directive');
    }
    if (args.length !== 1) {
      throwError(state, 'YAML directive accepts exactly one argument');
    }
    match = /^([0-9]+)\.([0-9]+)$/.exec(args[0]);
    if (match === null) {
      throwError(state, 'ill-formed argument of the YAML directive');
    }
    major = parseInt(match[1], 10);
    minor = parseInt(match[2], 10);
    if (major !== 1) {
      throwError(state, 'unacceptable YAML version of the document');
    }
    state.version = args[0];
    state.checkLineBreaks = (minor < 2);
    if (minor !== 1 && minor !== 2) {
      throwWarning(state, 'unsupported YAML version of the document');
    }
  },
  TAG: function handleTagDirective(state, name, args) {
    var handle, prefix;
    if (args.length !== 2) {
      throwError(state, 'TAG directive accepts exactly two arguments');
    }
    handle = args[0];
    prefix = args[1];
    if (!PATTERN_TAG_HANDLE.test(handle)) {
      throwError(state, 'ill-formed tag handle (first argument) of the TAG directive');
    }
    if (_hasOwnProperty$1.call(state.tagMap, handle)) {
      throwError(state, 'there is a previously declared suffix for "' + handle + '" tag handle');
    }
    if (!PATTERN_TAG_URI.test(prefix)) {
      throwError(state, 'ill-formed tag prefix (second argument) of the TAG directive');
    }
    try {
      prefix = decodeURIComponent(prefix);
    } catch (err) {
      throwError(state, 'tag prefix is malformed: ' + prefix);
    }
    state.tagMap[handle] = prefix;
  }
};
function captureSegment(state, start, end, checkJson) {
  var _position, _length, _character, _result;
  if (start < end) {
    _result = state.input.slice(start, end);
    if (checkJson) {
      for (_position = 0, _length = _result.length; _position < _length; _position += 1) {
        _character = _result.charCodeAt(_position);
        if (!(_character === 0x09 ||
              (0x20 <= _character && _character <= 0x10FFFF))) {
          throwError(state, 'expected valid JSON character');
        }
      }
    } else if (PATTERN_NON_PRINTABLE.test(_result)) {
      throwError(state, 'the stream contains non-printable characters');
    }
    state.result += _result;
  }
}
function mergeMappings(state, destination, source, overridableKeys) {
  var sourceKeys, key, index, quantity;
  if (!common.isObject(source)) {
    throwError(state, 'cannot merge mappings; the provided source object is unacceptable');
  }
  sourceKeys = Object.keys(source);
  for (index = 0, quantity = sourceKeys.length; index < quantity; index += 1) {
    key = sourceKeys[index];
    if (!_hasOwnProperty$1.call(destination, key)) {
      destination[key] = source[key];
      overridableKeys[key] = true;
    }
  }
}
function storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, valueNode,
  startLine, startLineStart, startPos) {
  var index, quantity;
  if (Array.isArray(keyNode)) {
    keyNode = Array.prototype.slice.call(keyNode);
    for (index = 0, quantity = keyNode.length; index < quantity; index += 1) {
      if (Array.isArray(keyNode[index])) {
        throwError(state, 'nested arrays are not supported inside keys');
      }
      if (typeof keyNode === 'object' && _class(keyNode[index]) === '[object Object]') {
        keyNode[index] = '[object Object]';
      }
    }
  }
  if (typeof keyNode === 'object' && _class(keyNode) === '[object Object]') {
    keyNode = '[object Object]';
  }
  keyNode = String(keyNode);
  if (_result === null) {
    _result = {};
  }
  if (keyTag === 'tag:yaml.org,2002:merge') {
    if (Array.isArray(valueNode)) {
      for (index = 0, quantity = valueNode.length; index < quantity; index += 1) {
        mergeMappings(state, _result, valueNode[index], overridableKeys);
      }
    } else {
      mergeMappings(state, _result, valueNode, overridableKeys);
    }
  } else {
    if (!state.json &&
        !_hasOwnProperty$1.call(overridableKeys, keyNode) &&
        _hasOwnProperty$1.call(_result, keyNode)) {
      state.line = startLine || state.line;
      state.lineStart = startLineStart || state.lineStart;
      state.position = startPos || state.position;
      throwError(state, 'duplicated mapping key');
    }
    if (keyNode === '__proto__') {
      Object.defineProperty(_result, keyNode, {
        configurable: true,
        enumerable: true,
        writable: true,
        value: valueNode
      });
    } else {
      _result[keyNode] = valueNode;
    }
    delete overridableKeys[keyNode];
  }
  return _result;
}
function readLineBreak(state) {
  var ch;
  ch = state.input.charCodeAt(state.position);
  if (ch === 0x0A) {
    state.position++;
  } else if (ch === 0x0D) {
    state.position++;
    if (state.input.charCodeAt(state.position) === 0x0A) {
      state.position++;
    }
  } else {
    throwError(state, 'a line break is expected');
  }
  state.line += 1;
  state.lineStart = state.position;
  state.firstTabInLine = -1;
}
function skipSeparationSpace(state, allowComments, checkIndent) {
  var lineBreaks = 0,
      ch = state.input.charCodeAt(state.position);
  while (ch !== 0) {
    while (is_WHITE_SPACE(ch)) {
      if (ch === 0x09 && state.firstTabInLine === -1) {
        state.firstTabInLine = state.position;
      }
      ch = state.input.charCodeAt(++state.position);
    }
    if (allowComments && ch === 0x23) {
      do {
        ch = state.input.charCodeAt(++state.position);
      } while (ch !== 0x0A && ch !== 0x0D && ch !== 0);
    }
    if (is_EOL(ch)) {
      readLineBreak(state);
      ch = state.input.charCodeAt(state.position);
      lineBreaks++;
      state.lineIndent = 0;
      while (ch === 0x20) {
        state.lineIndent++;
        ch = state.input.charCodeAt(++state.position);
      }
    } else {
      break;
    }
  }
  if (checkIndent !== -1 && lineBreaks !== 0 && state.lineIndent < checkIndent) {
    throwWarning(state, 'deficient indentation');
  }
  return lineBreaks;
}
function testDocumentSeparator(state) {
  var _position = state.position,
      ch;
  ch = state.input.charCodeAt(_position);
  if ((ch === 0x2D || ch === 0x2E) &&
      ch === state.input.charCodeAt(_position + 1) &&
      ch === state.input.charCodeAt(_position + 2)) {
    _position += 3;
    ch = state.input.charCodeAt(_position);
    if (ch === 0 || is_WS_OR_EOL(ch)) {
      return true;
    }
  }
  return false;
}
function writeFoldedLines(state, count) {
  if (count === 1) {
    state.result += ' ';
  } else if (count > 1) {
    state.result += common.repeat('\n', count - 1);
  }
}
function readPlainScalar(state, nodeIndent, withinFlowCollection) {
  var preceding,
      following,
      captureStart,
      captureEnd,
      hasPendingContent,
      _line,
      _lineStart,
      _lineIndent,
      _kind = state.kind,
      _result = state.result,
      ch;
  ch = state.input.charCodeAt(state.position);
  if (is_WS_OR_EOL(ch)      ||
      is_FLOW_INDICATOR(ch) ||
      ch === 0x23    ||
      ch === 0x26    ||
      ch === 0x2A    ||
      ch === 0x21    ||
      ch === 0x7C    ||
      ch === 0x3E    ||
      ch === 0x27    ||
      ch === 0x22    ||
      ch === 0x25    ||
      ch === 0x40    ||
      ch === 0x60) {
    return false;
  }
  if (ch === 0x3F || ch === 0x2D) {
    following = state.input.charCodeAt(state.position + 1);
    if (is_WS_OR_EOL(following) ||
        withinFlowCollection && is_FLOW_INDICATOR(following)) {
      return false;
    }
  }
  state.kind = 'scalar';
  state.result = '';
  captureStart = captureEnd = state.position;
  hasPendingContent = false;
  while (ch !== 0) {
    if (ch === 0x3A) {
      following = state.input.charCodeAt(state.position + 1);
      if (is_WS_OR_EOL(following) ||
          withinFlowCollection && is_FLOW_INDICATOR(following)) {
        break;
      }
    } else if (ch === 0x23) {
      preceding = state.input.charCodeAt(state.position - 1);
      if (is_WS_OR_EOL(preceding)) {
        break;
      }
    } else if ((state.position === state.lineStart && testDocumentSeparator(state)) ||
               withinFlowCollection && is_FLOW_INDICATOR(ch)) {
      break;
    } else if (is_EOL(ch)) {
      _line = state.line;
      _lineStart = state.lineStart;
      _lineIndent = state.lineIndent;
      skipSeparationSpace(state, false, -1);
      if (state.lineIndent >= nodeIndent) {
        hasPendingContent = true;
        ch = state.input.charCodeAt(state.position);
        continue;
      } else {
        state.position = captureEnd;
        state.line = _line;
        state.lineStart = _lineStart;
        state.lineIndent = _lineIndent;
        break;
      }
    }
    if (hasPendingContent) {
      captureSegment(state, captureStart, captureEnd, false);
      writeFoldedLines(state, state.line - _line);
      captureStart = captureEnd = state.position;
      hasPendingContent = false;
    }
    if (!is_WHITE_SPACE(ch)) {
      captureEnd = state.position + 1;
    }
    ch = state.input.charCodeAt(++state.position);
  }
  captureSegment(state, captureStart, captureEnd, false);
  if (state.result) {
    return true;
  }
  state.kind = _kind;
  state.result = _result;
  return false;
}
function readSingleQuotedScalar(state, nodeIndent) {
  var ch,
      captureStart, captureEnd;
  ch = state.input.charCodeAt(state.position);
  if (ch !== 0x27) {
    return false;
  }
  state.kind = 'scalar';
  state.result = '';
  state.position++;
  captureStart = captureEnd = state.position;
  while ((ch = state.input.charCodeAt(state.position)) !== 0) {
    if (ch === 0x27) {
      captureSegment(state, captureStart, state.position, true);
      ch = state.input.charCodeAt(++state.position);
      if (ch === 0x27) {
        captureStart = state.position;
        state.position++;
        captureEnd = state.position;
      } else {
        return true;
      }
    } else if (is_EOL(ch)) {
      captureSegment(state, captureStart, captureEnd, true);
      writeFoldedLines(state, skipSeparationSpace(state, false, nodeIndent));
      captureStart = captureEnd = state.position;
    } else if (state.position === state.lineStart && testDocumentSeparator(state)) {
      throwError(state, 'unexpected end of the document within a single quoted scalar');
    } else {
      state.position++;
      captureEnd = state.position;
    }
  }
  throwError(state, 'unexpected end of the stream within a single quoted scalar');
}
function readDoubleQuotedScalar(state, nodeIndent) {
  var captureStart,
      captureEnd,
      hexLength,
      hexResult,
      tmp,
      ch;
  ch = state.input.charCodeAt(state.position);
  if (ch !== 0x22) {
    return false;
  }
  state.kind = 'scalar';
  state.result = '';
  state.position++;
  captureStart = captureEnd = state.position;
  while ((ch = state.input.charCodeAt(state.position)) !== 0) {
    if (ch === 0x22) {
      captureSegment(state, captureStart, state.position, true);
      state.position++;
      return true;
    } else if (ch === 0x5C) {
      captureSegment(state, captureStart, state.position, true);
      ch = state.input.charCodeAt(++state.position);
      if (is_EOL(ch)) {
        skipSeparationSpace(state, false, nodeIndent);
      } else if (ch < 256 && simpleEscapeCheck[ch]) {
        state.result += simpleEscapeMap[ch];
        state.position++;
      } else if ((tmp = escapedHexLen(ch)) > 0) {
        hexLength = tmp;
        hexResult = 0;
        for (; hexLength > 0; hexLength--) {
          ch = state.input.charCodeAt(++state.position);
          if ((tmp = fromHexCode(ch)) >= 0) {
            hexResult = (hexResult << 4) + tmp;
          } else {
            throwError(state, 'expected hexadecimal character');
          }
        }
        state.result += charFromCodepoint(hexResult);
        state.position++;
      } else {
        throwError(state, 'unknown escape sequence');
      }
      captureStart = captureEnd = state.position;
    } else if (is_EOL(ch)) {
      captureSegment(state, captureStart, captureEnd, true);
      writeFoldedLines(state, skipSeparationSpace(state, false, nodeIndent));
      captureStart = captureEnd = state.position;
    } else if (state.position === state.lineStart && testDocumentSeparator(state)) {
      throwError(state, 'unexpected end of the document within a double quoted scalar');
    } else {
      state.position++;
      captureEnd = state.position;
    }
  }
  throwError(state, 'unexpected end of the stream within a double quoted scalar');
}
function readFlowCollection(state, nodeIndent) {
  var readNext = true,
      _line,
      _lineStart,
      _pos,
      _tag     = state.tag,
      _result,
      _anchor  = state.anchor,
      following,
      terminator,
      isPair,
      isExplicitPair,
      isMapping,
      overridableKeys = Object.create(null),
      keyNode,
      keyTag,
      valueNode,
      ch;
  ch = state.input.charCodeAt(state.position);
  if (ch === 0x5B) {
    terminator = 0x5D;
    isMapping = false;
    _result = [];
  } else if (ch === 0x7B) {
    terminator = 0x7D;
    isMapping = true;
    _result = {};
  } else {
    return false;
  }
  if (state.anchor !== null) {
    state.anchorMap[state.anchor] = _result;
  }
  ch = state.input.charCodeAt(++state.position);
  while (ch !== 0) {
    skipSeparationSpace(state, true, nodeIndent);
    ch = state.input.charCodeAt(state.position);
    if (ch === terminator) {
      state.position++;
      state.tag = _tag;
      state.anchor = _anchor;
      state.kind = isMapping ? 'mapping' : 'sequence';
      state.result = _result;
      return true;
    } else if (!readNext) {
      throwError(state, 'missed comma between flow collection entries');
    } else if (ch === 0x2C) {
      throwError(state, "expected the node content, but found ','");
    }
    keyTag = keyNode = valueNode = null;
    isPair = isExplicitPair = false;
    if (ch === 0x3F) {
      following = state.input.charCodeAt(state.position + 1);
      if (is_WS_OR_EOL(following)) {
        isPair = isExplicitPair = true;
        state.position++;
        skipSeparationSpace(state, true, nodeIndent);
      }
    }
    _line = state.line;
    _lineStart = state.lineStart;
    _pos = state.position;
    composeNode(state, nodeIndent, CONTEXT_FLOW_IN, false, true);
    keyTag = state.tag;
    keyNode = state.result;
    skipSeparationSpace(state, true, nodeIndent);
    ch = state.input.charCodeAt(state.position);
    if ((isExplicitPair || state.line === _line) && ch === 0x3A) {
      isPair = true;
      ch = state.input.charCodeAt(++state.position);
      skipSeparationSpace(state, true, nodeIndent);
      composeNode(state, nodeIndent, CONTEXT_FLOW_IN, false, true);
      valueNode = state.result;
    }
    if (isMapping) {
      storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, valueNode, _line, _lineStart, _pos);
    } else if (isPair) {
      _result.push(storeMappingPair(state, null, overridableKeys, keyTag, keyNode, valueNode, _line, _lineStart, _pos));
    } else {
      _result.push(keyNode);
    }
    skipSeparationSpace(state, true, nodeIndent);
    ch = state.input.charCodeAt(state.position);
    if (ch === 0x2C) {
      readNext = true;
      ch = state.input.charCodeAt(++state.position);
    } else {
      readNext = false;
    }
  }
  throwError(state, 'unexpected end of the stream within a flow collection');
}
function readBlockScalar(state, nodeIndent) {
  var captureStart,
      folding,
      chomping       = CHOMPING_CLIP,
      didReadContent = false,
      detectedIndent = false,
      textIndent     = nodeIndent,
      emptyLines     = 0,
      atMoreIndented = false,
      tmp,
      ch;
  ch = state.input.charCodeAt(state.position);
  if (ch === 0x7C) {
    folding = false;
  } else if (ch === 0x3E) {
    folding = true;
  } else {
    return false;
  }
  state.kind = 'scalar';
  state.result = '';
  while (ch !== 0) {
    ch = state.input.charCodeAt(++state.position);
    if (ch === 0x2B || ch === 0x2D) {
      if (CHOMPING_CLIP === chomping) {
        chomping = (ch === 0x2B) ? CHOMPING_KEEP : CHOMPING_STRIP;
      } else {
        throwError(state, 'repeat of a chomping mode identifier');
      }
    } else if ((tmp = fromDecimalCode(ch)) >= 0) {
      if (tmp === 0) {
        throwError(state, 'bad explicit indentation width of a block scalar; it cannot be less than one');
      } else if (!detectedIndent) {
        textIndent = nodeIndent + tmp - 1;
        detectedIndent = true;
      } else {
        throwError(state, 'repeat of an indentation width identifier');
      }
    } else {
      break;
    }
  }
  if (is_WHITE_SPACE(ch)) {
    do { ch = state.input.charCodeAt(++state.position); }
    while (is_WHITE_SPACE(ch));
    if (ch === 0x23) {
      do { ch = state.input.charCodeAt(++state.position); }
      while (!is_EOL(ch) && (ch !== 0));
    }
  }
  while (ch !== 0) {
    readLineBreak(state);
    state.lineIndent = 0;
    ch = state.input.charCodeAt(state.position);
    while ((!detectedIndent || state.lineIndent < textIndent) &&
           (ch === 0x20)) {
      state.lineIndent++;
      ch = state.input.charCodeAt(++state.position);
    }
    if (!detectedIndent && state.lineIndent > textIndent) {
      textIndent = state.lineIndent;
    }
    if (is_EOL(ch)) {
      emptyLines++;
      continue;
    }
    if (state.lineIndent < textIndent) {
      if (chomping === CHOMPING_KEEP) {
        state.result += common.repeat('\n', didReadContent ? 1 + emptyLines : emptyLines);
      } else if (chomping === CHOMPING_CLIP) {
        if (didReadContent) {
          state.result += '\n';
        }
      }
      break;
    }
    if (folding) {
      if (is_WHITE_SPACE(ch)) {
        atMoreIndented = true;
        state.result += common.repeat('\n', didReadContent ? 1 + emptyLines : emptyLines);
      } else if (atMoreIndented) {
        atMoreIndented = false;
        state.result += common.repeat('\n', emptyLines + 1);
      } else if (emptyLines === 0) {
        if (didReadContent) {
          state.result += ' ';
        }
      } else {
        state.result += common.repeat('\n', emptyLines);
      }
    } else {
      state.result += common.repeat('\n', didReadContent ? 1 + emptyLines : emptyLines);
    }
    didReadContent = true;
    detectedIndent = true;
    emptyLines = 0;
    captureStart = state.position;
    while (!is_EOL(ch) && (ch !== 0)) {
      ch = state.input.charCodeAt(++state.position);
    }
    captureSegment(state, captureStart, state.position, false);
  }
  return true;
}
function readBlockSequence(state, nodeIndent) {
  var _line,
      _tag      = state.tag,
      _anchor   = state.anchor,
      _result   = [],
      following,
      detected  = false,
      ch;
  if (state.firstTabInLine !== -1) return false;
  if (state.anchor !== null) {
    state.anchorMap[state.anchor] = _result;
  }
  ch = state.input.charCodeAt(state.position);
  while (ch !== 0) {
    if (state.firstTabInLine !== -1) {
      state.position = state.firstTabInLine;
      throwError(state, 'tab characters must not be used in indentation');
    }
    if (ch !== 0x2D) {
      break;
    }
    following = state.input.charCodeAt(state.position + 1);
    if (!is_WS_OR_EOL(following)) {
      break;
    }
    detected = true;
    state.position++;
    if (skipSeparationSpace(state, true, -1)) {
      if (state.lineIndent <= nodeIndent) {
        _result.push(null);
        ch = state.input.charCodeAt(state.position);
        continue;
      }
    }
    _line = state.line;
    composeNode(state, nodeIndent, CONTEXT_BLOCK_IN, false, true);
    _result.push(state.result);
    skipSeparationSpace(state, true, -1);
    ch = state.input.charCodeAt(state.position);
    if ((state.line === _line || state.lineIndent > nodeIndent) && (ch !== 0)) {
      throwError(state, 'bad indentation of a sequence entry');
    } else if (state.lineIndent < nodeIndent) {
      break;
    }
  }
  if (detected) {
    state.tag = _tag;
    state.anchor = _anchor;
    state.kind = 'sequence';
    state.result = _result;
    return true;
  }
  return false;
}
function readBlockMapping(state, nodeIndent, flowIndent) {
  var following,
      allowCompact,
      _line,
      _keyLine,
      _keyLineStart,
      _keyPos,
      _tag          = state.tag,
      _anchor       = state.anchor,
      _result       = {},
      overridableKeys = Object.create(null),
      keyTag        = null,
      keyNode       = null,
      valueNode     = null,
      atExplicitKey = false,
      detected      = false,
      ch;
  if (state.firstTabInLine !== -1) return false;
  if (state.anchor !== null) {
    state.anchorMap[state.anchor] = _result;
  }
  ch = state.input.charCodeAt(state.position);
  while (ch !== 0) {
    if (!atExplicitKey && state.firstTabInLine !== -1) {
      state.position = state.firstTabInLine;
      throwError(state, 'tab characters must not be used in indentation');
    }
    following = state.input.charCodeAt(state.position + 1);
    _line = state.line;
    if ((ch === 0x3F || ch === 0x3A) && is_WS_OR_EOL(following)) {
      if (ch === 0x3F) {
        if (atExplicitKey) {
          storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, null, _keyLine, _keyLineStart, _keyPos);
          keyTag = keyNode = valueNode = null;
        }
        detected = true;
        atExplicitKey = true;
        allowCompact = true;
      } else if (atExplicitKey) {
        atExplicitKey = false;
        allowCompact = true;
      } else {
        throwError(state, 'incomplete explicit mapping pair; a key node is missed; or followed by a non-tabulated empty line');
      }
      state.position += 1;
      ch = following;
    } else {
      _keyLine = state.line;
      _keyLineStart = state.lineStart;
      _keyPos = state.position;
      if (!composeNode(state, flowIndent, CONTEXT_FLOW_OUT, false, true)) {
        break;
      }
      if (state.line === _line) {
        ch = state.input.charCodeAt(state.position);
        while (is_WHITE_SPACE(ch)) {
          ch = state.input.charCodeAt(++state.position);
        }
        if (ch === 0x3A) {
          ch = state.input.charCodeAt(++state.position);
          if (!is_WS_OR_EOL(ch)) {
            throwError(state, 'a whitespace character is expected after the key-value separator within a block mapping');
          }
          if (atExplicitKey) {
            storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, null, _keyLine, _keyLineStart, _keyPos);
            keyTag = keyNode = valueNode = null;
          }
          detected = true;
          atExplicitKey = false;
          allowCompact = false;
          keyTag = state.tag;
          keyNode = state.result;
        } else if (detected) {
          throwError(state, 'can not read an implicit mapping pair; a colon is missed');
        } else {
          state.tag = _tag;
          state.anchor = _anchor;
          return true;
        }
      } else if (detected) {
        throwError(state, 'can not read a block mapping entry; a multiline key may not be an implicit key');
      } else {
        state.tag = _tag;
        state.anchor = _anchor;
        return true;
      }
    }
    if (state.line === _line || state.lineIndent > nodeIndent) {
      if (atExplicitKey) {
        _keyLine = state.line;
        _keyLineStart = state.lineStart;
        _keyPos = state.position;
      }
      if (composeNode(state, nodeIndent, CONTEXT_BLOCK_OUT, true, allowCompact)) {
        if (atExplicitKey) {
          keyNode = state.result;
        } else {
          valueNode = state.result;
        }
      }
      if (!atExplicitKey) {
        storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, valueNode, _keyLine, _keyLineStart, _keyPos);
        keyTag = keyNode = valueNode = null;
      }
      skipSeparationSpace(state, true, -1);
      ch = state.input.charCodeAt(state.position);
    }
    if ((state.line === _line || state.lineIndent > nodeIndent) && (ch !== 0)) {
      throwError(state, 'bad indentation of a mapping entry');
    } else if (state.lineIndent < nodeIndent) {
      break;
    }
  }
  if (atExplicitKey) {
    storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, null, _keyLine, _keyLineStart, _keyPos);
  }
  if (detected) {
    state.tag = _tag;
    state.anchor = _anchor;
    state.kind = 'mapping';
    state.result = _result;
  }
  return detected;
}
function readTagProperty(state) {
  var _position,
      isVerbatim = false,
      isNamed    = false,
      tagHandle,
      tagName,
      ch;
  ch = state.input.charCodeAt(state.position);
  if (ch !== 0x21) return false;
  if (state.tag !== null) {
    throwError(state, 'duplication of a tag property');
  }
  ch = state.input.charCodeAt(++state.position);
  if (ch === 0x3C) {
    isVerbatim = true;
    ch = state.input.charCodeAt(++state.position);
  } else if (ch === 0x21) {
    isNamed = true;
    tagHandle = '!!';
    ch = state.input.charCodeAt(++state.position);
  } else {
    tagHandle = '!';
  }
  _position = state.position;
  if (isVerbatim) {
    do { ch = state.input.charCodeAt(++state.position); }
    while (ch !== 0 && ch !== 0x3E);
    if (state.position < state.length) {
      tagName = state.input.slice(_position, state.position);
      ch = state.input.charCodeAt(++state.position);
    } else {
      throwError(state, 'unexpected end of the stream within a verbatim tag');
    }
  } else {
    while (ch !== 0 && !is_WS_OR_EOL(ch)) {
      if (ch === 0x21) {
        if (!isNamed) {
          tagHandle = state.input.slice(_position - 1, state.position + 1);
          if (!PATTERN_TAG_HANDLE.test(tagHandle)) {
            throwError(state, 'named tag handle cannot contain such characters');
          }
          isNamed = true;
          _position = state.position + 1;
        } else {
          throwError(state, 'tag suffix cannot contain exclamation marks');
        }
      }
      ch = state.input.charCodeAt(++state.position);
    }
    tagName = state.input.slice(_position, state.position);
    if (PATTERN_FLOW_INDICATORS.test(tagName)) {
      throwError(state, 'tag suffix cannot contain flow indicator characters');
    }
  }
  if (tagName && !PATTERN_TAG_URI.test(tagName)) {
    throwError(state, 'tag name cannot contain such characters: ' + tagName);
  }
  try {
    tagName = decodeURIComponent(tagName);
  } catch (err) {
    throwError(state, 'tag name is malformed: ' + tagName);
  }
  if (isVerbatim) {
    state.tag = tagName;
  } else if (_hasOwnProperty$1.call(state.tagMap, tagHandle)) {
    state.tag = state.tagMap[tagHandle] + tagName;
  } else if (tagHandle === '!') {
    state.tag = '!' + tagName;
  } else if (tagHandle === '!!') {
    state.tag = 'tag:yaml.org,2002:' + tagName;
  } else {
    throwError(state, 'undeclared tag handle "' + tagHandle + '"');
  }
  return true;
}
function readAnchorProperty(state) {
  var _position,
      ch;
  ch = state.input.charCodeAt(state.position);
  if (ch !== 0x26) return false;
  if (state.anchor !== null) {
    throwError(state, 'duplication of an anchor property');
  }
  ch = state.input.charCodeAt(++state.position);
  _position = state.position;
  while (ch !== 0 && !is_WS_OR_EOL(ch) && !is_FLOW_INDICATOR(ch)) {
    ch = state.input.charCodeAt(++state.position);
  }
  if (state.position === _position) {
    throwError(state, 'name of an anchor node must contain at least one character');
  }
  state.anchor = state.input.slice(_position, state.position);
  return true;
}
function readAlias(state) {
  var _position, alias,
      ch;
  ch = state.input.charCodeAt(state.position);
  if (ch !== 0x2A) return false;
  ch = state.input.charCodeAt(++state.position);
  _position = state.position;
  while (ch !== 0 && !is_WS_OR_EOL(ch) && !is_FLOW_INDICATOR(ch)) {
    ch = state.input.charCodeAt(++state.position);
  }
  if (state.position === _position) {
    throwError(state, 'name of an alias node must contain at least one character');
  }
  alias = state.input.slice(_position, state.position);
  if (!_hasOwnProperty$1.call(state.anchorMap, alias)) {
    throwError(state, 'unidentified alias "' + alias + '"');
  }
  state.result = state.anchorMap[alias];
  skipSeparationSpace(state, true, -1);
  return true;
}
function composeNode(state, parentIndent, nodeContext, allowToSeek, allowCompact) {
  var allowBlockStyles,
      allowBlockScalars,
      allowBlockCollections,
      indentStatus = 1,
      atNewLine  = false,
      hasContent = false,
      typeIndex,
      typeQuantity,
      typeList,
      type,
      flowIndent,
      blockIndent;
  if (state.listener !== null) {
    state.listener('open', state);
  }
  state.tag    = null;
  state.anchor = null;
  state.kind   = null;
  state.result = null;
  allowBlockStyles = allowBlockScalars = allowBlockCollections =
    CONTEXT_BLOCK_OUT === nodeContext ||
    CONTEXT_BLOCK_IN  === nodeContext;
  if (allowToSeek) {
    if (skipSeparationSpace(state, true, -1)) {
      atNewLine = true;
      if (state.lineIndent > parentIndent) {
        indentStatus = 1;
      } else if (state.lineIndent === parentIndent) {
        indentStatus = 0;
      } else if (state.lineIndent < parentIndent) {
        indentStatus = -1;
      }
    }
  }
  if (indentStatus === 1) {
    while (readTagProperty(state) || readAnchorProperty(state)) {
      if (skipSeparationSpace(state, true, -1)) {
        atNewLine = true;
        allowBlockCollections = allowBlockStyles;
        if (state.lineIndent > parentIndent) {
          indentStatus = 1;
        } else if (state.lineIndent === parentIndent) {
          indentStatus = 0;
        } else if (state.lineIndent < parentIndent) {
          indentStatus = -1;
        }
      } else {
        allowBlockCollections = false;
      }
    }
  }
  if (allowBlockCollections) {
    allowBlockCollections = atNewLine || allowCompact;
  }
  if (indentStatus === 1 || CONTEXT_BLOCK_OUT === nodeContext) {
    if (CONTEXT_FLOW_IN === nodeContext || CONTEXT_FLOW_OUT === nodeContext) {
      flowIndent = parentIndent;
    } else {
      flowIndent = parentIndent + 1;
    }
    blockIndent = state.position - state.lineStart;
    if (indentStatus === 1) {
      if (allowBlockCollections &&
          (readBlockSequence(state, blockIndent) ||
           readBlockMapping(state, blockIndent, flowIndent)) ||
          readFlowCollection(state, flowIndent)) {
        hasContent = true;
      } else {
        if ((allowBlockScalars && readBlockScalar(state, flowIndent)) ||
            readSingleQuotedScalar(state, flowIndent) ||
            readDoubleQuotedScalar(state, flowIndent)) {
          hasContent = true;
        } else if (readAlias(state)) {
          hasContent = true;
          if (state.tag !== null || state.anchor !== null) {
            throwError(state, 'alias node should not have any properties');
          }
        } else if (readPlainScalar(state, flowIndent, CONTEXT_FLOW_IN === nodeContext)) {
          hasContent = true;
          if (state.tag === null) {
            state.tag = '?';
          }
        }
        if (state.anchor !== null) {
          state.anchorMap[state.anchor] = state.result;
        }
      }
    } else if (indentStatus === 0) {
      hasContent = allowBlockCollections && readBlockSequence(state, blockIndent);
    }
  }
  if (state.tag === null) {
    if (state.anchor !== null) {
      state.anchorMap[state.anchor] = state.result;
    }
  } else if (state.tag === '?') {
    if (state.result !== null && state.kind !== 'scalar') {
      throwError(state, 'unacceptable node kind for !<?> tag; it should be "scalar", not "' + state.kind + '"');
    }
    for (typeIndex = 0, typeQuantity = state.implicitTypes.length; typeIndex < typeQuantity; typeIndex += 1) {
      type = state.implicitTypes[typeIndex];
      if (type.resolve(state.result)) {
        state.result = type.construct(state.result);
        state.tag = type.tag;
        if (state.anchor !== null) {
          state.anchorMap[state.anchor] = state.result;
        }
        break;
      }
    }
  } else if (state.tag !== '!') {
    if (_hasOwnProperty$1.call(state.typeMap[state.kind || 'fallback'], state.tag)) {
      type = state.typeMap[state.kind || 'fallback'][state.tag];
    } else {
      type = null;
      typeList = state.typeMap.multi[state.kind || 'fallback'];
      for (typeIndex = 0, typeQuantity = typeList.length; typeIndex < typeQuantity; typeIndex += 1) {
        if (state.tag.slice(0, typeList[typeIndex].tag.length) === typeList[typeIndex].tag) {
          type = typeList[typeIndex];
          break;
        }
      }
    }
    if (!type) {
      throwError(state, 'unknown tag !<' + state.tag + '>');
    }
    if (state.result !== null && type.kind !== state.kind) {
      throwError(state, 'unacceptable node kind for !<' + state.tag + '> tag; it should be "' + type.kind + '", not "' + state.kind + '"');
    }
    if (!type.resolve(state.result, state.tag)) {
      throwError(state, 'cannot resolve a node with !<' + state.tag + '> explicit tag');
    } else {
      state.result = type.construct(state.result, state.tag);
      if (state.anchor !== null) {
        state.anchorMap[state.anchor] = state.result;
      }
    }
  }
  if (state.listener !== null) {
    state.listener('close', state);
  }
  return state.tag !== null ||  state.anchor !== null || hasContent;
}
function readDocument(state) {
  var documentStart = state.position,
      _position,
      directiveName,
      directiveArgs,
      hasDirectives = false,
      ch;
  state.version = null;
  state.checkLineBreaks = state.legacy;
  state.tagMap = Object.create(null);
  state.anchorMap = Object.create(null);
  while ((ch = state.input.charCodeAt(state.position)) !== 0) {
    skipSeparationSpace(state, true, -1);
    ch = state.input.charCodeAt(state.position);
    if (state.lineIndent > 0 || ch !== 0x25) {
      break;
    }
    hasDirectives = true;
    ch = state.input.charCodeAt(++state.position);
    _position = state.position;
    while (ch !== 0 && !is_WS_OR_EOL(ch)) {
      ch = state.input.charCodeAt(++state.position);
    }
    directiveName = state.input.slice(_position, state.position);
    directiveArgs = [];
    if (directiveName.length < 1) {
      throwError(state, 'directive name must not be less than one character in length');
    }
    while (ch !== 0) {
      while (is_WHITE_SPACE(ch)) {
        ch = state.input.charCodeAt(++state.position);
      }
      if (ch === 0x23) {
        do { ch = state.input.charCodeAt(++state.position); }
        while (ch !== 0 && !is_EOL(ch));
        break;
      }
      if (is_EOL(ch)) break;
      _position = state.position;
      while (ch !== 0 && !is_WS_OR_EOL(ch)) {
        ch = state.input.charCodeAt(++state.position);
      }
      directiveArgs.push(state.input.slice(_position, state.position));
    }
    if (ch !== 0) readLineBreak(state);
    if (_hasOwnProperty$1.call(directiveHandlers, directiveName)) {
      directiveHandlers[directiveName](state, directiveName, directiveArgs);
    } else {
      throwWarning(state, 'unknown document directive "' + directiveName + '"');
    }
  }
  skipSeparationSpace(state, true, -1);
  if (state.lineIndent === 0 &&
      state.input.charCodeAt(state.position)     === 0x2D &&
      state.input.charCodeAt(state.position + 1) === 0x2D &&
      state.input.charCodeAt(state.position + 2) === 0x2D) {
    state.position += 3;
    skipSeparationSpace(state, true, -1);
  } else if (hasDirectives) {
    throwError(state, 'directives end mark is expected');
  }
  composeNode(state, state.lineIndent - 1, CONTEXT_BLOCK_OUT, false, true);
  skipSeparationSpace(state, true, -1);
  if (state.checkLineBreaks &&
      PATTERN_NON_ASCII_LINE_BREAKS.test(state.input.slice(documentStart, state.position))) {
    throwWarning(state, 'non-ASCII line breaks are interpreted as content');
  }
  state.documents.push(state.result);
  if (state.position === state.lineStart && testDocumentSeparator(state)) {
    if (state.input.charCodeAt(state.position) === 0x2E) {
      state.position += 3;
      skipSeparationSpace(state, true, -1);
    }
    return;
  }
  if (state.position < (state.length - 1)) {
    throwError(state, 'end of the stream or a document separator is expected');
  } else {
    return;
  }
}
function loadDocuments(input, options) {
  input = String(input);
  options = options || {};
  if (input.length !== 0) {
    if (input.charCodeAt(input.length - 1) !== 0x0A &&
        input.charCodeAt(input.length - 1) !== 0x0D) {
      input += '\n';
    }
    if (input.charCodeAt(0) === 0xFEFF) {
      input = input.slice(1);
    }
  }
  var state = new State$1(input, options);
  var nullpos = input.indexOf('\0');
  if (nullpos !== -1) {
    state.position = nullpos;
    throwError(state, 'null byte is not allowed in input');
  }
  state.input += '\0';
  while (state.input.charCodeAt(state.position) === 0x20) {
    state.lineIndent += 1;
    state.position += 1;
  }
  while (state.position < (state.length - 1)) {
    readDocument(state);
  }
  return state.documents;
}
function loadAll$1(input, iterator, options) {
  if (iterator !== null && typeof iterator === 'object' && typeof options === 'undefined') {
    options = iterator;
    iterator = null;
  }
  var documents = loadDocuments(input, options);
  if (typeof iterator !== 'function') {
    return documents;
  }
  for (var index = 0, length = documents.length; index < length; index += 1) {
    iterator(documents[index]);
  }
}
function load$1(input, options) {
  var documents = loadDocuments(input, options);
  if (documents.length === 0) {
    return undefined;
  } else if (documents.length === 1) {
    return documents[0];
  }
  throw new exception('expected a single document in the stream, but found more');
}
var loadAll_1 = loadAll$1;
var load_1    = load$1;
var loader = {
	loadAll: loadAll_1,
	load: load_1
};
var _toString       = Object.prototype.toString;
var _hasOwnProperty = Object.prototype.hasOwnProperty;
var CHAR_BOM                  = 0xFEFF;
var CHAR_TAB                  = 0x09;
var CHAR_LINE_FEED            = 0x0A;
var CHAR_CARRIAGE_RETURN      = 0x0D;
var CHAR_SPACE                = 0x20;
var CHAR_EXCLAMATION          = 0x21;
var CHAR_DOUBLE_QUOTE         = 0x22;
var CHAR_SHARP                = 0x23;
var CHAR_PERCENT              = 0x25;
var CHAR_AMPERSAND            = 0x26;
var CHAR_SINGLE_QUOTE         = 0x27;
var CHAR_ASTERISK             = 0x2A;
var CHAR_COMMA                = 0x2C;
var CHAR_MINUS                = 0x2D;
var CHAR_COLON                = 0x3A;
var CHAR_EQUALS               = 0x3D;
var CHAR_GREATER_THAN         = 0x3E;
var CHAR_QUESTION             = 0x3F;
var CHAR_COMMERCIAL_AT        = 0x40;
var CHAR_LEFT_SQUARE_BRACKET  = 0x5B;
var CHAR_RIGHT_SQUARE_BRACKET = 0x5D;
var CHAR_GRAVE_ACCENT         = 0x60;
var CHAR_LEFT_CURLY_BRACKET   = 0x7B;
var CHAR_VERTICAL_LINE        = 0x7C;
var CHAR_RIGHT_CURLY_BRACKET  = 0x7D;
var ESCAPE_SEQUENCES = {};
ESCAPE_SEQUENCES[0x00]   = '\\0';
ESCAPE_SEQUENCES[0x07]   = '\\a';
ESCAPE_SEQUENCES[0x08]   = '\\b';
ESCAPE_SEQUENCES[0x09]   = '\\t';
ESCAPE_SEQUENCES[0x0A]   = '\\n';
ESCAPE_SEQUENCES[0x0B]   = '\\v';
ESCAPE_SEQUENCES[0x0C]   = '\\f';
ESCAPE_SEQUENCES[0x0D]   = '\\r';
ESCAPE_SEQUENCES[0x1B]   = '\\e';
ESCAPE_SEQUENCES[0x22]   = '\\"';
ESCAPE_SEQUENCES[0x5C]   = '\\\\';
ESCAPE_SEQUENCES[0x85]   = '\\N';
ESCAPE_SEQUENCES[0xA0]   = '\\_';
ESCAPE_SEQUENCES[0x2028] = '\\L';
ESCAPE_SEQUENCES[0x2029] = '\\P';
var DEPRECATED_BOOLEANS_SYNTAX = [
  'y', 'Y', 'yes', 'Yes', 'YES', 'on', 'On', 'ON',
  'n', 'N', 'no', 'No', 'NO', 'off', 'Off', 'OFF'
];
var DEPRECATED_BASE60_SYNTAX = /^[-+]?[0-9_]+(?::[0-9_]+)+(?:\.[0-9_]*)?$/;
function compileStyleMap(schema, map) {
  var result, keys, index, length, tag, style, type;
  if (map === null) return {};
  result = {};
  keys = Object.keys(map);
  for (index = 0, length = keys.length; index < length; index += 1) {
    tag = keys[index];
    style = String(map[tag]);
    if (tag.slice(0, 2) === '!!') {
      tag = 'tag:yaml.org,2002:' + tag.slice(2);
    }
    type = schema.compiledTypeMap['fallback'][tag];
    if (type && _hasOwnProperty.call(type.styleAliases, style)) {
      style = type.styleAliases[style];
    }
    result[tag] = style;
  }
  return result;
}
function encodeHex(character) {
  var string, handle, length;
  string = character.toString(16).toUpperCase();
  if (character <= 0xFF) {
    handle = 'x';
    length = 2;
  } else if (character <= 0xFFFF) {
    handle = 'u';
    length = 4;
  } else if (character <= 0xFFFFFFFF) {
    handle = 'U';
    length = 8;
  } else {
    throw new exception('code point within a string may not be greater than 0xFFFFFFFF');
  }
  return '\\' + handle + common.repeat('0', length - string.length) + string;
}
var QUOTING_TYPE_SINGLE = 1,
    QUOTING_TYPE_DOUBLE = 2;
function State(options) {
  this.schema        = options['schema'] || _default;
  this.indent        = Math.max(1, (options['indent'] || 2));
  this.noArrayIndent = options['noArrayIndent'] || false;
  this.skipInvalid   = options['skipInvalid'] || false;
  this.flowLevel     = (common.isNothing(options['flowLevel']) ? -1 : options['flowLevel']);
  this.styleMap      = compileStyleMap(this.schema, options['styles'] || null);
  this.sortKeys      = options['sortKeys'] || false;
  this.lineWidth     = options['lineWidth'] || 80;
  this.noRefs        = options['noRefs'] || false;
  this.noCompatMode  = options['noCompatMode'] || false;
  this.condenseFlow  = options['condenseFlow'] || false;
  this.quotingType   = options['quotingType'] === '"' ? QUOTING_TYPE_DOUBLE : QUOTING_TYPE_SINGLE;
  this.forceQuotes   = options['forceQuotes'] || false;
  this.replacer      = typeof options['replacer'] === 'function' ? options['replacer'] : null;
  this.implicitTypes = this.schema.compiledImplicit;
  this.explicitTypes = this.schema.compiledExplicit;
  this.tag = null;
  this.result = '';
  this.duplicates = [];
  this.usedDuplicates = null;
}
function indentString(string, spaces) {
  var ind = common.repeat(' ', spaces),
      position = 0,
      next = -1,
      result = '',
      line,
      length = string.length;
  while (position < length) {
    next = string.indexOf('\n', position);
    if (next === -1) {
      line = string.slice(position);
      position = length;
    } else {
      line = string.slice(position, next + 1);
      position = next + 1;
    }
    if (line.length && line !== '\n') result += ind;
    result += line;
  }
  return result;
}
function generateNextLine(state, level) {
  return '\n' + common.repeat(' ', state.indent * level);
}
function testImplicitResolving(state, str) {
  var index, length, type;
  for (index = 0, length = state.implicitTypes.length; index < length; index += 1) {
    type = state.implicitTypes[index];
    if (type.resolve(str)) {
      return true;
    }
  }
  return false;
}
function isWhitespace(c) {
  return c === CHAR_SPACE || c === CHAR_TAB;
}
function isPrintable(c) {
  return  (0x00020 <= c && c <= 0x00007E)
      || ((0x000A1 <= c && c <= 0x00D7FF) && c !== 0x2028 && c !== 0x2029)
      || ((0x0E000 <= c && c <= 0x00FFFD) && c !== CHAR_BOM)
      ||  (0x10000 <= c && c <= 0x10FFFF);
}
function isNsCharOrWhitespace(c) {
  return isPrintable(c)
    && c !== CHAR_BOM
    && c !== CHAR_CARRIAGE_RETURN
    && c !== CHAR_LINE_FEED;
}
function isPlainSafe(c, prev, inblock) {
  var cIsNsCharOrWhitespace = isNsCharOrWhitespace(c);
  var cIsNsChar = cIsNsCharOrWhitespace && !isWhitespace(c);
  return (
    inblock ?
      cIsNsCharOrWhitespace
      : cIsNsCharOrWhitespace
        && c !== CHAR_COMMA
        && c !== CHAR_LEFT_SQUARE_BRACKET
        && c !== CHAR_RIGHT_SQUARE_BRACKET
        && c !== CHAR_LEFT_CURLY_BRACKET
        && c !== CHAR_RIGHT_CURLY_BRACKET
  )
    && c !== CHAR_SHARP
    && !(prev === CHAR_COLON && !cIsNsChar)
    || (isNsCharOrWhitespace(prev) && !isWhitespace(prev) && c === CHAR_SHARP)
    || (prev === CHAR_COLON && cIsNsChar);
}
function isPlainSafeFirst(c) {
  return isPrintable(c) && c !== CHAR_BOM
    && !isWhitespace(c)
    && c !== CHAR_MINUS
    && c !== CHAR_QUESTION
    && c !== CHAR_COLON
    && c !== CHAR_COMMA
    && c !== CHAR_LEFT_SQUARE_BRACKET
    && c !== CHAR_RIGHT_SQUARE_BRACKET
    && c !== CHAR_LEFT_CURLY_BRACKET
    && c !== CHAR_RIGHT_CURLY_BRACKET
    && c !== CHAR_SHARP
    && c !== CHAR_AMPERSAND
    && c !== CHAR_ASTERISK
    && c !== CHAR_EXCLAMATION
    && c !== CHAR_VERTICAL_LINE
    && c !== CHAR_EQUALS
    && c !== CHAR_GREATER_THAN
    && c !== CHAR_SINGLE_QUOTE
    && c !== CHAR_DOUBLE_QUOTE
    && c !== CHAR_PERCENT
    && c !== CHAR_COMMERCIAL_AT
    && c !== CHAR_GRAVE_ACCENT;
}
function isPlainSafeLast(c) {
  return !isWhitespace(c) && c !== CHAR_COLON;
}
function codePointAt(string, pos) {
  var first = string.charCodeAt(pos), second;
  if (first >= 0xD800 && first <= 0xDBFF && pos + 1 < string.length) {
    second = string.charCodeAt(pos + 1);
    if (second >= 0xDC00 && second <= 0xDFFF) {
      return (first - 0xD800) * 0x400 + second - 0xDC00 + 0x10000;
    }
  }
  return first;
}
function needIndentIndicator(string) {
  var leadingSpaceRe = /^\n* /;
  return leadingSpaceRe.test(string);
}
var STYLE_PLAIN   = 1,
    STYLE_SINGLE  = 2,
    STYLE_LITERAL = 3,
    STYLE_FOLDED  = 4,
    STYLE_DOUBLE  = 5;
function chooseScalarStyle(string, singleLineOnly, indentPerLevel, lineWidth,
  testAmbiguousType, quotingType, forceQuotes, inblock) {
  var i;
  var char = 0;
  var prevChar = null;
  var hasLineBreak = false;
  var hasFoldableLine = false;
  var shouldTrackWidth = lineWidth !== -1;
  var previousLineBreak = -1;
  var plain = isPlainSafeFirst(codePointAt(string, 0))
          && isPlainSafeLast(codePointAt(string, string.length - 1));
  if (singleLineOnly || forceQuotes) {
    for (i = 0; i < string.length; char >= 0x10000 ? i += 2 : i++) {
      char = codePointAt(string, i);
      if (!isPrintable(char)) {
        return STYLE_DOUBLE;
      }
      plain = plain && isPlainSafe(char, prevChar, inblock);
      prevChar = char;
    }
  } else {
    for (i = 0; i < string.length; char >= 0x10000 ? i += 2 : i++) {
      char = codePointAt(string, i);
      if (char === CHAR_LINE_FEED) {
        hasLineBreak = true;
        if (shouldTrackWidth) {
          hasFoldableLine = hasFoldableLine ||
            (i - previousLineBreak - 1 > lineWidth &&
             string[previousLineBreak + 1] !== ' ');
          previousLineBreak = i;
        }
      } else if (!isPrintable(char)) {
        return STYLE_DOUBLE;
      }
      plain = plain && isPlainSafe(char, prevChar, inblock);
      prevChar = char;
    }
    hasFoldableLine = hasFoldableLine || (shouldTrackWidth &&
      (i - previousLineBreak - 1 > lineWidth &&
       string[previousLineBreak + 1] !== ' '));
  }
  if (!hasLineBreak && !hasFoldableLine) {
    if (plain && !forceQuotes && !testAmbiguousType(string)) {
      return STYLE_PLAIN;
    }
    return quotingType === QUOTING_TYPE_DOUBLE ? STYLE_DOUBLE : STYLE_SINGLE;
  }
  if (indentPerLevel > 9 && needIndentIndicator(string)) {
    return STYLE_DOUBLE;
  }
  if (!forceQuotes) {
    return hasFoldableLine ? STYLE_FOLDED : STYLE_LITERAL;
  }
  return quotingType === QUOTING_TYPE_DOUBLE ? STYLE_DOUBLE : STYLE_SINGLE;
}
function writeScalar(state, string, level, iskey, inblock) {
  state.dump = (function () {
    if (string.length === 0) {
      return state.quotingType === QUOTING_TYPE_DOUBLE ? '""' : "''";
    }
    if (!state.noCompatMode) {
      if (DEPRECATED_BOOLEANS_SYNTAX.indexOf(string) !== -1 || DEPRECATED_BASE60_SYNTAX.test(string)) {
        return state.quotingType === QUOTING_TYPE_DOUBLE ? ('"' + string + '"') : ("'" + string + "'");
      }
    }
    var indent = state.indent * Math.max(1, level);
    var lineWidth = state.lineWidth === -1
      ? -1 : Math.max(Math.min(state.lineWidth, 40), state.lineWidth - indent);
    var singleLineOnly = iskey
      || (state.flowLevel > -1 && level >= state.flowLevel);
    function testAmbiguity(string) {
      return testImplicitResolving(state, string);
    }
    switch (chooseScalarStyle(string, singleLineOnly, state.indent, lineWidth,
      testAmbiguity, state.quotingType, state.forceQuotes && !iskey, inblock)) {
      case STYLE_PLAIN:
        return string;
      case STYLE_SINGLE:
        return "'" + string.replace(/'/g, "''") + "'";
      case STYLE_LITERAL:
        return '|' + blockHeader(string, state.indent)
          + dropEndingNewline(indentString(string, indent));
      case STYLE_FOLDED:
        return '>' + blockHeader(string, state.indent)
          + dropEndingNewline(indentString(foldString(string, lineWidth), indent));
      case STYLE_DOUBLE:
        return '"' + escapeString(string) + '"';
      default:
        throw new exception('impossible error: invalid scalar style');
    }
  }());
}
function blockHeader(string, indentPerLevel) {
  var indentIndicator = needIndentIndicator(string) ? String(indentPerLevel) : '';
  var clip =          string[string.length - 1] === '\n';
  var keep = clip && (string[string.length - 2] === '\n' || string === '\n');
  var chomp = keep ? '+' : (clip ? '' : '-');
  return indentIndicator + chomp + '\n';
}
function dropEndingNewline(string) {
  return string[string.length - 1] === '\n' ? string.slice(0, -1) : string;
}
function foldString(string, width) {
  var lineRe = /(\n+)([^\n]*)/g;
  var result = (function () {
    var nextLF = string.indexOf('\n');
    nextLF = nextLF !== -1 ? nextLF : string.length;
    lineRe.lastIndex = nextLF;
    return foldLine(string.slice(0, nextLF), width);
  }());
  var prevMoreIndented = string[0] === '\n' || string[0] === ' ';
  var moreIndented;
  var match;
  while ((match = lineRe.exec(string))) {
    var prefix = match[1], line = match[2];
    moreIndented = (line[0] === ' ');
    result += prefix
      + (!prevMoreIndented && !moreIndented && line !== ''
        ? '\n' : '')
      + foldLine(line, width);
    prevMoreIndented = moreIndented;
  }
  return result;
}
function foldLine(line, width) {
  if (line === '' || line[0] === ' ') return line;
  var breakRe = / [^ ]/g;
  var match;
  var start = 0, end, curr = 0, next = 0;
  var result = '';
  while ((match = breakRe.exec(line))) {
    next = match.index;
    if (next - start > width) {
      end = (curr > start) ? curr : next;
      result += '\n' + line.slice(start, end);
      start = end + 1;
    }
    curr = next;
  }
  result += '\n';
  if (line.length - start > width && curr > start) {
    result += line.slice(start, curr) + '\n' + line.slice(curr + 1);
  } else {
    result += line.slice(start);
  }
  return result.slice(1);
}
function escapeString(string) {
  var result = '';
  var char = 0;
  var escapeSeq;
  for (var i = 0; i < string.length; char >= 0x10000 ? i += 2 : i++) {
    char = codePointAt(string, i);
    escapeSeq = ESCAPE_SEQUENCES[char];
    if (!escapeSeq && isPrintable(char)) {
      result += string[i];
      if (char >= 0x10000) result += string[i + 1];
    } else {
      result += escapeSeq || encodeHex(char);
    }
  }
  return result;
}
function writeFlowSequence(state, level, object) {
  var _result = '',
      _tag    = state.tag,
      index,
      length,
      value;
  for (index = 0, length = object.length; index < length; index += 1) {
    value = object[index];
    if (state.replacer) {
      value = state.replacer.call(object, String(index), value);
    }
    if (writeNode(state, level, value, false, false) ||
        (typeof value === 'undefined' &&
         writeNode(state, level, null, false, false))) {
      if (_result !== '') _result += ',' + (!state.condenseFlow ? ' ' : '');
      _result += state.dump;
    }
  }
  state.tag = _tag;
  state.dump = '[' + _result + ']';
}
function writeBlockSequence(state, level, object, compact) {
  var _result = '',
      _tag    = state.tag,
      index,
      length,
      value;
  for (index = 0, length = object.length; index < length; index += 1) {
    value = object[index];
    if (state.replacer) {
      value = state.replacer.call(object, String(index), value);
    }
    if (writeNode(state, level + 1, value, true, true, false, true) ||
        (typeof value === 'undefined' &&
         writeNode(state, level + 1, null, true, true, false, true))) {
      if (!compact || _result !== '') {
        _result += generateNextLine(state, level);
      }
      if (state.dump && CHAR_LINE_FEED === state.dump.charCodeAt(0)) {
        _result += '-';
      } else {
        _result += '- ';
      }
      _result += state.dump;
    }
  }
  state.tag = _tag;
  state.dump = _result || '[]';
}
function writeFlowMapping(state, level, object) {
  var _result       = '',
      _tag          = state.tag,
      objectKeyList = Object.keys(object),
      index,
      length,
      objectKey,
      objectValue,
      pairBuffer;
  for (index = 0, length = objectKeyList.length; index < length; index += 1) {
    pairBuffer = '';
    if (_result !== '') pairBuffer += ', ';
    if (state.condenseFlow) pairBuffer += '"';
    objectKey = objectKeyList[index];
    objectValue = object[objectKey];
    if (state.replacer) {
      objectValue = state.replacer.call(object, objectKey, objectValue);
    }
    if (!writeNode(state, level, objectKey, false, false)) {
      continue;
    }
    if (state.dump.length > 1024) pairBuffer += '? ';
    pairBuffer += state.dump + (state.condenseFlow ? '"' : '') + ':' + (state.condenseFlow ? '' : ' ');
    if (!writeNode(state, level, objectValue, false, false)) {
      continue;
    }
    pairBuffer += state.dump;
    _result += pairBuffer;
  }
  state.tag = _tag;
  state.dump = '{' + _result + '}';
}
function writeBlockMapping(state, level, object, compact) {
  var _result       = '',
      _tag          = state.tag,
      objectKeyList = Object.keys(object),
      index,
      length,
      objectKey,
      objectValue,
      explicitPair,
      pairBuffer;
  if (state.sortKeys === true) {
    objectKeyList.sort();
  } else if (typeof state.sortKeys === 'function') {
    objectKeyList.sort(state.sortKeys);
  } else if (state.sortKeys) {
    throw new exception('sortKeys must be a boolean or a function');
  }
  for (index = 0, length = objectKeyList.length; index < length; index += 1) {
    pairBuffer = '';
    if (!compact || _result !== '') {
      pairBuffer += generateNextLine(state, level);
    }
    objectKey = objectKeyList[index];
    objectValue = object[objectKey];
    if (state.replacer) {
      objectValue = state.replacer.call(object, objectKey, objectValue);
    }
    if (!writeNode(state, level + 1, objectKey, true, true, true)) {
      continue;
    }
    explicitPair = (state.tag !== null && state.tag !== '?') ||
                   (state.dump && state.dump.length > 1024);
    if (explicitPair) {
      if (state.dump && CHAR_LINE_FEED === state.dump.charCodeAt(0)) {
        pairBuffer += '?';
      } else {
        pairBuffer += '? ';
      }
    }
    pairBuffer += state.dump;
    if (explicitPair) {
      pairBuffer += generateNextLine(state, level);
    }
    if (!writeNode(state, level + 1, objectValue, true, explicitPair)) {
      continue;
    }
    if (state.dump && CHAR_LINE_FEED === state.dump.charCodeAt(0)) {
      pairBuffer += ':';
    } else {
      pairBuffer += ': ';
    }
    pairBuffer += state.dump;
    _result += pairBuffer;
  }
  state.tag = _tag;
  state.dump = _result || '{}';
}
function detectType(state, object, explicit) {
  var _result, typeList, index, length, type, style;
  typeList = explicit ? state.explicitTypes : state.implicitTypes;
  for (index = 0, length = typeList.length; index < length; index += 1) {
    type = typeList[index];
    if ((type.instanceOf  || type.predicate) &&
        (!type.instanceOf || ((typeof object === 'object') && (object instanceof type.instanceOf))) &&
        (!type.predicate  || type.predicate(object))) {
      if (explicit) {
        if (type.multi && type.representName) {
          state.tag = type.representName(object);
        } else {
          state.tag = type.tag;
        }
      } else {
        state.tag = '?';
      }
      if (type.represent) {
        style = state.styleMap[type.tag] || type.defaultStyle;
        if (_toString.call(type.represent) === '[object Function]') {
          _result = type.represent(object, style);
        } else if (_hasOwnProperty.call(type.represent, style)) {
          _result = type.represent[style](object, style);
        } else {
          throw new exception('!<' + type.tag + '> tag resolver accepts not "' + style + '" style');
        }
        state.dump = _result;
      }
      return true;
    }
  }
  return false;
}
function writeNode(state, level, object, block, compact, iskey, isblockseq) {
  state.tag = null;
  state.dump = object;
  if (!detectType(state, object, false)) {
    detectType(state, object, true);
  }
  var type = _toString.call(state.dump);
  var inblock = block;
  var tagStr;
  if (block) {
    block = (state.flowLevel < 0 || state.flowLevel > level);
  }
  var objectOrArray = type === '[object Object]' || type === '[object Array]',
      duplicateIndex,
      duplicate;
  if (objectOrArray) {
    duplicateIndex = state.duplicates.indexOf(object);
    duplicate = duplicateIndex !== -1;
  }
  if ((state.tag !== null && state.tag !== '?') || duplicate || (state.indent !== 2 && level > 0)) {
    compact = false;
  }
  if (duplicate && state.usedDuplicates[duplicateIndex]) {
    state.dump = '*ref_' + duplicateIndex;
  } else {
    if (objectOrArray && duplicate && !state.usedDuplicates[duplicateIndex]) {
      state.usedDuplicates[duplicateIndex] = true;
    }
    if (type === '[object Object]') {
      if (block && (Object.keys(state.dump).length !== 0)) {
        writeBlockMapping(state, level, state.dump, compact);
        if (duplicate) {
          state.dump = '&ref_' + duplicateIndex + state.dump;
        }
      } else {
        writeFlowMapping(state, level, state.dump);
        if (duplicate) {
          state.dump = '&ref_' + duplicateIndex + ' ' + state.dump;
        }
      }
    } else if (type === '[object Array]') {
      if (block && (state.dump.length !== 0)) {
        if (state.noArrayIndent && !isblockseq && level > 0) {
          writeBlockSequence(state, level - 1, state.dump, compact);
        } else {
          writeBlockSequence(state, level, state.dump, compact);
        }
        if (duplicate) {
          state.dump = '&ref_' + duplicateIndex + state.dump;
        }
      } else {
        writeFlowSequence(state, level, state.dump);
        if (duplicate) {
          state.dump = '&ref_' + duplicateIndex + ' ' + state.dump;
        }
      }
    } else if (type === '[object String]') {
      if (state.tag !== '?') {
        writeScalar(state, state.dump, level, iskey, inblock);
      }
    } else if (type === '[object Undefined]') {
      return false;
    } else {
      if (state.skipInvalid) return false;
      throw new exception('unacceptable kind of an object to dump ' + type);
    }
    if (state.tag !== null && state.tag !== '?') {
      tagStr = encodeURI(
        state.tag[0] === '!' ? state.tag.slice(1) : state.tag
      ).replace(/!/g, '%21');
      if (state.tag[0] === '!') {
        tagStr = '!' + tagStr;
      } else if (tagStr.slice(0, 18) === 'tag:yaml.org,2002:') {
        tagStr = '!!' + tagStr.slice(18);
      } else {
        tagStr = '!<' + tagStr + '>';
      }
      state.dump = tagStr + ' ' + state.dump;
    }
  }
  return true;
}
function getDuplicateReferences(object, state) {
  var objects = [],
      duplicatesIndexes = [],
      index,
      length;
  inspectNode(object, objects, duplicatesIndexes);
  for (index = 0, length = duplicatesIndexes.length; index < length; index += 1) {
    state.duplicates.push(objects[duplicatesIndexes[index]]);
  }
  state.usedDuplicates = new Array(length);
}
function inspectNode(object, objects, duplicatesIndexes) {
  var objectKeyList,
      index,
      length;
  if (object !== null && typeof object === 'object') {
    index = objects.indexOf(object);
    if (index !== -1) {
      if (duplicatesIndexes.indexOf(index) === -1) {
        duplicatesIndexes.push(index);
      }
    } else {
      objects.push(object);
      if (Array.isArray(object)) {
        for (index = 0, length = object.length; index < length; index += 1) {
          inspectNode(object[index], objects, duplicatesIndexes);
        }
      } else {
        objectKeyList = Object.keys(object);
        for (index = 0, length = objectKeyList.length; index < length; index += 1) {
          inspectNode(object[objectKeyList[index]], objects, duplicatesIndexes);
        }
      }
    }
  }
}
function dump$1(input, options) {
  options = options || {};
  var state = new State(options);
  if (!state.noRefs) getDuplicateReferences(input, state);
  var value = input;
  if (state.replacer) {
    value = state.replacer.call({ '': value }, '', value);
  }
  if (writeNode(state, 0, value, true, true)) return state.dump + '\n';
  return '';
}
var dump_1 = dump$1;
var dumper = {
	dump: dump_1
};
function renamed(from, to) {
  return function () {
    throw new Error('Function yaml.' + from + ' is removed in js-yaml 4. ' +
      'Use yaml.' + to + ' instead, which is now safe by default.');
  };
}
var Type                = type;
var Schema              = schema;
var FAILSAFE_SCHEMA     = failsafe;
var JSON_SCHEMA         = json;
var CORE_SCHEMA         = core;
var DEFAULT_SCHEMA      = _default;
var load                = loader.load;
var loadAll             = loader.loadAll;
var dump                = dumper.dump;
var YAMLException       = exception;
var types = {
  binary:    binary,
  float:     float,
  map:       map,
  null:      _null,
  pairs:     pairs,
  set:       set,
  timestamp: timestamp,
  bool:      bool,
  int:       int,
  merge:     merge,
  omap:      omap,
  seq:       seq,
  str:       str
};
var safeLoad            = renamed('safeLoad', 'load');
var safeLoadAll         = renamed('safeLoadAll', 'loadAll');
var safeDump            = renamed('safeDump', 'dump');
var jsYaml = {
	Type: Type,
	Schema: Schema,
	FAILSAFE_SCHEMA: FAILSAFE_SCHEMA,
	JSON_SCHEMA: JSON_SCHEMA,
	CORE_SCHEMA: CORE_SCHEMA,
	DEFAULT_SCHEMA: DEFAULT_SCHEMA,
	load: load,
	loadAll: loadAll,
	dump: dump,
	YAMLException: YAMLException,
	types: types,
	safeLoad: safeLoad,
	safeLoadAll: safeLoadAll,
	safeDump: safeDump
};

const debug$1 = (
  typeof process === 'object' &&
  process.env &&
  process.env.NODE_DEBUG &&
  /\bsemver\b/i.test(process.env.NODE_DEBUG)
) ? (...args) => console.error('SEMVER', ...args)
  : () => {};
var debug_1 = debug$1;
getDefaultExportFromCjs(debug_1);

const SEMVER_SPEC_VERSION = '2.0.0';
const MAX_LENGTH$1 = 256;
const MAX_SAFE_INTEGER$1 = Number.MAX_SAFE_INTEGER ||
 9007199254740991;
const MAX_SAFE_COMPONENT_LENGTH = 16;
const MAX_SAFE_BUILD_LENGTH = MAX_LENGTH$1 - 6;
const RELEASE_TYPES = [
  'major',
  'premajor',
  'minor',
  'preminor',
  'patch',
  'prepatch',
  'prerelease',
];
var constants = {
  MAX_LENGTH: MAX_LENGTH$1,
  MAX_SAFE_COMPONENT_LENGTH,
  MAX_SAFE_BUILD_LENGTH,
  MAX_SAFE_INTEGER: MAX_SAFE_INTEGER$1,
  RELEASE_TYPES,
  SEMVER_SPEC_VERSION,
  FLAG_INCLUDE_PRERELEASE: 0b001,
  FLAG_LOOSE: 0b010,
};
getDefaultExportFromCjs(constants);

var re$1 = {exports: {}};

(function (module, exports) {
	const {
	  MAX_SAFE_COMPONENT_LENGTH,
	  MAX_SAFE_BUILD_LENGTH,
	  MAX_LENGTH,
	} = constants;
	const debug = debug_1;
	exports = module.exports = {};
	const re = exports.re = [];
	const safeRe = exports.safeRe = [];
	const src = exports.src = [];
	const t = exports.t = {};
	let R = 0;
	const LETTERDASHNUMBER = '[a-zA-Z0-9-]';
	const safeRegexReplacements = [
	  ['\\s', 1],
	  ['\\d', MAX_LENGTH],
	  [LETTERDASHNUMBER, MAX_SAFE_BUILD_LENGTH],
	];
	const makeSafeRegex = (value) => {
	  for (const [token, max] of safeRegexReplacements) {
	    value = value
	      .split(`${token}*`).join(`${token}{0,${max}}`)
	      .split(`${token}+`).join(`${token}{1,${max}}`);
	  }
	  return value
	};
	const createToken = (name, value, isGlobal) => {
	  const safe = makeSafeRegex(value);
	  const index = R++;
	  debug(name, index, value);
	  t[name] = index;
	  src[index] = value;
	  re[index] = new RegExp(value, isGlobal ? 'g' : undefined);
	  safeRe[index] = new RegExp(safe, isGlobal ? 'g' : undefined);
	};
	createToken('NUMERICIDENTIFIER', '0|[1-9]\\d*');
	createToken('NUMERICIDENTIFIERLOOSE', '\\d+');
	createToken('NONNUMERICIDENTIFIER', `\\d*[a-zA-Z-]${LETTERDASHNUMBER}*`);
	createToken('MAINVERSION', `(${src[t.NUMERICIDENTIFIER]})\\.` +
	                   `(${src[t.NUMERICIDENTIFIER]})\\.` +
	                   `(${src[t.NUMERICIDENTIFIER]})`);
	createToken('MAINVERSIONLOOSE', `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.` +
	                        `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.` +
	                        `(${src[t.NUMERICIDENTIFIERLOOSE]})`);
	createToken('PRERELEASEIDENTIFIER', `(?:${src[t.NUMERICIDENTIFIER]
	}|${src[t.NONNUMERICIDENTIFIER]})`);
	createToken('PRERELEASEIDENTIFIERLOOSE', `(?:${src[t.NUMERICIDENTIFIERLOOSE]
	}|${src[t.NONNUMERICIDENTIFIER]})`);
	createToken('PRERELEASE', `(?:-(${src[t.PRERELEASEIDENTIFIER]
	}(?:\\.${src[t.PRERELEASEIDENTIFIER]})*))`);
	createToken('PRERELEASELOOSE', `(?:-?(${src[t.PRERELEASEIDENTIFIERLOOSE]
	}(?:\\.${src[t.PRERELEASEIDENTIFIERLOOSE]})*))`);
	createToken('BUILDIDENTIFIER', `${LETTERDASHNUMBER}+`);
	createToken('BUILD', `(?:\\+(${src[t.BUILDIDENTIFIER]
	}(?:\\.${src[t.BUILDIDENTIFIER]})*))`);
	createToken('FULLPLAIN', `v?${src[t.MAINVERSION]
	}${src[t.PRERELEASE]}?${
	  src[t.BUILD]}?`);
	createToken('FULL', `^${src[t.FULLPLAIN]}$`);
	createToken('LOOSEPLAIN', `[v=\\s]*${src[t.MAINVERSIONLOOSE]
	}${src[t.PRERELEASELOOSE]}?${
	  src[t.BUILD]}?`);
	createToken('LOOSE', `^${src[t.LOOSEPLAIN]}$`);
	createToken('GTLT', '((?:<|>)?=?)');
	createToken('XRANGEIDENTIFIERLOOSE', `${src[t.NUMERICIDENTIFIERLOOSE]}|x|X|\\*`);
	createToken('XRANGEIDENTIFIER', `${src[t.NUMERICIDENTIFIER]}|x|X|\\*`);
	createToken('XRANGEPLAIN', `[v=\\s]*(${src[t.XRANGEIDENTIFIER]})` +
	                   `(?:\\.(${src[t.XRANGEIDENTIFIER]})` +
	                   `(?:\\.(${src[t.XRANGEIDENTIFIER]})` +
	                   `(?:${src[t.PRERELEASE]})?${
	                     src[t.BUILD]}?` +
	                   `)?)?`);
	createToken('XRANGEPLAINLOOSE', `[v=\\s]*(${src[t.XRANGEIDENTIFIERLOOSE]})` +
	                        `(?:\\.(${src[t.XRANGEIDENTIFIERLOOSE]})` +
	                        `(?:\\.(${src[t.XRANGEIDENTIFIERLOOSE]})` +
	                        `(?:${src[t.PRERELEASELOOSE]})?${
	                          src[t.BUILD]}?` +
	                        `)?)?`);
	createToken('XRANGE', `^${src[t.GTLT]}\\s*${src[t.XRANGEPLAIN]}$`);
	createToken('XRANGELOOSE', `^${src[t.GTLT]}\\s*${src[t.XRANGEPLAINLOOSE]}$`);
	createToken('COERCE', `${'(^|[^\\d])' +
	              '(\\d{1,'}${MAX_SAFE_COMPONENT_LENGTH}})` +
	              `(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?` +
	              `(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?` +
	              `(?:$|[^\\d])`);
	createToken('COERCERTL', src[t.COERCE], true);
	createToken('LONETILDE', '(?:~>?)');
	createToken('TILDETRIM', `(\\s*)${src[t.LONETILDE]}\\s+`, true);
	exports.tildeTrimReplace = '$1~';
	createToken('TILDE', `^${src[t.LONETILDE]}${src[t.XRANGEPLAIN]}$`);
	createToken('TILDELOOSE', `^${src[t.LONETILDE]}${src[t.XRANGEPLAINLOOSE]}$`);
	createToken('LONECARET', '(?:\\^)');
	createToken('CARETTRIM', `(\\s*)${src[t.LONECARET]}\\s+`, true);
	exports.caretTrimReplace = '$1^';
	createToken('CARET', `^${src[t.LONECARET]}${src[t.XRANGEPLAIN]}$`);
	createToken('CARETLOOSE', `^${src[t.LONECARET]}${src[t.XRANGEPLAINLOOSE]}$`);
	createToken('COMPARATORLOOSE', `^${src[t.GTLT]}\\s*(${src[t.LOOSEPLAIN]})$|^$`);
	createToken('COMPARATOR', `^${src[t.GTLT]}\\s*(${src[t.FULLPLAIN]})$|^$`);
	createToken('COMPARATORTRIM', `(\\s*)${src[t.GTLT]
	}\\s*(${src[t.LOOSEPLAIN]}|${src[t.XRANGEPLAIN]})`, true);
	exports.comparatorTrimReplace = '$1$2$3';
	createToken('HYPHENRANGE', `^\\s*(${src[t.XRANGEPLAIN]})` +
	                   `\\s+-\\s+` +
	                   `(${src[t.XRANGEPLAIN]})` +
	                   `\\s*$`);
	createToken('HYPHENRANGELOOSE', `^\\s*(${src[t.XRANGEPLAINLOOSE]})` +
	                        `\\s+-\\s+` +
	                        `(${src[t.XRANGEPLAINLOOSE]})` +
	                        `\\s*$`);
	createToken('STAR', '(<|>)?=?\\s*\\*');
	createToken('GTE0', '^\\s*>=\\s*0\\.0\\.0\\s*$');
	createToken('GTE0PRE', '^\\s*>=\\s*0\\.0\\.0-0\\s*$');
} (re$1, re$1.exports));
var reExports = re$1.exports;
getDefaultExportFromCjs(reExports);

const looseOption = Object.freeze({ loose: true });
const emptyOpts = Object.freeze({ });
const parseOptions$1 = options => {
  if (!options) {
    return emptyOpts
  }
  if (typeof options !== 'object') {
    return looseOption
  }
  return options
};
var parseOptions_1 = parseOptions$1;
getDefaultExportFromCjs(parseOptions_1);

const numeric = /^[0-9]+$/;
const compareIdentifiers$1 = (a, b) => {
  const anum = numeric.test(a);
  const bnum = numeric.test(b);
  if (anum && bnum) {
    a = +a;
    b = +b;
  }
  return a === b ? 0
    : (anum && !bnum) ? -1
    : (bnum && !anum) ? 1
    : a < b ? -1
    : 1
};
const rcompareIdentifiers = (a, b) => compareIdentifiers$1(b, a);
var identifiers = {
  compareIdentifiers: compareIdentifiers$1,
  rcompareIdentifiers,
};
getDefaultExportFromCjs(identifiers);

const debug = debug_1;
const { MAX_LENGTH, MAX_SAFE_INTEGER } = constants;
const { safeRe: re, t } = reExports;
const parseOptions = parseOptions_1;
const { compareIdentifiers } = identifiers;
let SemVer$2 = class SemVer {
  constructor (version, options) {
    options = parseOptions(options);
    if (version instanceof SemVer) {
      if (version.loose === !!options.loose &&
          version.includePrerelease === !!options.includePrerelease) {
        return version
      } else {
        version = version.version;
      }
    } else if (typeof version !== 'string') {
      throw new TypeError(`Invalid version. Must be a string. Got type "${typeof version}".`)
    }
    if (version.length > MAX_LENGTH) {
      throw new TypeError(
        `version is longer than ${MAX_LENGTH} characters`
      )
    }
    debug('SemVer', version, options);
    this.options = options;
    this.loose = !!options.loose;
    this.includePrerelease = !!options.includePrerelease;
    const m = version.trim().match(options.loose ? re[t.LOOSE] : re[t.FULL]);
    if (!m) {
      throw new TypeError(`Invalid Version: ${version}`)
    }
    this.raw = version;
    this.major = +m[1];
    this.minor = +m[2];
    this.patch = +m[3];
    if (this.major > MAX_SAFE_INTEGER || this.major < 0) {
      throw new TypeError('Invalid major version')
    }
    if (this.minor > MAX_SAFE_INTEGER || this.minor < 0) {
      throw new TypeError('Invalid minor version')
    }
    if (this.patch > MAX_SAFE_INTEGER || this.patch < 0) {
      throw new TypeError('Invalid patch version')
    }
    if (!m[4]) {
      this.prerelease = [];
    } else {
      this.prerelease = m[4].split('.').map((id) => {
        if (/^[0-9]+$/.test(id)) {
          const num = +id;
          if (num >= 0 && num < MAX_SAFE_INTEGER) {
            return num
          }
        }
        return id
      });
    }
    this.build = m[5] ? m[5].split('.') : [];
    this.format();
  }
  format () {
    this.version = `${this.major}.${this.minor}.${this.patch}`;
    if (this.prerelease.length) {
      this.version += `-${this.prerelease.join('.')}`;
    }
    return this.version
  }
  toString () {
    return this.version
  }
  compare (other) {
    debug('SemVer.compare', this.version, this.options, other);
    if (!(other instanceof SemVer)) {
      if (typeof other === 'string' && other === this.version) {
        return 0
      }
      other = new SemVer(other, this.options);
    }
    if (other.version === this.version) {
      return 0
    }
    return this.compareMain(other) || this.comparePre(other)
  }
  compareMain (other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options);
    }
    return (
      compareIdentifiers(this.major, other.major) ||
      compareIdentifiers(this.minor, other.minor) ||
      compareIdentifiers(this.patch, other.patch)
    )
  }
  comparePre (other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options);
    }
    if (this.prerelease.length && !other.prerelease.length) {
      return -1
    } else if (!this.prerelease.length && other.prerelease.length) {
      return 1
    } else if (!this.prerelease.length && !other.prerelease.length) {
      return 0
    }
    let i = 0;
    do {
      const a = this.prerelease[i];
      const b = other.prerelease[i];
      debug('prerelease compare', i, a, b);
      if (a === undefined && b === undefined) {
        return 0
      } else if (b === undefined) {
        return 1
      } else if (a === undefined) {
        return -1
      } else if (a === b) {
        continue
      } else {
        return compareIdentifiers(a, b)
      }
    } while (++i)
  }
  compareBuild (other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options);
    }
    let i = 0;
    do {
      const a = this.build[i];
      const b = other.build[i];
      debug('prerelease compare', i, a, b);
      if (a === undefined && b === undefined) {
        return 0
      } else if (b === undefined) {
        return 1
      } else if (a === undefined) {
        return -1
      } else if (a === b) {
        continue
      } else {
        return compareIdentifiers(a, b)
      }
    } while (++i)
  }
  inc (release, identifier, identifierBase) {
    switch (release) {
      case 'premajor':
        this.prerelease.length = 0;
        this.patch = 0;
        this.minor = 0;
        this.major++;
        this.inc('pre', identifier, identifierBase);
        break
      case 'preminor':
        this.prerelease.length = 0;
        this.patch = 0;
        this.minor++;
        this.inc('pre', identifier, identifierBase);
        break
      case 'prepatch':
        this.prerelease.length = 0;
        this.inc('patch', identifier, identifierBase);
        this.inc('pre', identifier, identifierBase);
        break
      case 'prerelease':
        if (this.prerelease.length === 0) {
          this.inc('patch', identifier, identifierBase);
        }
        this.inc('pre', identifier, identifierBase);
        break
      case 'major':
        if (
          this.minor !== 0 ||
          this.patch !== 0 ||
          this.prerelease.length === 0
        ) {
          this.major++;
        }
        this.minor = 0;
        this.patch = 0;
        this.prerelease = [];
        break
      case 'minor':
        if (this.patch !== 0 || this.prerelease.length === 0) {
          this.minor++;
        }
        this.patch = 0;
        this.prerelease = [];
        break
      case 'patch':
        if (this.prerelease.length === 0) {
          this.patch++;
        }
        this.prerelease = [];
        break
      case 'pre': {
        const base = Number(identifierBase) ? 1 : 0;
        if (!identifier && identifierBase === false) {
          throw new Error('invalid increment argument: identifier is empty')
        }
        if (this.prerelease.length === 0) {
          this.prerelease = [base];
        } else {
          let i = this.prerelease.length;
          while (--i >= 0) {
            if (typeof this.prerelease[i] === 'number') {
              this.prerelease[i]++;
              i = -2;
            }
          }
          if (i === -1) {
            if (identifier === this.prerelease.join('.') && identifierBase === false) {
              throw new Error('invalid increment argument: identifier already exists')
            }
            this.prerelease.push(base);
          }
        }
        if (identifier) {
          let prerelease = [identifier, base];
          if (identifierBase === false) {
            prerelease = [identifier];
          }
          if (compareIdentifiers(this.prerelease[0], identifier) === 0) {
            if (isNaN(this.prerelease[1])) {
              this.prerelease = prerelease;
            }
          } else {
            this.prerelease = prerelease;
          }
        }
        break
      }
      default:
        throw new Error(`invalid increment argument: ${release}`)
    }
    this.raw = this.format();
    if (this.build.length) {
      this.raw += `+${this.build.join('.')}`;
    }
    return this
  }
};
var semver = SemVer$2;
getDefaultExportFromCjs(semver);

const SemVer$1 = semver;
const parse = (version, options, throwErrors = false) => {
  if (version instanceof SemVer$1) {
    return version
  }
  try {
    return new SemVer$1(version, options)
  } catch (er) {
    if (!throwErrors) {
      return null
    }
    throw er
  }
};
var parse_1 = parse;
var semverParse = getDefaultExportFromCjs(parse_1);

const SemVer = semver;
const compare$1 = (a, b, loose) =>
  new SemVer(a, loose).compare(new SemVer(b, loose));
var compare_1 = compare$1;
getDefaultExportFromCjs(compare_1);

const compare = compare_1;
const lt = (a, b, loose) => compare(a, b, loose) < 0;
var lt_1 = lt;
var semverLt = getDefaultExportFromCjs(lt_1);

const allowedKeys = [
  "added",
  "napiVersion",
  "deprecated",
  "removed",
  "changes",
];
const changesExpectedKeys = ["version", "pr-url", "description"];
const VERSION_PLACEHOLDER = "REPLACEME";
const MAX_SAFE_SEMVER_VERSION = semverParse(
  Array.from({ length: 3 }, () => Number.MAX_SAFE_INTEGER).join("."),
);
const validVersionNumberRegex = /^v\d+\.\d+\.\d+$/;
const prUrlRegex = new RegExp("^https://github.com/nodejs/node/pull/\\d+$");
const privatePRUrl = "https://github.com/nodejs-private/node-private/pull/";
let releasedVersions;
let invalidVersionMessage = "version(s) must respect the pattern `vx.x.x` or";
if (process.env.NODE_RELEASED_VERSIONS) {
  console.log("Using release list from env...");
  releasedVersions = process.env.NODE_RELEASED_VERSIONS.split(",").map(
    (v) => `v${v}`,
  );
  invalidVersionMessage = `version not listed in the changelogs, `;
}
invalidVersionMessage += `use the placeholder \`${VERSION_PLACEHOLDER}\``;
const kContainsIllegalKey = Symbol("illegal key");
const kWrongKeyOrder = Symbol("Wrong key order");
function unorderedKeys(meta) {
  const keys = Object.keys(meta);
  let previousKeyIndex = -1;
  for (const key of keys) {
    const keyIndex = allowedKeys.indexOf(key);
    if (keyIndex <= previousKeyIndex) {
      return keyIndex === -1 ? kContainsIllegalKey : kWrongKeyOrder;
    }
    previousKeyIndex = keyIndex;
  }
}
function containsInvalidVersionNumber(version) {
  if (Array.isArray(version)) {
    return version.some(containsInvalidVersionNumber);
  }
  if (version === undefined || version === VERSION_PLACEHOLDER) return false;
  if (
    releasedVersions &&
    (version[1] !== "0" || (version[3] !== "0" && version[3] !== "1"))
  )
    return !releasedVersions.includes(version);
  return !validVersionNumberRegex.test(version);
}
const getValidSemver = (version) =>
  version === VERSION_PLACEHOLDER ? MAX_SAFE_SEMVER_VERSION : version;
function areVersionsUnordered(versions) {
  if (!Array.isArray(versions)) return false;
  for (let index = 1; index < versions.length; index++) {
    if (
      semverLt(
        getValidSemver(versions[index - 1]),
        getValidSemver(versions[index]),
      )
    ) {
      return true;
    }
  }
}
function invalidChangesKeys(change) {
  const keys = Object.keys(change);
  const { length } = keys;
  if (length !== changesExpectedKeys.length) return true;
  for (let index = 0; index < length; index++) {
    if (keys[index] !== changesExpectedKeys[index]) return true;
  }
}
function validateSecurityChange(file, node, change, index) {
  if ("commit" in change) {
    if (typeof change.commit !== "string" || isNaN(`0x${change.commit}`)) {
      file.message(
        `changes[${index}]: Ill-formed security change commit ID`,
        node,
      );
    }
    if (Object.keys(change)[1] === "commit") {
      change = { ...change };
      delete change.commit;
    }
  }
  if (invalidChangesKeys(change)) {
    const securityChangeExpectedKeys = [...changesExpectedKeys];
    securityChangeExpectedKeys[0] += "[, commit]";
    file.message(
      `changes[${index}]: Invalid keys. Expected keys are: ` +
        securityChangeExpectedKeys.join(", "),
      node,
    );
  }
}
function validateChanges(file, node, changes) {
  if (!Array.isArray(changes))
    return file.message("`changes` must be a YAML list", node);
  const changesVersions = [];
  for (let index = 0; index < changes.length; index++) {
    const change = changes[index];
    const isAncient =
      typeof change.version === "string" && change.version.startsWith("v0.");
    const isSecurityChange =
      !isAncient &&
      typeof change["pr-url"] === "string" &&
      change["pr-url"].startsWith(privatePRUrl);
    if (isSecurityChange) {
      validateSecurityChange(file, node, change, index);
    } else if (!isAncient && invalidChangesKeys(change)) {
      file.message(
        `changes[${index}]: Invalid keys. Expected keys are: ` +
          changesExpectedKeys.join(", "),
        node,
      );
    }
    if (containsInvalidVersionNumber(change.version)) {
      file.message(`changes[${index}]: ${invalidVersionMessage}`, node);
    } else if (areVersionsUnordered(change.version)) {
      file.message(`changes[${index}]: list of versions is not in order`, node);
    }
    if (!isAncient && !isSecurityChange && !prUrlRegex.test(change["pr-url"])) {
      file.message(
        `changes[${index}]: PR-URL does not match the expected pattern`,
        node,
      );
    }
    if (typeof change.description !== "string" || !change.description.length) {
      file.message(
        `changes[${index}]: must contain a non-empty description`,
        node,
      );
    } else if (!change.description.endsWith(".")) {
      file.message(
        `changes[${index}]: description must end with a period`,
        node,
      );
    }
    changesVersions.push(
      Array.isArray(change.version) ? change.version[0] : change.version,
    );
  }
  if (areVersionsUnordered(changesVersions)) {
    file.message("Items in `changes` list are not in order", node);
  }
}
function validateMeta(node, file, meta) {
  switch (unorderedKeys(meta)) {
    case kContainsIllegalKey:
      file.message(
        "YAML dictionary contains illegal keys. Accepted values are: " +
          allowedKeys.join(", "),
        node,
      );
      break;
    case kWrongKeyOrder:
      file.message(
        "YAML dictionary keys should be in this order: " +
          allowedKeys.join(", "),
        node,
      );
      break;
  }
  if (containsInvalidVersionNumber(meta.added)) {
    file.message(`Invalid \`added\` value: ${invalidVersionMessage}`, node);
  } else if (areVersionsUnordered(meta.added)) {
    file.message("Versions in `added` list are not in order", node);
  }
  if (containsInvalidVersionNumber(meta.deprecated)) {
    file.message(
      `Invalid \`deprecated\` value: ${invalidVersionMessage}`,
      node,
    );
  } else if (areVersionsUnordered(meta.deprecated)) {
    file.message("Versions in `deprecated` list are not in order", node);
  }
  if (containsInvalidVersionNumber(meta.removed)) {
    file.message(`Invalid \`removed\` value: ${invalidVersionMessage}`, node);
  } else if (areVersionsUnordered(meta.removed)) {
    file.message("Versions in `removed` list are not in order", node);
  }
  if ("changes" in meta) {
    validateChanges(file, node, meta.changes);
  }
}
function validateYAMLComments(tree, file) {
  visit$A(tree, "html", function visitor(node) {
    if (node.value.startsWith("<!--YAML\n"))
      file.message(
        "Expected `<!-- YAML`, found `<!--YAML`. Please add a space",
        node,
      );
    if (!node.value.startsWith("<!-- YAML\n")) return;
    try {
      const meta = jsYaml.load("#" + node.value.slice(0, -"-->".length));
      validateMeta(node, file, meta);
    } catch (e) {
      file.message(e, node);
    }
  });
}
const remarkLintNodejsYamlComments = lintRule(
  "remark-lint:nodejs-yaml-comments",
  validateYAMLComments,
);

const convert$5 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$5
      }
      if (typeof test === 'string') {
        return typeFactory$5(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$5(test) : propsFactory$5(test)
      }
      if (typeof test === 'function') {
        return castFactory$5(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$5(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$5(tests[index]);
  }
  return castFactory$5(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$5(check) {
  return castFactory$5(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$5(check) {
  return castFactory$5(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$5(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$5() {
  return true
}

function color$6(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$5 = true;
const EXIT$5 = false;
const SKIP$5 = 'skip';
const visitParents$5 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$5(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$6(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$5(visitor(node, parents));
            if (result[0] === EXIT$5) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$5) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$5) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$5(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$5, value]
  }
  return [value]
}

const visit$5 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$5(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

const remarkLintProhibitedStrings = lintRule('remark-lint:prohibited-strings', prohibitedStrings);
function testProhibited (val, content) {
  let regexpFlags = 'g';
  let no = val.no;
  if (!no) {
    no = escapeStringRegexp(val.yes);
    regexpFlags += 'i';
  }
  let regexpString = '(?<!\\.|@[a-zA-Z0-9/-]*)';
  let ignoreNextTo;
  if (val.ignoreNextTo) {
    if (Array.isArray(val.ignoreNextTo)) {
      const parts = val.ignoreNextTo.map(a => escapeStringRegexp(a)).join('|');
      ignoreNextTo = `(?:${parts})`;
    } else {
      ignoreNextTo = escapeStringRegexp(val.ignoreNextTo);
    }
  } else {
    ignoreNextTo = '';
  }
  const replaceCaptureGroups = !!val.replaceCaptureGroups;
  if (/^\b/.test(no)) {
    regexpString += '\\b';
  }
  if (ignoreNextTo) {
    regexpString += `(?<!${ignoreNextTo})`;
  }
  regexpString += `(${no})`;
  if (ignoreNextTo) {
    regexpString += `(?!${ignoreNextTo})`;
  }
  if (/\b$/.test(no)) {
    regexpString += '\\b';
  }
  regexpString += '(?!\\.\\w)';
  const re = new RegExp(regexpString, regexpFlags);
  const results = [];
  let result = re.exec(content);
  while (result) {
    if (result[1] !== val.yes) {
      let yes = val.yes;
      if (replaceCaptureGroups) {
        yes = result[1].replace(new RegExp(no), yes);
      }
      results.push({ result: result[1], index: result.index, yes: yes });
    }
    result = re.exec(content);
  }
  return results
}
function prohibitedStrings (ast, file, strings) {
  const myLocation = location(file);
  visit$5(ast, 'text', checkText);
  function checkText (node) {
    const content = node.value;
    const initial = pointStart(node).offset;
    strings.forEach((val) => {
      const results = testProhibited(val, content);
      if (results.length) {
        results.forEach(({ result, index, yes }) => {
          const message = val.yes ? `Use "${yes}" instead of "${result}"` : `Do not use "${result}"`;
          file.message(message, {
            start: myLocation.toPoint(initial + index),
            end: myLocation.toPoint(initial + index + [...result].length)
          });
        });
      }
    });
  }
}

const convert$4 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$4
      }
      if (typeof test === 'string') {
        return typeFactory$4(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$4(test) : propsFactory$4(test)
      }
      if (typeof test === 'function') {
        return castFactory$4(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$4(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$4(tests[index]);
  }
  return castFactory$4(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$4(check) {
  return castFactory$4(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$4(check) {
  return castFactory$4(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$4(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$4() {
  return true
}

function color$5(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$4 = true;
const EXIT$4 = false;
const SKIP$4 = 'skip';
const visitParents$4 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$4(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$5(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$4(visitor(node, parents));
            if (result[0] === EXIT$4) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$4) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$4) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$4(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$4, value]
  }
  return [value]
}

const visit$4 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$4(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that rules (thematic breaks, horizontal
 * rules) are consistent.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `string` (example: `'** * **'`, `'___'`)
 *     — thematic break to prefer
 * *   `'consistent'`
 *     — detect the first used style and warn when further rules differ
 *
 * ## Recommendation
 *
 * Rules consist of a `*`, `-`, or `_` character, which occurs at least three
 * times with nothing else except for arbitrary spaces or tabs on a single line.
 * Using spaces, tabs, and more than three markers seems unnecessary work to
 * type out.
 * Because asterisks can be used as a marker for more markdown constructs,
 * it’s recommended to use that for rules (and lists, emphasis, strong) too.
 * Due to this, it’s recommended to pass `'***'`.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats rules with `***` by default.
 * There are three settings to control rules:
 *
 * *   [`rule`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionsrule)
 *     (default: `'*'`) — marker
 * *   [`ruleRepetition`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionsrulerepetition)
 *     (default: `3`) — repetitions
 * *   [`ruleSpaces`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionsrulespaces)
 *     (default: `false`) — use spaces between markers
 *
 * @module rule-style
 * @summary
 *   remark-lint rule to warn when rule markers are inconsistent.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "config": "* * *"}
 *
 *   * * *
 *
 *   * * *
 *
 * @example
 *   {"name": "ok.md", "config": "_______"}
 *
 *   _______
 *
 *   _______
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ***
 *
 *   * * *
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   3:1-3:6: Rules should use `***`
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "config": "💩", "positionless": true}
 *
 *   1:1: Incorrect preferred rule style: provide a correct markdown rule or `'consistent'`
 */
const remarkLintRuleStyle = lintRule(
  {
    origin: 'remark-lint:rule-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-rule-style#readme'
  },
  (tree, file, option = 'consistent') => {
    const value = String(file);
    if (option !== 'consistent' && /[^-_* ]/.test(option)) {
      file.fail(
        "Incorrect preferred rule style: provide a correct markdown rule or `'consistent'`"
      );
    }
    visit$4(tree, 'thematicBreak', (node) => {
      const initial = pointStart(node).offset;
      const final = pointEnd(node).offset;
      if (typeof initial === 'number' && typeof final === 'number') {
        const rule = value.slice(initial, final);
        if (option === 'consistent') {
          option = rule;
        } else if (rule !== option) {
          file.message('Rules should use `' + option + '`', node);
        }
      }
    });
  }
);
var remarkLintRuleStyle$1 = remarkLintRuleStyle;

const convert$3 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$3
      }
      if (typeof test === 'string') {
        return typeFactory$3(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$3(test) : propsFactory$3(test)
      }
      if (typeof test === 'function') {
        return castFactory$3(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$3(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$3(tests[index]);
  }
  return castFactory$3(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$3(check) {
  return castFactory$3(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$3(check) {
  return castFactory$3(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$3(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$3() {
  return true
}

function color$4(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$3 = true;
const EXIT$3 = false;
const SKIP$3 = 'skip';
const visitParents$3 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$3(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$4(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$3(visitor(node, parents));
            if (result[0] === EXIT$3) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$3) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$3) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$3(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$3, value]
  }
  return [value]
}

const visit$3 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$3(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that strong markers are consistent.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `'*'`
 *     — prefer asterisks
 * *   `'_'`
 *     — prefer underscores
 * *   `'consistent'`
 *     — detect the first used style and warn when further strong differs
 *
 * ## Recommendation
 *
 * Underscores and asterisks work slightly different: asterisks can form strong
 * in more cases than underscores.
 * Because underscores are sometimes used to represent normal underscores inside
 * words, there are extra rules supporting that.
 * Asterisks can also be used as the marker of more constructs than underscores:
 * lists.
 * Due to having simpler parsing rules, looking more like syntax, and that they
 * can be used for more constructs, it’s recommended to prefer asterisks.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats strong with asterisks by default.
 * Pass
 * [`strong: '_'`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionsstrong)
 * to always use underscores.
 *
 * @module strong-marker
 * @summary
 *   remark-lint rule to warn when strong markers are inconsistent.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   **foo** and **bar**.
 *
 * @example
 *   {"name": "also-ok.md"}
 *
 *   __foo__ and __bar__.
 *
 * @example
 *   {"name": "ok.md", "config": "*"}
 *
 *   **foo**.
 *
 * @example
 *   {"name": "ok.md", "config": "_"}
 *
 *   __foo__.
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   **foo** and __bar__.
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:13-1:20: Strong should use `*` as a marker
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "config": "💩", "positionless": true}
 *
 *   1:1: Incorrect strong marker `💩`: use either `'consistent'`, `'*'`, or `'_'`
 */
const remarkLintStrongMarker = lintRule(
  {
    origin: 'remark-lint:strong-marker',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-strong-marker#readme'
  },
  (tree, file, option = 'consistent') => {
    const value = String(file);
    if (option !== '*' && option !== '_' && option !== 'consistent') {
      file.fail(
        'Incorrect strong marker `' +
          option +
          "`: use either `'consistent'`, `'*'`, or `'_'`"
      );
    }
    visit$3(tree, 'strong', (node) => {
      const start = pointStart(node).offset;
      if (typeof start === 'number') {
        const marker =  (value.charAt(start));
        if (option === 'consistent') {
          option = marker;
        } else if (marker !== option) {
          file.message('Strong should use `' + option + '` as a marker', node);
        }
      }
    });
  }
);
var remarkLintStrongMarker$1 = remarkLintStrongMarker;

const convert$2 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$2
      }
      if (typeof test === 'string') {
        return typeFactory$2(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$2(test) : propsFactory$2(test)
      }
      if (typeof test === 'function') {
        return castFactory$2(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$2(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$2(tests[index]);
  }
  return castFactory$2(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$2(check) {
  return castFactory$2(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$2(check) {
  return castFactory$2(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$2(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$2() {
  return true
}

function color$3(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$2 = true;
const EXIT$2 = false;
const SKIP$2 = 'skip';
const visitParents$2 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$2(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$3(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$2(visitor(node, parents));
            if (result[0] === EXIT$2) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$2) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$2) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$2(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$2, value]
  }
  return [value]
}

const visit$2 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$2(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that table cells are padded consistently.
 * Tables are a GFM feature enabled with
 * [`remark-gfm`](https://github.com/remarkjs/remark-gfm).
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `'padded'`
 *     — prefer at least one space between pipes and content
 * *   `'compact'`
 *     — prefer zero spaces between pipes and content
 * *   `'consistent'`
 *     — detect the first used style and warn when further tables differ
 *
 * ## Recommendation
 *
 * It’s recommended to use at least one space between pipes and content for
 * legibility of the markup (`'padded'`).
 *
 * ## Fix
 *
 * [`remark-gfm`](https://github.com/remarkjs/remark-gfm)
 * formats all table cells as padded by default.
 * Pass
 * [`tableCellPadding: false`](https://github.com/remarkjs/remark-gfm#optionstablecellpadding)
 * to use a more compact style.
 *
 * @module table-cell-padding
 * @summary
 *   remark-lint rule to warn when table cells are inconsistently padded.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "config": "padded", "gfm": true}
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "config": "padded", "gfm": true}
 *
 *   | A    |    B |
 *   | :----|----: |
 *   | Alpha|Bravo |
 *
 *   | C      |    D |
 *   | :----- | ---: |
 *   |Charlie | Delta|
 *
 *   Too much padding isn’t good either:
 *
 *   | E     | F        |   G    |      H |
 *   | :---- | -------- | :----: | -----: |
 *   | Echo  | Foxtrot  |  Golf  |  Hotel |
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "config": "padded", "gfm": true}
 *
 *   3:8: Cell should be padded
 *   3:9: Cell should be padded
 *   7:2: Cell should be padded
 *   7:17: Cell should be padded
 *   13:7: Cell should be padded with 1 space, not 2
 *   13:18: Cell should be padded with 1 space, not 2
 *   13:23: Cell should be padded with 1 space, not 2
 *   13:27: Cell should be padded with 1 space, not 2
 *   13:32: Cell should be padded with 1 space, not 2
 *
 * @example
 *   {"name": "ok.md", "config": "compact", "gfm": true}
 *
 *   |A    |B    |
 *   |-----|-----|
 *   |Alpha|Bravo|
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "config": "compact", "gfm": true}
 *
 *   |   A    | B    |
 *   |   -----| -----|
 *   |   Alpha| Bravo|
 *
 *   |C      |     D|
 *   |:------|-----:|
 *   |Charlie|Delta |
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "config": "compact", "gfm": true}
 *
 *   3:5: Cell should be compact
 *   3:12: Cell should be compact
 *   7:15: Cell should be compact
 *
 * @example
 *   {"name": "ok-padded.md", "config": "consistent", "gfm": true}
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 *   | C       | D     |
 *   | ------- | ----- |
 *   | Charlie | Delta |
 *
 * @example
 *   {"name": "not-ok-padded.md", "label": "input", "config": "consistent", "gfm": true}
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 *   | C      |     D |
 *   | :----- | ----: |
 *   |Charlie | Delta |
 *
 * @example
 *   {"name": "not-ok-padded.md", "label": "output", "config": "consistent", "gfm": true}
 *
 *   7:2: Cell should be padded
 *
 * @example
 *   {"name": "ok-compact.md", "config": "consistent", "gfm": true}
 *
 *   |A    |B    |
 *   |-----|-----|
 *   |Alpha|Bravo|
 *
 *   |C      |D    |
 *   |-------|-----|
 *   |Charlie|Delta|
 *
 * @example
 *   {"name": "not-ok-compact.md", "label": "input", "config": "consistent", "gfm": true}
 *
 *   |A    |B    |
 *   |-----|-----|
 *   |Alpha|Bravo|
 *
 *   |C      |     D|
 *   |:------|-----:|
 *   |Charlie|Delta |
 *
 * @example
 *   {"name": "not-ok-compact.md", "label": "output", "config": "consistent", "gfm": true}
 *
 *   7:15: Cell should be compact
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "config": "💩", "positionless": true, "gfm": true}
 *
 *   1:1: Incorrect table cell padding style `💩`, expected `'padded'`, `'compact'`, or `'consistent'`
 *
 * @example
 *   {"name": "empty.md", "label": "input", "config": "padded", "gfm": true}
 *
 *   <!-- Empty cells are OK, but those surrounding them may not be. -->
 *
 *   |        | Alpha | Bravo|
 *   | ------ | ----- | ---: |
 *   | Charlie|       |  Echo|
 *
 * @example
 *   {"name": "empty.md", "label": "output", "config": "padded", "gfm": true}
 *
 *   3:25: Cell should be padded
 *   5:10: Cell should be padded
 *   5:25: Cell should be padded
 *
 * @example
 *   {"name": "missing-body.md", "config": "padded", "gfm": true}
 *
 *   <!-- Missing cells are fine as well. -->
 *
 *   | Alpha | Bravo   | Charlie |
 *   | ----- | ------- | ------- |
 *   | Delta |
 *   | Echo  | Foxtrot |
 */
const remarkLintTableCellPadding = lintRule(
  {
    origin: 'remark-lint:table-cell-padding',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-table-cell-padding#readme'
  },
  (tree, file, option = 'consistent') => {
    if (
      option !== 'padded' &&
      option !== 'compact' &&
      option !== 'consistent'
    ) {
      file.fail(
        'Incorrect table cell padding style `' +
          option +
          "`, expected `'padded'`, `'compact'`, or `'consistent'`"
      );
    }
    visit$2(tree, 'table', (node) => {
      const rows = node.children;
      const align = node.align || [];
      const sizes = [];
      const entries = [];
      let index = -1;
      while (++index < align.length) {
        const alignment = align[index];
        sizes[index] = alignment === 'center' ? 3 : alignment ? 2 : 1;
      }
      index = -1;
      while (++index < rows.length) {
        const row = rows[index];
        let column = -1;
        while (++column < row.children.length) {
          const cell = row.children[column];
          const cellStart = pointStart(cell).offset;
          const cellEnd = pointEnd(cell).offset;
          const contentStart = pointStart(cell.children[0]).offset;
          const contentEnd = pointEnd(
            cell.children[cell.children.length - 1]
          ).offset;
          if (
            typeof cellStart !== 'number' ||
            typeof cellEnd !== 'number' ||
            typeof contentStart !== 'number' ||
            typeof contentEnd !== 'number'
          ) {
            continue
          }
          entries.push({
            node: cell,
            start: contentStart - cellStart - 1,
            end:
              cellEnd -
              contentEnd -
              (column === row.children.length - 1 ? 1 : 0),
            column
          });
          sizes[column] = Math.max(
            sizes[column] || 0,
            contentEnd - contentStart
          );
        }
      }
      const style =
        option === 'consistent'
          ? entries[0] && (!entries[0].start || !entries[0].end)
            ? 0
            : 1
          : option === 'padded'
          ? 1
          : 0;
      index = -1;
      while (++index < entries.length) {
        checkSide('start', entries[index], style, sizes);
        checkSide('end', entries[index], style, sizes);
      }
      return SKIP$2
    });
    function checkSide(side, entry, style, sizes) {
      const cell = entry.node;
      const column = entry.column;
      const spacing = entry[side];
      if (spacing === undefined || spacing === style) {
        return
      }
      let reason = 'Cell should be ';
      if (style === 0) {
        if (size(cell) < sizes[column]) {
          return
        }
        reason += 'compact';
      } else {
        reason += 'padded';
        if (spacing > style) {
          if (size(cell) < sizes[column]) {
            return
          }
          reason += ' with 1 space, not ' + spacing;
        }
      }
      file.message(
        reason,
        side === 'start'
          ? pointStart(cell.children[0])
          : pointEnd(cell.children[cell.children.length - 1])
      );
    }
  }
);
var remarkLintTableCellPadding$1 = remarkLintTableCellPadding;
function size(node) {
  const head = pointStart(node.children[0]).offset;
  const tail = pointEnd(node.children[node.children.length - 1]).offset;
  return typeof head === 'number' && typeof tail === 'number' ? tail - head : 0
}

const convert$1 =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok$1
      }
      if (typeof test === 'string') {
        return typeFactory$1(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory$1(test) : propsFactory$1(test)
      }
      if (typeof test === 'function') {
        return castFactory$1(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory$1(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert$1(tests[index]);
  }
  return castFactory$1(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory$1(check) {
  return castFactory$1(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory$1(check) {
  return castFactory$1(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory$1(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok$1() {
  return true
}

function color$2(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE$1 = true;
const EXIT$1 = false;
const SKIP$1 = 'skip';
const visitParents$1 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert$1(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$2(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$1(visitor(node, parents));
            if (result[0] === EXIT$1) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP$1) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT$1) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult$1(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE$1, value]
  }
  return [value]
}

const visit$1 =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents$1(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that tables have initial and final
 * delimiters.
 * Tables are a GFM feature enabled with
 * [`remark-gfm`](https://github.com/remarkjs/remark-gfm).
 *
 * ## API
 *
 * There are no options.
 *
 * ## Recommendation
 *
 * While tables don’t require initial or final delimiters (pipes before the
 * first and after the last cells in a row), it arguably does look weird.
 *
 * ## Fix
 *
 * [`remark-gfm`](https://github.com/remarkjs/remark-gfm)
 * formats all tables with initial and final delimiters.
 *
 * @module table-pipes
 * @summary
 *   remark-lint rule to warn when tables are missing initial and final
 *   delimiters.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md", "gfm": true}
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "gfm": true}
 *
 *   A     | B
 *   ----- | -----
 *   Alpha | Bravo
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "gfm": true}
 *
 *   1:1: Missing initial pipe in table fence
 *   1:10: Missing final pipe in table fence
 *   3:1: Missing initial pipe in table fence
 *   3:14: Missing final pipe in table fence
 */
const reasonStart = 'Missing initial pipe in table fence';
const reasonEnd = 'Missing final pipe in table fence';
const remarkLintTablePipes = lintRule(
  {
    origin: 'remark-lint:table-pipes',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-table-pipes#readme'
  },
  (tree, file) => {
    const value = String(file);
    visit$1(tree, 'table', (node) => {
      let index = -1;
      while (++index < node.children.length) {
        const row = node.children[index];
        const start = pointStart(row);
        const end = pointEnd(row);
        if (
          typeof start.offset === 'number' &&
          value.charCodeAt(start.offset) !== 124
        ) {
          file.message(reasonStart, start);
        }
        if (
          typeof end.offset === 'number' &&
          value.charCodeAt(end.offset - 1) !== 124
        ) {
          file.message(reasonEnd, end);
        }
      }
    });
  }
);
var remarkLintTablePipes$1 = remarkLintTablePipes;

const convert =
  (
    function (test) {
      if (test === undefined || test === null) {
        return ok
      }
      if (typeof test === 'string') {
        return typeFactory(test)
      }
      if (typeof test === 'object') {
        return Array.isArray(test) ? anyFactory(test) : propsFactory(test)
      }
      if (typeof test === 'function') {
        return castFactory(test)
      }
      throw new Error('Expected function, string, or object as test')
    }
  );
function anyFactory(tests) {
  const checks = [];
  let index = -1;
  while (++index < tests.length) {
    checks[index] = convert(tests[index]);
  }
  return castFactory(any)
  function any(...parameters) {
    let index = -1;
    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }
    return false
  }
}
function propsFactory(check) {
  return castFactory(all)
  function all(node) {
    let key;
    for (key in check) {
      if (node[key] !== check[key]) return false
    }
    return true
  }
}
function typeFactory(check) {
  return castFactory(type)
  function type(node) {
    return node && node.type === check
  }
}
function castFactory(check) {
  return assertion
  function assertion(node, ...parameters) {
    return Boolean(
      node &&
        typeof node === 'object' &&
        'type' in node &&
        Boolean(check.call(this, node, ...parameters))
    )
  }
}
function ok() {
  return true
}

function color$1(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

const CONTINUE = true;
const EXIT = false;
const SKIP = 'skip';
const visitParents =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      const is = convert(test);
      const step = reverse ? -1 : 1;
      factory(tree, undefined, [])();
      function factory(node, index, parents) {
        const value = node && typeof node === 'object' ? node : {};
        if (typeof value.type === 'string') {
          const name =
            typeof value.tagName === 'string'
              ? value.tagName
              :
              typeof value.name === 'string'
              ? value.name
              : undefined;
          Object.defineProperty(visit, 'name', {
            value:
              'node (' + color$1(node.type + (name ? '<' + name + '>' : '')) + ')'
          });
        }
        return visit
        function visit() {
          let result = [];
          let subresult;
          let offset;
          let grandparents;
          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult(visitor(node, parents));
            if (result[0] === EXIT) {
              return result
            }
          }
          if (node.children && result[0] !== SKIP) {
            offset = (reverse ? node.children.length : -1) + step;
            grandparents = parents.concat(node);
            while (offset > -1 && offset < node.children.length) {
              subresult = factory(node.children[offset], offset, grandparents)();
              if (subresult[0] === EXIT) {
                return subresult
              }
              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step;
            }
          }
          return result
        }
      }
    }
  );
function toResult(value) {
  if (Array.isArray(value)) {
    return value
  }
  if (typeof value === 'number') {
    return [CONTINUE, value]
  }
  return [value]
}

const visit =
  (
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }
      visitParents(tree, test, overload, reverse);
      function overload(node, parents) {
        const parent = parents[parents.length - 1];
        return visitor(
          node,
          parent ? parent.children.indexOf(node) : null,
          parent
        )
      }
    }
  );

/**
 * ## When should I use this?
 *
 * You can use this package to check that unordered list markers (bullets)
 * are consistent.
 *
 * ## API
 *
 * The following options (default: `'consistent'`) are accepted:
 *
 * *   `'*'`
 *     — prefer asterisks
 * *   `'+'`
 *     — prefer plusses
 * *   `'-'`
 *     — prefer dashes
 * *   `'consistent'`
 *     — detect the first used style and warn when further markers differ
 *
 * ## Recommendation
 *
 * Because asterisks can be used as a marker for more markdown constructs,
 * it’s recommended to use that for lists (and thematic breaks, emphasis,
 * strong) too.
 *
 * ## Fix
 *
 * [`remark-stringify`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify)
 * formats ordered lists with asterisks by default.
 * Pass
 * [`bullet: '+'` or `bullet: '-'`](https://github.com/remarkjs/remark/tree/main/packages/remark-stringify#optionsbullet)
 * to always use plusses or dashes.
 *
 * @module unordered-list-marker-style
 * @summary
 *   remark-lint rule to warn when unordered list markers are inconsistent.
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @example
 *   {"name": "ok.md"}
 *
 *   By default (`'consistent'`), if the file uses only one marker,
 *   that’s OK.
 *
 *   * Foo
 *   * Bar
 *   * Baz
 *
 *   Ordered lists are not affected.
 *
 *   1. Foo
 *   2. Bar
 *   3. Baz
 *
 * @example
 *   {"name": "ok.md", "config": "*"}
 *
 *   * Foo
 *
 * @example
 *   {"name": "ok.md", "config": "-"}
 *
 *   - Foo
 *
 * @example
 *   {"name": "ok.md", "config": "+"}
 *
 *   + Foo
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   * Foo
 *   - Bar
 *   + Baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   2:1-2:6: Marker style should be `*`
 *   3:1-3:6: Marker style should be `*`
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "config": "💩", "positionless": true}
 *
 *   1:1: Incorrect unordered list item marker style `💩`: use either `'-'`, `'*'`, or `'+'`
 */
const markers = new Set(['-', '*', '+']);
const remarkLintUnorderedListMarkerStyle = lintRule(
  {
    origin: 'remark-lint:unordered-list-marker-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-unordered-list-marker-style#readme'
  },
  (tree, file, option = 'consistent') => {
    const value = String(file);
    if (option !== 'consistent' && !markers.has(option)) {
      file.fail(
        'Incorrect unordered list item marker style `' +
          option +
          "`: use either `'-'`, `'*'`, or `'+'`"
      );
    }
    visit(tree, 'list', (node) => {
      if (node.ordered) return
      let index = -1;
      while (++index < node.children.length) {
        const child = node.children[index];
        if (!generated(child)) {
          const marker =  (
            value
              .slice(
                pointStart(child).offset,
                pointStart(child.children[0]).offset
              )
              .replace(/\[[x ]?]\s*$/i, '')
              .replace(/\s/g, '')
          );
          if (option === 'consistent') {
            option = marker;
          } else if (marker !== option) {
            file.message('Marker style should be `' + option + '`', child);
          }
        }
      }
    });
  }
);
var remarkLintUnorderedListMarkerStyle$1 = remarkLintUnorderedListMarkerStyle;

const plugins = [
  remarkGfm,
  remarkPresetLintRecommended$1,
  [remarkLintBlockquoteIndentation$1, 2],
  [remarkLintCheckboxCharacterStyle$1, { checked: "x", unchecked: " " }],
  remarkLintCheckboxContentIndent$1,
  [remarkLintCodeBlockStyle$1, "fenced"],
  remarkLintDefinitionSpacing$1,
  [
    remarkLintFencedCodeFlag$1,
    {
      flags: [
        "bash",
        "c",
        "cjs",
        "coffee",
        "console",
        "cpp",
        "diff",
        "http",
        "js",
        "json",
        "markdown",
        "mjs",
        "powershell",
        "r",
        "text",
        "ts",
      ],
    },
  ],
  [remarkLintFencedCodeMarker$1, "`"],
  [remarkLintFileExtension$1, "md"],
  remarkLintFinalDefinition$1,
  [remarkLintFirstHeadingLevel$1, 1],
  [remarkLintHeadingStyle$1, "atx"],
  [remarkLintListItemIndent$1, "space"],
  remarkLintMaximumLineLength$1,
  remarkLintNoConsecutiveBlankLines$1,
  remarkLintNoFileNameArticles$1,
  remarkLintNoFileNameConsecutiveDashes$1,
  remarkLintNofileNameOuterDashes$1,
  remarkLintNoHeadingIndent$1,
  remarkLintNoMultipleToplevelHeadings$1,
  remarkLintNoShellDollars$1,
  remarkLintNoTableIndentation$1,
  remarkLintNoTabs$1,
  remarkLintNoTrailingSpaces$1,
  remarkLintNodejsLinks,
  remarkLintNodejsYamlComments,
  [
    remarkLintProhibitedStrings,
    [
      { yes: "End-of-Life" },
      { no: "filesystem", yes: "file system" },
      { yes: "GitHub" },
      { no: "hostname", yes: "host name" },
      { yes: "JavaScript" },
      { no: "[Ll]ong[ -][Tt]erm [Ss]upport", yes: "Long Term Support" },
      { no: "Node", yes: "Node.js", ignoreNextTo: "-API" },
      { yes: "Node.js" },
      { no: "Node[Jj][Ss]", yes: "Node.js" },
      { no: "Node\\.js's?", yes: "the Node.js" },
      { no: "[Nn]ote that", yes: "<nothing>" },
      { yes: "RFC" },
      { no: "[Rr][Ff][Cc]\\d+", yes: "RFC <number>" },
      { yes: "Unix" },
      { yes: "Valgrind" },
      { yes: "V8" },
    ],
  ],
  remarkLintRuleStyle$1,
  [remarkLintStrongMarker$1, "*"],
  [remarkLintTableCellPadding$1, "padded"],
  remarkLintTablePipes$1,
  [remarkLintUnorderedListMarkerStyle$1, "*"],
];
const settings = {
  emphasis: "_",
  listItemIndent: "one",
  tightDefinitions: true,
};
const remarkPresetLintNode = { plugins, settings };

class VFileMessage extends Error {
  constructor(causeOrReason, optionsOrParentOrPlace, origin) {
    super();
    if (typeof optionsOrParentOrPlace === 'string') {
      origin = optionsOrParentOrPlace;
      optionsOrParentOrPlace = undefined;
    }
    let reason = '';
    let options = {};
    let legacyCause = false;
    if (optionsOrParentOrPlace) {
      if (
        'line' in optionsOrParentOrPlace &&
        'column' in optionsOrParentOrPlace
      ) {
        options = {place: optionsOrParentOrPlace};
      }
      else if (
        'start' in optionsOrParentOrPlace &&
        'end' in optionsOrParentOrPlace
      ) {
        options = {place: optionsOrParentOrPlace};
      }
      else if ('type' in optionsOrParentOrPlace) {
        options = {
          ancestors: [optionsOrParentOrPlace],
          place: optionsOrParentOrPlace.position
        };
      }
      else {
        options = {...optionsOrParentOrPlace};
      }
    }
    if (typeof causeOrReason === 'string') {
      reason = causeOrReason;
    }
    else if (!options.cause && causeOrReason) {
      legacyCause = true;
      reason = causeOrReason.message;
      options.cause = causeOrReason;
    }
    if (!options.ruleId && !options.source && typeof origin === 'string') {
      const index = origin.indexOf(':');
      if (index === -1) {
        options.ruleId = origin;
      } else {
        options.source = origin.slice(0, index);
        options.ruleId = origin.slice(index + 1);
      }
    }
    if (!options.place && options.ancestors && options.ancestors) {
      const parent = options.ancestors[options.ancestors.length - 1];
      if (parent) {
        options.place = parent.position;
      }
    }
    const start =
      options.place && 'start' in options.place
        ? options.place.start
        : options.place;
    this.ancestors = options.ancestors || undefined;
    this.cause = options.cause || undefined;
    this.column = start ? start.column : undefined;
    this.fatal = undefined;
    this.file;
    this.message = reason;
    this.line = start ? start.line : undefined;
    this.name = stringifyPosition$2(options.place) || '1:1';
    this.place = options.place || undefined;
    this.reason = this.message;
    this.ruleId = options.ruleId || undefined;
    this.source = options.source || undefined;
    this.stack =
      legacyCause && options.cause && typeof options.cause.stack === 'string'
        ? options.cause.stack
        : '';
    this.actual;
    this.expected;
    this.note;
    this.url;
  }
}
VFileMessage.prototype.file = '';
VFileMessage.prototype.name = '';
VFileMessage.prototype.reason = '';
VFileMessage.prototype.message = '';
VFileMessage.prototype.stack = '';
VFileMessage.prototype.column = undefined;
VFileMessage.prototype.line = undefined;
VFileMessage.prototype.ancestors = undefined;
VFileMessage.prototype.cause = undefined;
VFileMessage.prototype.fatal = undefined;
VFileMessage.prototype.place = undefined;
VFileMessage.prototype.ruleId = undefined;
VFileMessage.prototype.source = undefined;

function isUrl(fileUrlOrPath) {
  return Boolean(
    fileUrlOrPath !== null &&
      typeof fileUrlOrPath === 'object' &&
      'href' in fileUrlOrPath &&
      fileUrlOrPath.href &&
      'protocol' in fileUrlOrPath &&
      fileUrlOrPath.protocol &&
      fileUrlOrPath.auth === undefined
  )
}

const order =  ([
  'history',
  'path',
  'basename',
  'stem',
  'extname',
  'dirname'
]);
class VFile {
  constructor(value) {
    let options;
    if (!value) {
      options = {};
    } else if (isUrl(value)) {
      options = {path: value};
    } else if (typeof value === 'string' || isUint8Array$1(value)) {
      options = {value};
    } else {
      options = value;
    }
    this.cwd = process$1.cwd();
    this.data = {};
    this.history = [];
    this.messages = [];
    this.value;
    this.map;
    this.result;
    this.stored;
    let index = -1;
    while (++index < order.length) {
      const prop = order[index];
      if (
        prop in options &&
        options[prop] !== undefined &&
        options[prop] !== null
      ) {
        this[prop] = prop === 'history' ? [...options[prop]] : options[prop];
      }
    }
    let prop;
    for (prop in options) {
      if (!order.includes(prop)) {
        this[prop] = options[prop];
      }
    }
  }
  get basename() {
    return typeof this.path === 'string' ? path$1.basename(this.path) : undefined
  }
  set basename(basename) {
    assertNonEmpty(basename, 'basename');
    assertPart(basename, 'basename');
    this.path = path$1.join(this.dirname || '', basename);
  }
  get dirname() {
    return typeof this.path === 'string' ? path$1.dirname(this.path) : undefined
  }
  set dirname(dirname) {
    assertPath(this.basename, 'dirname');
    this.path = path$1.join(dirname || '', this.basename);
  }
  get extname() {
    return typeof this.path === 'string' ? path$1.extname(this.path) : undefined
  }
  set extname(extname) {
    assertPart(extname, 'extname');
    assertPath(this.dirname, 'extname');
    if (extname) {
      if (extname.codePointAt(0) !== 46 ) {
        throw new Error('`extname` must start with `.`')
      }
      if (extname.includes('.', 1)) {
        throw new Error('`extname` cannot contain multiple dots')
      }
    }
    this.path = path$1.join(this.dirname, this.stem + (extname || ''));
  }
  get path() {
    return this.history[this.history.length - 1]
  }
  set path(path) {
    if (isUrl(path)) {
      path = fileURLToPath(path);
    }
    assertNonEmpty(path, 'path');
    if (this.path !== path) {
      this.history.push(path);
    }
  }
  get stem() {
    return typeof this.path === 'string'
      ? path$1.basename(this.path, this.extname)
      : undefined
  }
  set stem(stem) {
    assertNonEmpty(stem, 'stem');
    assertPart(stem, 'stem');
    this.path = path$1.join(this.dirname || '', stem + (this.extname || ''));
  }
  fail(causeOrReason, optionsOrParentOrPlace, origin) {
    const message = this.message(causeOrReason, optionsOrParentOrPlace, origin);
    message.fatal = true;
    throw message
  }
  info(causeOrReason, optionsOrParentOrPlace, origin) {
    const message = this.message(causeOrReason, optionsOrParentOrPlace, origin);
    message.fatal = undefined;
    return message
  }
  message(causeOrReason, optionsOrParentOrPlace, origin) {
    const message = new VFileMessage(
      causeOrReason,
      optionsOrParentOrPlace,
      origin
    );
    if (this.path) {
      message.name = this.path + ':' + message.name;
      message.file = this.path;
    }
    message.fatal = false;
    this.messages.push(message);
    return message
  }
  toString(encoding) {
    if (this.value === undefined) {
      return ''
    }
    if (typeof this.value === 'string') {
      return this.value
    }
    const decoder = new TextDecoder(encoding || undefined);
    return decoder.decode(this.value)
  }
}
function assertPart(part, name) {
  if (part && part.includes(path$1.sep)) {
    throw new Error(
      '`' + name + '` cannot be a path: did not expect `' + path$1.sep + '`'
    )
  }
}
function assertNonEmpty(part, name) {
  if (!part) {
    throw new Error('`' + name + '` cannot be empty')
  }
}
function assertPath(path, name) {
  if (!path) {
    throw new Error('Setting `' + name + '` requires `path` to be set too')
  }
}
function isUint8Array$1(value) {
  return Boolean(
    value &&
      typeof value === 'object' &&
      'byteLength' in value &&
      'byteOffset' in value
  )
}

function read(description, options, callback) {
  const file = toVFile(description);
  if (!callback && typeof options === 'function') {
    callback = options;
    options = undefined;
  }
  if (!callback) {
    return new Promise(executor)
  }
  executor(resolve, callback);
  function resolve(result) {
    callback(undefined, result);
  }
  function executor(resolve, reject) {
    let fp;
    try {
      fp = path$1.resolve(file.cwd, file.path);
    } catch (error) {
      const exception =  (error);
      return reject(exception)
    }
    fs$1.readFile(fp, options, done);
    function done(error, result) {
      if (error) {
        reject(error);
      } else {
        file.value = result;
        resolve(file);
      }
    }
  }
}
function toVFile(description) {
  if (typeof description === 'string' || description instanceof URL) {
    description = {path: description};
  } else if (isUint8Array(description)) {
    description = {path: new TextDecoder().decode(description)};
  }
  return looksLikeAVFile(description) ? description : new VFile(description)
}
function looksLikeAVFile(value) {
  return Boolean(
    value &&
      typeof value === 'object' &&
      'message' in value &&
      'messages' in value
  )
}
function isUint8Array(value) {
  return Boolean(
    value &&
      typeof value === 'object' &&
      'byteLength' in value &&
      'byteOffset' in value
  )
}

function ansiRegex({onlyFirst = false} = {}) {
	const pattern = [
	    '[\\u001B\\u009B][[\\]()#;?]*(?:(?:(?:(?:;[-a-zA-Z\\d\\/#&.:=?%@~_]+)*|[a-zA-Z\\d]+(?:;[-a-zA-Z\\d\\/#&.:=?%@~_]*)*)?\\u0007)',
		'(?:(?:\\d{1,4}(?:;\\d{0,4})*)?[\\dA-PR-TZcf-ntqry=><~]))'
	].join('|');
	return new RegExp(pattern, onlyFirst ? undefined : 'g');
}

const regex = ansiRegex();
function stripAnsi(string) {
	if (typeof string !== 'string') {
		throw new TypeError(`Expected a \`string\`, got \`${typeof string}\``);
	}
	return string.replace(regex, '');
}

var eastasianwidth = {exports: {}};

(function (module) {
	var eaw = {};
	{
	  module.exports = eaw;
	}
	eaw.eastAsianWidth = function(character) {
	  var x = character.charCodeAt(0);
	  var y = (character.length == 2) ? character.charCodeAt(1) : 0;
	  var codePoint = x;
	  if ((0xD800 <= x && x <= 0xDBFF) && (0xDC00 <= y && y <= 0xDFFF)) {
	    x &= 0x3FF;
	    y &= 0x3FF;
	    codePoint = (x << 10) | y;
	    codePoint += 0x10000;
	  }
	  if ((0x3000 == codePoint) ||
	      (0xFF01 <= codePoint && codePoint <= 0xFF60) ||
	      (0xFFE0 <= codePoint && codePoint <= 0xFFE6)) {
	    return 'F';
	  }
	  if ((0x20A9 == codePoint) ||
	      (0xFF61 <= codePoint && codePoint <= 0xFFBE) ||
	      (0xFFC2 <= codePoint && codePoint <= 0xFFC7) ||
	      (0xFFCA <= codePoint && codePoint <= 0xFFCF) ||
	      (0xFFD2 <= codePoint && codePoint <= 0xFFD7) ||
	      (0xFFDA <= codePoint && codePoint <= 0xFFDC) ||
	      (0xFFE8 <= codePoint && codePoint <= 0xFFEE)) {
	    return 'H';
	  }
	  if ((0x1100 <= codePoint && codePoint <= 0x115F) ||
	      (0x11A3 <= codePoint && codePoint <= 0x11A7) ||
	      (0x11FA <= codePoint && codePoint <= 0x11FF) ||
	      (0x2329 <= codePoint && codePoint <= 0x232A) ||
	      (0x2E80 <= codePoint && codePoint <= 0x2E99) ||
	      (0x2E9B <= codePoint && codePoint <= 0x2EF3) ||
	      (0x2F00 <= codePoint && codePoint <= 0x2FD5) ||
	      (0x2FF0 <= codePoint && codePoint <= 0x2FFB) ||
	      (0x3001 <= codePoint && codePoint <= 0x303E) ||
	      (0x3041 <= codePoint && codePoint <= 0x3096) ||
	      (0x3099 <= codePoint && codePoint <= 0x30FF) ||
	      (0x3105 <= codePoint && codePoint <= 0x312D) ||
	      (0x3131 <= codePoint && codePoint <= 0x318E) ||
	      (0x3190 <= codePoint && codePoint <= 0x31BA) ||
	      (0x31C0 <= codePoint && codePoint <= 0x31E3) ||
	      (0x31F0 <= codePoint && codePoint <= 0x321E) ||
	      (0x3220 <= codePoint && codePoint <= 0x3247) ||
	      (0x3250 <= codePoint && codePoint <= 0x32FE) ||
	      (0x3300 <= codePoint && codePoint <= 0x4DBF) ||
	      (0x4E00 <= codePoint && codePoint <= 0xA48C) ||
	      (0xA490 <= codePoint && codePoint <= 0xA4C6) ||
	      (0xA960 <= codePoint && codePoint <= 0xA97C) ||
	      (0xAC00 <= codePoint && codePoint <= 0xD7A3) ||
	      (0xD7B0 <= codePoint && codePoint <= 0xD7C6) ||
	      (0xD7CB <= codePoint && codePoint <= 0xD7FB) ||
	      (0xF900 <= codePoint && codePoint <= 0xFAFF) ||
	      (0xFE10 <= codePoint && codePoint <= 0xFE19) ||
	      (0xFE30 <= codePoint && codePoint <= 0xFE52) ||
	      (0xFE54 <= codePoint && codePoint <= 0xFE66) ||
	      (0xFE68 <= codePoint && codePoint <= 0xFE6B) ||
	      (0x1B000 <= codePoint && codePoint <= 0x1B001) ||
	      (0x1F200 <= codePoint && codePoint <= 0x1F202) ||
	      (0x1F210 <= codePoint && codePoint <= 0x1F23A) ||
	      (0x1F240 <= codePoint && codePoint <= 0x1F248) ||
	      (0x1F250 <= codePoint && codePoint <= 0x1F251) ||
	      (0x20000 <= codePoint && codePoint <= 0x2F73F) ||
	      (0x2B740 <= codePoint && codePoint <= 0x2FFFD) ||
	      (0x30000 <= codePoint && codePoint <= 0x3FFFD)) {
	    return 'W';
	  }
	  if ((0x0020 <= codePoint && codePoint <= 0x007E) ||
	      (0x00A2 <= codePoint && codePoint <= 0x00A3) ||
	      (0x00A5 <= codePoint && codePoint <= 0x00A6) ||
	      (0x00AC == codePoint) ||
	      (0x00AF == codePoint) ||
	      (0x27E6 <= codePoint && codePoint <= 0x27ED) ||
	      (0x2985 <= codePoint && codePoint <= 0x2986)) {
	    return 'Na';
	  }
	  if ((0x00A1 == codePoint) ||
	      (0x00A4 == codePoint) ||
	      (0x00A7 <= codePoint && codePoint <= 0x00A8) ||
	      (0x00AA == codePoint) ||
	      (0x00AD <= codePoint && codePoint <= 0x00AE) ||
	      (0x00B0 <= codePoint && codePoint <= 0x00B4) ||
	      (0x00B6 <= codePoint && codePoint <= 0x00BA) ||
	      (0x00BC <= codePoint && codePoint <= 0x00BF) ||
	      (0x00C6 == codePoint) ||
	      (0x00D0 == codePoint) ||
	      (0x00D7 <= codePoint && codePoint <= 0x00D8) ||
	      (0x00DE <= codePoint && codePoint <= 0x00E1) ||
	      (0x00E6 == codePoint) ||
	      (0x00E8 <= codePoint && codePoint <= 0x00EA) ||
	      (0x00EC <= codePoint && codePoint <= 0x00ED) ||
	      (0x00F0 == codePoint) ||
	      (0x00F2 <= codePoint && codePoint <= 0x00F3) ||
	      (0x00F7 <= codePoint && codePoint <= 0x00FA) ||
	      (0x00FC == codePoint) ||
	      (0x00FE == codePoint) ||
	      (0x0101 == codePoint) ||
	      (0x0111 == codePoint) ||
	      (0x0113 == codePoint) ||
	      (0x011B == codePoint) ||
	      (0x0126 <= codePoint && codePoint <= 0x0127) ||
	      (0x012B == codePoint) ||
	      (0x0131 <= codePoint && codePoint <= 0x0133) ||
	      (0x0138 == codePoint) ||
	      (0x013F <= codePoint && codePoint <= 0x0142) ||
	      (0x0144 == codePoint) ||
	      (0x0148 <= codePoint && codePoint <= 0x014B) ||
	      (0x014D == codePoint) ||
	      (0x0152 <= codePoint && codePoint <= 0x0153) ||
	      (0x0166 <= codePoint && codePoint <= 0x0167) ||
	      (0x016B == codePoint) ||
	      (0x01CE == codePoint) ||
	      (0x01D0 == codePoint) ||
	      (0x01D2 == codePoint) ||
	      (0x01D4 == codePoint) ||
	      (0x01D6 == codePoint) ||
	      (0x01D8 == codePoint) ||
	      (0x01DA == codePoint) ||
	      (0x01DC == codePoint) ||
	      (0x0251 == codePoint) ||
	      (0x0261 == codePoint) ||
	      (0x02C4 == codePoint) ||
	      (0x02C7 == codePoint) ||
	      (0x02C9 <= codePoint && codePoint <= 0x02CB) ||
	      (0x02CD == codePoint) ||
	      (0x02D0 == codePoint) ||
	      (0x02D8 <= codePoint && codePoint <= 0x02DB) ||
	      (0x02DD == codePoint) ||
	      (0x02DF == codePoint) ||
	      (0x0300 <= codePoint && codePoint <= 0x036F) ||
	      (0x0391 <= codePoint && codePoint <= 0x03A1) ||
	      (0x03A3 <= codePoint && codePoint <= 0x03A9) ||
	      (0x03B1 <= codePoint && codePoint <= 0x03C1) ||
	      (0x03C3 <= codePoint && codePoint <= 0x03C9) ||
	      (0x0401 == codePoint) ||
	      (0x0410 <= codePoint && codePoint <= 0x044F) ||
	      (0x0451 == codePoint) ||
	      (0x2010 == codePoint) ||
	      (0x2013 <= codePoint && codePoint <= 0x2016) ||
	      (0x2018 <= codePoint && codePoint <= 0x2019) ||
	      (0x201C <= codePoint && codePoint <= 0x201D) ||
	      (0x2020 <= codePoint && codePoint <= 0x2022) ||
	      (0x2024 <= codePoint && codePoint <= 0x2027) ||
	      (0x2030 == codePoint) ||
	      (0x2032 <= codePoint && codePoint <= 0x2033) ||
	      (0x2035 == codePoint) ||
	      (0x203B == codePoint) ||
	      (0x203E == codePoint) ||
	      (0x2074 == codePoint) ||
	      (0x207F == codePoint) ||
	      (0x2081 <= codePoint && codePoint <= 0x2084) ||
	      (0x20AC == codePoint) ||
	      (0x2103 == codePoint) ||
	      (0x2105 == codePoint) ||
	      (0x2109 == codePoint) ||
	      (0x2113 == codePoint) ||
	      (0x2116 == codePoint) ||
	      (0x2121 <= codePoint && codePoint <= 0x2122) ||
	      (0x2126 == codePoint) ||
	      (0x212B == codePoint) ||
	      (0x2153 <= codePoint && codePoint <= 0x2154) ||
	      (0x215B <= codePoint && codePoint <= 0x215E) ||
	      (0x2160 <= codePoint && codePoint <= 0x216B) ||
	      (0x2170 <= codePoint && codePoint <= 0x2179) ||
	      (0x2189 == codePoint) ||
	      (0x2190 <= codePoint && codePoint <= 0x2199) ||
	      (0x21B8 <= codePoint && codePoint <= 0x21B9) ||
	      (0x21D2 == codePoint) ||
	      (0x21D4 == codePoint) ||
	      (0x21E7 == codePoint) ||
	      (0x2200 == codePoint) ||
	      (0x2202 <= codePoint && codePoint <= 0x2203) ||
	      (0x2207 <= codePoint && codePoint <= 0x2208) ||
	      (0x220B == codePoint) ||
	      (0x220F == codePoint) ||
	      (0x2211 == codePoint) ||
	      (0x2215 == codePoint) ||
	      (0x221A == codePoint) ||
	      (0x221D <= codePoint && codePoint <= 0x2220) ||
	      (0x2223 == codePoint) ||
	      (0x2225 == codePoint) ||
	      (0x2227 <= codePoint && codePoint <= 0x222C) ||
	      (0x222E == codePoint) ||
	      (0x2234 <= codePoint && codePoint <= 0x2237) ||
	      (0x223C <= codePoint && codePoint <= 0x223D) ||
	      (0x2248 == codePoint) ||
	      (0x224C == codePoint) ||
	      (0x2252 == codePoint) ||
	      (0x2260 <= codePoint && codePoint <= 0x2261) ||
	      (0x2264 <= codePoint && codePoint <= 0x2267) ||
	      (0x226A <= codePoint && codePoint <= 0x226B) ||
	      (0x226E <= codePoint && codePoint <= 0x226F) ||
	      (0x2282 <= codePoint && codePoint <= 0x2283) ||
	      (0x2286 <= codePoint && codePoint <= 0x2287) ||
	      (0x2295 == codePoint) ||
	      (0x2299 == codePoint) ||
	      (0x22A5 == codePoint) ||
	      (0x22BF == codePoint) ||
	      (0x2312 == codePoint) ||
	      (0x2460 <= codePoint && codePoint <= 0x24E9) ||
	      (0x24EB <= codePoint && codePoint <= 0x254B) ||
	      (0x2550 <= codePoint && codePoint <= 0x2573) ||
	      (0x2580 <= codePoint && codePoint <= 0x258F) ||
	      (0x2592 <= codePoint && codePoint <= 0x2595) ||
	      (0x25A0 <= codePoint && codePoint <= 0x25A1) ||
	      (0x25A3 <= codePoint && codePoint <= 0x25A9) ||
	      (0x25B2 <= codePoint && codePoint <= 0x25B3) ||
	      (0x25B6 <= codePoint && codePoint <= 0x25B7) ||
	      (0x25BC <= codePoint && codePoint <= 0x25BD) ||
	      (0x25C0 <= codePoint && codePoint <= 0x25C1) ||
	      (0x25C6 <= codePoint && codePoint <= 0x25C8) ||
	      (0x25CB == codePoint) ||
	      (0x25CE <= codePoint && codePoint <= 0x25D1) ||
	      (0x25E2 <= codePoint && codePoint <= 0x25E5) ||
	      (0x25EF == codePoint) ||
	      (0x2605 <= codePoint && codePoint <= 0x2606) ||
	      (0x2609 == codePoint) ||
	      (0x260E <= codePoint && codePoint <= 0x260F) ||
	      (0x2614 <= codePoint && codePoint <= 0x2615) ||
	      (0x261C == codePoint) ||
	      (0x261E == codePoint) ||
	      (0x2640 == codePoint) ||
	      (0x2642 == codePoint) ||
	      (0x2660 <= codePoint && codePoint <= 0x2661) ||
	      (0x2663 <= codePoint && codePoint <= 0x2665) ||
	      (0x2667 <= codePoint && codePoint <= 0x266A) ||
	      (0x266C <= codePoint && codePoint <= 0x266D) ||
	      (0x266F == codePoint) ||
	      (0x269E <= codePoint && codePoint <= 0x269F) ||
	      (0x26BE <= codePoint && codePoint <= 0x26BF) ||
	      (0x26C4 <= codePoint && codePoint <= 0x26CD) ||
	      (0x26CF <= codePoint && codePoint <= 0x26E1) ||
	      (0x26E3 == codePoint) ||
	      (0x26E8 <= codePoint && codePoint <= 0x26FF) ||
	      (0x273D == codePoint) ||
	      (0x2757 == codePoint) ||
	      (0x2776 <= codePoint && codePoint <= 0x277F) ||
	      (0x2B55 <= codePoint && codePoint <= 0x2B59) ||
	      (0x3248 <= codePoint && codePoint <= 0x324F) ||
	      (0xE000 <= codePoint && codePoint <= 0xF8FF) ||
	      (0xFE00 <= codePoint && codePoint <= 0xFE0F) ||
	      (0xFFFD == codePoint) ||
	      (0x1F100 <= codePoint && codePoint <= 0x1F10A) ||
	      (0x1F110 <= codePoint && codePoint <= 0x1F12D) ||
	      (0x1F130 <= codePoint && codePoint <= 0x1F169) ||
	      (0x1F170 <= codePoint && codePoint <= 0x1F19A) ||
	      (0xE0100 <= codePoint && codePoint <= 0xE01EF) ||
	      (0xF0000 <= codePoint && codePoint <= 0xFFFFD) ||
	      (0x100000 <= codePoint && codePoint <= 0x10FFFD)) {
	    return 'A';
	  }
	  return 'N';
	};
	eaw.characterLength = function(character) {
	  var code = this.eastAsianWidth(character);
	  if (code == 'F' || code == 'W' || code == 'A') {
	    return 2;
	  } else {
	    return 1;
	  }
	};
	function stringToArray(string) {
	  return string.match(/[\uD800-\uDBFF][\uDC00-\uDFFF]|[^\uD800-\uDFFF]/g) || [];
	}
	eaw.length = function(string) {
	  var characters = stringToArray(string);
	  var len = 0;
	  for (var i = 0; i < characters.length; i++) {
	    len = len + this.characterLength(characters[i]);
	  }
	  return len;
	};
	eaw.slice = function(text, start, end) {
	  textLen = eaw.length(text);
	  start = start ? start : 0;
	  end = end ? end : 1;
	  if (start < 0) {
	      start = textLen + start;
	  }
	  if (end < 0) {
	      end = textLen + end;
	  }
	  var result = '';
	  var eawLen = 0;
	  var chars = stringToArray(text);
	  for (var i = 0; i < chars.length; i++) {
	    var char = chars[i];
	    var charLen = eaw.length(char);
	    if (eawLen >= start - (charLen == 2 ? 1 : 0)) {
	        if (eawLen + charLen <= end) {
	            result += char;
	        } else {
	            break;
	        }
	    }
	    eawLen += charLen;
	  }
	  return result;
	};
} (eastasianwidth));
var eastasianwidthExports = eastasianwidth.exports;
var eastAsianWidth = getDefaultExportFromCjs(eastasianwidthExports);

var emojiRegex = () => {
	return /[#*0-9]\uFE0F?\u20E3|[\xA9\xAE\u203C\u2049\u2122\u2139\u2194-\u2199\u21A9\u21AA\u231A\u231B\u2328\u23CF\u23ED-\u23EF\u23F1\u23F2\u23F8-\u23FA\u24C2\u25AA\u25AB\u25B6\u25C0\u25FB\u25FC\u25FE\u2600-\u2604\u260E\u2611\u2614\u2615\u2618\u2620\u2622\u2623\u2626\u262A\u262E\u262F\u2638-\u263A\u2640\u2642\u2648-\u2653\u265F\u2660\u2663\u2665\u2666\u2668\u267B\u267E\u267F\u2692\u2694-\u2697\u2699\u269B\u269C\u26A0\u26A7\u26AA\u26B0\u26B1\u26BD\u26BE\u26C4\u26C8\u26CF\u26D1\u26E9\u26F0-\u26F5\u26F7\u26F8\u26FA\u2702\u2708\u2709\u270F\u2712\u2714\u2716\u271D\u2721\u2733\u2734\u2744\u2747\u2757\u2763\u27A1\u2934\u2935\u2B05-\u2B07\u2B1B\u2B1C\u2B55\u3030\u303D\u3297\u3299]\uFE0F?|[\u261D\u270C\u270D](?:\uFE0F|\uD83C[\uDFFB-\uDFFF])?|[\u270A\u270B](?:\uD83C[\uDFFB-\uDFFF])?|[\u23E9-\u23EC\u23F0\u23F3\u25FD\u2693\u26A1\u26AB\u26C5\u26CE\u26D4\u26EA\u26FD\u2705\u2728\u274C\u274E\u2753-\u2755\u2795-\u2797\u27B0\u27BF\u2B50]|\u26D3\uFE0F?(?:\u200D\uD83D\uDCA5)?|\u26F9(?:\uFE0F|\uD83C[\uDFFB-\uDFFF])?(?:\u200D[\u2640\u2642]\uFE0F?)?|\u2764\uFE0F?(?:\u200D(?:\uD83D\uDD25|\uD83E\uDE79))?|\uD83C(?:[\uDC04\uDD70\uDD71\uDD7E\uDD7F\uDE02\uDE37\uDF21\uDF24-\uDF2C\uDF36\uDF7D\uDF96\uDF97\uDF99-\uDF9B\uDF9E\uDF9F\uDFCD\uDFCE\uDFD4-\uDFDF\uDFF5\uDFF7]\uFE0F?|[\uDF85\uDFC2\uDFC7](?:\uD83C[\uDFFB-\uDFFF])?|[\uDFC4\uDFCA](?:\uD83C[\uDFFB-\uDFFF])?(?:\u200D[\u2640\u2642]\uFE0F?)?|[\uDFCB\uDFCC](?:\uFE0F|\uD83C[\uDFFB-\uDFFF])?(?:\u200D[\u2640\u2642]\uFE0F?)?|[\uDCCF\uDD8E\uDD91-\uDD9A\uDE01\uDE1A\uDE2F\uDE32-\uDE36\uDE38-\uDE3A\uDE50\uDE51\uDF00-\uDF20\uDF2D-\uDF35\uDF37-\uDF43\uDF45-\uDF4A\uDF4C-\uDF7C\uDF7E-\uDF84\uDF86-\uDF93\uDFA0-\uDFC1\uDFC5\uDFC6\uDFC8\uDFC9\uDFCF-\uDFD3\uDFE0-\uDFF0\uDFF8-\uDFFF]|\uDDE6\uD83C[\uDDE8-\uDDEC\uDDEE\uDDF1\uDDF2\uDDF4\uDDF6-\uDDFA\uDDFC\uDDFD\uDDFF]|\uDDE7\uD83C[\uDDE6\uDDE7\uDDE9-\uDDEF\uDDF1-\uDDF4\uDDF6-\uDDF9\uDDFB\uDDFC\uDDFE\uDDFF]|\uDDE8\uD83C[\uDDE6\uDDE8\uDDE9\uDDEB-\uDDEE\uDDF0-\uDDF5\uDDF7\uDDFA-\uDDFF]|\uDDE9\uD83C[\uDDEA\uDDEC\uDDEF\uDDF0\uDDF2\uDDF4\uDDFF]|\uDDEA\uD83C[\uDDE6\uDDE8\uDDEA\uDDEC\uDDED\uDDF7-\uDDFA]|\uDDEB\uD83C[\uDDEE-\uDDF0\uDDF2\uDDF4\uDDF7]|\uDDEC\uD83C[\uDDE6\uDDE7\uDDE9-\uDDEE\uDDF1-\uDDF3\uDDF5-\uDDFA\uDDFC\uDDFE]|\uDDED\uD83C[\uDDF0\uDDF2\uDDF3\uDDF7\uDDF9\uDDFA]|\uDDEE\uD83C[\uDDE8-\uDDEA\uDDF1-\uDDF4\uDDF6-\uDDF9]|\uDDEF\uD83C[\uDDEA\uDDF2\uDDF4\uDDF5]|\uDDF0\uD83C[\uDDEA\uDDEC-\uDDEE\uDDF2\uDDF3\uDDF5\uDDF7\uDDFC\uDDFE\uDDFF]|\uDDF1\uD83C[\uDDE6-\uDDE8\uDDEE\uDDF0\uDDF7-\uDDFB\uDDFE]|\uDDF2\uD83C[\uDDE6\uDDE8-\uDDED\uDDF0-\uDDFF]|\uDDF3\uD83C[\uDDE6\uDDE8\uDDEA-\uDDEC\uDDEE\uDDF1\uDDF4\uDDF5\uDDF7\uDDFA\uDDFF]|\uDDF4\uD83C\uDDF2|\uDDF5\uD83C[\uDDE6\uDDEA-\uDDED\uDDF0-\uDDF3\uDDF7-\uDDF9\uDDFC\uDDFE]|\uDDF6\uD83C\uDDE6|\uDDF7\uD83C[\uDDEA\uDDF4\uDDF8\uDDFA\uDDFC]|\uDDF8\uD83C[\uDDE6-\uDDEA\uDDEC-\uDDF4\uDDF7-\uDDF9\uDDFB\uDDFD-\uDDFF]|\uDDF9\uD83C[\uDDE6\uDDE8\uDDE9\uDDEB-\uDDED\uDDEF-\uDDF4\uDDF7\uDDF9\uDDFB\uDDFC\uDDFF]|\uDDFA\uD83C[\uDDE6\uDDEC\uDDF2\uDDF3\uDDF8\uDDFE\uDDFF]|\uDDFB\uD83C[\uDDE6\uDDE8\uDDEA\uDDEC\uDDEE\uDDF3\uDDFA]|\uDDFC\uD83C[\uDDEB\uDDF8]|\uDDFD\uD83C\uDDF0|\uDDFE\uD83C[\uDDEA\uDDF9]|\uDDFF\uD83C[\uDDE6\uDDF2\uDDFC]|\uDF44(?:\u200D\uD83D\uDFEB)?|\uDF4B(?:\u200D\uD83D\uDFE9)?|\uDFC3(?:\uD83C[\uDFFB-\uDFFF])?(?:\u200D(?:[\u2640\u2642]\uFE0F?(?:\u200D\u27A1\uFE0F?)?|\u27A1\uFE0F?))?|\uDFF3\uFE0F?(?:\u200D(?:\u26A7\uFE0F?|\uD83C\uDF08))?|\uDFF4(?:\u200D\u2620\uFE0F?|\uDB40\uDC67\uDB40\uDC62\uDB40(?:\uDC65\uDB40\uDC6E\uDB40\uDC67|\uDC73\uDB40\uDC63\uDB40\uDC74|\uDC77\uDB40\uDC6C\uDB40\uDC73)\uDB40\uDC7F)?)|\uD83D(?:[\uDC3F\uDCFD\uDD49\uDD4A\uDD6F\uDD70\uDD73\uDD76-\uDD79\uDD87\uDD8A-\uDD8D\uDDA5\uDDA8\uDDB1\uDDB2\uDDBC\uDDC2-\uDDC4\uDDD1-\uDDD3\uDDDC-\uDDDE\uDDE1\uDDE3\uDDE8\uDDEF\uDDF3\uDDFA\uDECB\uDECD-\uDECF\uDEE0-\uDEE5\uDEE9\uDEF0\uDEF3]\uFE0F?|[\uDC42\uDC43\uDC46-\uDC50\uDC66\uDC67\uDC6B-\uDC6D\uDC72\uDC74-\uDC76\uDC78\uDC7C\uDC83\uDC85\uDC8F\uDC91\uDCAA\uDD7A\uDD95\uDD96\uDE4C\uDE4F\uDEC0\uDECC](?:\uD83C[\uDFFB-\uDFFF])?|[\uDC6E\uDC70\uDC71\uDC73\uDC77\uDC81\uDC82\uDC86\uDC87\uDE45-\uDE47\uDE4B\uDE4D\uDE4E\uDEA3\uDEB4\uDEB5](?:\uD83C[\uDFFB-\uDFFF])?(?:\u200D[\u2640\u2642]\uFE0F?)?|[\uDD74\uDD90](?:\uFE0F|\uD83C[\uDFFB-\uDFFF])?|[\uDC00-\uDC07\uDC09-\uDC14\uDC16-\uDC25\uDC27-\uDC3A\uDC3C-\uDC3E\uDC40\uDC44\uDC45\uDC51-\uDC65\uDC6A\uDC79-\uDC7B\uDC7D-\uDC80\uDC84\uDC88-\uDC8E\uDC90\uDC92-\uDCA9\uDCAB-\uDCFC\uDCFF-\uDD3D\uDD4B-\uDD4E\uDD50-\uDD67\uDDA4\uDDFB-\uDE2D\uDE2F-\uDE34\uDE37-\uDE41\uDE43\uDE44\uDE48-\uDE4A\uDE80-\uDEA2\uDEA4-\uDEB3\uDEB7-\uDEBF\uDEC1-\uDEC5\uDED0-\uDED2\uDED5-\uDED7\uDEDC-\uDEDF\uDEEB\uDEEC\uDEF4-\uDEFC\uDFE0-\uDFEB\uDFF0]|\uDC08(?:\u200D\u2B1B)?|\uDC15(?:\u200D\uD83E\uDDBA)?|\uDC26(?:\u200D(?:\u2B1B|\uD83D\uDD25))?|\uDC3B(?:\u200D\u2744\uFE0F?)?|\uDC41\uFE0F?(?:\u200D\uD83D\uDDE8\uFE0F?)?|\uDC68(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:\uDC8B\u200D\uD83D)?\uDC68|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D(?:[\uDC68\uDC69]\u200D\uD83D(?:\uDC66(?:\u200D\uD83D\uDC66)?|\uDC67(?:\u200D\uD83D[\uDC66\uDC67])?)|[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uDC66(?:\u200D\uD83D\uDC66)?|\uDC67(?:\u200D\uD83D[\uDC66\uDC67])?)|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]))|\uD83C(?:\uDFFB(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:\uDC8B\u200D\uD83D)?\uDC68\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D\uDC68\uD83C[\uDFFC-\uDFFF])))?|\uDFFC(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:\uDC8B\u200D\uD83D)?\uDC68\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D\uDC68\uD83C[\uDFFB\uDFFD-\uDFFF])))?|\uDFFD(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:\uDC8B\u200D\uD83D)?\uDC68\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D\uDC68\uD83C[\uDFFB\uDFFC\uDFFE\uDFFF])))?|\uDFFE(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:\uDC8B\u200D\uD83D)?\uDC68\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D\uDC68\uD83C[\uDFFB-\uDFFD\uDFFF])))?|\uDFFF(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:\uDC8B\u200D\uD83D)?\uDC68\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D\uDC68\uD83C[\uDFFB-\uDFFE])))?))?|\uDC69(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:\uDC8B\u200D\uD83D)?[\uDC68\uDC69]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D(?:[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uDC66(?:\u200D\uD83D\uDC66)?|\uDC67(?:\u200D\uD83D[\uDC66\uDC67])?|\uDC69\u200D\uD83D(?:\uDC66(?:\u200D\uD83D\uDC66)?|\uDC67(?:\u200D\uD83D[\uDC66\uDC67])?))|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]))|\uD83C(?:\uDFFB(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:[\uDC68\uDC69]|\uDC8B\u200D\uD83D[\uDC68\uDC69])\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D[\uDC68\uDC69]\uD83C[\uDFFC-\uDFFF])))?|\uDFFC(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:[\uDC68\uDC69]|\uDC8B\u200D\uD83D[\uDC68\uDC69])\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D[\uDC68\uDC69]\uD83C[\uDFFB\uDFFD-\uDFFF])))?|\uDFFD(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:[\uDC68\uDC69]|\uDC8B\u200D\uD83D[\uDC68\uDC69])\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D[\uDC68\uDC69]\uD83C[\uDFFB\uDFFC\uDFFE\uDFFF])))?|\uDFFE(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:[\uDC68\uDC69]|\uDC8B\u200D\uD83D[\uDC68\uDC69])\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D[\uDC68\uDC69]\uD83C[\uDFFB-\uDFFD\uDFFF])))?|\uDFFF(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D\uD83D(?:[\uDC68\uDC69]|\uDC8B\u200D\uD83D[\uDC68\uDC69])\uD83C[\uDFFB-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83D[\uDC68\uDC69]\uD83C[\uDFFB-\uDFFE])))?))?|\uDC6F(?:\u200D[\u2640\u2642]\uFE0F?)?|\uDD75(?:\uFE0F|\uD83C[\uDFFB-\uDFFF])?(?:\u200D[\u2640\u2642]\uFE0F?)?|\uDE2E(?:\u200D\uD83D\uDCA8)?|\uDE35(?:\u200D\uD83D\uDCAB)?|\uDE36(?:\u200D\uD83C\uDF2B\uFE0F?)?|\uDE42(?:\u200D[\u2194\u2195]\uFE0F?)?|\uDEB6(?:\uD83C[\uDFFB-\uDFFF])?(?:\u200D(?:[\u2640\u2642]\uFE0F?(?:\u200D\u27A1\uFE0F?)?|\u27A1\uFE0F?))?)|\uD83E(?:[\uDD0C\uDD0F\uDD18-\uDD1F\uDD30-\uDD34\uDD36\uDD77\uDDB5\uDDB6\uDDBB\uDDD2\uDDD3\uDDD5\uDEC3-\uDEC5\uDEF0\uDEF2-\uDEF8](?:\uD83C[\uDFFB-\uDFFF])?|[\uDD26\uDD35\uDD37-\uDD39\uDD3D\uDD3E\uDDB8\uDDB9\uDDCD\uDDCF\uDDD4\uDDD6-\uDDDD](?:\uD83C[\uDFFB-\uDFFF])?(?:\u200D[\u2640\u2642]\uFE0F?)?|[\uDDDE\uDDDF](?:\u200D[\u2640\u2642]\uFE0F?)?|[\uDD0D\uDD0E\uDD10-\uDD17\uDD20-\uDD25\uDD27-\uDD2F\uDD3A\uDD3F-\uDD45\uDD47-\uDD76\uDD78-\uDDB4\uDDB7\uDDBA\uDDBC-\uDDCC\uDDD0\uDDE0-\uDDFF\uDE70-\uDE7C\uDE80-\uDE88\uDE90-\uDEBD\uDEBF-\uDEC2\uDECE-\uDEDB\uDEE0-\uDEE8]|\uDD3C(?:\u200D[\u2640\u2642]\uFE0F?|\uD83C[\uDFFB-\uDFFF])?|\uDDCE(?:\uD83C[\uDFFB-\uDFFF])?(?:\u200D(?:[\u2640\u2642]\uFE0F?(?:\u200D\u27A1\uFE0F?)?|\u27A1\uFE0F?))?|\uDDD1(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83E\uDDD1|\uDDD1\u200D\uD83E\uDDD2(?:\u200D\uD83E\uDDD2)?|\uDDD2(?:\u200D\uD83E\uDDD2)?))|\uD83C(?:\uDFFB(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1\uD83C[\uDFFC-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83E\uDDD1\uD83C[\uDFFB-\uDFFF])))?|\uDFFC(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1\uD83C[\uDFFB\uDFFD-\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83E\uDDD1\uD83C[\uDFFB-\uDFFF])))?|\uDFFD(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1\uD83C[\uDFFB\uDFFC\uDFFE\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83E\uDDD1\uD83C[\uDFFB-\uDFFF])))?|\uDFFE(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1\uD83C[\uDFFB-\uDFFD\uDFFF]|\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83E\uDDD1\uD83C[\uDFFB-\uDFFF])))?|\uDFFF(?:\u200D(?:[\u2695\u2696\u2708]\uFE0F?|\u2764\uFE0F?\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1\uD83C[\uDFFB-\uDFFE]|\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E(?:[\uDDAF\uDDBC\uDDBD](?:\u200D\u27A1\uFE0F?)?|[\uDDB0-\uDDB3]|\uDD1D\u200D\uD83E\uDDD1\uD83C[\uDFFB-\uDFFF])))?))?|\uDEF1(?:\uD83C(?:\uDFFB(?:\u200D\uD83E\uDEF2\uD83C[\uDFFC-\uDFFF])?|\uDFFC(?:\u200D\uD83E\uDEF2\uD83C[\uDFFB\uDFFD-\uDFFF])?|\uDFFD(?:\u200D\uD83E\uDEF2\uD83C[\uDFFB\uDFFC\uDFFE\uDFFF])?|\uDFFE(?:\u200D\uD83E\uDEF2\uD83C[\uDFFB-\uDFFD\uDFFF])?|\uDFFF(?:\u200D\uD83E\uDEF2\uD83C[\uDFFB-\uDFFE])?))?)/g;
};

function stringWidth(string, options) {
	if (typeof string !== 'string' || string.length === 0) {
		return 0;
	}
	options = {
		ambiguousIsNarrow: true,
		countAnsiEscapeCodes: false,
		...options,
	};
	if (!options.countAnsiEscapeCodes) {
		string = stripAnsi(string);
	}
	if (string.length === 0) {
		return 0;
	}
	const ambiguousCharacterWidth = options.ambiguousIsNarrow ? 1 : 2;
	let width = 0;
	for (const {segment: character} of new Intl.Segmenter().segment(string)) {
		const codePoint = character.codePointAt(0);
		if (codePoint <= 0x1F || (codePoint >= 0x7F && codePoint <= 0x9F)) {
			continue;
		}
		if (codePoint >= 0x3_00 && codePoint <= 0x3_6F) {
			continue;
		}
		if (emojiRegex().test(character)) {
			width += 2;
			continue;
		}
		const code = eastAsianWidth.eastAsianWidth(character);
		switch (code) {
			case 'F':
			case 'W': {
				width += 2;
				break;
			}
			case 'A': {
				width += ambiguousCharacterWidth;
				break;
			}
			default: {
				width += 1;
			}
		}
	}
	return width;
}

function compareFile(a, b) {
  return compareString(a, b, 'path')
}
function compareMessage(a, b) {
  return (
    compareNumber(a, b, 'line') ||
    compareNumber(a, b, 'column') ||
    compareBoolean(a, b, 'fatal') ||
    compareString(a, b, 'source') ||
    compareString(a, b, 'ruleId') ||
    compareString(a, b, 'reason')
  )
}
function compareBoolean(a, b, field) {
  return scoreNullableBoolean(a[field]) - scoreNullableBoolean(b[field])
}
function compareNumber(a, b, field) {
  return (a[field] || 0) - (b[field] || 0)
}
function compareString(a, b, field) {
  return String(a[field] || '').localeCompare(String(b[field] || ''))
}
function scoreNullableBoolean(value) {
  return value ? 0 : value === false ? 1 : 2
}

function statistics(value) {
  const result = {fatal: 0, warn: 0, info: 0};
  if (!value) {
    throw new TypeError(
      'Expected file or message for `value`, not `' + value + '`'
    )
  }
  if (Array.isArray(value)) {
    list(value);
  } else {
    one(value);
  }
  return {
    fatal: result.fatal,
    nonfatal: result.warn + result.info,
    warn: result.warn,
    info: result.info,
    total: result.fatal + result.warn + result.info
  }
  function list(value) {
    let index = -1;
    while (++index < value.length) {
      one(value[index]);
    }
  }
  function one(value) {
    if ('messages' in value) return list(value.messages)
    result[value.fatal ? 'fatal' : value.fatal === false ? 'warn' : 'info']++;
  }
}

function hasFlag(flag, argv = globalThis.Deno ? globalThis.Deno.args : process$1.argv) {
	const prefix = flag.startsWith('-') ? '' : (flag.length === 1 ? '-' : '--');
	const position = argv.indexOf(prefix + flag);
	const terminatorPosition = argv.indexOf('--');
	return position !== -1 && (terminatorPosition === -1 || position < terminatorPosition);
}
const {env} = process$1;
let flagForceColor;
if (
	hasFlag('no-color')
	|| hasFlag('no-colors')
	|| hasFlag('color=false')
	|| hasFlag('color=never')
) {
	flagForceColor = 0;
} else if (
	hasFlag('color')
	|| hasFlag('colors')
	|| hasFlag('color=true')
	|| hasFlag('color=always')
) {
	flagForceColor = 1;
}
function envForceColor() {
	if ('FORCE_COLOR' in env) {
		if (env.FORCE_COLOR === 'true') {
			return 1;
		}
		if (env.FORCE_COLOR === 'false') {
			return 0;
		}
		return env.FORCE_COLOR.length === 0 ? 1 : Math.min(Number.parseInt(env.FORCE_COLOR, 10), 3);
	}
}
function translateLevel(level) {
	if (level === 0) {
		return false;
	}
	return {
		level,
		hasBasic: true,
		has256: level >= 2,
		has16m: level >= 3,
	};
}
function _supportsColor(haveStream, {streamIsTTY, sniffFlags = true} = {}) {
	const noFlagForceColor = envForceColor();
	if (noFlagForceColor !== undefined) {
		flagForceColor = noFlagForceColor;
	}
	const forceColor = sniffFlags ? flagForceColor : noFlagForceColor;
	if (forceColor === 0) {
		return 0;
	}
	if (sniffFlags) {
		if (hasFlag('color=16m')
			|| hasFlag('color=full')
			|| hasFlag('color=truecolor')) {
			return 3;
		}
		if (hasFlag('color=256')) {
			return 2;
		}
	}
	if ('TF_BUILD' in env && 'AGENT_NAME' in env) {
		return 1;
	}
	if (haveStream && !streamIsTTY && forceColor === undefined) {
		return 0;
	}
	const min = forceColor || 0;
	if (env.TERM === 'dumb') {
		return min;
	}
	if (process$1.platform === 'win32') {
		const osRelease = os.release().split('.');
		if (
			Number(osRelease[0]) >= 10
			&& Number(osRelease[2]) >= 10_586
		) {
			return Number(osRelease[2]) >= 14_931 ? 3 : 2;
		}
		return 1;
	}
	if ('CI' in env) {
		if ('GITHUB_ACTIONS' in env || 'GITEA_ACTIONS' in env) {
			return 3;
		}
		if (['TRAVIS', 'CIRCLECI', 'APPVEYOR', 'GITLAB_CI', 'BUILDKITE', 'DRONE'].some(sign => sign in env) || env.CI_NAME === 'codeship') {
			return 1;
		}
		return min;
	}
	if ('TEAMCITY_VERSION' in env) {
		return /^(9\.(0*[1-9]\d*)\.|\d{2,}\.)/.test(env.TEAMCITY_VERSION) ? 1 : 0;
	}
	if (env.COLORTERM === 'truecolor') {
		return 3;
	}
	if (env.TERM === 'xterm-kitty') {
		return 3;
	}
	if ('TERM_PROGRAM' in env) {
		const version = Number.parseInt((env.TERM_PROGRAM_VERSION || '').split('.')[0], 10);
		switch (env.TERM_PROGRAM) {
			case 'iTerm.app': {
				return version >= 3 ? 3 : 2;
			}
			case 'Apple_Terminal': {
				return 2;
			}
		}
	}
	if (/-256(color)?$/i.test(env.TERM)) {
		return 2;
	}
	if (/^screen|^xterm|^vt100|^vt220|^rxvt|color|ansi|cygwin|linux/i.test(env.TERM)) {
		return 1;
	}
	if ('COLORTERM' in env) {
		return 1;
	}
	return min;
}
function createSupportsColor(stream, options = {}) {
	const level = _supportsColor(stream, {
		streamIsTTY: stream && stream.isTTY,
		...options,
	});
	return translateLevel(level);
}
const supportsColor = {
	stdout: createSupportsColor({isTTY: tty.isatty(1)}),
	stderr: createSupportsColor({isTTY: tty.isatty(2)}),
};

const color = supportsColor.stderr.hasBasic;

const eol = /\r?\n|\r/;
function reporter(files, options) {
  if (
    !files ||
    ('name' in files && 'message' in files)
  ) {
    throw new TypeError(
      'Unexpected value for `files`, expected one or more `VFile`s'
    )
  }
  const settings = options || {};
  const colorEnabled =
    typeof settings.color === 'boolean' ? settings.color : color;
  let oneFileMode = false;
  if (Array.isArray(files)) ; else {
    oneFileMode = true;
    files = [files];
  }
  return serializeRows(
    createRows(
      {
        defaultName: settings.defaultName || undefined,
        oneFileMode,
        quiet: settings.quiet || false,
        silent: settings.silent || false,
        traceLimit:
          typeof settings.traceLimit === 'number' ? settings.traceLimit : 10,
        verbose: settings.verbose || false,
        bold: colorEnabled ? '\u001B[1m' : '',
        underline: colorEnabled ? '\u001B[4m' : '',
        normalIntensity: colorEnabled ? '\u001B[22m' : '',
        noUnderline: colorEnabled ? '\u001B[24m' : '',
        red: colorEnabled ? '\u001B[31m' : '',
        cyan: colorEnabled ? '\u001B[36m' : '',
        green: colorEnabled ? '\u001B[32m' : '',
        yellow: colorEnabled ? '\u001B[33m' : '',
        defaultColor: colorEnabled ? '\u001B[39m' : ''
      },
      files
    )
  )
}
function createAncestorsLines(state, ancestors) {
  const min =
    ancestors.length > state.traceLimit
      ? ancestors.length - state.traceLimit
      : 0;
  let index = ancestors.length;
  const lines = [];
  if (index > min) {
    lines.unshift('  ' + state.bold + '[trace]' + state.normalIntensity + ':');
  }
  while (index-- > min) {
    const node = ancestors[index];
    const value = node;
    const name =
      typeof value.tagName === 'string'
        ? value.tagName
        :
        typeof value.name === 'string'
        ? value.name
        : undefined;
    const position = stringifyPosition$2(node.position);
    lines.push(
      '    at ' +
        state.yellow +
        node.type +
        (name ? '<' + name + '>' : '') +
        state.defaultColor +
        (position ? ' (' + position + ')' : '')
    );
  }
  return lines
}
function createByline(state, stats) {
  let result = '';
  if (stats.fatal) {
    result =
      state.red +
      '✖' +
      state.defaultColor +
      ' ' +
      stats.fatal +
      ' ' +
      (fatalToLabel(true) + (stats.fatal === 1 ? '' : 's'));
  }
  if (stats.warn) {
    result =
      (result ? result + ', ' : '') +
      (state.yellow + '⚠' + state.defaultColor) +
      ' ' +
      stats.warn +
      ' ' +
      (fatalToLabel(false) + (stats.warn === 1 ? '' : 's'));
  }
  if (stats.total !== stats.fatal && stats.total !== stats.warn) {
    result = stats.total + ' messages (' + result + ')';
  }
  return result
}
function createCauseLines(state, cause) {
  const lines = ['  ' + state.bold + '[cause]' + state.normalIntensity + ':'];
  let foundReasonableCause = false;
  if (cause !== null && typeof cause === 'object') {
    const stackValue =
      ('stack' in cause ? String(cause.stack) : undefined) ||
      ('message' in cause ? String(cause.message) : undefined);
    if (typeof stackValue === 'string') {
      foundReasonableCause = true;
      const stackLines = stackValue.split(eol);
      stackLines[0] = '    ' + stackLines[0];
      lines.push(...stackLines);
      if ('cause' in cause && cause.cause) {
        lines.push(...createCauseLines(state, cause.cause));
      }
    }
  }
  if (!foundReasonableCause) {
    lines.push('    ' + cause);
  }
  return lines
}
function createFileLine(state, file) {
  const stats = statistics(file.messages);
  const fromPath = file.history[0];
  const toPath = file.path;
  let left = '';
  let right = '';
  if (!state.oneFileMode || state.defaultName || fromPath) {
    const name = fromPath || state.defaultName || '<stdin>';
    left =
      state.underline +
      (stats.fatal ? state.red : stats.total ? state.yellow : state.green) +
      name +
      state.defaultColor +
      state.noUnderline +
      (file.stored && name !== toPath ? ' > ' + toPath : '');
  }
  if (file.stored) {
    right = state.yellow + 'written' + state.defaultColor;
  } else if (!stats.total) {
    right = 'no issues found';
  }
  return left && right ? left + ': ' + right : left + right
}
function createNoteLines(state, note) {
  const noteLines = note.split(eol);
  let index = -1;
  while (++index < noteLines.length) {
    noteLines[index] = '    ' + noteLines[index];
  }
  return [
    '  ' + state.bold + '[note]' + state.normalIntensity + ':',
    ...noteLines
  ]
}
function createMessageLine(state, message) {
  const label = fatalToLabel(message.fatal);
  let reason = message.stack || message.message;
  const match = eol.exec(reason);
  let rest = [];
  if (match) {
    rest = reason.slice(match.index + 1).split(eol);
    reason = reason.slice(0, match.index);
  }
  const place = message.place || message.position;
  const row = [
    stringifyPosition$2(place),
    (label === 'error' ? state.red : state.yellow) + label + state.defaultColor,
    formatReason(state, reason),
    message.ruleId || '',
    message.source || ''
  ];
  if (message.cause) {
    rest.push(...createCauseLines(state, message.cause));
  }
  if (state.verbose && message.url) {
    rest.push(...createUrlLines(state, message.url));
  }
  if (state.verbose && message.note) {
    rest.push(...createNoteLines(state, message.note));
  }
  if (state.verbose && message.ancestors) {
    rest.push(...createAncestorsLines(state, message.ancestors));
  }
  return [row, ...rest]
}
function createRows(state, files) {
  const sortedFiles = [...files].sort(compareFile);
  const all = [];
  let index = -1;
  const rows = [];
  let lastWasMessage = false;
  while (++index < sortedFiles.length) {
    const file = sortedFiles[index];
    const messages = [...file.messages].sort(compareMessage);
    const messageRows = [];
    let offset = -1;
    while (++offset < messages.length) {
      const message = messages[offset];
      if (!state.silent || message.fatal) {
        all.push(message);
        messageRows.push(...createMessageLine(state, message));
      }
    }
    if ((!state.quiet && !state.silent) || messageRows.length > 0) {
      const line = createFileLine(state, file);
      if (lastWasMessage && line) rows.push('');
      if (line) rows.push(line);
      if (messageRows.length > 0) rows.push(...messageRows);
      lastWasMessage = messageRows.length > 0;
    }
  }
  const stats = statistics(all);
  if (stats.fatal || stats.warn) {
    rows.push('', createByline(state, stats));
  }
  return rows
}
function createUrlLines(state, url) {
  return [
    '  ' + state.bold + '[url]' + state.normalIntensity + ':',
    '    ' + url
  ]
}
function formatReason(state, reason) {
  const result = [];
  const splits = [];
  let index = reason.indexOf('`');
  while (index !== -1) {
    const split = {index, size: 1};
    splits.push(split);
    while (reason.codePointAt(index + 1) === 96) {
      split.size++;
      index++;
    }
    index = reason.indexOf('`', index + 1);
  }
  index = -1;
  let textStart = 0;
  while (++index < splits.length) {
    let closeIndex = index;
    let close;
    while (++closeIndex < splits.length) {
      if (splits[index].size === splits[closeIndex].size) {
        close = splits[closeIndex];
        break
      }
    }
    if (close) {
      const codeStart = splits[index].index;
      const codeEnd = close.index + close.size;
      result.push(
        reason.slice(textStart, codeStart) +
          state.cyan +
          reason.slice(codeStart, codeEnd) +
          state.defaultColor
      );
      textStart = codeEnd;
      index = closeIndex;
    }
  }
  result.push(reason.slice(textStart));
  return state.bold + result.join('') + state.normalIntensity
}
function fatalToLabel(value) {
  return value ? 'error' : value === false ? 'warning' : 'info'
}
function serializeRows(rows) {
  const sizes = [];
  let index = -1;
  while (++index < rows.length) {
    const row = rows[index];
    if (typeof row === 'string') ; else {
      let cellIndex = -1;
      while (++cellIndex < row.length) {
        const current = sizes[cellIndex] || 0;
        const size = stringWidth(row[cellIndex]);
        if (size > current) {
          sizes[cellIndex] = size;
        }
      }
    }
  }
  const lines = [];
  index = -1;
  while (++index < rows.length) {
    const row = rows[index];
    let line = '';
    if (typeof row === 'string') {
      line = row;
    } else {
      let cellIndex = -1;
      while (++cellIndex < row.length) {
        const cell = row[cellIndex] || '';
        const max = (sizes[cellIndex] || 0) + 1;
        line += cell + ' '.repeat(max - stringWidth(cell));
      }
    }
    lines.push(line.trimEnd());
  }
  return lines.join('\n')
}

const paths = process.argv.slice(2);
if (!paths.length) {
  console.error('Usage: lint-md.mjs <path> [<path> ...]');
  process.exit(1);
}
let format = false;
if (paths[0] === '--format') {
  paths.shift();
  format = true;
}
const linter = unified()
  .use(remarkParse)
  .use(remarkPresetLintNode)
  .use(remarkStringify);
paths.forEach(async (path) => {
  const file = await read(path);
  const fileContents = file.toString();
  const result = await linter.process(file);
  const isDifferent = fileContents !== result.toString();
  if (format) {
    if (isDifferent) {
      fs.writeFileSync(path, result.toString());
    }
  } else {
    if (isDifferent) {
      process.exitCode = 1;
      const cmd = process.platform === 'win32' ? 'vcbuild' : 'make';
      console.error(`${path} is not formatted. Please run '${cmd} format-md'.`);
    }
    if (result.messages.length) {
      process.exitCode = 1;
      console.error(reporter(result));
    }
  }
});
