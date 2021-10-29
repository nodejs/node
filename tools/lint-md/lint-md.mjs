import fs from 'fs';
import path$1 from 'path';
import { fileURLToPath, pathToFileURL, URL as URL$1 } from 'url';
import process$1 from 'process';
import os from 'os';
import tty from 'tty';

/**
 * Throw a given error.
 *
 * @param {Error | null | undefined} [error]
 */
function bail(error) {
  if (error) {
    throw error
  }
}

var commonjsGlobal = typeof globalThis !== 'undefined' ? globalThis : typeof window !== 'undefined' ? window : typeof global !== 'undefined' ? global : typeof self !== 'undefined' ? self : {};

function commonjsRequire (path) {
	throw new Error('Could not dynamically require "' + path + '". Please configure the dynamicRequireTargets or/and ignoreDynamicRequires option of @rollup/plugin-commonjs appropriately for this require call to work.');
}

/*!
 * Determine if an object is a Buffer
 *
 * @author   Feross Aboukhadijeh <https://feross.org>
 * @license  MIT
 */

var isBuffer = function isBuffer (obj) {
  return obj != null && obj.constructor != null &&
    typeof obj.constructor.isBuffer === 'function' && obj.constructor.isBuffer(obj)
};

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
	// Not own constructor property must be Object
	if (obj.constructor && !hasOwnConstructor && !hasIsPrototypeOf) {
		return false;
	}

	// Own properties are enumerated firstly, so to speed up,
	// if last one is own, then all properties are own.
	var key;
	for (key in obj) { /**/ }

	return typeof key === 'undefined' || hasOwn.call(obj, key);
};

// If name is '__proto__', and Object.defineProperty is available, define __proto__ as an own property on target
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

// Return undefined instead of __proto__ if '__proto__' is not an own property
var getProperty = function getProperty(obj, name) {
	if (name === '__proto__') {
		if (!hasOwn.call(obj, name)) {
			return void 0;
		} else if (gOPD) {
			// In early versions of node, obj['__proto__'] is buggy when obj has
			// __proto__ as an own property. Object.getOwnPropertyDescriptor() works.
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

	// Handle a deep copy situation
	if (typeof target === 'boolean') {
		deep = target;
		target = arguments[1] || {};
		// skip the boolean and the target
		i = 2;
	}
	if (target == null || (typeof target !== 'object' && typeof target !== 'function')) {
		target = {};
	}

	for (; i < length; ++i) {
		options = arguments[i];
		// Only deal with non-null/undefined values
		if (options != null) {
			// Extend the base object
			for (name in options) {
				src = getProperty(target, name);
				copy = getProperty(options, name);

				// Prevent never-ending loop
				if (target !== copy) {
					// Recurse if we're merging plain objects or arrays
					if (deep && copy && (isPlainObject$1(copy) || (copyIsArray = isArray(copy)))) {
						if (copyIsArray) {
							copyIsArray = false;
							clone = src && isArray(src) ? src : [];
						} else {
							clone = src && isPlainObject$1(src) ? src : {};
						}

						// Never move original objects, clone them
						setProperty(target, { name: name, newValue: extend(deep, clone, copy) });

					// Don't bring in undefined values
					} else if (typeof copy !== 'undefined') {
						setProperty(target, { name: name, newValue: copy });
					}
				}
			}
		}
	}

	// Return the modified object
	return target;
};

function isPlainObject(value) {
	if (Object.prototype.toString.call(value) !== '[object Object]') {
		return false;
	}

	const prototype = Object.getPrototypeOf(value);
	return prototype === null || prototype === Object.prototype;
}

/**
 * @typedef {(error?: Error|null|undefined, ...output: any[]) => void} Callback
 * @typedef {(...input: any[]) => any} Middleware
 *
 * @typedef {(...input: any[]) => void} Run Call all middleware.
 * @typedef {(fn: Middleware) => Pipeline} Use Add `fn` (middleware) to the list.
 * @typedef {{run: Run, use: Use}} Pipeline
 */

/**
 * Create new middleware.
 *
 * @returns {Pipeline}
 */
function trough() {
  /** @type {Middleware[]} */
  const fns = [];
  /** @type {Pipeline} */
  const pipeline = {run, use};

  return pipeline

  /** @type {Run} */
  function run(...values) {
    let middlewareIndex = -1;
    /** @type {Callback} */
    const callback = values.pop();

    if (typeof callback !== 'function') {
      throw new TypeError('Expected function as last argument, not ' + callback)
    }

    next(null, ...values);

    /**
     * Run the next `fn`, or we’re done.
     *
     * @param {Error|null|undefined} error
     * @param {any[]} output
     */
    function next(error, ...output) {
      const fn = fns[++middlewareIndex];
      let index = -1;

      if (error) {
        callback(error);
        return
      }

      // Copy non-nullish input into values.
      while (++index < values.length) {
        if (output[index] === null || output[index] === undefined) {
          output[index] = values[index];
        }
      }

      // Save the newly created `output` for the next call.
      values = output;

      // Next or done.
      if (fn) {
        wrap(fn, next)(...output);
      } else {
        callback(null, ...output);
      }
    }
  }

  /** @type {Use} */
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

/**
 * Wrap `middleware`.
 * Can be sync or async; return a promise, receive a callback, or return new
 * values and errors.
 *
 * @param {Middleware} middleware
 * @param {Callback} callback
 */
function wrap(middleware, callback) {
  /** @type {boolean} */
  let called;

  return wrapped

  /**
   * Call `middleware`.
   * @param {any[]} parameters
   * @returns {void}
   */
  function wrapped(...parameters) {
    const fnExpectsCallback = middleware.length > parameters.length;
    /** @type {any} */
    let result;

    if (fnExpectsCallback) {
      parameters.push(done);
    }

    try {
      result = middleware(...parameters);
    } catch (error) {
      /** @type {Error} */
      const exception = error;

      // Well, this is quite the pickle.
      // `middleware` received a callback and called it synchronously, but that
      // threw an error.
      // The only thing left to do is to throw the thing instead.
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

  /**
   * Call `callback`, only once.
   * @type {Callback}
   */
  function done(error, ...output) {
    if (!called) {
      called = true;
      callback(error, ...output);
    }
  }

  /**
   * Call `done` with one value.
   *
   * @param {any} [value]
   */
  function then(value) {
    done(null, value);
  }
}

var own$8 = {}.hasOwnProperty;

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Position} Position
 * @typedef {import('unist').Point} Point
 */

/**
 * Stringify one point, a position (start and end points), or a node’s
 * positional information.
 *
 * @param {Node|Position|Point} [value]
 * @returns {string}
 */
function stringifyPosition(value) {
  // Nothing.
  if (!value || typeof value !== 'object') {
    return ''
  }

  // Node.
  if (own$8.call(value, 'position') || own$8.call(value, 'type')) {
    // @ts-ignore looks like a node.
    return position(value.position)
  }

  // Position.
  if (own$8.call(value, 'start') || own$8.call(value, 'end')) {
    // @ts-ignore looks like a position.
    return position(value)
  }

  // Point.
  if (own$8.call(value, 'line') || own$8.call(value, 'column')) {
    // @ts-ignore looks like a point.
    return point$1(value)
  }

  // ?
  return ''
}

/**
 * @param {Point} point
 * @returns {string}
 */
function point$1(point) {
  return index(point && point.line) + ':' + index(point && point.column)
}

/**
 * @param {Position} pos
 * @returns {string}
 */
function position(pos) {
  return point$1(pos && pos.start) + '-' + point$1(pos && pos.end)
}

/**
 * @param {number} value
 * @returns {number}
 */
function index(value) {
  return value && typeof value === 'number' ? value : 1
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Position} Position
 * @typedef {import('unist').Point} Point
 */

class VFileMessage extends Error {
  /**
   * Constructor of a message for `reason` at `place` from `origin`.
   * When an error is passed in as `reason`, copies the `stack`.
   *
   * @param {string|Error} reason Reason for message (`string` or `Error`). Uses the stack and message of the error if given.
   * @param {Node|Position|Point} [place] Place at which the message occurred in a file (`Node`, `Position`, or `Point`, optional).
   * @param {string} [origin] Place in code the message originates from (`string`, optional).
   */
  constructor(reason, place, origin) {
    /** @type {[string?, string?]} */
    var parts = [null, null];
    /** @type {Position} */
    var position = {
      start: {line: null, column: null},
      end: {line: null, column: null}
    };
    /** @type {number} */
    var index;

    super();

    if (typeof place === 'string') {
      origin = place;
      place = null;
    }

    if (typeof origin === 'string') {
      index = origin.indexOf(':');

      if (index === -1) {
        parts[1] = origin;
      } else {
        parts[0] = origin.slice(0, index);
        parts[1] = origin.slice(index + 1);
      }
    }

    if (place) {
      // Node.
      if ('type' in place || 'position' in place) {
        if (place.position) {
          position = place.position;
        }
      }
      // Position.
      else if ('start' in place || 'end' in place) {
        // @ts-ignore Looks like a position.
        position = place;
      }
      // Point.
      else if ('line' in place || 'column' in place) {
        // @ts-ignore Looks like a point.
        position.start = place;
      }
    }

    // Fields from `Error`
    this.name = stringifyPosition(place) || '1:1';
    this.message = typeof reason === 'object' ? reason.message : reason;
    this.stack = typeof reason === 'object' ? reason.stack : '';

    /**
     * Reason for message.
     * @type {string}
     */
    this.reason = this.message;
    /**
     * Starting line of error.
     * @type {number?}
     */
    this.line = position.start.line;
    /**
     * Starting column of error.
     * @type {number?}
     */
    this.column = position.start.column;
    /**
     * Namespace of warning.
     * @type {string?}
     */
    this.source = parts[0];
    /**
     * Category of message.
     * @type {string?}
     */
    this.ruleId = parts[1];
    /**
     * Full range information, when available.
     * Has start and end properties, both set to an object with line and column, set to number?.
     * @type {Position?}
     */
    this.position = position;

    // The following fields are “well known”.
    // Not standard.
    // Feel free to add other non-standard fields to your messages.

    /* eslint-disable no-unused-expressions */
    /**
     * You may add a file property with a path of a file (used throughout the VFile ecosystem).
     * @type {string?}
     */
    this.file;
    /**
     * If true, marks associated file as no longer processable.
     * @type {boolean?}
     */
    this.fatal;
    /**
     * You may add a url property with a link to documentation for the message.
     * @type {string?}
     */
    this.url;
    /**
     * You may add a note property with a long form description of the message (supported by vfile-reporter).
     * @type {string?}
     */
    this.note;
    /* eslint-enable no-unused-expressions */
  }
}

VFileMessage.prototype.file = '';
VFileMessage.prototype.name = '';
VFileMessage.prototype.reason = '';
VFileMessage.prototype.message = '';
VFileMessage.prototype.stack = '';
VFileMessage.prototype.fatal = null;
VFileMessage.prototype.column = null;
VFileMessage.prototype.line = null;
VFileMessage.prototype.source = null;
VFileMessage.prototype.ruleId = null;
VFileMessage.prototype.position = null;

const proc = process$1;

/**
 * @typedef URL
 * @property {string} hash
 * @property {string} host
 * @property {string} hostname
 * @property {string} href
 * @property {string} origin
 * @property {string} password
 * @property {string} pathname
 * @property {string} port
 * @property {string} protocol
 * @property {string} search
 * @property {any} searchParams
 * @property {string} username
 * @property {() => string} toString
 * @property {() => string} toJSON
 */

/**
 * @param {unknown} fileURLOrPath
 * @returns {fileURLOrPath is URL}
 */
// From: <https://github.com/nodejs/node/blob/fcf8ba4/lib/internal/url.js#L1501>
function isUrl(fileURLOrPath) {
  return (
    fileURLOrPath !== null &&
    typeof fileURLOrPath === 'object' &&
    // @ts-expect-error: indexable.
    fileURLOrPath.href &&
    // @ts-expect-error: indexable.
    fileURLOrPath.origin
  )
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Position} Position
 * @typedef {import('unist').Point} Point
 * @typedef {import('./minurl.shared.js').URL} URL
 *
 * @typedef {'ascii'|'utf8'|'utf-8'|'utf16le'|'ucs2'|'ucs-2'|'base64'|'latin1'|'binary'|'hex'} BufferEncoding
 *   Encodings supported by the buffer class.
 *   This is a copy of the typing from Node, copied to prevent Node globals from
 *   being needed.
 *   Copied from: <https://github.com/DefinitelyTyped/DefinitelyTyped/blob/a2bc1d8/types/node/globals.d.ts#L174>
 *
 * @typedef {string|Uint8Array} VFileValue
 *   Contents of the file.
 *   Can either be text, or a Buffer like structure.
 *   This does not directly use type `Buffer`, because it can also be used in a
 *   browser context.
 *   Instead this leverages `Uint8Array` which is the base type for `Buffer`,
 *   and a native JavaScript construct.
 *
 * @typedef {VFileValue|VFileOptions|VFile|URL} VFileCompatible
 *   Things that can be passed to the constructor.
 *
 * @typedef VFileCoreOptions
 * @property {VFileValue} [value]
 * @property {string} [cwd]
 * @property {Array.<string>} [history]
 * @property {string|URL} [path]
 * @property {string} [basename]
 * @property {string} [stem]
 * @property {string} [extname]
 * @property {string} [dirname]
 * @property {Object.<string, unknown>} [data]
 *
 * @typedef {{[key: string]: unknown} & VFileCoreOptions} VFileOptions
 *   Configuration: a bunch of keys that will be shallow copied over to the new
 *   file.
 *
 * @typedef {Object.<string, unknown>} VFileReporterSettings
 * @typedef {<T = VFileReporterSettings>(files: VFile[], options: T) => string} VFileReporter
 */

// Order of setting (least specific to most), we need this because otherwise
// `{stem: 'a', path: '~/b.js'}` would throw, as a path is needed before a
// stem can be set.
const order = ['history', 'path', 'basename', 'stem', 'extname', 'dirname'];

class VFile {
  /**
   * Create a new virtual file.
   *
   * If `options` is `string` or `Buffer`, treats it as `{value: options}`.
   * If `options` is a `VFile`, shallow copies its data over to the new file.
   * All other given fields are set on the newly created `VFile`.
   *
   * Path related properties are set in the following order (least specific to
   * most specific): `history`, `path`, `basename`, `stem`, `extname`,
   * `dirname`.
   *
   * It’s not possible to set either `dirname` or `extname` without setting
   * either `history`, `path`, `basename`, or `stem` as well.
   *
   * @param {VFileCompatible} [value]
   */
  constructor(value) {
    /** @type {VFileOptions} */
    let options;

    if (!value) {
      options = {};
    } else if (typeof value === 'string' || isBuffer(value)) {
      // @ts-expect-error Looks like a buffer.
      options = {value};
    } else if (isUrl(value)) {
      options = {path: value};
    } else {
      // @ts-expect-error Looks like file or options.
      options = value;
    }

    /**
     * Place to store custom information.
     * It’s OK to store custom data directly on the file, moving it to `data`
     * gives a little more privacy.
     * @type {Object.<string, unknown>}
     */
    this.data = {};

    /**
     * List of messages associated with the file.
     * @type {Array.<VFileMessage>}
     */
    this.messages = [];

    /**
     * List of file paths the file moved between.
     * @type {Array.<string>}
     */
    this.history = [];

    /**
     * Base of `path`.
     * Defaults to `process.cwd()` (`/` in browsers).
     * @type {string}
     */
    this.cwd = proc.cwd();

    /* eslint-disable no-unused-expressions */
    /**
     * Raw value.
     * @type {VFileValue}
     */
    this.value;

    // The below are non-standard, they are “well-known”.
    // As in, used in several tools.

    /**
     * Whether a file was saved to disk.
     * This is used by vfile reporters.
     * @type {boolean}
     */
    this.stored;

    /**
     * Sometimes files have a non-string representation.
     * This can be stored in the `result` field.
     * One example is when turning markdown into React nodes.
     * This is used by unified to store non-string results.
     * @type {unknown}
     */
    this.result;

    /**
     * Sometimes files have a source map associated with them.
     * This can be stored in the `map` field.
     * This should be a `RawSourceMap` type from the `source-map` module.
     * @type {unknown}
     */
    this.map;
    /* eslint-enable no-unused-expressions */

    // Set path related properties in the correct order.
    let index = -1;

    while (++index < order.length) {
      const prop = order[index];

      // Note: we specifically use `in` instead of `hasOwnProperty` to accept
      // `vfile`s too.
      if (prop in options && options[prop] !== undefined) {
        // @ts-expect-error: TS is confused by the different types for `history`.
        this[prop] = prop === 'history' ? [...options[prop]] : options[prop];
      }
    }

    /** @type {string} */
    let prop;

    // Set non-path related properties.
    for (prop in options) {
      // @ts-expect-error: fine to set other things.
      if (!order.includes(prop)) this[prop] = options[prop];
    }
  }

  /**
   * Access full path (`~/index.min.js`).
   *
   * @returns {string}
   */
  get path() {
    return this.history[this.history.length - 1]
  }

  /**
   * Set full path (`~/index.min.js`).
   * Cannot be nullified.
   *
   * @param {string|URL} path
   */
  set path(path) {
    if (isUrl(path)) {
      path = fileURLToPath(path);
    }

    assertNonEmpty(path, 'path');

    if (this.path !== path) {
      this.history.push(path);
    }
  }

  /**
   * Access parent path (`~`).
   */
  get dirname() {
    return typeof this.path === 'string' ? path$1.dirname(this.path) : undefined
  }

  /**
   * Set parent path (`~`).
   * Cannot be set if there's no `path` yet.
   */
  set dirname(dirname) {
    assertPath(this.basename, 'dirname');
    this.path = path$1.join(dirname || '', this.basename);
  }

  /**
   * Access basename (including extname) (`index.min.js`).
   */
  get basename() {
    return typeof this.path === 'string' ? path$1.basename(this.path) : undefined
  }

  /**
   * Set basename (`index.min.js`).
   * Cannot contain path separators.
   * Cannot be nullified either (use `file.path = file.dirname` instead).
   */
  set basename(basename) {
    assertNonEmpty(basename, 'basename');
    assertPart(basename, 'basename');
    this.path = path$1.join(this.dirname || '', basename);
  }

  /**
   * Access extname (including dot) (`.js`).
   */
  get extname() {
    return typeof this.path === 'string' ? path$1.extname(this.path) : undefined
  }

  /**
   * Set extname (including dot) (`.js`).
   * Cannot be set if there's no `path` yet and cannot contain path separators.
   */
  set extname(extname) {
    assertPart(extname, 'extname');
    assertPath(this.dirname, 'extname');

    if (extname) {
      if (extname.charCodeAt(0) !== 46 /* `.` */) {
        throw new Error('`extname` must start with `.`')
      }

      if (extname.includes('.', 1)) {
        throw new Error('`extname` cannot contain multiple dots')
      }
    }

    this.path = path$1.join(this.dirname, this.stem + (extname || ''));
  }

  /**
   * Access stem (w/o extname) (`index.min`).
   */
  get stem() {
    return typeof this.path === 'string'
      ? path$1.basename(this.path, this.extname)
      : undefined
  }

  /**
   * Set stem (w/o extname) (`index.min`).
   * Cannot be nullified, and cannot contain path separators.
   */
  set stem(stem) {
    assertNonEmpty(stem, 'stem');
    assertPart(stem, 'stem');
    this.path = path$1.join(this.dirname || '', stem + (this.extname || ''));
  }

  /**
   * Serialize the file.
   *
   * @param {BufferEncoding} [encoding='utf8'] If `file.value` is a buffer, `encoding` is used to serialize buffers.
   * @returns {string}
   */
  toString(encoding) {
    // @ts-expect-error string’s don’t accept the parameter, but buffers do.
    return (this.value || '').toString(encoding)
  }

  /**
   * Create a message and associates it w/ the file.
   *
   * @param {string|Error} reason Reason for message (`string` or `Error`). Uses the stack and message of the error if given.
   * @param {Node|Position|Point} [place] Place at which the message occurred in a file (`Node`, `Position`, or `Point`, optional).
   * @param {string} [origin] Place in code the message originates from (`string`, optional).
   * @returns {VFileMessage}
   */
  message(reason, place, origin) {
    const message = new VFileMessage(reason, place, origin);

    if (this.path) {
      message.name = this.path + ':' + message.name;
      message.file = this.path;
    }

    message.fatal = false;

    this.messages.push(message);

    return message
  }

  /**
   * Info: create a message, associate it with the file, and mark the fatality
   * as `null`.
   * Calls `message()` internally.
   *
   * @param {string|Error} reason Reason for message (`string` or `Error`). Uses the stack and message of the error if given.
   * @param {Node|Position|Point} [place] Place at which the message occurred in a file (`Node`, `Position`, or `Point`, optional).
   * @param {string} [origin] Place in code the message originates from (`string`, optional).
   * @returns {VFileMessage}
   */
  info(reason, place, origin) {
    const message = this.message(reason, place, origin);

    message.fatal = null;

    return message
  }

  /**
   * Fail: create a message, associate it with the file, mark the fatality as
   * `true`.
   * Note: fatal errors mean a file is no longer processable.
   * Calls `message()` internally.
   *
   * @param {string|Error} reason Reason for message (`string` or `Error`). Uses the stack and message of the error if given.
   * @param {Node|Position|Point} [place] Place at which the message occurred in a file (`Node`, `Position`, or `Point`, optional).
   * @param {string} [origin] Place in code the message originates from (`string`, optional).
   * @returns {never}
   */
  fail(reason, place, origin) {
    const message = this.message(reason, place, origin);

    message.fatal = true;

    throw message
  }
}

/**
 * Assert that `part` is not a path (as in, does not contain `path.sep`).
 *
 * @param {string|undefined} part
 * @param {string} name
 * @returns {void}
 */
function assertPart(part, name) {
  if (part && part.includes(path$1.sep)) {
    throw new Error(
      '`' + name + '` cannot be a path: did not expect `' + path$1.sep + '`'
    )
  }
}

/**
 * Assert that `part` is not empty.
 *
 * @param {string|undefined} part
 * @param {string} name
 * @returns {asserts part is string}
 */
function assertNonEmpty(part, name) {
  if (!part) {
    throw new Error('`' + name + '` cannot be empty')
  }
}

/**
 * Assert `path` exists.
 *
 * @param {string|undefined} path
 * @param {string} name
 * @returns {asserts path is string}
 */
function assertPath(path, name) {
  if (!path) {
    throw new Error('Setting `' + name + '` requires `path` to be set too')
  }
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('vfile').VFileCompatible} VFileCompatible
 * @typedef {import('vfile').VFileValue} VFileValue
 * @typedef {import('..').Processor} Processor
 * @typedef {import('..').Plugin} Plugin
 * @typedef {import('..').Preset} Preset
 * @typedef {import('..').Pluggable} Pluggable
 * @typedef {import('..').PluggableList} PluggableList
 * @typedef {import('..').Transformer} Transformer
 * @typedef {import('..').Parser} Parser
 * @typedef {import('..').Compiler} Compiler
 * @typedef {import('..').RunCallback} RunCallback
 * @typedef {import('..').ProcessCallback} ProcessCallback
 *
 * @typedef Context
 * @property {Node} tree
 * @property {VFile} file
 */

// Expose a frozen processor.
const unified = base().freeze();

const own$7 = {}.hasOwnProperty;

// Function to create the first processor.
/**
 * @returns {Processor}
 */
function base() {
  const transformers = trough();
  /** @type {Processor['attachers']} */
  const attachers = [];
  /** @type {Record<string, unknown>} */
  let namespace = {};
  /** @type {boolean|undefined} */
  let frozen;
  let freezeIndex = -1;

  // Data management.
  // @ts-expect-error: overloads are handled.
  processor.data = data;
  processor.Parser = undefined;
  processor.Compiler = undefined;

  // Lock.
  processor.freeze = freeze;

  // Plugins.
  processor.attachers = attachers;
  // @ts-expect-error: overloads are handled.
  processor.use = use;

  // API.
  processor.parse = parse;
  processor.stringify = stringify;
  // @ts-expect-error: overloads are handled.
  processor.run = run;
  processor.runSync = runSync;
  // @ts-expect-error: overloads are handled.
  processor.process = process;
  processor.processSync = processSync;

  // Expose.
  return processor

  // Create a new processor based on the processor in the current scope.
  /** @type {Processor} */
  function processor() {
    const destination = base();
    let index = -1;

    while (++index < attachers.length) {
      destination.use(...attachers[index]);
    }

    destination.data(extend$1(true, {}, namespace));

    return destination
  }

  /**
   * @param {string|Record<string, unknown>} [key]
   * @param {unknown} [value]
   * @returns {unknown}
   */
  function data(key, value) {
    if (typeof key === 'string') {
      // Set `key`.
      if (arguments.length === 2) {
        assertUnfrozen('data', frozen);
        namespace[key] = value;
        return processor
      }

      // Get `key`.
      return (own$7.call(namespace, key) && namespace[key]) || null
    }

    // Set space.
    if (key) {
      assertUnfrozen('data', frozen);
      namespace = key;
      return processor
    }

    // Get space.
    return namespace
  }

  /** @type {Processor['freeze']} */
  function freeze() {
    if (frozen) {
      return processor
    }

    while (++freezeIndex < attachers.length) {
      const [attacher, ...options] = attachers[freezeIndex];

      if (options[0] === false) {
        continue
      }

      if (options[0] === true) {
        options[1] = undefined;
      }

      /** @type {Transformer|void} */
      const transformer = attacher.call(processor, ...options);

      if (typeof transformer === 'function') {
        transformers.use(transformer);
      }
    }

    frozen = true;
    freezeIndex = Number.POSITIVE_INFINITY;

    return processor
  }

  /**
   * @param {Pluggable|null|undefined} [value]
   * @param {...unknown} options
   * @returns {Processor}
   */
  function use(value, ...options) {
    /** @type {Record<string, unknown>|undefined} */
    let settings;

    assertUnfrozen('use', frozen);

    if (value === null || value === undefined) ; else if (typeof value === 'function') {
      addPlugin(value, ...options);
    } else if (typeof value === 'object') {
      if (Array.isArray(value)) {
        addList(value);
      } else {
        addPreset(value);
      }
    } else {
      throw new TypeError('Expected usable value, not `' + value + '`')
    }

    if (settings) {
      namespace.settings = Object.assign(namespace.settings || {}, settings);
    }

    return processor

    /**
     * @param {import('..').Pluggable<unknown[]>} value
     * @returns {void}
     */
    function add(value) {
      if (typeof value === 'function') {
        addPlugin(value);
      } else if (typeof value === 'object') {
        if (Array.isArray(value)) {
          const [plugin, ...options] = value;
          addPlugin(plugin, ...options);
        } else {
          addPreset(value);
        }
      } else {
        throw new TypeError('Expected usable value, not `' + value + '`')
      }
    }

    /**
     * @param {Preset} result
     * @returns {void}
     */
    function addPreset(result) {
      addList(result.plugins);

      if (result.settings) {
        settings = Object.assign(settings || {}, result.settings);
      }
    }

    /**
     * @param {PluggableList|null|undefined} [plugins]
     * @returns {void}
     */
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

    /**
     * @param {Plugin} plugin
     * @param {...unknown} [value]
     * @returns {void}
     */
    function addPlugin(plugin, value) {
      let index = -1;
      /** @type {Processor['attachers'][number]|undefined} */
      let entry;

      while (++index < attachers.length) {
        if (attachers[index][0] === plugin) {
          entry = attachers[index];
          break
        }
      }

      if (entry) {
        if (isPlainObject(entry[1]) && isPlainObject(value)) {
          value = extend$1(true, entry[1], value);
        }

        entry[1] = value;
      } else {
        // @ts-expect-error: fine.
        attachers.push([...arguments]);
      }
    }
  }

  /** @type {Processor['parse']} */
  function parse(doc) {
    processor.freeze();
    const file = vfile(doc);
    const Parser = processor.Parser;
    assertParser('parse', Parser);

    if (newable(Parser, 'parse')) {
      // @ts-expect-error: `newable` checks this.
      return new Parser(String(file), file).parse()
    }

    // @ts-expect-error: `newable` checks this.
    return Parser(String(file), file) // eslint-disable-line new-cap
  }

  /** @type {Processor['stringify']} */
  function stringify(node, doc) {
    processor.freeze();
    const file = vfile(doc);
    const Compiler = processor.Compiler;
    assertCompiler('stringify', Compiler);
    assertNode(node);

    if (newable(Compiler, 'compile')) {
      // @ts-expect-error: `newable` checks this.
      return new Compiler(node, file).compile()
    }

    // @ts-expect-error: `newable` checks this.
    return Compiler(node, file) // eslint-disable-line new-cap
  }

  /**
   * @param {Node} node
   * @param {VFileCompatible|RunCallback} [doc]
   * @param {RunCallback} [callback]
   * @returns {Promise<Node>|void}
   */
  function run(node, doc, callback) {
    assertNode(node);
    processor.freeze();

    if (!callback && typeof doc === 'function') {
      callback = doc;
      doc = undefined;
    }

    if (!callback) {
      return new Promise(executor)
    }

    executor(null, callback);

    /**
     * @param {null|((node: Node) => void)} resolve
     * @param {(error: Error) => void} reject
     * @returns {void}
     */
    function executor(resolve, reject) {
      // @ts-expect-error: `doc` can’t be a callback anymore, we checked.
      transformers.run(node, vfile(doc), done);

      /**
       * @param {Error|null} error
       * @param {Node} tree
       * @param {VFile} file
       * @returns {void}
       */
      function done(error, tree, file) {
        tree = tree || node;
        if (error) {
          reject(error);
        } else if (resolve) {
          resolve(tree);
        } else {
          // @ts-expect-error: `callback` is defined if `resolve` is not.
          callback(null, tree, file);
        }
      }
    }
  }

  /** @type {Processor['runSync']} */
  function runSync(node, file) {
    /** @type {Node|undefined} */
    let result;
    /** @type {boolean|undefined} */
    let complete;

    processor.run(node, file, done);

    assertDone('runSync', 'run', complete);

    // @ts-expect-error: we either bailed on an error or have a tree.
    return result

    /**
     * @param {Error|null} [error]
     * @param {Node} [tree]
     * @returns {void}
     */
    function done(error, tree) {
      bail(error);
      result = tree;
      complete = true;
    }
  }

  /**
   * @param {VFileCompatible} doc
   * @param {ProcessCallback} [callback]
   * @returns {Promise<VFile>|undefined}
   */
  function process(doc, callback) {
    processor.freeze();
    assertParser('process', processor.Parser);
    assertCompiler('process', processor.Compiler);

    if (!callback) {
      return new Promise(executor)
    }

    executor(null, callback);

    /**
     * @param {null|((file: VFile) => void)} resolve
     * @param {(error?: Error|null|undefined) => void} reject
     * @returns {void}
     */
    function executor(resolve, reject) {
      const file = vfile(doc);

      processor.run(processor.parse(file), file, (error, tree, file) => {
        if (error || !tree || !file) {
          done(error);
        } else {
          /** @type {unknown} */
          const result = processor.stringify(tree, file);

          if (result === undefined || result === null) ; else if (looksLikeAVFileValue(result)) {
            file.value = result;
          } else {
            file.result = result;
          }

          done(error, file);
        }
      });

      /**
       * @param {Error|null|undefined} [error]
       * @param {VFile|undefined} [file]
       * @returns {void}
       */
      function done(error, file) {
        if (error || !file) {
          reject(error);
        } else if (resolve) {
          resolve(file);
        } else {
          // @ts-expect-error: `callback` is defined if `resolve` is not.
          callback(null, file);
        }
      }
    }
  }

  /** @type {Processor['processSync']} */
  function processSync(doc) {
    /** @type {boolean|undefined} */
    let complete;

    processor.freeze();
    assertParser('processSync', processor.Parser);
    assertCompiler('processSync', processor.Compiler);

    const file = vfile(doc);

    processor.process(file, done);

    assertDone('processSync', 'process', complete);

    return file

    /**
     * @param {Error|null|undefined} [error]
     * @returns {void}
     */
    function done(error) {
      complete = true;
      bail(error);
    }
  }
}

/**
 * Check if `value` is a constructor.
 *
 * @param {unknown} value
 * @param {string} name
 * @returns {boolean}
 */
function newable(value, name) {
  return (
    typeof value === 'function' &&
    // Prototypes do exist.
    // type-coverage:ignore-next-line
    value.prototype &&
    // A function with keys in its prototype is probably a constructor.
    // Classes’ prototype methods are not enumerable, so we check if some value
    // exists in the prototype.
    // type-coverage:ignore-next-line
    (keys(value.prototype) || name in value.prototype)
  )
}

/**
 * Check if `value` is an object with keys.
 *
 * @param {Record<string, unknown>} value
 * @returns {boolean}
 */
function keys(value) {
  /** @type {string} */
  let key;

  for (key in value) {
    if (own$7.call(value, key)) {
      return true
    }
  }

  return false
}

/**
 * Assert a parser is available.
 *
 * @param {string} name
 * @param {unknown} value
 * @returns {asserts value is Parser}
 */
function assertParser(name, value) {
  if (typeof value !== 'function') {
    throw new TypeError('Cannot `' + name + '` without `Parser`')
  }
}

/**
 * Assert a compiler is available.
 *
 * @param {string} name
 * @param {unknown} value
 * @returns {asserts value is Compiler}
 */
function assertCompiler(name, value) {
  if (typeof value !== 'function') {
    throw new TypeError('Cannot `' + name + '` without `Compiler`')
  }
}

/**
 * Assert the processor is not frozen.
 *
 * @param {string} name
 * @param {unknown} frozen
 * @returns {asserts frozen is false}
 */
function assertUnfrozen(name, frozen) {
  if (frozen) {
    throw new Error(
      'Cannot call `' +
        name +
        '` on a frozen processor.\nCreate a new processor first, by calling it: use `processor()` instead of `processor`.'
    )
  }
}

/**
 * Assert `node` is a unist node.
 *
 * @param {unknown} node
 * @returns {asserts node is Node}
 */
function assertNode(node) {
  // `isPlainObj` unfortunately uses `any` instead of `unknown`.
  // type-coverage:ignore-next-line
  if (!isPlainObject(node) || typeof node.type !== 'string') {
    throw new TypeError('Expected node, got `' + node + '`')
    // Fine.
  }
}

/**
 * Assert that `complete` is `true`.
 *
 * @param {string} name
 * @param {string} asyncName
 * @param {unknown} complete
 * @returns {asserts complete is true}
 */
function assertDone(name, asyncName, complete) {
  if (!complete) {
    throw new Error(
      '`' + name + '` finished async. Use `' + asyncName + '` instead'
    )
  }
}

/**
 * @param {VFileCompatible} [value]
 * @returns {VFile}
 */
function vfile(value) {
  return looksLikeAVFile$1(value) ? value : new VFile(value)
}

/**
 * @param {VFileCompatible} [value]
 * @returns {value is VFile}
 */
function looksLikeAVFile$1(value) {
  return Boolean(
    value &&
      typeof value === 'object' &&
      'message' in value &&
      'messages' in value
  )
}

/**
 * @param {unknown} [value]
 * @returns {value is VFileValue}
 */
function looksLikeAVFileValue(value) {
  return typeof value === 'string' || isBuffer(value)
}

/**
 * @typedef Options
 * @property {boolean} [includeImageAlt=true]
 */

/**
 * Get the text content of a node.
 * Prefer the node’s plain-text fields, otherwise serialize its children,
 * and if the given value is an array, serialize the nodes in it.
 *
 * @param {unknown} node
 * @param {Options} [options]
 * @returns {string}
 */
function toString(node, options) {
  var {includeImageAlt = true} = options || {};
  return one(node, includeImageAlt)
}

/**
 * @param {unknown} node
 * @param {boolean} includeImageAlt
 * @returns {string}
 */
function one(node, includeImageAlt) {
  return (
    (node &&
      typeof node === 'object' &&
      // @ts-ignore looks like a literal.
      (node.value ||
        // @ts-ignore looks like an image.
        (includeImageAlt ? node.alt : '') ||
        // @ts-ignore looks like a parent.
        ('children' in node && all(node.children, includeImageAlt)) ||
        (Array.isArray(node) && all(node, includeImageAlt)))) ||
    ''
  )
}

/**
 * @param {Array.<unknown>} values
 * @param {boolean} includeImageAlt
 * @returns {string}
 */
function all(values, includeImageAlt) {
  /** @type {Array.<string>} */
  var result = [];
  var index = -1;

  while (++index < values.length) {
    result[index] = one(values[index], includeImageAlt);
  }

  return result.join('')
}

/**
 * Like `Array#splice`, but smarter for giant arrays.
 *
 * `Array#splice` takes all items to be inserted as individual argument which
 * causes a stack overflow in V8 when trying to insert 100k items for instance.
 *
 * Otherwise, this does not return the removed items, and takes `items` as an
 * array instead of rest parameters.
 *
 * @template {unknown} T
 * @param {T[]} list
 * @param {number} start
 * @param {number} remove
 * @param {T[]} items
 * @returns {void}
 */
function splice(list, start, remove, items) {
  const end = list.length;
  let chunkStart = 0;
  /** @type {unknown[]} */

  let parameters; // Make start between zero and `end` (included).

  if (start < 0) {
    start = -start > end ? 0 : end + start;
  } else {
    start = start > end ? end : start;
  }

  remove = remove > 0 ? remove : 0; // No need to chunk the items if there’s only a couple (10k) items.

  if (items.length < 10000) {
    parameters = Array.from(items);
    parameters.unshift(start, remove) // @ts-expect-error Hush, it’s fine.
    ;[].splice.apply(list, parameters);
  } else {
    // Delete `remove` items starting from `start`
    if (remove) [].splice.apply(list, [start, remove]); // Insert the items in chunks to not cause stack overflows.

    while (chunkStart < items.length) {
      parameters = items.slice(chunkStart, chunkStart + 10000);
      parameters.unshift(start, 0) // @ts-expect-error Hush, it’s fine.
      ;[].splice.apply(list, parameters);
      chunkStart += 10000;
      start += 10000;
    }
  }
}
/**
 * Append `items` (an array) at the end of `list` (another array).
 * When `list` was empty, returns `items` instead.
 *
 * This prevents a potentially expensive operation when `list` is empty,
 * and adds items in batches to prevent V8 from hanging.
 *
 * @template {unknown} T
 * @param {T[]} list
 * @param {T[]} items
 * @returns {T[]}
 */

function push(list, items) {
  if (list.length > 0) {
    splice(list, list.length, 0, items);
    return list
  }

  return items
}

/**
 * @typedef {import('micromark-util-types').NormalizedExtension} NormalizedExtension
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').HtmlExtension} HtmlExtension
 */

const hasOwnProperty = {}.hasOwnProperty;

/**
 * Combine several syntax extensions into one.
 *
 * @param {Extension[]} extensions List of syntax extensions.
 * @returns {NormalizedExtension} A single combined extension.
 */
function combineExtensions(extensions) {
  /** @type {NormalizedExtension} */
  const all = {};
  let index = -1;

  while (++index < extensions.length) {
    syntaxExtension(all, extensions[index]);
  }

  return all
}

/**
 * Merge `extension` into `all`.
 *
 * @param {NormalizedExtension} all Extension to merge into.
 * @param {Extension} extension Extension to merge.
 * @returns {void}
 */
function syntaxExtension(all, extension) {
  /** @type {string} */
  let hook;

  for (hook in extension) {
    const maybe = hasOwnProperty.call(all, hook) ? all[hook] : undefined;
    const left = maybe || (all[hook] = {});
    const right = extension[hook];
    /** @type {string} */
    let code;

    for (code in right) {
      if (!hasOwnProperty.call(left, code)) left[code] = [];
      const value = right[code];
      constructs(
        // @ts-expect-error Looks like a list.
        left[code],
        Array.isArray(value) ? value : value ? [value] : []
      );
    }
  }
}

/**
 * Merge `list` into `existing` (both lists of constructs).
 * Mutates `existing`.
 *
 * @param {unknown[]} existing
 * @param {unknown[]} list
 * @returns {void}
 */
function constructs(existing, list) {
  let index = -1;
  /** @type {unknown[]} */
  const before = [];

  while (++index < list.length) {
(list[index].add === 'after' ? existing : before).push(list[index]);
  }

  splice(existing, 0, 0, before);
}

// This module is generated by `script/`.
//
// CommonMark handles attention (emphasis, strong) markers based on what comes
// before or after them.
// One such difference is if those characters are Unicode punctuation.
// This script is generated from the Unicode data.
const unicodePunctuationRegex =
  /[!-/:-@[-`{-~\u00A1\u00A7\u00AB\u00B6\u00B7\u00BB\u00BF\u037E\u0387\u055A-\u055F\u0589\u058A\u05BE\u05C0\u05C3\u05C6\u05F3\u05F4\u0609\u060A\u060C\u060D\u061B\u061E\u061F\u066A-\u066D\u06D4\u0700-\u070D\u07F7-\u07F9\u0830-\u083E\u085E\u0964\u0965\u0970\u09FD\u0A76\u0AF0\u0C77\u0C84\u0DF4\u0E4F\u0E5A\u0E5B\u0F04-\u0F12\u0F14\u0F3A-\u0F3D\u0F85\u0FD0-\u0FD4\u0FD9\u0FDA\u104A-\u104F\u10FB\u1360-\u1368\u1400\u166E\u169B\u169C\u16EB-\u16ED\u1735\u1736\u17D4-\u17D6\u17D8-\u17DA\u1800-\u180A\u1944\u1945\u1A1E\u1A1F\u1AA0-\u1AA6\u1AA8-\u1AAD\u1B5A-\u1B60\u1BFC-\u1BFF\u1C3B-\u1C3F\u1C7E\u1C7F\u1CC0-\u1CC7\u1CD3\u2010-\u2027\u2030-\u2043\u2045-\u2051\u2053-\u205E\u207D\u207E\u208D\u208E\u2308-\u230B\u2329\u232A\u2768-\u2775\u27C5\u27C6\u27E6-\u27EF\u2983-\u2998\u29D8-\u29DB\u29FC\u29FD\u2CF9-\u2CFC\u2CFE\u2CFF\u2D70\u2E00-\u2E2E\u2E30-\u2E4F\u2E52\u3001-\u3003\u3008-\u3011\u3014-\u301F\u3030\u303D\u30A0\u30FB\uA4FE\uA4FF\uA60D-\uA60F\uA673\uA67E\uA6F2-\uA6F7\uA874-\uA877\uA8CE\uA8CF\uA8F8-\uA8FA\uA8FC\uA92E\uA92F\uA95F\uA9C1-\uA9CD\uA9DE\uA9DF\uAA5C-\uAA5F\uAADE\uAADF\uAAF0\uAAF1\uABEB\uFD3E\uFD3F\uFE10-\uFE19\uFE30-\uFE52\uFE54-\uFE61\uFE63\uFE68\uFE6A\uFE6B\uFF01-\uFF03\uFF05-\uFF0A\uFF0C-\uFF0F\uFF1A\uFF1B\uFF1F\uFF20\uFF3B-\uFF3D\uFF3F\uFF5B\uFF5D\uFF5F-\uFF65]/;

/**
 * @typedef {import('micromark-util-types').Code} Code
 */
/**
 * Check whether the character code represents an ASCII alpha (`a` through `z`,
 * case insensitive).
 *
 * An **ASCII alpha** is an ASCII upper alpha or ASCII lower alpha.
 *
 * An **ASCII upper alpha** is a character in the inclusive range U+0041 (`A`)
 * to U+005A (`Z`).
 *
 * An **ASCII lower alpha** is a character in the inclusive range U+0061 (`a`)
 * to U+007A (`z`).
 */

const asciiAlpha = regexCheck(/[A-Za-z]/);
/**
 * Check whether the character code represents an ASCII digit (`0` through `9`).
 *
 * An **ASCII digit** is a character in the inclusive range U+0030 (`0`) to
 * U+0039 (`9`).
 */

const asciiDigit = regexCheck(/\d/);
/**
 * Check whether the character code represents an ASCII hex digit (`a` through
 * `f`, case insensitive, or `0` through `9`).
 *
 * An **ASCII hex digit** is an ASCII digit (see `asciiDigit`), ASCII upper hex
 * digit, or an ASCII lower hex digit.
 *
 * An **ASCII upper hex digit** is a character in the inclusive range U+0041
 * (`A`) to U+0046 (`F`).
 *
 * An **ASCII lower hex digit** is a character in the inclusive range U+0061
 * (`a`) to U+0066 (`f`).
 */

const asciiHexDigit = regexCheck(/[\dA-Fa-f]/);
/**
 * Check whether the character code represents an ASCII alphanumeric (`a`
 * through `z`, case insensitive, or `0` through `9`).
 *
 * An **ASCII alphanumeric** is an ASCII digit (see `asciiDigit`) or ASCII alpha
 * (see `asciiAlpha`).
 */

const asciiAlphanumeric = regexCheck(/[\dA-Za-z]/);
/**
 * Check whether the character code represents ASCII punctuation.
 *
 * An **ASCII punctuation** is a character in the inclusive ranges U+0021
 * EXCLAMATION MARK (`!`) to U+002F SLASH (`/`), U+003A COLON (`:`) to U+0040 AT
 * SIGN (`@`), U+005B LEFT SQUARE BRACKET (`[`) to U+0060 GRAVE ACCENT
 * (`` ` ``), or U+007B LEFT CURLY BRACE (`{`) to U+007E TILDE (`~`).
 */

const asciiPunctuation = regexCheck(/[!-/:-@[-`{-~]/);
/**
 * Check whether the character code represents an ASCII atext.
 *
 * atext is an ASCII alphanumeric (see `asciiAlphanumeric`), or a character in
 * the inclusive ranges U+0023 NUMBER SIGN (`#`) to U+0027 APOSTROPHE (`'`),
 * U+002A ASTERISK (`*`), U+002B PLUS SIGN (`+`), U+002D DASH (`-`), U+002F
 * SLASH (`/`), U+003D EQUALS TO (`=`), U+003F QUESTION MARK (`?`), U+005E
 * CARET (`^`) to U+0060 GRAVE ACCENT (`` ` ``), or U+007B LEFT CURLY BRACE
 * (`{`) to U+007E TILDE (`~`).
 *
 * See:
 * **\[RFC5322]**:
 * [Internet Message Format](https://tools.ietf.org/html/rfc5322).
 * P. Resnick.
 * IETF.
 */

const asciiAtext = regexCheck(/[#-'*+\--9=?A-Z^-~]/);
/**
 * Check whether a character code is an ASCII control character.
 *
 * An **ASCII control** is a character in the inclusive range U+0000 NULL (NUL)
 * to U+001F (US), or U+007F (DEL).
 *
 * @param {Code} code
 * @returns {code is number}
 */

function asciiControl(code) {
  return (
    // Special whitespace codes (which have negative values), C0 and Control
    // character DEL
    code !== null && (code < 32 || code === 127)
  )
}
/**
 * Check whether a character code is a markdown line ending (see
 * `markdownLineEnding`) or markdown space (see `markdownSpace`).
 *
 * @param {Code} code
 * @returns {code is number}
 */

function markdownLineEndingOrSpace(code) {
  return code !== null && (code < 0 || code === 32)
}
/**
 * Check whether a character code is a markdown line ending.
 *
 * A **markdown line ending** is the virtual characters M-0003 CARRIAGE RETURN
 * LINE FEED (CRLF), M-0004 LINE FEED (LF) and M-0005 CARRIAGE RETURN (CR).
 *
 * In micromark, the actual character U+000A LINE FEED (LF) and U+000D CARRIAGE
 * RETURN (CR) are replaced by these virtual characters depending on whether
 * they occurred together.
 *
 * @param {Code} code
 * @returns {code is number}
 */

function markdownLineEnding(code) {
  return code !== null && code < -2
}
/**
 * Check whether a character code is a markdown space.
 *
 * A **markdown space** is the concrete character U+0020 SPACE (SP) and the
 * virtual characters M-0001 VIRTUAL SPACE (VS) and M-0002 HORIZONTAL TAB (HT).
 *
 * In micromark, the actual character U+0009 CHARACTER TABULATION (HT) is
 * replaced by one M-0002 HORIZONTAL TAB (HT) and between 0 and 3 M-0001 VIRTUAL
 * SPACE (VS) characters, depending on the column at which the tab occurred.
 *
 * @param {Code} code
 * @returns {code is number}
 */

function markdownSpace(code) {
  return code === -2 || code === -1 || code === 32
}
/**
 * Check whether the character code represents Unicode whitespace.
 *
 * Note that this does handle micromark specific markdown whitespace characters.
 * See `markdownLineEndingOrSpace` to check that.
 *
 * A **Unicode whitespace** is a character in the Unicode `Zs` (Separator,
 * Space) category, or U+0009 CHARACTER TABULATION (HT), U+000A LINE FEED (LF),
 * U+000C (FF), or U+000D CARRIAGE RETURN (CR) (**\[UNICODE]**).
 *
 * See:
 * **\[UNICODE]**:
 * [The Unicode Standard](https://www.unicode.org/versions/).
 * Unicode Consortium.
 */

const unicodeWhitespace = regexCheck(/\s/);
/**
 * Check whether the character code represents Unicode punctuation.
 *
 * A **Unicode punctuation** is a character in the Unicode `Pc` (Punctuation,
 * Connector), `Pd` (Punctuation, Dash), `Pe` (Punctuation, Close), `Pf`
 * (Punctuation, Final quote), `Pi` (Punctuation, Initial quote), `Po`
 * (Punctuation, Other), or `Ps` (Punctuation, Open) categories, or an ASCII
 * punctuation (see `asciiPunctuation`).
 *
 * See:
 * **\[UNICODE]**:
 * [The Unicode Standard](https://www.unicode.org/versions/).
 * Unicode Consortium.
 */
// Size note: removing ASCII from the regex and using `asciiPunctuation` here
// In fact adds to the bundle size.

const unicodePunctuation = regexCheck(unicodePunctuationRegex);
/**
 * Create a code check from a regex.
 *
 * @param {RegExp} regex
 * @returns {(code: Code) => code is number}
 */

function regexCheck(regex) {
  return check
  /**
   * Check whether a code matches the bound regex.
   *
   * @param {Code} code Character code
   * @returns {code is number} Whether the character code matches the bound regex
   */

  function check(code) {
    return code !== null && regex.test(String.fromCharCode(code))
  }
}

/**
 * @typedef {import('micromark-util-types').Effects} Effects
 * @typedef {import('micromark-util-types').State} State
 */
/**
 * @param {Effects} effects
 * @param {State} ok
 * @param {string} type
 * @param {number} [max=Infinity]
 * @returns {State}
 */

function factorySpace(effects, ok, type, max) {
  const limit = max ? max - 1 : Number.POSITIVE_INFINITY;
  let size = 0;
  return start
  /** @type {State} */

  function start(code) {
    if (markdownSpace(code)) {
      effects.enter(type);
      return prefix(code)
    }

    return ok(code)
  }
  /** @type {State} */

  function prefix(code) {
    if (markdownSpace(code) && size++ < limit) {
      effects.consume(code);
      return prefix
    }

    effects.exit(type);
    return ok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').InitialConstruct} InitialConstruct
 * @typedef {import('micromark-util-types').Initializer} Initializer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {InitialConstruct} */
const content$1 = {
  tokenize: initializeContent
};
/** @type {Initializer} */

function initializeContent(effects) {
  const contentStart = effects.attempt(
    this.parser.constructs.contentInitial,
    afterContentStartConstruct,
    paragraphInitial
  );
  /** @type {Token} */

  let previous;
  return contentStart
  /** @type {State} */

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
  /** @type {State} */

  function paragraphInitial(code) {
    effects.enter('paragraph');
    return lineStart(code)
  }
  /** @type {State} */

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
  /** @type {State} */

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
    } // Data.

    effects.consume(code);
    return data
  }
}

/**
 * @typedef {import('micromark-util-types').InitialConstruct} InitialConstruct
 * @typedef {import('micromark-util-types').Initializer} Initializer
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Point} Point
 */
/** @type {InitialConstruct} */

const document$1 = {
  tokenize: initializeDocument
};
/** @type {Construct} */

const containerConstruct = {
  tokenize: tokenizeContainer
};
/** @type {Initializer} */

function initializeDocument(effects) {
  const self = this;
  /** @type {StackItem[]} */

  const stack = [];
  let continued = 0;
  /** @type {TokenizeContext|undefined} */

  let childFlow;
  /** @type {Token|undefined} */

  let childToken;
  /** @type {number} */

  let lineStartOffset;
  return start
  /** @type {State} */

  function start(code) {
    // First we iterate through the open blocks, starting with the root
    // document, and descending through last children down to the last open
    // block.
    // Each block imposes a condition that the line must satisfy if the block is
    // to remain open.
    // For example, a block quote requires a `>` character.
    // A paragraph requires a non-blank line.
    // In this phase we may match all or just some of the open blocks.
    // But we cannot close unmatched blocks yet, because we may have a lazy
    // continuation line.
    if (continued < stack.length) {
      const item = stack[continued];
      self.containerState = item[1];
      return effects.attempt(
        item[0].continuation,
        documentContinue,
        checkNewContainers
      )(code)
    } // Done.

    return checkNewContainers(code)
  }
  /** @type {State} */

  function documentContinue(code) {
    continued++; // Note: this field is called `_closeFlow` but it also closes containers.
    // Perhaps a good idea to rename it but it’s already used in the wild by
    // extensions.

    if (self.containerState._closeFlow) {
      self.containerState._closeFlow = undefined;

      if (childFlow) {
        closeFlow();
      } // Note: this algorithm for moving events around is similar to the
      // algorithm when dealing with lazy lines in `writeToChild`.

      const indexBeforeExits = self.events.length;
      let indexBeforeFlow = indexBeforeExits;
      /** @type {Point|undefined} */

      let point; // Find the flow chunk.

      while (indexBeforeFlow--) {
        if (
          self.events[indexBeforeFlow][0] === 'exit' &&
          self.events[indexBeforeFlow][1].type === 'chunkFlow'
        ) {
          point = self.events[indexBeforeFlow][1].end;
          break
        }
      }

      exitContainers(continued); // Fix positions.

      let index = indexBeforeExits;

      while (index < self.events.length) {
        self.events[index][1].end = Object.assign({}, point);
        index++;
      } // Inject the exits earlier (they’re still also at the end).

      splice(
        self.events,
        indexBeforeFlow + 1,
        0,
        self.events.slice(indexBeforeExits)
      ); // Discard the duplicate exits.

      self.events.length = index;
      return checkNewContainers(code)
    }

    return start(code)
  }
  /** @type {State} */

  function checkNewContainers(code) {
    // Next, after consuming the continuation markers for existing blocks, we
    // look for new block starts (e.g. `>` for a block quote).
    // If we encounter a new block start, we close any blocks unmatched in
    // step 1 before creating the new block as a child of the last matched
    // block.
    if (continued === stack.length) {
      // No need to `check` whether there’s a container, of `exitContainers`
      // would be moot.
      // We can instead immediately `attempt` to parse one.
      if (!childFlow) {
        return documentContinued(code)
      } // If we have concrete content, such as block HTML or fenced code,
      // we can’t have containers “pierce” into them, so we can immediately
      // start.

      if (childFlow.currentConstruct && childFlow.currentConstruct.concrete) {
        return flowStart(code)
      } // If we do have flow, it could still be a blank line,
      // but we’d be interrupting it w/ a new container if there’s a current
      // construct.

      self.interrupt = Boolean(childFlow.currentConstruct);
    } // Check if there is a new container.

    self.containerState = {};
    return effects.check(
      containerConstruct,
      thereIsANewContainer,
      thereIsNoNewContainer
    )(code)
  }
  /** @type {State} */

  function thereIsANewContainer(code) {
    if (childFlow) closeFlow();
    exitContainers(continued);
    return documentContinued(code)
  }
  /** @type {State} */

  function thereIsNoNewContainer(code) {
    self.parser.lazy[self.now().line] = continued !== stack.length;
    lineStartOffset = self.now().offset;
    return flowStart(code)
  }
  /** @type {State} */

  function documentContinued(code) {
    // Try new containers.
    self.containerState = {};
    return effects.attempt(
      containerConstruct,
      containerContinue,
      flowStart
    )(code)
  }
  /** @type {State} */

  function containerContinue(code) {
    continued++;
    stack.push([self.currentConstruct, self.containerState]); // Try another.

    return documentContinued(code)
  }
  /** @type {State} */

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
  /** @type {State} */

  function flowContinue(code) {
    if (code === null) {
      writeToChild(effects.exit('chunkFlow'), true);
      exitContainers(0);
      effects.consume(code);
      return
    }

    if (markdownLineEnding(code)) {
      effects.consume(code);
      writeToChild(effects.exit('chunkFlow')); // Get ready for the next line.

      continued = 0;
      self.interrupt = undefined;
      return start
    }

    effects.consume(code);
    return flowContinue
  }
  /**
   * @param {Token} token
   * @param {boolean} [eof]
   * @returns {void}
   */

  function writeToChild(token, eof) {
    const stream = self.sliceStream(token);
    if (eof) stream.push(null);
    token.previous = childToken;
    if (childToken) childToken.next = token;
    childToken = token;
    childFlow.defineSkip(token.start);
    childFlow.write(stream); // Alright, so we just added a lazy line:
    //
    // ```markdown
    // > a
    // b.
    //
    // Or:
    //
    // > ~~~c
    // d
    //
    // Or:
    //
    // > | e |
    // f
    // ```
    //
    // The construct in the second example (fenced code) does not accept lazy
    // lines, so it marked itself as done at the end of its first line, and
    // then the content construct parses `d`.
    // Most constructs in markdown match on the first line: if the first line
    // forms a construct, a non-lazy line can’t “unmake” it.
    //
    // The construct in the third example is potentially a GFM table, and
    // those are *weird*.
    // It *could* be a table, from the first line, if the following line
    // matches a condition.
    // In this case, that second line is lazy, which “unmakes” the first line
    // and turns the whole into one content block.
    //
    // We’ve now parsed the non-lazy and the lazy line, and can figure out
    // whether the lazy line started a new flow block.
    // If it did, we exit the current containers between the two flow blocks.

    if (self.parser.lazy[token.start.line]) {
      let index = childFlow.events.length;

      while (index--) {
        if (
          // The token starts before the line ending…
          childFlow.events[index][1].start.offset < lineStartOffset && // …and either is not ended yet…
          (!childFlow.events[index][1].end || // …or ends after it.
            childFlow.events[index][1].end.offset > lineStartOffset)
        ) {
          // Exit: there’s still something open, which means it’s a lazy line
          // part of something.
          return
        }
      } // Note: this algorithm for moving events around is similar to the
      // algorithm when closing flow in `documentContinue`.

      const indexBeforeExits = self.events.length;
      let indexBeforeFlow = indexBeforeExits;
      /** @type {boolean|undefined} */

      let seen;
      /** @type {Point|undefined} */

      let point; // Find the previous chunk (the one before the lazy line).

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

      exitContainers(continued); // Fix positions.

      index = indexBeforeExits;

      while (index < self.events.length) {
        self.events[index][1].end = Object.assign({}, point);
        index++;
      } // Inject the exits earlier (they’re still also at the end).

      splice(
        self.events,
        indexBeforeFlow + 1,
        0,
        self.events.slice(indexBeforeExits)
      ); // Discard the duplicate exits.

      self.events.length = index;
    }
  }
  /**
   * @param {number} size
   * @returns {void}
   */

  function exitContainers(size) {
    let index = stack.length; // Exit open containers.

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
/** @type {Tokenizer} */

function tokenizeContainer(effects, ok, nok) {
  return factorySpace(
    effects,
    effects.attempt(this.parser.constructs.document, ok, nok),
    'linePrefix',
    this.parser.constructs.disable.null.includes('codeIndented') ? undefined : 4
  )
}

/**
 * @typedef {import('micromark-util-types').Code} Code
 */

/**
 * Classify whether a character code represents whitespace, punctuation, or
 * something else.
 *
 * Used for attention (emphasis, strong), whose sequences can open or close
 * based on the class of surrounding characters.
 *
 * Note that eof (`null`) is seen as whitespace.
 *
 * @param {Code} code
 * @returns {number|undefined}
 */
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

/**
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').Resolver} Resolver
 */

/**
 * Call all `resolveAll`s.
 *
 * @param {{resolveAll?: Resolver}[]} constructs
 * @param {Event[]} events
 * @param {TokenizeContext} context
 * @returns {Event[]}
 */
function resolveAll(constructs, events, context) {
  /** @type {Resolver[]} */
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

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').Code} Code
 * @typedef {import('micromark-util-types').Point} Point
 */

/** @type {Construct} */
const attention = {
  name: 'attention',
  tokenize: tokenizeAttention,
  resolveAll: resolveAllAttention
};
/**
 * Take all events and resolve attention to emphasis or strong.
 *
 * @type {Resolver}
 */

function resolveAllAttention(events, context) {
  let index = -1;
  /** @type {number} */

  let open;
  /** @type {Token} */

  let group;
  /** @type {Token} */

  let text;
  /** @type {Token} */

  let openingSequence;
  /** @type {Token} */

  let closingSequence;
  /** @type {number} */

  let use;
  /** @type {Event[]} */

  let nextEvents;
  /** @type {number} */

  let offset; // Walk through all events.
  //
  // Note: performance of this is fine on an mb of normal markdown, but it’s
  // a bottleneck for malicious stuff.

  while (++index < events.length) {
    // Find a token that can close.
    if (
      events[index][0] === 'enter' &&
      events[index][1].type === 'attentionSequence' &&
      events[index][1]._close
    ) {
      open = index; // Now walk back to find an opener.

      while (open--) {
        // Find a token that can open the closer.
        if (
          events[open][0] === 'exit' &&
          events[open][1].type === 'attentionSequence' &&
          events[open][1]._open && // If the markers are the same:
          context.sliceSerialize(events[open][1]).charCodeAt(0) ===
            context.sliceSerialize(events[index][1]).charCodeAt(0)
        ) {
          // If the opening can close or the closing can open,
          // and the close size *is not* a multiple of three,
          // but the sum of the opening and closing size *is* multiple of three,
          // then don’t match.
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
          } // Number of markers to use from the sequence.

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
          nextEvents = []; // If there are more markers in the opening, add them before.

          if (events[open][1].end.offset - events[open][1].start.offset) {
            nextEvents = push(nextEvents, [
              ['enter', events[open][1], context],
              ['exit', events[open][1], context]
            ]);
          } // Opening.

          nextEvents = push(nextEvents, [
            ['enter', group, context],
            ['enter', openingSequence, context],
            ['exit', openingSequence, context],
            ['enter', text, context]
          ]); // Between.

          nextEvents = push(
            nextEvents,
            resolveAll(
              context.parser.constructs.insideSpan.null,
              events.slice(open + 1, index),
              context
            )
          ); // Closing.

          nextEvents = push(nextEvents, [
            ['exit', text, context],
            ['enter', closingSequence, context],
            ['exit', closingSequence, context],
            ['exit', group, context]
          ]); // If there are more markers in the closing, add them after.

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
  } // Remove remaining sequences.

  index = -1;

  while (++index < events.length) {
    if (events[index][1].type === 'attentionSequence') {
      events[index][1].type = 'data';
    }
  }

  return events
}
/** @type {Tokenizer} */

function tokenizeAttention(effects, ok) {
  const attentionMarkers = this.parser.constructs.attentionMarkers.null;
  const previous = this.previous;
  const before = classifyCharacter(previous);
  /** @type {NonNullable<Code>} */

  let marker;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('attentionSequence');
    marker = code;
    return sequence(code)
  }
  /** @type {State} */

  function sequence(code) {
    if (code === marker) {
      effects.consume(code);
      return sequence
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
/**
 * Move a point a bit.
 *
 * Note: `move` only works inside lines! It’s not possible to move past other
 * chunks (replacement characters, tabs, or line endings).
 *
 * @param {Point} point
 * @param {number} offset
 * @returns {void}
 */

function movePoint(point, offset) {
  point.column += offset;
  point.offset += offset;
  point._bufferIndex += offset;
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const autolink = {
  name: 'autolink',
  tokenize: tokenizeAutolink
};
/** @type {Tokenizer} */

function tokenizeAutolink(effects, ok, nok) {
  let size = 1;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('autolink');
    effects.enter('autolinkMarker');
    effects.consume(code);
    effects.exit('autolinkMarker');
    effects.enter('autolinkProtocol');
    return open
  }
  /** @type {State} */

  function open(code) {
    if (asciiAlpha(code)) {
      effects.consume(code);
      return schemeOrEmailAtext
    }

    return asciiAtext(code) ? emailAtext(code) : nok(code)
  }
  /** @type {State} */

  function schemeOrEmailAtext(code) {
    return code === 43 || code === 45 || code === 46 || asciiAlphanumeric(code)
      ? schemeInsideOrEmailAtext(code)
      : emailAtext(code)
  }
  /** @type {State} */

  function schemeInsideOrEmailAtext(code) {
    if (code === 58) {
      effects.consume(code);
      return urlInside
    }

    if (
      (code === 43 || code === 45 || code === 46 || asciiAlphanumeric(code)) &&
      size++ < 32
    ) {
      effects.consume(code);
      return schemeInsideOrEmailAtext
    }

    return emailAtext(code)
  }
  /** @type {State} */

  function urlInside(code) {
    if (code === 62) {
      effects.exit('autolinkProtocol');
      return end(code)
    }

    if (code === null || code === 32 || code === 60 || asciiControl(code)) {
      return nok(code)
    }

    effects.consume(code);
    return urlInside
  }
  /** @type {State} */

  function emailAtext(code) {
    if (code === 64) {
      effects.consume(code);
      size = 0;
      return emailAtSignOrDot
    }

    if (asciiAtext(code)) {
      effects.consume(code);
      return emailAtext
    }

    return nok(code)
  }
  /** @type {State} */

  function emailAtSignOrDot(code) {
    return asciiAlphanumeric(code) ? emailLabel(code) : nok(code)
  }
  /** @type {State} */

  function emailLabel(code) {
    if (code === 46) {
      effects.consume(code);
      size = 0;
      return emailAtSignOrDot
    }

    if (code === 62) {
      // Exit, then change the type.
      effects.exit('autolinkProtocol').type = 'autolinkEmail';
      return end(code)
    }

    return emailValue(code)
  }
  /** @type {State} */

  function emailValue(code) {
    if ((code === 45 || asciiAlphanumeric(code)) && size++ < 63) {
      effects.consume(code);
      return code === 45 ? emailValue : emailLabel
    }

    return nok(code)
  }
  /** @type {State} */

  function end(code) {
    effects.enter('autolinkMarker');
    effects.consume(code);
    effects.exit('autolinkMarker');
    effects.exit('autolink');
    return ok
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const blankLine = {
  tokenize: tokenizeBlankLine,
  partial: true
};
/** @type {Tokenizer} */

function tokenizeBlankLine(effects, ok, nok) {
  return factorySpace(effects, afterWhitespace, 'linePrefix')
  /** @type {State} */

  function afterWhitespace(code) {
    return code === null || markdownLineEnding(code) ? ok(code) : nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Exiter} Exiter
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const blockQuote = {
  name: 'blockQuote',
  tokenize: tokenizeBlockQuoteStart,
  continuation: {
    tokenize: tokenizeBlockQuoteContinuation
  },
  exit: exit$1
};
/** @type {Tokenizer} */

function tokenizeBlockQuoteStart(effects, ok, nok) {
  const self = this;
  return start
  /** @type {State} */

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
  /** @type {State} */

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
/** @type {Tokenizer} */

function tokenizeBlockQuoteContinuation(effects, ok, nok) {
  return factorySpace(
    effects,
    effects.attempt(blockQuote, ok, nok),
    'linePrefix',
    this.parser.constructs.disable.null.includes('codeIndented') ? undefined : 4
  )
}
/** @type {Exiter} */

function exit$1(effects) {
  effects.exit('blockQuote');
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const characterEscape = {
  name: 'characterEscape',
  tokenize: tokenizeCharacterEscape
};
/** @type {Tokenizer} */

function tokenizeCharacterEscape(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('characterEscape');
    effects.enter('escapeMarker');
    effects.consume(code);
    effects.exit('escapeMarker');
    return open
  }
  /** @type {State} */

  function open(code) {
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

var characterEntities = {
  AEli: 'Æ',
  AElig: 'Æ',
  AM: '&',
  AMP: '&',
  Aacut: 'Á',
  Aacute: 'Á',
  Abreve: 'Ă',
  Acir: 'Â',
  Acirc: 'Â',
  Acy: 'А',
  Afr: '𝔄',
  Agrav: 'À',
  Agrave: 'À',
  Alpha: 'Α',
  Amacr: 'Ā',
  And: '⩓',
  Aogon: 'Ą',
  Aopf: '𝔸',
  ApplyFunction: '⁡',
  Arin: 'Å',
  Aring: 'Å',
  Ascr: '𝒜',
  Assign: '≔',
  Atild: 'Ã',
  Atilde: 'Ã',
  Aum: 'Ä',
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
  COP: '©',
  COPY: '©',
  Cacute: 'Ć',
  Cap: '⋒',
  CapitalDifferentialD: 'ⅅ',
  Cayleys: 'ℭ',
  Ccaron: 'Č',
  Ccedi: 'Ç',
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
  ET: 'Ð',
  ETH: 'Ð',
  Eacut: 'É',
  Eacute: 'É',
  Ecaron: 'Ě',
  Ecir: 'Ê',
  Ecirc: 'Ê',
  Ecy: 'Э',
  Edot: 'Ė',
  Efr: '𝔈',
  Egrav: 'È',
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
  Eum: 'Ë',
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
  G: '>',
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
  Iacut: 'Í',
  Iacute: 'Í',
  Icir: 'Î',
  Icirc: 'Î',
  Icy: 'И',
  Idot: 'İ',
  Ifr: 'ℑ',
  Igrav: 'Ì',
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
  Ium: 'Ï',
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
  L: '<',
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
  Ntild: 'Ñ',
  Ntilde: 'Ñ',
  Nu: 'Ν',
  OElig: 'Œ',
  Oacut: 'Ó',
  Oacute: 'Ó',
  Ocir: 'Ô',
  Ocirc: 'Ô',
  Ocy: 'О',
  Odblac: 'Ő',
  Ofr: '𝔒',
  Ograv: 'Ò',
  Ograve: 'Ò',
  Omacr: 'Ō',
  Omega: 'Ω',
  Omicron: 'Ο',
  Oopf: '𝕆',
  OpenCurlyDoubleQuote: '“',
  OpenCurlyQuote: '‘',
  Or: '⩔',
  Oscr: '𝒪',
  Oslas: 'Ø',
  Oslash: 'Ø',
  Otild: 'Õ',
  Otilde: 'Õ',
  Otimes: '⨷',
  Oum: 'Ö',
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
  QUO: '"',
  QUOT: '"',
  Qfr: '𝔔',
  Qopf: 'ℚ',
  Qscr: '𝒬',
  RBarr: '⤐',
  RE: '®',
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
  THOR: 'Þ',
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
  Uacut: 'Ú',
  Uacute: 'Ú',
  Uarr: '↟',
  Uarrocir: '⥉',
  Ubrcy: 'Ў',
  Ubreve: 'Ŭ',
  Ucir: 'Û',
  Ucirc: 'Û',
  Ucy: 'У',
  Udblac: 'Ű',
  Ufr: '𝔘',
  Ugrav: 'Ù',
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
  Uum: 'Ü',
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
  Yacut: 'Ý',
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
  aacut: 'á',
  aacute: 'á',
  abreve: 'ă',
  ac: '∾',
  acE: '∾̳',
  acd: '∿',
  acir: 'â',
  acirc: 'â',
  acut: '´',
  acute: '´',
  acy: 'а',
  aeli: 'æ',
  aelig: 'æ',
  af: '⁡',
  afr: '𝔞',
  agrav: 'à',
  agrave: 'à',
  alefsym: 'ℵ',
  aleph: 'ℵ',
  alpha: 'α',
  amacr: 'ā',
  amalg: '⨿',
  am: '&',
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
  arin: 'å',
  aring: 'å',
  ascr: '𝒶',
  ast: '*',
  asymp: '≈',
  asympeq: '≍',
  atild: 'ã',
  atilde: 'ã',
  aum: 'ä',
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
  brvba: '¦',
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
  ccedi: 'ç',
  ccedil: 'ç',
  ccirc: 'ĉ',
  ccups: '⩌',
  ccupssm: '⩐',
  cdot: 'ċ',
  cedi: '¸',
  cedil: '¸',
  cemptyv: '⦲',
  cen: '¢',
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
  cop: '©',
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
  curre: '¤',
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
  de: '°',
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
  divid: '÷',
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
  eacut: 'é',
  eacute: 'é',
  easter: '⩮',
  ecaron: 'ě',
  ecir: 'ê',
  ecirc: 'ê',
  ecolon: '≕',
  ecy: 'э',
  edot: 'ė',
  ee: 'ⅇ',
  efDot: '≒',
  efr: '𝔢',
  eg: '⪚',
  egrav: 'è',
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
  et: 'ð',
  eth: 'ð',
  eum: 'ë',
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
  frac1: '¼',
  frac12: '½',
  frac13: '⅓',
  frac14: '¼',
  frac15: '⅕',
  frac16: '⅙',
  frac18: '⅛',
  frac23: '⅔',
  frac25: '⅖',
  frac3: '¾',
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
  g: '>',
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
  iacut: 'í',
  iacute: 'í',
  ic: '⁣',
  icir: 'î',
  icirc: 'î',
  icy: 'и',
  iecy: 'е',
  iexc: '¡',
  iexcl: '¡',
  iff: '⇔',
  ifr: '𝔦',
  igrav: 'ì',
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
  iques: '¿',
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
  ium: 'ï',
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
  laqu: '«',
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
  l: '<',
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
  mac: '¯',
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
  micr: 'µ',
  micro: 'µ',
  mid: '∣',
  midast: '*',
  midcir: '⫰',
  middo: '·',
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
  nbs: ' ',
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
  no: '¬',
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
  ntild: 'ñ',
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
  oacut: 'ó',
  oacute: 'ó',
  oast: '⊛',
  ocir: 'ô',
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
  ograv: 'ò',
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
  ord: 'º',
  order: 'ℴ',
  orderof: 'ℴ',
  ordf: 'ª',
  ordm: 'º',
  origof: '⊶',
  oror: '⩖',
  orslope: '⩗',
  orv: '⩛',
  oscr: 'ℴ',
  oslas: 'ø',
  oslash: 'ø',
  osol: '⊘',
  otild: 'õ',
  otilde: 'õ',
  otimes: '⊗',
  otimesas: '⨶',
  oum: 'ö',
  ouml: 'ö',
  ovbar: '⌽',
  par: '¶',
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
  plusm: '±',
  plusmn: '±',
  plussim: '⨦',
  plustwo: '⨧',
  pm: '±',
  pointint: '⨕',
  popf: '𝕡',
  poun: '£',
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
  quo: '"',
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
  raqu: '»',
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
  re: '®',
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
  sec: '§',
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
  sh: '­',
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
  sup: '⊃',
  sup1: '¹',
  sup2: '²',
  sup3: '³',
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
  szli: 'ß',
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
  thor: 'þ',
  thorn: 'þ',
  tilde: '˜',
  time: '×',
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
  uacut: 'ú',
  uacute: 'ú',
  uarr: '↑',
  ubrcy: 'ў',
  ubreve: 'ŭ',
  ucir: 'û',
  ucirc: 'û',
  ucy: 'у',
  udarr: '⇅',
  udblac: 'ű',
  udhar: '⥮',
  ufisht: '⥾',
  ufr: '𝔲',
  ugrav: 'ù',
  ugrave: 'ù',
  uharl: '↿',
  uharr: '↾',
  uhblk: '▀',
  ulcorn: '⌜',
  ulcorner: '⌜',
  ulcrop: '⌏',
  ultri: '◸',
  umacr: 'ū',
  um: '¨',
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
  uum: 'ü',
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
  yacut: 'ý',
  yacute: 'ý',
  yacy: 'я',
  ycirc: 'ŷ',
  ycy: 'ы',
  ye: '¥',
  yen: '¥',
  yfr: '𝔶',
  yicy: 'ї',
  yopf: '𝕪',
  yscr: '𝓎',
  yucy: 'ю',
  yum: 'ÿ',
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

var own$6 = {}.hasOwnProperty;

/**
 * @param {string} characters
 * @returns {string|false}
 */
function decodeEntity(characters) {
  return own$6.call(characterEntities, characters)
    ? characterEntities[characters]
    : false
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

/** @type {Construct} */
const characterReference = {
  name: 'characterReference',
  tokenize: tokenizeCharacterReference
};
/** @type {Tokenizer} */

function tokenizeCharacterReference(effects, ok, nok) {
  const self = this;
  let size = 0;
  /** @type {number} */

  let max;
  /** @type {(code: Code) => code is number} */

  let test;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('characterReference');
    effects.enter('characterReferenceMarker');
    effects.consume(code);
    effects.exit('characterReferenceMarker');
    return open
  }
  /** @type {State} */

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
  /** @type {State} */

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
  /** @type {State} */

  function value(code) {
    /** @type {Token} */
    let token;

    if (code === 59 && size) {
      token = effects.exit('characterReferenceValue');

      if (
        test === asciiAlphanumeric &&
        !decodeEntity(self.sliceSerialize(token))
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

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

/** @type {Construct} */
const codeFenced = {
  name: 'codeFenced',
  tokenize: tokenizeCodeFenced,
  concrete: true
};
/** @type {Tokenizer} */

function tokenizeCodeFenced(effects, ok, nok) {
  const self = this;
  /** @type {Construct} */

  const closingFenceConstruct = {
    tokenize: tokenizeClosingFence,
    partial: true
  };
  /** @type {Construct} */

  const nonLazyLine = {
    tokenize: tokenizeNonLazyLine,
    partial: true
  };
  const tail = this.events[this.events.length - 1];
  const initialPrefix =
    tail && tail[1].type === 'linePrefix'
      ? tail[2].sliceSerialize(tail[1], true).length
      : 0;
  let sizeOpen = 0;
  /** @type {NonNullable<Code>} */

  let marker;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('codeFenced');
    effects.enter('codeFencedFence');
    effects.enter('codeFencedFenceSequence');
    marker = code;
    return sequenceOpen(code)
  }
  /** @type {State} */

  function sequenceOpen(code) {
    if (code === marker) {
      effects.consume(code);
      sizeOpen++;
      return sequenceOpen
    }

    effects.exit('codeFencedFenceSequence');
    return sizeOpen < 3
      ? nok(code)
      : factorySpace(effects, infoOpen, 'whitespace')(code)
  }
  /** @type {State} */

  function infoOpen(code) {
    if (code === null || markdownLineEnding(code)) {
      return openAfter(code)
    }

    effects.enter('codeFencedFenceInfo');
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return info(code)
  }
  /** @type {State} */

  function info(code) {
    if (code === null || markdownLineEndingOrSpace(code)) {
      effects.exit('chunkString');
      effects.exit('codeFencedFenceInfo');
      return factorySpace(effects, infoAfter, 'whitespace')(code)
    }

    if (code === 96 && code === marker) return nok(code)
    effects.consume(code);
    return info
  }
  /** @type {State} */

  function infoAfter(code) {
    if (code === null || markdownLineEnding(code)) {
      return openAfter(code)
    }

    effects.enter('codeFencedFenceMeta');
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return meta(code)
  }
  /** @type {State} */

  function meta(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('chunkString');
      effects.exit('codeFencedFenceMeta');
      return openAfter(code)
    }

    if (code === 96 && code === marker) return nok(code)
    effects.consume(code);
    return meta
  }
  /** @type {State} */

  function openAfter(code) {
    effects.exit('codeFencedFence');
    return self.interrupt ? ok(code) : contentStart(code)
  }
  /** @type {State} */

  function contentStart(code) {
    if (code === null) {
      return after(code)
    }

    if (markdownLineEnding(code)) {
      return effects.attempt(
        nonLazyLine,
        effects.attempt(
          closingFenceConstruct,
          after,
          initialPrefix
            ? factorySpace(
                effects,
                contentStart,
                'linePrefix',
                initialPrefix + 1
              )
            : contentStart
        ),
        after
      )(code)
    }

    effects.enter('codeFlowValue');
    return contentContinue(code)
  }
  /** @type {State} */

  function contentContinue(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('codeFlowValue');
      return contentStart(code)
    }

    effects.consume(code);
    return contentContinue
  }
  /** @type {State} */

  function after(code) {
    effects.exit('codeFenced');
    return ok(code)
  }
  /** @type {Tokenizer} */

  function tokenizeNonLazyLine(effects, ok, nok) {
    const self = this;
    return start
    /** @type {State} */

    function start(code) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return lineStart
    }
    /** @type {State} */

    function lineStart(code) {
      return self.parser.lazy[self.now().line] ? nok(code) : ok(code)
    }
  }
  /** @type {Tokenizer} */

  function tokenizeClosingFence(effects, ok, nok) {
    let size = 0;
    return factorySpace(
      effects,
      closingSequenceStart,
      'linePrefix',
      this.parser.constructs.disable.null.includes('codeIndented')
        ? undefined
        : 4
    )
    /** @type {State} */

    function closingSequenceStart(code) {
      effects.enter('codeFencedFence');
      effects.enter('codeFencedFenceSequence');
      return closingSequence(code)
    }
    /** @type {State} */

    function closingSequence(code) {
      if (code === marker) {
        effects.consume(code);
        size++;
        return closingSequence
      }

      if (size < sizeOpen) return nok(code)
      effects.exit('codeFencedFenceSequence');
      return factorySpace(effects, closingSequenceEnd, 'whitespace')(code)
    }
    /** @type {State} */

    function closingSequenceEnd(code) {
      if (code === null || markdownLineEnding(code)) {
        effects.exit('codeFencedFence');
        return ok(code)
      }

      return nok(code)
    }
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const codeIndented = {
  name: 'codeIndented',
  tokenize: tokenizeCodeIndented
};
/** @type {Construct} */

const indentedContent = {
  tokenize: tokenizeIndentedContent,
  partial: true
};
/** @type {Tokenizer} */

function tokenizeCodeIndented(effects, ok, nok) {
  const self = this;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('codeIndented');
    return factorySpace(effects, afterStartPrefix, 'linePrefix', 4 + 1)(code)
  }
  /** @type {State} */

  function afterStartPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'linePrefix' &&
      tail[2].sliceSerialize(tail[1], true).length >= 4
      ? afterPrefix(code)
      : nok(code)
  }
  /** @type {State} */

  function afterPrefix(code) {
    if (code === null) {
      return after(code)
    }

    if (markdownLineEnding(code)) {
      return effects.attempt(indentedContent, afterPrefix, after)(code)
    }

    effects.enter('codeFlowValue');
    return content(code)
  }
  /** @type {State} */

  function content(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('codeFlowValue');
      return afterPrefix(code)
    }

    effects.consume(code);
    return content
  }
  /** @type {State} */

  function after(code) {
    effects.exit('codeIndented');
    return ok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeIndentedContent(effects, ok, nok) {
  const self = this;
  return start
  /** @type {State} */

  function start(code) {
    // If this is a lazy line, it can’t be code.
    if (self.parser.lazy[self.now().line]) {
      return nok(code)
    }

    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return start
    }

    return factorySpace(effects, afterPrefix, 'linePrefix', 4 + 1)(code)
  }
  /** @type {State} */

  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'linePrefix' &&
      tail[2].sliceSerialize(tail[1], true).length >= 4
      ? ok(code)
      : markdownLineEnding(code)
      ? start(code)
      : nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Previous} Previous
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const codeText = {
  name: 'codeText',
  tokenize: tokenizeCodeText,
  resolve: resolveCodeText,
  previous: previous$1
};
/** @type {Resolver} */

function resolveCodeText(events) {
  let tailExitIndex = events.length - 4;
  let headEnterIndex = 3;
  /** @type {number} */

  let index;
  /** @type {number|undefined} */

  let enter; // If we start and end with an EOL or a space.

  if (
    (events[headEnterIndex][1].type === 'lineEnding' ||
      events[headEnterIndex][1].type === 'space') &&
    (events[tailExitIndex][1].type === 'lineEnding' ||
      events[tailExitIndex][1].type === 'space')
  ) {
    index = headEnterIndex; // And we have data.

    while (++index < tailExitIndex) {
      if (events[index][1].type === 'codeTextData') {
        // Then we have padding.
        events[headEnterIndex][1].type = 'codeTextPadding';
        events[tailExitIndex][1].type = 'codeTextPadding';
        headEnterIndex += 2;
        tailExitIndex -= 2;
        break
      }
    }
  } // Merge adjacent spaces and data.

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
/** @type {Previous} */

function previous$1(code) {
  // If there is a previous code, there will always be a tail.
  return (
    code !== 96 ||
    this.events[this.events.length - 1][1].type === 'characterEscape'
  )
}
/** @type {Tokenizer} */

function tokenizeCodeText(effects, ok, nok) {
  let sizeOpen = 0;
  /** @type {number} */

  let size;
  /** @type {Token} */

  let token;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('codeText');
    effects.enter('codeTextSequence');
    return openingSequence(code)
  }
  /** @type {State} */

  function openingSequence(code) {
    if (code === 96) {
      effects.consume(code);
      sizeOpen++;
      return openingSequence
    }

    effects.exit('codeTextSequence');
    return gap(code)
  }
  /** @type {State} */

  function gap(code) {
    // EOF.
    if (code === null) {
      return nok(code)
    } // Closing fence?
    // Could also be data.

    if (code === 96) {
      token = effects.enter('codeTextSequence');
      size = 0;
      return closingSequence(code)
    } // Tabs don’t work, and virtual spaces don’t make sense.

    if (code === 32) {
      effects.enter('space');
      effects.consume(code);
      effects.exit('space');
      return gap
    }

    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return gap
    } // Data.

    effects.enter('codeTextData');
    return data(code)
  } // In code.

  /** @type {State} */

  function data(code) {
    if (
      code === null ||
      code === 32 ||
      code === 96 ||
      markdownLineEnding(code)
    ) {
      effects.exit('codeTextData');
      return gap(code)
    }

    effects.consume(code);
    return data
  } // Closing fence.

  /** @type {State} */

  function closingSequence(code) {
    // More.
    if (code === 96) {
      effects.consume(code);
      size++;
      return closingSequence
    } // Done!

    if (size === sizeOpen) {
      effects.exit('codeTextSequence');
      effects.exit('codeText');
      return ok(code)
    } // More or less accents: mark as data.

    token.type = 'codeTextData';
    return data(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').Chunk} Chunk
 * @typedef {import('micromark-util-types').Event} Event
 */

/**
 * Tokenize subcontent.
 *
 * @param {Event[]} events
 * @returns {boolean}
 */
function subtokenize(events) {
  /** @type {Record<string, number>} */
  const jumps = {};
  let index = -1;
  /** @type {Event} */

  let event;
  /** @type {number|undefined} */

  let lineIndex;
  /** @type {number} */

  let otherIndex;
  /** @type {Event} */

  let otherEvent;
  /** @type {Event[]} */

  let parameters;
  /** @type {Event[]} */

  let subevents;
  /** @type {boolean|undefined} */

  let more;

  while (++index < events.length) {
    while (index in jumps) {
      index = jumps[index];
    }

    event = events[index]; // Add a hook for the GFM tasklist extension, which needs to know if text
    // is in the first content of a list item.

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
    } // Enter.

    if (event[0] === 'enter') {
      if (event[1].contentType) {
        Object.assign(jumps, subcontent(events, index));
        index = jumps[index];
        more = true;
      }
    } // Exit.
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
        // Fix position.
        event[1].end = Object.assign({}, events[lineIndex][1].start); // Switch container exit w/ line endings.

        parameters = events.slice(lineIndex, index);
        parameters.unshift(event);
        splice(events, lineIndex, index - lineIndex + 1, parameters);
      }
    }
  }

  return !more
}
/**
 * Tokenize embedded tokens.
 *
 * @param {Event[]} events
 * @param {number} eventIndex
 * @returns {Record<string, number>}
 */

function subcontent(events, eventIndex) {
  const token = events[eventIndex][1];
  const context = events[eventIndex][2];
  let startPosition = eventIndex - 1;
  /** @type {number[]} */

  const startPositions = [];
  const tokenizer =
    token._tokenizer || context.parser[token.contentType](token.start);
  const childEvents = tokenizer.events;
  /** @type {[number, number][]} */

  const jumps = [];
  /** @type {Record<string, number>} */

  const gaps = {};
  /** @type {Chunk[]} */

  let stream;
  /** @type {Token|undefined} */

  let previous;
  let index = -1;
  /** @type {Token|undefined} */

  let current = token;
  let adjust = 0;
  let start = 0;
  const breaks = [start]; // Loop forward through the linked tokens to pass them in order to the
  // subtokenizer.

  while (current) {
    // Find the position of the event for this token.
    while (events[++startPosition][1] !== current) {
      // Empty.
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
    } // Unravel the next token.

    previous = current;
    current = current.next;
  } // Now, loop back through all events (and linked tokens), to figure out which
  // parts belong where.

  current = token;

  while (++index < childEvents.length) {
    if (
      // Find a void token that includes a break.
      childEvents[index][0] === 'exit' &&
      childEvents[index - 1][0] === 'enter' &&
      childEvents[index][1].type === childEvents[index - 1][1].type &&
      childEvents[index][1].start.line !== childEvents[index][1].end.line
    ) {
      start = index + 1;
      breaks.push(start); // Help GC.

      current._tokenizer = undefined;
      current.previous = undefined;
      current = current.next;
    }
  } // Help GC.

  tokenizer.events = []; // If there’s one more token (which is the cases for lines that end in an
  // EOF), that’s perfect: the last point we found starts it.
  // If there isn’t then make sure any remaining content is added to it.

  if (current) {
    // Help GC.
    current._tokenizer = undefined;
    current.previous = undefined;
  } else {
    breaks.pop();
  } // Now splice the events from the subtokenizer into the current events,
  // moving back to front so that splice indices aren’t affected.

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

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */

/**
 * No name because it must not be turned off.
 * @type {Construct}
 */
const content = {
  tokenize: tokenizeContent,
  resolve: resolveContent
};
/** @type {Construct} */

const continuationConstruct = {
  tokenize: tokenizeContinuation,
  partial: true
};
/**
 * Content is transparent: it’s parsed right now. That way, definitions are also
 * parsed right now: before text in paragraphs (specifically, media) are parsed.
 *
 * @type {Resolver}
 */

function resolveContent(events) {
  subtokenize(events);
  return events
}
/** @type {Tokenizer} */

function tokenizeContent(effects, ok) {
  /** @type {Token} */
  let previous;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('content');
    previous = effects.enter('chunkContent', {
      contentType: 'content'
    });
    return data(code)
  }
  /** @type {State} */

  function data(code) {
    if (code === null) {
      return contentEnd(code)
    }

    if (markdownLineEnding(code)) {
      return effects.check(
        continuationConstruct,
        contentContinue,
        contentEnd
      )(code)
    } // Data.

    effects.consume(code);
    return data
  }
  /** @type {State} */

  function contentEnd(code) {
    effects.exit('chunkContent');
    effects.exit('content');
    return ok(code)
  }
  /** @type {State} */

  function contentContinue(code) {
    effects.consume(code);
    effects.exit('chunkContent');
    previous.next = effects.enter('chunkContent', {
      contentType: 'content',
      previous
    });
    previous = previous.next;
    return data
  }
}
/** @type {Tokenizer} */

function tokenizeContinuation(effects, ok, nok) {
  const self = this;
  return startLookahead
  /** @type {State} */

  function startLookahead(code) {
    effects.exit('chunkContent');
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return factorySpace(effects, prefixed, 'linePrefix')
  }
  /** @type {State} */

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

/**
 * @typedef {import('micromark-util-types').Effects} Effects
 * @typedef {import('micromark-util-types').State} State
 */

/**
 * @param {Effects} effects
 * @param {State} ok
 * @param {State} nok
 * @param {string} type
 * @param {string} literalType
 * @param {string} literalMarkerType
 * @param {string} rawType
 * @param {string} stringType
 * @param {number} [max=Infinity]
 * @returns {State}
 */
// eslint-disable-next-line max-params
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
  /** @type {State} */

  function start(code) {
    if (code === 60) {
      effects.enter(type);
      effects.enter(literalType);
      effects.enter(literalMarkerType);
      effects.consume(code);
      effects.exit(literalMarkerType);
      return destinationEnclosedBefore
    }

    if (code === null || code === 41 || asciiControl(code)) {
      return nok(code)
    }

    effects.enter(type);
    effects.enter(rawType);
    effects.enter(stringType);
    effects.enter('chunkString', {
      contentType: 'string'
    });
    return destinationRaw(code)
  }
  /** @type {State} */

  function destinationEnclosedBefore(code) {
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
    return destinationEnclosed(code)
  }
  /** @type {State} */

  function destinationEnclosed(code) {
    if (code === 62) {
      effects.exit('chunkString');
      effects.exit(stringType);
      return destinationEnclosedBefore(code)
    }

    if (code === null || code === 60 || markdownLineEnding(code)) {
      return nok(code)
    }

    effects.consume(code);
    return code === 92 ? destinationEnclosedEscape : destinationEnclosed
  }
  /** @type {State} */

  function destinationEnclosedEscape(code) {
    if (code === 60 || code === 62 || code === 92) {
      effects.consume(code);
      return destinationEnclosed
    }

    return destinationEnclosed(code)
  }
  /** @type {State} */

  function destinationRaw(code) {
    if (code === 40) {
      if (++balance > limit) return nok(code)
      effects.consume(code);
      return destinationRaw
    }

    if (code === 41) {
      if (!balance--) {
        effects.exit('chunkString');
        effects.exit(stringType);
        effects.exit(rawType);
        effects.exit(type);
        return ok(code)
      }

      effects.consume(code);
      return destinationRaw
    }

    if (code === null || markdownLineEndingOrSpace(code)) {
      if (balance) return nok(code)
      effects.exit('chunkString');
      effects.exit(stringType);
      effects.exit(rawType);
      effects.exit(type);
      return ok(code)
    }

    if (asciiControl(code)) return nok(code)
    effects.consume(code);
    return code === 92 ? destinationRawEscape : destinationRaw
  }
  /** @type {State} */

  function destinationRawEscape(code) {
    if (code === 40 || code === 41 || code === 92) {
      effects.consume(code);
      return destinationRaw
    }

    return destinationRaw(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Effects} Effects
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').State} State
 */

/**
 * @this {TokenizeContext}
 * @param {Effects} effects
 * @param {State} ok
 * @param {State} nok
 * @param {string} type
 * @param {string} markerType
 * @param {string} stringType
 * @returns {State}
 */
// eslint-disable-next-line max-params
function factoryLabel(effects, ok, nok, type, markerType, stringType) {
  const self = this;
  let size = 0;
  /** @type {boolean} */

  let data;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter(type);
    effects.enter(markerType);
    effects.consume(code);
    effects.exit(markerType);
    effects.enter(stringType);
    return atBreak
  }
  /** @type {State} */

  function atBreak(code) {
    if (
      code === null ||
      code === 91 ||
      (code === 93 && !data) ||
      /* To do: remove in the future once we’ve switched from
       * `micromark-extension-footnote` to `micromark-extension-gfm-footnote`,
       * which doesn’t need this */

      /* Hidden footnotes hook */

      /* c8 ignore next 3 */
      (code === 94 &&
        !size &&
        '_hiddenFootnoteSupport' in self.parser.constructs) ||
      size > 999
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
    return label(code)
  }
  /** @type {State} */

  function label(code) {
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
    data = data || !markdownSpace(code);
    return code === 92 ? labelEscape : label
  }
  /** @type {State} */

  function labelEscape(code) {
    if (code === 91 || code === 92 || code === 93) {
      effects.consume(code);
      size++;
      return label
    }

    return label(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Effects} Effects
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

/**
 * @param {Effects} effects
 * @param {State} ok
 * @param {State} nok
 * @param {string} type
 * @param {string} markerType
 * @param {string} stringType
 * @returns {State}
 */
// eslint-disable-next-line max-params
function factoryTitle(effects, ok, nok, type, markerType, stringType) {
  /** @type {NonNullable<Code>} */
  let marker;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter(type);
    effects.enter(markerType);
    effects.consume(code);
    effects.exit(markerType);
    marker = code === 40 ? 41 : code;
    return atFirstTitleBreak
  }
  /** @type {State} */

  function atFirstTitleBreak(code) {
    if (code === marker) {
      effects.enter(markerType);
      effects.consume(code);
      effects.exit(markerType);
      effects.exit(type);
      return ok
    }

    effects.enter(stringType);
    return atTitleBreak(code)
  }
  /** @type {State} */

  function atTitleBreak(code) {
    if (code === marker) {
      effects.exit(stringType);
      return atFirstTitleBreak(marker)
    }

    if (code === null) {
      return nok(code)
    } // Note: blank lines can’t exist in content.

    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return factorySpace(effects, atTitleBreak, 'linePrefix')
    }

    effects.enter('chunkString', {
      contentType: 'string'
    });
    return title(code)
  }
  /** @type {State} */

  function title(code) {
    if (code === marker || code === null || markdownLineEnding(code)) {
      effects.exit('chunkString');
      return atTitleBreak(code)
    }

    effects.consume(code);
    return code === 92 ? titleEscape : title
  }
  /** @type {State} */

  function titleEscape(code) {
    if (code === marker || code === 92) {
      effects.consume(code);
      return title
    }

    return title(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Effects} Effects
 * @typedef {import('micromark-util-types').State} State
 */

/**
 * @param {Effects} effects
 * @param {State} ok
 */
function factoryWhitespace(effects, ok) {
  /** @type {boolean} */
  let seen;
  return start
  /** @type {State} */

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

/**
 * Normalize an identifier (such as used in definitions).
 *
 * @param {string} value
 * @returns {string}
 */
function normalizeIdentifier(value) {
  return (
    value // Collapse Markdown whitespace.
      .replace(/[\t\n\r ]+/g, ' ') // Trim.
      .replace(/^ | $/g, '') // Some characters are considered “uppercase”, but if their lowercase
      // counterpart is uppercased will result in a different uppercase
      // character.
      // Hence, to get that form, we perform both lower- and uppercase.
      // Upper case makes sure keys will not interact with default prototypal
      // methods: no method is uppercase.
      .toLowerCase()
      .toUpperCase()
  )
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const definition$1 = {
  name: 'definition',
  tokenize: tokenizeDefinition
};
/** @type {Construct} */

const titleConstruct = {
  tokenize: tokenizeTitle,
  partial: true
};
/** @type {Tokenizer} */

function tokenizeDefinition(effects, ok, nok) {
  const self = this;
  /** @type {string} */

  let identifier;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('definition');
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
  /** @type {State} */

  function labelAfter(code) {
    identifier = normalizeIdentifier(
      self.sliceSerialize(self.events[self.events.length - 1][1]).slice(1, -1)
    );

    if (code === 58) {
      effects.enter('definitionMarker');
      effects.consume(code);
      effects.exit('definitionMarker'); // Note: blank lines can’t exist in content.

      return factoryWhitespace(
        effects,
        factoryDestination(
          effects,
          effects.attempt(
            titleConstruct,
            factorySpace(effects, after, 'whitespace'),
            factorySpace(effects, after, 'whitespace')
          ),
          nok,
          'definitionDestination',
          'definitionDestinationLiteral',
          'definitionDestinationLiteralMarker',
          'definitionDestinationRaw',
          'definitionDestinationString'
        )
      )
    }

    return nok(code)
  }
  /** @type {State} */

  function after(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('definition');

      if (!self.parser.defined.includes(identifier)) {
        self.parser.defined.push(identifier);
      }

      return ok(code)
    }

    return nok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeTitle(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, before)(code)
      : nok(code)
  }
  /** @type {State} */

  function before(code) {
    if (code === 34 || code === 39 || code === 40) {
      return factoryTitle(
        effects,
        factorySpace(effects, after, 'whitespace'),
        nok,
        'definitionTitle',
        'definitionTitleMarker',
        'definitionTitleString'
      )(code)
    }

    return nok(code)
  }
  /** @type {State} */

  function after(code) {
    return code === null || markdownLineEnding(code) ? ok(code) : nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const hardBreakEscape = {
  name: 'hardBreakEscape',
  tokenize: tokenizeHardBreakEscape
};
/** @type {Tokenizer} */

function tokenizeHardBreakEscape(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('hardBreakEscape');
    effects.enter('escapeMarker');
    effects.consume(code);
    return open
  }
  /** @type {State} */

  function open(code) {
    if (markdownLineEnding(code)) {
      effects.exit('escapeMarker');
      effects.exit('hardBreakEscape');
      return ok(code)
    }

    return nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const headingAtx = {
  name: 'headingAtx',
  tokenize: tokenizeHeadingAtx,
  resolve: resolveHeadingAtx
};
/** @type {Resolver} */

function resolveHeadingAtx(events, context) {
  let contentEnd = events.length - 2;
  let contentStart = 3;
  /** @type {Token} */

  let content;
  /** @type {Token} */

  let text; // Prefix whitespace, part of the opening.

  if (events[contentStart][1].type === 'whitespace') {
    contentStart += 2;
  } // Suffix whitespace, part of the closing.

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
      // @ts-expect-error Constants are fine to assign.
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
/** @type {Tokenizer} */

function tokenizeHeadingAtx(effects, ok, nok) {
  const self = this;
  let size = 0;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('atxHeading');
    effects.enter('atxHeadingSequence');
    return fenceOpenInside(code)
  }
  /** @type {State} */

  function fenceOpenInside(code) {
    if (code === 35 && size++ < 6) {
      effects.consume(code);
      return fenceOpenInside
    }

    if (code === null || markdownLineEndingOrSpace(code)) {
      effects.exit('atxHeadingSequence');
      return self.interrupt ? ok(code) : headingBreak(code)
    }

    return nok(code)
  }
  /** @type {State} */

  function headingBreak(code) {
    if (code === 35) {
      effects.enter('atxHeadingSequence');
      return sequence(code)
    }

    if (code === null || markdownLineEnding(code)) {
      effects.exit('atxHeading');
      return ok(code)
    }

    if (markdownSpace(code)) {
      return factorySpace(effects, headingBreak, 'whitespace')(code)
    }

    effects.enter('atxHeadingText');
    return data(code)
  }
  /** @type {State} */

  function sequence(code) {
    if (code === 35) {
      effects.consume(code);
      return sequence
    }

    effects.exit('atxHeadingSequence');
    return headingBreak(code)
  }
  /** @type {State} */

  function data(code) {
    if (code === null || code === 35 || markdownLineEndingOrSpace(code)) {
      effects.exit('atxHeadingText');
      return headingBreak(code)
    }

    effects.consume(code);
    return data
  }
}

/**
 * List of lowercase HTML tag names which when parsing HTML (flow), result
 * in more relaxed rules (condition 6): because they are known blocks, the
 * HTML-like syntax doesn’t have to be strictly parsed.
 * For tag names not in this list, a more strict algorithm (condition 7) is used
 * to detect whether the HTML-like syntax is seen as HTML (flow) or not.
 *
 * This is copied from:
 * <https://spec.commonmark.org/0.29/#html-blocks>.
 */
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
  'section',
  'source',
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

/**
 * List of lowercase HTML tag names which when parsing HTML (flow), result in
 * HTML that can include lines w/o exiting, until a closing tag also in this
 * list is found (condition 1).
 *
 * This module is copied from:
 * <https://spec.commonmark.org/0.29/#html-blocks>.
 *
 * Note that `textarea` is not available in `CommonMark@0.29` but has been
 * merged to the primary branch and is slated to be released in the next release
 * of CommonMark.
 */
const htmlRawNames = ['pre', 'script', 'style', 'textarea'];

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */
/** @type {Construct} */

const htmlFlow = {
  name: 'htmlFlow',
  tokenize: tokenizeHtmlFlow,
  resolveTo: resolveToHtmlFlow,
  concrete: true
};
/** @type {Construct} */

const nextBlankConstruct = {
  tokenize: tokenizeNextBlank,
  partial: true
};
/** @type {Resolver} */

function resolveToHtmlFlow(events) {
  let index = events.length;

  while (index--) {
    if (events[index][0] === 'enter' && events[index][1].type === 'htmlFlow') {
      break
    }
  }

  if (index > 1 && events[index - 2][1].type === 'linePrefix') {
    // Add the prefix start to the HTML token.
    events[index][1].start = events[index - 2][1].start; // Add the prefix start to the HTML line token.

    events[index + 1][1].start = events[index - 2][1].start; // Remove the line prefix.

    events.splice(index - 2, 2);
  }

  return events
}
/** @type {Tokenizer} */

function tokenizeHtmlFlow(effects, ok, nok) {
  const self = this;
  /** @type {number} */

  let kind;
  /** @type {boolean} */

  let startTag;
  /** @type {string} */

  let buffer;
  /** @type {number} */

  let index;
  /** @type {Code} */

  let marker;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('htmlFlow');
    effects.enter('htmlFlowData');
    effects.consume(code);
    return open
  }
  /** @type {State} */

  function open(code) {
    if (code === 33) {
      effects.consume(code);
      return declarationStart
    }

    if (code === 47) {
      effects.consume(code);
      return tagCloseStart
    }

    if (code === 63) {
      effects.consume(code);
      kind = 3; // While we’re in an instruction instead of a declaration, we’re on a `?`
      // right now, so we do need to search for `>`, similar to declarations.

      return self.interrupt ? ok : continuationDeclarationInside
    }

    if (asciiAlpha(code)) {
      effects.consume(code);
      buffer = String.fromCharCode(code);
      startTag = true;
      return tagName
    }

    return nok(code)
  }
  /** @type {State} */

  function declarationStart(code) {
    if (code === 45) {
      effects.consume(code);
      kind = 2;
      return commentOpenInside
    }

    if (code === 91) {
      effects.consume(code);
      kind = 5;
      buffer = 'CDATA[';
      index = 0;
      return cdataOpenInside
    }

    if (asciiAlpha(code)) {
      effects.consume(code);
      kind = 4;
      return self.interrupt ? ok : continuationDeclarationInside
    }

    return nok(code)
  }
  /** @type {State} */

  function commentOpenInside(code) {
    if (code === 45) {
      effects.consume(code);
      return self.interrupt ? ok : continuationDeclarationInside
    }

    return nok(code)
  }
  /** @type {State} */

  function cdataOpenInside(code) {
    if (code === buffer.charCodeAt(index++)) {
      effects.consume(code);
      return index === buffer.length
        ? self.interrupt
          ? ok
          : continuation
        : cdataOpenInside
    }

    return nok(code)
  }
  /** @type {State} */

  function tagCloseStart(code) {
    if (asciiAlpha(code)) {
      effects.consume(code);
      buffer = String.fromCharCode(code);
      return tagName
    }

    return nok(code)
  }
  /** @type {State} */

  function tagName(code) {
    if (
      code === null ||
      code === 47 ||
      code === 62 ||
      markdownLineEndingOrSpace(code)
    ) {
      if (
        code !== 47 &&
        startTag &&
        htmlRawNames.includes(buffer.toLowerCase())
      ) {
        kind = 1;
        return self.interrupt ? ok(code) : continuation(code)
      }

      if (htmlBlockNames.includes(buffer.toLowerCase())) {
        kind = 6;

        if (code === 47) {
          effects.consume(code);
          return basicSelfClosing
        }

        return self.interrupt ? ok(code) : continuation(code)
      }

      kind = 7; // Do not support complete HTML when interrupting

      return self.interrupt && !self.parser.lazy[self.now().line]
        ? nok(code)
        : startTag
        ? completeAttributeNameBefore(code)
        : completeClosingTagAfter(code)
    }

    if (code === 45 || asciiAlphanumeric(code)) {
      effects.consume(code);
      buffer += String.fromCharCode(code);
      return tagName
    }

    return nok(code)
  }
  /** @type {State} */

  function basicSelfClosing(code) {
    if (code === 62) {
      effects.consume(code);
      return self.interrupt ? ok : continuation
    }

    return nok(code)
  }
  /** @type {State} */

  function completeClosingTagAfter(code) {
    if (markdownSpace(code)) {
      effects.consume(code);
      return completeClosingTagAfter
    }

    return completeEnd(code)
  }
  /** @type {State} */

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
  /** @type {State} */

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
  /** @type {State} */

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
  /** @type {State} */

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
      marker = code;
      return completeAttributeValueQuoted
    }

    if (markdownSpace(code)) {
      effects.consume(code);
      return completeAttributeValueBefore
    }

    marker = null;
    return completeAttributeValueUnquoted(code)
  }
  /** @type {State} */

  function completeAttributeValueQuoted(code) {
    if (code === null || markdownLineEnding(code)) {
      return nok(code)
    }

    if (code === marker) {
      effects.consume(code);
      return completeAttributeValueQuotedAfter
    }

    effects.consume(code);
    return completeAttributeValueQuoted
  }
  /** @type {State} */

  function completeAttributeValueUnquoted(code) {
    if (
      code === null ||
      code === 34 ||
      code === 39 ||
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
  /** @type {State} */

  function completeAttributeValueQuotedAfter(code) {
    if (code === 47 || code === 62 || markdownSpace(code)) {
      return completeAttributeNameBefore(code)
    }

    return nok(code)
  }
  /** @type {State} */

  function completeEnd(code) {
    if (code === 62) {
      effects.consume(code);
      return completeAfter
    }

    return nok(code)
  }
  /** @type {State} */

  function completeAfter(code) {
    if (markdownSpace(code)) {
      effects.consume(code);
      return completeAfter
    }

    return code === null || markdownLineEnding(code)
      ? continuation(code)
      : nok(code)
  }
  /** @type {State} */

  function continuation(code) {
    if (code === 45 && kind === 2) {
      effects.consume(code);
      return continuationCommentInside
    }

    if (code === 60 && kind === 1) {
      effects.consume(code);
      return continuationRawTagOpen
    }

    if (code === 62 && kind === 4) {
      effects.consume(code);
      return continuationClose
    }

    if (code === 63 && kind === 3) {
      effects.consume(code);
      return continuationDeclarationInside
    }

    if (code === 93 && kind === 5) {
      effects.consume(code);
      return continuationCharacterDataInside
    }

    if (markdownLineEnding(code) && (kind === 6 || kind === 7)) {
      return effects.check(
        nextBlankConstruct,
        continuationClose,
        continuationAtLineEnding
      )(code)
    }

    if (code === null || markdownLineEnding(code)) {
      return continuationAtLineEnding(code)
    }

    effects.consume(code);
    return continuation
  }
  /** @type {State} */

  function continuationAtLineEnding(code) {
    effects.exit('htmlFlowData');
    return htmlContinueStart(code)
  }
  /** @type {State} */

  function htmlContinueStart(code) {
    if (code === null) {
      return done(code)
    }

    if (markdownLineEnding(code)) {
      return effects.attempt(
        {
          tokenize: htmlLineEnd,
          partial: true
        },
        htmlContinueStart,
        done
      )(code)
    }

    effects.enter('htmlFlowData');
    return continuation(code)
  }
  /** @type {Tokenizer} */

  function htmlLineEnd(effects, ok, nok) {
    return start
    /** @type {State} */

    function start(code) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return lineStart
    }
    /** @type {State} */

    function lineStart(code) {
      return self.parser.lazy[self.now().line] ? nok(code) : ok(code)
    }
  }
  /** @type {State} */

  function continuationCommentInside(code) {
    if (code === 45) {
      effects.consume(code);
      return continuationDeclarationInside
    }

    return continuation(code)
  }
  /** @type {State} */

  function continuationRawTagOpen(code) {
    if (code === 47) {
      effects.consume(code);
      buffer = '';
      return continuationRawEndTag
    }

    return continuation(code)
  }
  /** @type {State} */

  function continuationRawEndTag(code) {
    if (code === 62 && htmlRawNames.includes(buffer.toLowerCase())) {
      effects.consume(code);
      return continuationClose
    }

    if (asciiAlpha(code) && buffer.length < 8) {
      effects.consume(code);
      buffer += String.fromCharCode(code);
      return continuationRawEndTag
    }

    return continuation(code)
  }
  /** @type {State} */

  function continuationCharacterDataInside(code) {
    if (code === 93) {
      effects.consume(code);
      return continuationDeclarationInside
    }

    return continuation(code)
  }
  /** @type {State} */

  function continuationDeclarationInside(code) {
    if (code === 62) {
      effects.consume(code);
      return continuationClose
    }

    return continuation(code)
  }
  /** @type {State} */

  function continuationClose(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('htmlFlowData');
      return done(code)
    }

    effects.consume(code);
    return continuationClose
  }
  /** @type {State} */

  function done(code) {
    effects.exit('htmlFlow');
    return ok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeNextBlank(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.exit('htmlFlowData');
    effects.enter('lineEndingBlank');
    effects.consume(code);
    effects.exit('lineEndingBlank');
    return effects.attempt(blankLine, ok, nok)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

/** @type {Construct} */
const htmlText = {
  name: 'htmlText',
  tokenize: tokenizeHtmlText
};
/** @type {Tokenizer} */

function tokenizeHtmlText(effects, ok, nok) {
  const self = this;
  /** @type {NonNullable<Code>|undefined} */

  let marker;
  /** @type {string} */

  let buffer;
  /** @type {number} */

  let index;
  /** @type {State} */

  let returnState;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('htmlText');
    effects.enter('htmlTextData');
    effects.consume(code);
    return open
  }
  /** @type {State} */

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
  /** @type {State} */

  function declarationOpen(code) {
    if (code === 45) {
      effects.consume(code);
      return commentOpen
    }

    if (code === 91) {
      effects.consume(code);
      buffer = 'CDATA[';
      index = 0;
      return cdataOpen
    }

    if (asciiAlpha(code)) {
      effects.consume(code);
      return declaration
    }

    return nok(code)
  }
  /** @type {State} */

  function commentOpen(code) {
    if (code === 45) {
      effects.consume(code);
      return commentStart
    }

    return nok(code)
  }
  /** @type {State} */

  function commentStart(code) {
    if (code === null || code === 62) {
      return nok(code)
    }

    if (code === 45) {
      effects.consume(code);
      return commentStartDash
    }

    return comment(code)
  }
  /** @type {State} */

  function commentStartDash(code) {
    if (code === null || code === 62) {
      return nok(code)
    }

    return comment(code)
  }
  /** @type {State} */

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
      return atLineEnding(code)
    }

    effects.consume(code);
    return comment
  }
  /** @type {State} */

  function commentClose(code) {
    if (code === 45) {
      effects.consume(code);
      return end
    }

    return comment(code)
  }
  /** @type {State} */

  function cdataOpen(code) {
    if (code === buffer.charCodeAt(index++)) {
      effects.consume(code);
      return index === buffer.length ? cdata : cdataOpen
    }

    return nok(code)
  }
  /** @type {State} */

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
      return atLineEnding(code)
    }

    effects.consume(code);
    return cdata
  }
  /** @type {State} */

  function cdataClose(code) {
    if (code === 93) {
      effects.consume(code);
      return cdataEnd
    }

    return cdata(code)
  }
  /** @type {State} */

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
  /** @type {State} */

  function declaration(code) {
    if (code === null || code === 62) {
      return end(code)
    }

    if (markdownLineEnding(code)) {
      returnState = declaration;
      return atLineEnding(code)
    }

    effects.consume(code);
    return declaration
  }
  /** @type {State} */

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
      return atLineEnding(code)
    }

    effects.consume(code);
    return instruction
  }
  /** @type {State} */

  function instructionClose(code) {
    return code === 62 ? end(code) : instruction(code)
  }
  /** @type {State} */

  function tagCloseStart(code) {
    if (asciiAlpha(code)) {
      effects.consume(code);
      return tagClose
    }

    return nok(code)
  }
  /** @type {State} */

  function tagClose(code) {
    if (code === 45 || asciiAlphanumeric(code)) {
      effects.consume(code);
      return tagClose
    }

    return tagCloseBetween(code)
  }
  /** @type {State} */

  function tagCloseBetween(code) {
    if (markdownLineEnding(code)) {
      returnState = tagCloseBetween;
      return atLineEnding(code)
    }

    if (markdownSpace(code)) {
      effects.consume(code);
      return tagCloseBetween
    }

    return end(code)
  }
  /** @type {State} */

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
  /** @type {State} */

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
      return atLineEnding(code)
    }

    if (markdownSpace(code)) {
      effects.consume(code);
      return tagOpenBetween
    }

    return end(code)
  }
  /** @type {State} */

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
  /** @type {State} */

  function tagOpenAttributeNameAfter(code) {
    if (code === 61) {
      effects.consume(code);
      return tagOpenAttributeValueBefore
    }

    if (markdownLineEnding(code)) {
      returnState = tagOpenAttributeNameAfter;
      return atLineEnding(code)
    }

    if (markdownSpace(code)) {
      effects.consume(code);
      return tagOpenAttributeNameAfter
    }

    return tagOpenBetween(code)
  }
  /** @type {State} */

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
      return atLineEnding(code)
    }

    if (markdownSpace(code)) {
      effects.consume(code);
      return tagOpenAttributeValueBefore
    }

    effects.consume(code);
    marker = undefined;
    return tagOpenAttributeValueUnquoted
  }
  /** @type {State} */

  function tagOpenAttributeValueQuoted(code) {
    if (code === marker) {
      effects.consume(code);
      return tagOpenAttributeValueQuotedAfter
    }

    if (code === null) {
      return nok(code)
    }

    if (markdownLineEnding(code)) {
      returnState = tagOpenAttributeValueQuoted;
      return atLineEnding(code)
    }

    effects.consume(code);
    return tagOpenAttributeValueQuoted
  }
  /** @type {State} */

  function tagOpenAttributeValueQuotedAfter(code) {
    if (code === 62 || code === 47 || markdownLineEndingOrSpace(code)) {
      return tagOpenBetween(code)
    }

    return nok(code)
  }
  /** @type {State} */

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

    if (code === 62 || markdownLineEndingOrSpace(code)) {
      return tagOpenBetween(code)
    }

    effects.consume(code);
    return tagOpenAttributeValueUnquoted
  } // We can’t have blank lines in content, so no need to worry about empty
  // tokens.

  /** @type {State} */

  function atLineEnding(code) {
    effects.exit('htmlTextData');
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return factorySpace(
      effects,
      afterPrefix,
      'linePrefix',
      self.parser.constructs.disable.null.includes('codeIndented')
        ? undefined
        : 4
    )
  }
  /** @type {State} */

  function afterPrefix(code) {
    effects.enter('htmlTextData');
    return returnState(code)
  }
  /** @type {State} */

  function end(code) {
    if (code === 62) {
      effects.consume(code);
      effects.exit('htmlTextData');
      effects.exit('htmlText');
      return ok
    }

    return nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

/** @type {Construct} */
const labelEnd = {
  name: 'labelEnd',
  tokenize: tokenizeLabelEnd,
  resolveTo: resolveToLabelEnd,
  resolveAll: resolveAllLabelEnd
};
/** @type {Construct} */

const resourceConstruct = {
  tokenize: tokenizeResource
};
/** @type {Construct} */

const fullReferenceConstruct = {
  tokenize: tokenizeFullReference
};
/** @type {Construct} */

const collapsedReferenceConstruct = {
  tokenize: tokenizeCollapsedReference
};
/** @type {Resolver} */

function resolveAllLabelEnd(events) {
  let index = -1;
  /** @type {Token} */

  let token;

  while (++index < events.length) {
    token = events[index][1];

    if (
      token.type === 'labelImage' ||
      token.type === 'labelLink' ||
      token.type === 'labelEnd'
    ) {
      // Remove the marker.
      events.splice(index + 1, token.type === 'labelImage' ? 4 : 2);
      token.type = 'data';
      index++;
    }
  }

  return events
}
/** @type {Resolver} */

function resolveToLabelEnd(events, context) {
  let index = events.length;
  let offset = 0;
  /** @type {Token} */

  let token;
  /** @type {number|undefined} */

  let open;
  /** @type {number|undefined} */

  let close;
  /** @type {Event[]} */

  let media; // Find an opening.

  while (index--) {
    token = events[index][1];

    if (open) {
      // If we see another link, or inactive link label, we’ve been here before.
      if (
        token.type === 'link' ||
        (token.type === 'labelLink' && token._inactive)
      ) {
        break
      } // Mark other link openings as inactive, as we can’t have links in
      // links.

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
  ]; // Opening marker.

  media = push(media, events.slice(open + 1, open + offset + 3)); // Text open.

  media = push(media, [['enter', text, context]]); // Between.

  media = push(
    media,
    resolveAll(
      context.parser.constructs.insideSpan.null,
      events.slice(open + offset + 4, close - 3),
      context
    )
  ); // Text close, marker close, label close.

  media = push(media, [
    ['exit', text, context],
    events[close - 2],
    events[close - 1],
    ['exit', label, context]
  ]); // Reference, resource, or so.

  media = push(media, events.slice(close + 1)); // Media close.

  media = push(media, [['exit', group, context]]);
  splice(events, open, events.length, media);
  return events
}
/** @type {Tokenizer} */

function tokenizeLabelEnd(effects, ok, nok) {
  const self = this;
  let index = self.events.length;
  /** @type {Token} */

  let labelStart;
  /** @type {boolean} */

  let defined; // Find an opening.

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
  /** @type {State} */

  function start(code) {
    if (!labelStart) {
      return nok(code)
    } // It’s a balanced bracket, but contains a link.

    if (labelStart._inactive) return balanced(code)
    defined = self.parser.defined.includes(
      normalizeIdentifier(
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
    return afterLabelEnd
  }
  /** @type {State} */

  function afterLabelEnd(code) {
    // Resource: `[asd](fgh)`.
    if (code === 40) {
      return effects.attempt(
        resourceConstruct,
        ok,
        defined ? ok : balanced
      )(code)
    } // Collapsed (`[asd][]`) or full (`[asd][fgh]`) reference?

    if (code === 91) {
      return effects.attempt(
        fullReferenceConstruct,
        ok,
        defined
          ? effects.attempt(collapsedReferenceConstruct, ok, balanced)
          : balanced
      )(code)
    } // Shortcut reference: `[asd]`?

    return defined ? ok(code) : balanced(code)
  }
  /** @type {State} */

  function balanced(code) {
    labelStart._balanced = true;
    return nok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeResource(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('resource');
    effects.enter('resourceMarker');
    effects.consume(code);
    effects.exit('resourceMarker');
    return factoryWhitespace(effects, open)
  }
  /** @type {State} */

  function open(code) {
    if (code === 41) {
      return end(code)
    }

    return factoryDestination(
      effects,
      destinationAfter,
      nok,
      'resourceDestination',
      'resourceDestinationLiteral',
      'resourceDestinationLiteralMarker',
      'resourceDestinationRaw',
      'resourceDestinationString',
      3
    )(code)
  }
  /** @type {State} */

  function destinationAfter(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, between)(code)
      : end(code)
  }
  /** @type {State} */

  function between(code) {
    if (code === 34 || code === 39 || code === 40) {
      return factoryTitle(
        effects,
        factoryWhitespace(effects, end),
        nok,
        'resourceTitle',
        'resourceTitleMarker',
        'resourceTitleString'
      )(code)
    }

    return end(code)
  }
  /** @type {State} */

  function end(code) {
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
/** @type {Tokenizer} */

function tokenizeFullReference(effects, ok, nok) {
  const self = this;
  return start
  /** @type {State} */

  function start(code) {
    return factoryLabel.call(
      self,
      effects,
      afterLabel,
      nok,
      'reference',
      'referenceMarker',
      'referenceString'
    )(code)
  }
  /** @type {State} */

  function afterLabel(code) {
    return self.parser.defined.includes(
      normalizeIdentifier(
        self.sliceSerialize(self.events[self.events.length - 1][1]).slice(1, -1)
      )
    )
      ? ok(code)
      : nok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeCollapsedReference(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('reference');
    effects.enter('referenceMarker');
    effects.consume(code);
    effects.exit('referenceMarker');
    return open
  }
  /** @type {State} */

  function open(code) {
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

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */
/** @type {Construct} */

const labelStartImage = {
  name: 'labelStartImage',
  tokenize: tokenizeLabelStartImage,
  resolveAll: labelEnd.resolveAll
};
/** @type {Tokenizer} */

function tokenizeLabelStartImage(effects, ok, nok) {
  const self = this;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('labelImage');
    effects.enter('labelImageMarker');
    effects.consume(code);
    effects.exit('labelImageMarker');
    return open
  }
  /** @type {State} */

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
  /** @type {State} */

  function after(code) {
    /* To do: remove in the future once we’ve switched from
     * `micromark-extension-footnote` to `micromark-extension-gfm-footnote`,
     * which doesn’t need this */

    /* Hidden footnotes hook */

    /* c8 ignore next 3 */
    return code === 94 && '_hiddenFootnoteSupport' in self.parser.constructs
      ? nok(code)
      : ok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */
/** @type {Construct} */

const labelStartLink = {
  name: 'labelStartLink',
  tokenize: tokenizeLabelStartLink,
  resolveAll: labelEnd.resolveAll
};
/** @type {Tokenizer} */

function tokenizeLabelStartLink(effects, ok, nok) {
  const self = this;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('labelLink');
    effects.enter('labelMarker');
    effects.consume(code);
    effects.exit('labelMarker');
    effects.exit('labelLink');
    return after
  }
  /** @type {State} */

  function after(code) {
    /* To do: remove in the future once we’ve switched from
     * `micromark-extension-footnote` to `micromark-extension-gfm-footnote`,
     * which doesn’t need this */

    /* Hidden footnotes hook. */

    /* c8 ignore next 3 */
    return code === 94 && '_hiddenFootnoteSupport' in self.parser.constructs
      ? nok(code)
      : ok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {Construct} */
const lineEnding = {
  name: 'lineEnding',
  tokenize: tokenizeLineEnding
};
/** @type {Tokenizer} */

function tokenizeLineEnding(effects, ok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('lineEnding');
    effects.consume(code);
    effects.exit('lineEnding');
    return factorySpace(effects, ok, 'linePrefix')
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

/** @type {Construct} */
const thematicBreak$1 = {
  name: 'thematicBreak',
  tokenize: tokenizeThematicBreak
};
/** @type {Tokenizer} */

function tokenizeThematicBreak(effects, ok, nok) {
  let size = 0;
  /** @type {NonNullable<Code>} */

  let marker;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('thematicBreak');
    marker = code;
    return atBreak(code)
  }
  /** @type {State} */

  function atBreak(code) {
    if (code === marker) {
      effects.enter('thematicBreakSequence');
      return sequence(code)
    }

    if (markdownSpace(code)) {
      return factorySpace(effects, atBreak, 'whitespace')(code)
    }

    if (size < 3 || (code !== null && !markdownLineEnding(code))) {
      return nok(code)
    }

    effects.exit('thematicBreak');
    return ok(code)
  }
  /** @type {State} */

  function sequence(code) {
    if (code === marker) {
      effects.consume(code);
      size++;
      return sequence
    }

    effects.exit('thematicBreakSequence');
    return atBreak(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').Exiter} Exiter
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */
/** @type {Construct} */

const list$1 = {
  name: 'list',
  tokenize: tokenizeListStart,
  continuation: {
    tokenize: tokenizeListContinuation
  },
  exit: tokenizeListEnd
};
/** @type {Construct} */

const listItemPrefixWhitespaceConstruct = {
  tokenize: tokenizeListItemPrefixWhitespace,
  partial: true
};
/** @type {Construct} */

const indentConstruct = {
  tokenize: tokenizeIndent$1,
  partial: true
};
/**
 * @type {Tokenizer}
 * @this {TokenizeContextWithState}
 */

function tokenizeListStart(effects, ok, nok) {
  const self = this;
  const tail = self.events[self.events.length - 1];
  let initialSize =
    tail && tail[1].type === 'linePrefix'
      ? tail[2].sliceSerialize(tail[1], true).length
      : 0;
  let size = 0;
  return start
  /** @type {State} */

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
  /** @type {State} */

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
  /**
   * @type {State}
   **/

  function atMarker(code) {
    effects.enter('listItemMarker');
    effects.consume(code);
    effects.exit('listItemMarker');
    self.containerState.marker = self.containerState.marker || code;
    return effects.check(
      blankLine, // Can’t be empty when interrupting.
      self.interrupt ? nok : onBlank,
      effects.attempt(
        listItemPrefixWhitespaceConstruct,
        endOfPrefix,
        otherPrefix
      )
    )
  }
  /** @type {State} */

  function onBlank(code) {
    self.containerState.initialBlankLine = true;
    initialSize++;
    return endOfPrefix(code)
  }
  /** @type {State} */

  function otherPrefix(code) {
    if (markdownSpace(code)) {
      effects.enter('listItemPrefixWhitespace');
      effects.consume(code);
      effects.exit('listItemPrefixWhitespace');
      return endOfPrefix
    }

    return nok(code)
  }
  /** @type {State} */

  function endOfPrefix(code) {
    self.containerState.size =
      initialSize +
      self.sliceSerialize(effects.exit('listItemPrefix'), true).length;
    return ok(code)
  }
}
/**
 * @type {Tokenizer}
 * @this {TokenizeContextWithState}
 */

function tokenizeListContinuation(effects, ok, nok) {
  const self = this;
  self.containerState._closeFlow = undefined;
  return effects.check(blankLine, onBlank, notBlank)
  /** @type {State} */

  function onBlank(code) {
    self.containerState.furtherBlankLines =
      self.containerState.furtherBlankLines ||
      self.containerState.initialBlankLine; // We have a blank line.
    // Still, try to consume at most the items size.

    return factorySpace(
      effects,
      ok,
      'listItemIndent',
      self.containerState.size + 1
    )(code)
  }
  /** @type {State} */

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
  /** @type {State} */

  function notInCurrentItem(code) {
    // While we do continue, we signal that the flow should be closed.
    self.containerState._closeFlow = true; // As we’re closing flow, we’re no longer interrupting.

    self.interrupt = undefined;
    return factorySpace(
      effects,
      effects.attempt(list$1, ok, nok),
      'linePrefix',
      self.parser.constructs.disable.null.includes('codeIndented')
        ? undefined
        : 4
    )(code)
  }
}
/**
 * @type {Tokenizer}
 * @this {TokenizeContextWithState}
 */

function tokenizeIndent$1(effects, ok, nok) {
  const self = this;
  return factorySpace(
    effects,
    afterPrefix,
    'listItemIndent',
    self.containerState.size + 1
  )
  /** @type {State} */

  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'listItemIndent' &&
      tail[2].sliceSerialize(tail[1], true).length === self.containerState.size
      ? ok(code)
      : nok(code)
  }
}
/**
 * @type {Exiter}
 * @this {TokenizeContextWithState}
 */

function tokenizeListEnd(effects) {
  effects.exit(this.containerState.type);
}
/**
 * @type {Tokenizer}
 * @this {TokenizeContextWithState}
 */

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
  /** @type {State} */

  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return !markdownSpace(code) &&
      tail &&
      tail[1].type === 'listItemPrefixWhitespace'
      ? ok(code)
      : nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

/** @type {Construct} */
const setextUnderline = {
  name: 'setextUnderline',
  tokenize: tokenizeSetextUnderline,
  resolveTo: resolveToSetextUnderline
};
/** @type {Resolver} */

function resolveToSetextUnderline(events, context) {
  let index = events.length;
  /** @type {number|undefined} */

  let content;
  /** @type {number|undefined} */

  let text;
  /** @type {number|undefined} */

  let definition; // Find the opening of the content.
  // It’ll always exist: we don’t tokenize if it isn’t there.

  while (index--) {
    if (events[index][0] === 'enter') {
      if (events[index][1].type === 'content') {
        content = index;
        break
      }

      if (events[index][1].type === 'paragraph') {
        text = index;
      }
    } // Exit
    else {
      if (events[index][1].type === 'content') {
        // Remove the content end (if needed we’ll add it later)
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
  }; // Change the paragraph to setext heading text.

  events[text][1].type = 'setextHeadingText'; // If we have definitions in the content, we’ll keep on having content,
  // but we need move it.

  if (definition) {
    events.splice(text, 0, ['enter', heading, context]);
    events.splice(definition + 1, 0, ['exit', events[content][1], context]);
    events[content][1].end = Object.assign({}, events[definition][1].end);
  } else {
    events[content][1] = heading;
  } // Add the heading exit at the end.

  events.push(['exit', heading, context]);
  return events
}
/** @type {Tokenizer} */

function tokenizeSetextUnderline(effects, ok, nok) {
  const self = this;
  let index = self.events.length;
  /** @type {NonNullable<Code>} */

  let marker;
  /** @type {boolean} */

  let paragraph; // Find an opening.

  while (index--) {
    // Skip enter/exit of line ending, line prefix, and content.
    // We can now either have a definition or a paragraph.
    if (
      self.events[index][1].type !== 'lineEnding' &&
      self.events[index][1].type !== 'linePrefix' &&
      self.events[index][1].type !== 'content'
    ) {
      paragraph = self.events[index][1].type === 'paragraph';
      break
    }
  }

  return start
  /** @type {State} */

  function start(code) {
    if (!self.parser.lazy[self.now().line] && (self.interrupt || paragraph)) {
      effects.enter('setextHeadingLine');
      effects.enter('setextHeadingLineSequence');
      marker = code;
      return closingSequence(code)
    }

    return nok(code)
  }
  /** @type {State} */

  function closingSequence(code) {
    if (code === marker) {
      effects.consume(code);
      return closingSequence
    }

    effects.exit('setextHeadingLineSequence');
    return factorySpace(effects, closingSequenceEnd, 'lineSuffix')(code)
  }
  /** @type {State} */

  function closingSequenceEnd(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('setextHeadingLine');
      return ok(code)
    }

    return nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').InitialConstruct} InitialConstruct
 * @typedef {import('micromark-util-types').Initializer} Initializer
 * @typedef {import('micromark-util-types').State} State
 */

/** @type {InitialConstruct} */
const flow$1 = {
  tokenize: initializeFlow
};
/** @type {Initializer} */

function initializeFlow(effects) {
  const self = this;
  const initial = effects.attempt(
    // Try to parse a blank line.
    blankLine,
    atBlankEnding, // Try to parse initial flow (essentially, only code).
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
  /** @type {State} */

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
  /** @type {State} */

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

/**
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Initializer} Initializer
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').InitialConstruct} InitialConstruct
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */
const resolver = {
  resolveAll: createResolver()
};
const string$1 = initializeFactory('string');
const text$3 = initializeFactory('text');
/**
 * @param {'string'|'text'} field
 * @returns {InitialConstruct}
 */

function initializeFactory(field) {
  return {
    tokenize: initializeText,
    resolveAll: createResolver(
      field === 'text' ? resolveAllLineSuffixes : undefined
    )
  }
  /** @type {Initializer} */

  function initializeText(effects) {
    const self = this;
    const constructs = this.parser.constructs[field];
    const text = effects.attempt(constructs, start, notText);
    return start
    /** @type {State} */

    function start(code) {
      return atBreak(code) ? text(code) : notText(code)
    }
    /** @type {State} */

    function notText(code) {
      if (code === null) {
        effects.consume(code);
        return
      }

      effects.enter('data');
      effects.consume(code);
      return data
    }
    /** @type {State} */

    function data(code) {
      if (atBreak(code)) {
        effects.exit('data');
        return text(code)
      } // Data.

      effects.consume(code);
      return data
    }
    /**
     * @param {Code} code
     * @returns {boolean}
     */

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
/**
 * @param {Resolver} [extraResolver]
 * @returns {Resolver}
 */

function createResolver(extraResolver) {
  return resolveAllText
  /** @type {Resolver} */

  function resolveAllText(events, context) {
    let index = -1;
    /** @type {number|undefined} */

    let enter; // A rather boring computation (to merge adjacent `data` events) which
    // improves mm performance by 29%.

    while (++index <= events.length) {
      if (enter === undefined) {
        if (events[index] && events[index][1].type === 'data') {
          enter = index;
          index++;
        }
      } else if (!events[index] || events[index][1].type !== 'data') {
        // Don’t do anything if there is one data token.
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
/**
 * A rather ugly set of instructions which again looks at chunks in the input
 * stream.
 * The reason to do this here is that it is *much* faster to parse in reverse.
 * And that we can’t hook into `null` to split the line suffix before an EOF.
 * To do: figure out if we can make this into a clean utility, or even in core.
 * As it will be useful for GFMs literal autolink extension (and maybe even
 * tables?)
 *
 * @type {Resolver}
 */

function resolveAllLineSuffixes(events, context) {
  let eventIndex = -1;

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
      /** @type {boolean|undefined} */

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
        } // Number
        else if (chunk === -2) {
          tabs = true;
          size++;
        } else if (chunk === -1) ; else {
          // Replacement character, exit.
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

/**
 * @typedef {import('micromark-util-types').Code} Code
 * @typedef {import('micromark-util-types').Chunk} Chunk
 * @typedef {import('micromark-util-types').Point} Point
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').Effects} Effects
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').InitialConstruct} InitialConstruct
 * @typedef {import('micromark-util-types').ConstructRecord} ConstructRecord
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').ParseContext} ParseContext
 */

/**
 * Create a tokenizer.
 * Tokenizers deal with one type of data (e.g., containers, flow, text).
 * The parser is the object dealing with it all.
 * `initialize` works like other constructs, except that only its `tokenize`
 * function is used, in which case it doesn’t receive an `ok` or `nok`.
 * `from` can be given to set the point before the first character, although
 * when further lines are indented, they must be set with `defineSkip`.
 *
 * @param {ParseContext} parser
 * @param {InitialConstruct} initialize
 * @param {Omit<Point, '_index'|'_bufferIndex'>} [from]
 * @returns {TokenizeContext}
 */
function createTokenizer(parser, initialize, from) {
  /** @type {Point} */
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
  /** @type {Record<string, number>} */

  const columnStart = {};
  /** @type {Construct[]} */

  const resolveAllConstructs = [];
  /** @type {Chunk[]} */

  let chunks = [];
  /** @type {Token[]} */

  let stack = [];
  /**
   * Tools used for tokenizing.
   *
   * @type {Effects}
   */

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
  /**
   * State and tools for resolving and serializing.
   *
   * @type {TokenizeContext}
   */

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
  /**
   * The state function.
   *
   * @type {State|void}
   */

  let state = initialize.tokenize.call(context, effects);

  if (initialize.resolveAll) {
    resolveAllConstructs.push(initialize);
  }

  return context
  /** @type {TokenizeContext['write']} */

  function write(slice) {
    chunks = push(chunks, slice);
    main(); // Exit if we’re not done, resolve might change stuff.

    if (chunks[chunks.length - 1] !== null) {
      return []
    }

    addResult(initialize, 0); // Otherwise, resolve, and exit.

    context.events = resolveAll(resolveAllConstructs, context.events, context);
    return context.events
  } //
  // Tools.
  //

  /** @type {TokenizeContext['sliceSerialize']} */

  function sliceSerialize(token, expandTabs) {
    return serializeChunks(sliceStream(token), expandTabs)
  }
  /** @type {TokenizeContext['sliceStream']} */

  function sliceStream(token) {
    return sliceChunks(chunks, token)
  }
  /** @type {TokenizeContext['now']} */

  function now() {
    return Object.assign({}, point)
  }
  /** @type {TokenizeContext['defineSkip']} */

  function defineSkip(value) {
    columnStart[value.line] = value.column;
    accountForPotentialSkip();
  } //
  // State management.
  //

  /**
   * Main loop (note that `_index` and `_bufferIndex` in `point` are modified by
   * `consume`).
   * Here is where we walk through the chunks, which either include strings of
   * several characters, or numerical character codes.
   * The reason to do this in a loop instead of a call is so the stack can
   * drain.
   *
   * @returns {void}
   */

  function main() {
    /** @type {number} */
    let chunkIndex;

    while (point._index < chunks.length) {
      const chunk = chunks[point._index]; // If we’re in a buffer chunk, loop through it.

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
  /**
   * Deal with one code.
   *
   * @param {Code} code
   * @returns {void}
   */

  function go(code) {
    state = state(code);
  }
  /** @type {Effects['consume']} */

  function consume(code) {
    if (markdownLineEnding(code)) {
      point.line++;
      point.column = 1;
      point.offset += code === -3 ? 2 : 1;
      accountForPotentialSkip();
    } else if (code !== -1) {
      point.column++;
      point.offset++;
    } // Not in a string chunk.

    if (point._bufferIndex < 0) {
      point._index++;
    } else {
      point._bufferIndex++; // At end of string chunk.
      // @ts-expect-error Points w/ non-negative `_bufferIndex` reference
      // strings.

      if (point._bufferIndex === chunks[point._index].length) {
        point._bufferIndex = -1;
        point._index++;
      }
    } // Expose the previous character.

    context.previous = code; // Mark as consumed.
  }
  /** @type {Effects['enter']} */

  function enter(type, fields) {
    /** @type {Token} */
    // @ts-expect-error Patch instead of assign required fields to help GC.
    const token = fields || {};
    token.type = type;
    token.start = now();
    context.events.push(['enter', token, context]);
    stack.push(token);
    return token
  }
  /** @type {Effects['exit']} */

  function exit(type) {
    const token = stack.pop();
    token.end = now();
    context.events.push(['exit', token, context]);
    return token
  }
  /**
   * Use results.
   *
   * @type {ReturnHandle}
   */

  function onsuccessfulconstruct(construct, info) {
    addResult(construct, info.from);
  }
  /**
   * Discard results.
   *
   * @type {ReturnHandle}
   */

  function onsuccessfulcheck(_, info) {
    info.restore();
  }
  /**
   * Factory to attempt/check/interrupt.
   *
   * @param {ReturnHandle} onreturn
   * @param {Record<string, unknown>} [fields]
   */

  function constructFactory(onreturn, fields) {
    return hook
    /**
     * Handle either an object mapping codes to constructs, a list of
     * constructs, or a single construct.
     *
     * @param {Construct|Construct[]|ConstructRecord} constructs
     * @param {State} returnState
     * @param {State} [bogusState]
     * @returns {State}
     */

    function hook(constructs, returnState, bogusState) {
      /** @type {Construct[]} */
      let listOfConstructs;
      /** @type {number} */

      let constructIndex;
      /** @type {Construct} */

      let currentConstruct;
      /** @type {Info} */

      let info;
      return Array.isArray(constructs)
        ? /* c8 ignore next 1 */
          handleListOfConstructs(constructs)
        : 'tokenize' in constructs // @ts-expect-error Looks like a construct.
        ? handleListOfConstructs([constructs])
        : handleMapOfConstructs(constructs)
      /**
       * Handle a list of construct.
       *
       * @param {ConstructRecord} map
       * @returns {State}
       */

      function handleMapOfConstructs(map) {
        return start
        /** @type {State} */

        function start(code) {
          const def = code !== null && map[code];
          const all = code !== null && map.null;
          const list = [
            // To do: add more extension tests.

            /* c8 ignore next 2 */
            ...(Array.isArray(def) ? def : def ? [def] : []),
            ...(Array.isArray(all) ? all : all ? [all] : [])
          ];
          return handleListOfConstructs(list)(code)
        }
      }
      /**
       * Handle a list of construct.
       *
       * @param {Construct[]} list
       * @returns {State}
       */

      function handleListOfConstructs(list) {
        listOfConstructs = list;
        constructIndex = 0;

        if (list.length === 0) {
          return bogusState
        }

        return handleConstruct(list[constructIndex])
      }
      /**
       * Handle a single construct.
       *
       * @param {Construct} construct
       * @returns {State}
       */

      function handleConstruct(construct) {
        return start
        /** @type {State} */

        function start(code) {
          // To do: not needed to store if there is no bogus state, probably?
          // Currently doesn’t work because `inspect` in document does a check
          // w/o a bogus, which doesn’t make sense. But it does seem to help perf
          // by not storing.
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
            // If we do have fields, create an object w/ `context` as its
            // prototype.
            // This allows a “live binding”, which is needed for `interrupt`.
            fields ? Object.assign(Object.create(context), fields) : context,
            effects,
            ok,
            nok
          )(code)
        }
      }
      /** @type {State} */

      function ok(code) {
        onreturn(currentConstruct, info);
        return returnState
      }
      /** @type {State} */

      function nok(code) {
        info.restore();

        if (++constructIndex < listOfConstructs.length) {
          return handleConstruct(listOfConstructs[constructIndex])
        }

        return bogusState
      }
    }
  }
  /**
   * @param {Construct} construct
   * @param {number} from
   * @returns {void}
   */

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
  /**
   * Store state.
   *
   * @returns {Info}
   */

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
    /**
     * Restore state.
     *
     * @returns {void}
     */

    function restore() {
      point = startPoint;
      context.previous = startPrevious;
      context.currentConstruct = startCurrentConstruct;
      context.events.length = startEventsIndex;
      stack = startStack;
      accountForPotentialSkip();
    }
  }
  /**
   * Move the current point a bit forward in the line when it’s on a column
   * skip.
   *
   * @returns {void}
   */

  function accountForPotentialSkip() {
    if (point.line in columnStart && point.column < 2) {
      point.column = columnStart[point.line];
      point.offset += columnStart[point.line] - 1;
    }
  }
}
/**
 * Get the chunks from a slice of chunks in the range of a token.
 *
 * @param {Chunk[]} chunks
 * @param {Pick<Token, 'start'|'end'>} token
 * @returns {Chunk[]}
 */

function sliceChunks(chunks, token) {
  const startIndex = token.start._index;
  const startBufferIndex = token.start._bufferIndex;
  const endIndex = token.end._index;
  const endBufferIndex = token.end._bufferIndex;
  /** @type {Chunk[]} */

  let view;

  if (startIndex === endIndex) {
    // @ts-expect-error `_bufferIndex` is used on string chunks.
    view = [chunks[startIndex].slice(startBufferIndex, endBufferIndex)];
  } else {
    view = chunks.slice(startIndex, endIndex);

    if (startBufferIndex > -1) {
      // @ts-expect-error `_bufferIndex` is used on string chunks.
      view[0] = view[0].slice(startBufferIndex);
    }

    if (endBufferIndex > 0) {
      // @ts-expect-error `_bufferIndex` is used on string chunks.
      view.push(chunks[endIndex].slice(0, endBufferIndex));
    }
  }

  return view
}
/**
 * Get the string value of a slice of chunks.
 *
 * @param {Chunk[]} chunks
 * @param {boolean} [expandTabs=false]
 * @returns {string}
 */

function serializeChunks(chunks, expandTabs) {
  let index = -1;
  /** @type {string[]} */

  const result = [];
  /** @type {boolean|undefined} */

  let atTab;

  while (++index < chunks.length) {
    const chunk = chunks[index];
    /** @type {string} */

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
          // Currently only replacement character.
          value = String.fromCharCode(chunk);
        }
      }

    atTab = chunk === -2;
    result.push(value);
  }

  return result.join('')
}

/**
 * @typedef {import('micromark-util-types').Extension} Extension
 */
/** @type {Extension['document']} */

const document = {
  [42]: list$1,
  [43]: list$1,
  [45]: list$1,
  [48]: list$1,
  [49]: list$1,
  [50]: list$1,
  [51]: list$1,
  [52]: list$1,
  [53]: list$1,
  [54]: list$1,
  [55]: list$1,
  [56]: list$1,
  [57]: list$1,
  [62]: blockQuote
};
/** @type {Extension['contentInitial']} */

const contentInitial = {
  [91]: definition$1
};
/** @type {Extension['flowInitial']} */

const flowInitial = {
  [-2]: codeIndented,
  [-1]: codeIndented,
  [32]: codeIndented
};
/** @type {Extension['flow']} */

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
/** @type {Extension['string']} */

const string = {
  [38]: characterReference,
  [92]: characterEscape
};
/** @type {Extension['text']} */

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
/** @type {Extension['insideSpan']} */

const insideSpan = {
  null: [attention, resolver]
};
/** @type {Extension['attentionMarkers']} */

const attentionMarkers = {
  null: [42, 95]
};
/** @type {Extension['disable']} */

const disable = {
  null: []
};

var defaultConstructs = /*#__PURE__*/Object.freeze({
  __proto__: null,
  document: document,
  contentInitial: contentInitial,
  flowInitial: flowInitial,
  flow: flow,
  string: string,
  text: text$2,
  insideSpan: insideSpan,
  attentionMarkers: attentionMarkers,
  disable: disable
});

/**
 * @typedef {import('micromark-util-types').InitialConstruct} InitialConstruct
 * @typedef {import('micromark-util-types').FullNormalizedExtension} FullNormalizedExtension
 * @typedef {import('micromark-util-types').ParseOptions} ParseOptions
 * @typedef {import('micromark-util-types').ParseContext} ParseContext
 * @typedef {import('micromark-util-types').Create} Create
 */
/**
 * @param {ParseOptions} [options]
 * @returns {ParseContext}
 */

function parse$1(options = {}) {
  /** @type {FullNormalizedExtension} */
  // @ts-expect-error `defaultConstructs` is full, so the result will be too.
  const constructs = combineExtensions(
    // @ts-expect-error Same as above.
    [defaultConstructs].concat(options.extensions || [])
  );
  /** @type {ParseContext} */

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
  /**
   * @param {InitialConstruct} initial
   */

  function create(initial) {
    return creator
    /** @type {Create} */

    function creator(from) {
      return createTokenizer(parser, initial, from)
    }
  }
}

/**
 * @typedef {import('micromark-util-types').Encoding} Encoding
 * @typedef {import('micromark-util-types').Value} Value
 * @typedef {import('micromark-util-types').Chunk} Chunk
 * @typedef {import('micromark-util-types').Code} Code
 */

/**
 * @callback Preprocessor
 * @param {Value} value
 * @param {Encoding} [encoding]
 * @param {boolean} [end=false]
 * @returns {Chunk[]}
 */
const search = /[\0\t\n\r]/g;
/**
 * @returns {Preprocessor}
 */

function preprocess() {
  let column = 1;
  let buffer = '';
  /** @type {boolean|undefined} */

  let start = true;
  /** @type {boolean|undefined} */

  let atCarriageReturn;
  return preprocessor
  /** @type {Preprocessor} */

  function preprocessor(value, encoding, end) {
    /** @type {Chunk[]} */
    const chunks = [];
    /** @type {RegExpMatchArray|null} */

    let match;
    /** @type {number} */

    let next;
    /** @type {number} */

    let startPosition;
    /** @type {number} */

    let endPosition;
    /** @type {Code} */

    let code; // @ts-expect-error `Buffer` does allow an encoding.

    value = buffer + value.toString(encoding);
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

/**
 * @typedef {import('micromark-util-types').Event} Event
 */
/**
 * @param {Event[]} events
 * @returns {Event[]}
 */

function postprocess(events) {
  while (!subtokenize(events)) {
    // Empty
  }

  return events
}

/**
 * Turn the number (in string form as either hexa- or plain decimal) coming from
 * a numeric character reference into a character.
 *
 * @param {string} value
 *   Value to decode.
 * @param {number} base
 *   Numeric base.
 * @returns {string}
 */
function decodeNumericCharacterReference(value, base) {
  const code = Number.parseInt(value, base);

  if (
    // C0 except for HT, LF, FF, CR, space
    code < 9 ||
    code === 11 ||
    (code > 13 && code < 32) || // Control character (DEL) of the basic block and C1 controls.
    (code > 126 && code < 160) || // Lone high surrogates and low surrogates.
    (code > 55295 && code < 57344) || // Noncharacters.
    (code > 64975 && code < 65008) ||
    (code & 65535) === 65535 ||
    (code & 65535) === 65534 || // Out of range
    code > 1114111
  ) {
    return '\uFFFD'
  }

  return String.fromCharCode(code)
}

const characterEscapeOrReference =
  /\\([!-/:-@[-`{-~])|&(#(?:\d{1,7}|x[\da-f]{1,6})|[\da-z]{1,31});/gi;
/**
 * Utility to decode markdown strings (which occur in places such as fenced
 * code info strings, destinations, labels, and titles).
 * The “string” content type allows character escapes and -references.
 * This decodes those.
 *
 * @param {string} value
 * @returns {string}
 */

function decodeString(value) {
  return value.replace(characterEscapeOrReference, decode)
}
/**
 * @param {string} $0
 * @param {string} $1
 * @param {string} $2
 * @returns {string}
 */

function decode($0, $1, $2) {
  if ($1) {
    // Escape.
    return $1
  } // Reference.

  const head = $2.charCodeAt(0);

  if (head === 35) {
    const head = $2.charCodeAt(1);
    const hex = head === 120 || head === 88;
    return decodeNumericCharacterReference($2.slice(hex ? 2 : 1), hex ? 16 : 10)
  }

  return decodeEntity($2) || $0
}

/**
 * @typedef {import('micromark-util-types').Encoding} Encoding
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').ParseOptions} ParseOptions
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').Value} Value
 * @typedef {import('unist').Parent} UnistParent
 * @typedef {import('unist').Point} Point
 * @typedef {import('mdast').PhrasingContent} PhrasingContent
 * @typedef {import('mdast').Content} Content
 * @typedef {Root|Content} Node
 * @typedef {Extract<Node, UnistParent>} Parent
 * @typedef {import('mdast').Break} Break
 * @typedef {import('mdast').Blockquote} Blockquote
 * @typedef {import('mdast').Code} Code
 * @typedef {import('mdast').Definition} Definition
 * @typedef {import('mdast').Emphasis} Emphasis
 * @typedef {import('mdast').Heading} Heading
 * @typedef {import('mdast').HTML} HTML
 * @typedef {import('mdast').Image} Image
 * @typedef {import('mdast').ImageReference} ImageReference
 * @typedef {import('mdast').InlineCode} InlineCode
 * @typedef {import('mdast').Link} Link
 * @typedef {import('mdast').LinkReference} LinkReference
 * @typedef {import('mdast').List} List
 * @typedef {import('mdast').ListItem} ListItem
 * @typedef {import('mdast').Paragraph} Paragraph
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Strong} Strong
 * @typedef {import('mdast').Text} Text
 * @typedef {import('mdast').ThematicBreak} ThematicBreak
 *
 * @typedef {UnistParent & {type: 'fragment', children: PhrasingContent[]}} Fragment
 */
const own$5 = {}.hasOwnProperty;
/**
 * @param value Markdown to parse (`string` or `Buffer`).
 * @param [encoding] Character encoding to understand `value` as when it’s a `Buffer` (`string`, default: `'utf8'`).
 * @param [options] Configuration
 */

const fromMarkdown =
  /**
   * @type {(
   *   ((value: Value, encoding: Encoding, options?: Options) => Root) &
   *   ((value: Value, options?: Options) => Root)
   * )}
   */

  /**
   * @param {Value} value
   * @param {Encoding} [encoding]
   * @param {Options} [options]
   * @returns {Root}
   */
  function (value, encoding, options) {
    if (typeof encoding !== 'string') {
      options = encoding;
      encoding = undefined;
    }

    return compiler(options)(
      postprocess(
        parse$1(options).document().write(preprocess()(value, encoding, true))
      )
    )
  };
/**
 * Note this compiler only understand complete buffering, not streaming.
 *
 * @param {Options} [options]
 */

function compiler(options = {}) {
  /** @type {NormalizedExtension} */
  // @ts-expect-error: our base has all required fields, so the result will too.
  const config = configure$1(
    {
      transforms: [],
      canContainEols: [
        'emphasis',
        'fragment',
        'heading',
        'paragraph',
        'strong'
      ],
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
    },
    options.mdastExtensions || []
  );
  /** @type {CompileData} */

  const data = {};
  return compile
  /**
   * @param {Array.<Event>} events
   * @returns {Root}
   */

  function compile(events) {
    /** @type {Root} */
    let tree = {
      type: 'root',
      children: []
    };
    /** @type {CompileContext['stack']} */

    const stack = [tree];
    /** @type {CompileContext['tokenStack']} */

    const tokenStack = [];
    /** @type {Array.<number>} */

    const listStack = [];
    /** @type {Omit<CompileContext, 'sliceSerialize'>} */

    const context = {
      stack,
      tokenStack,
      config,
      enter,
      exit,
      buffer,
      resume,
      setData,
      getData
    };
    let index = -1;

    while (++index < events.length) {
      // We preprocess lists to add `listItem` tokens, and to infer whether
      // items the list itself are spread out.
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

      if (own$5.call(handler, events[index][1].type)) {
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

    if (tokenStack.length > 0) {
      throw new Error(
        'Cannot close document, a token (`' +
          tokenStack[tokenStack.length - 1].type +
          '`, ' +
          stringifyPosition({
            start: tokenStack[tokenStack.length - 1].start,
            end: tokenStack[tokenStack.length - 1].end
          }) +
          ') is still open'
      )
    } // Figure out `root` position.

    tree.position = {
      start: point(
        events.length > 0
          ? events[0][1].start
          : {
              line: 1,
              column: 1,
              offset: 0
            }
      ),
      end: point(
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
  /**
   * @param {Array.<Event>} events
   * @param {number} start
   * @param {number} length
   * @returns {number}
   */

  function prepareList(events, start, length) {
    let index = start - 1;
    let containerBalance = -1;
    let listSpread = false;
    /** @type {Token|undefined} */

    let listItem;
    /** @type {number|undefined} */

    let lineIndex;
    /** @type {number|undefined} */

    let firstBlankLineIndex;
    /** @type {boolean|undefined} */

    let atMarker;

    while (++index <= length) {
      const event = events[index];

      if (
        event[1].type === 'listUnordered' ||
        event[1].type === 'listOrdered' ||
        event[1].type === 'blockQuote'
      ) {
        if (event[0] === 'enter') {
          containerBalance++;
        } else {
          containerBalance--;
        }

        atMarker = undefined;
      } else if (event[1].type === 'lineEndingBlank') {
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
      } else if (
        event[1].type === 'linePrefix' ||
        event[1].type === 'listItemValue' ||
        event[1].type === 'listItemMarker' ||
        event[1].type === 'listItemPrefix' ||
        event[1].type === 'listItemPrefixWhitespace'
      ) ; else {
        atMarker = undefined;
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
            // @ts-expect-error Patched.
            listItem._spread = true;
          } // Fix position.

          listItem.end = Object.assign(
            {},
            lineIndex ? events[lineIndex][1].start : event[1].end
          );
          events.splice(lineIndex || index, 0, ['exit', listItem, event[2]]);
          index++;
          length++;
        } // Create a new list item.

        if (event[1].type === 'listItemPrefix') {
          listItem = {
            type: 'listItem',
            // @ts-expect-error Patched
            _spread: false,
            start: Object.assign({}, event[1].start)
          }; // @ts-expect-error: `listItem` is most definitely defined, TS...

          events.splice(index, 0, ['enter', listItem, event[2]]);
          index++;
          length++;
          firstBlankLineIndex = undefined;
          atMarker = true;
        }
      }
    } // @ts-expect-error Patched.

    events[start][1]._spread = listSpread;
    return length
  }
  /**
   * @type {CompileContext['setData']}
   * @param [value]
   */

  function setData(key, value) {
    data[key] = value;
  }
  /**
   * @type {CompileContext['getData']}
   * @template {string} K
   * @param {K} key
   * @returns {CompileData[K]}
   */

  function getData(key) {
    return data[key]
  }
  /**
   * @param {Point} d
   * @returns {Point}
   */

  function point(d) {
    return {
      line: d.line,
      column: d.column,
      offset: d.offset
    }
  }
  /**
   * @param {(token: Token) => Node} create
   * @param {Handle} [and]
   * @returns {Handle}
   */

  function opener(create, and) {
    return open
    /**
     * @this {CompileContext}
     * @param {Token} token
     * @returns {void}
     */

    function open(token) {
      enter.call(this, create(token), token);
      if (and) and.call(this, token);
    }
  }
  /** @type {CompileContext['buffer']} */

  function buffer() {
    this.stack.push({
      type: 'fragment',
      children: []
    });
  }
  /**
   * @type {CompileContext['enter']}
   * @template {Node} N
   * @this {CompileContext}
   * @param {N} node
   * @param {Token} token
   * @returns {N}
   */

  function enter(node, token) {
    const parent = this.stack[this.stack.length - 1];
    // @ts-expect-error: Assume `Node` can exist as a child of `parent`.
    parent.children.push(node);
    this.stack.push(node);
    this.tokenStack.push(token); // @ts-expect-error: `end` will be patched later.

    node.position = {
      start: point(token.start)
    };
    return node
  }
  /**
   * @param {Handle} [and]
   * @returns {Handle}
   */

  function closer(and) {
    return close
    /**
     * @this {CompileContext}
     * @param {Token} token
     * @returns {void}
     */

    function close(token) {
      if (and) and.call(this, token);
      exit.call(this, token);
    }
  }
  /** @type {CompileContext['exit']} */

  function exit(token) {
    const node = this.stack.pop();
    const open = this.tokenStack.pop();

    if (!open) {
      throw new Error(
        'Cannot close `' +
          token.type +
          '` (' +
          stringifyPosition({
            start: token.start,
            end: token.end
          }) +
          '): it’s not open'
      )
    } else if (open.type !== token.type) {
      throw new Error(
        'Cannot close `' +
          token.type +
          '` (' +
          stringifyPosition({
            start: token.start,
            end: token.end
          }) +
          '): a different token (`' +
          open.type +
          '`, ' +
          stringifyPosition({
            start: open.start,
            end: open.end
          }) +
          ') is open'
      )
    }

    node.position.end = point(token.end);
    return node
  }
  /**
   * @this {CompileContext}
   * @returns {string}
   */

  function resume() {
    return toString(this.stack.pop())
  } //
  // Handlers.
  //

  /** @type {Handle} */

  function onenterlistordered() {
    setData('expectingFirstListItemValue', true);
  }
  /** @type {Handle} */

  function onenterlistitemvalue(token) {
    if (getData('expectingFirstListItemValue')) {
      const ancestor =
        /** @type {List} */
        this.stack[this.stack.length - 2];
      ancestor.start = Number.parseInt(this.sliceSerialize(token), 10);
      setData('expectingFirstListItemValue');
    }
  }
  /** @type {Handle} */

  function onexitcodefencedfenceinfo() {
    const data = this.resume();
    const node =
      /** @type {Code} */
      this.stack[this.stack.length - 1];
    node.lang = data;
  }
  /** @type {Handle} */

  function onexitcodefencedfencemeta() {
    const data = this.resume();
    const node =
      /** @type {Code} */
      this.stack[this.stack.length - 1];
    node.meta = data;
  }
  /** @type {Handle} */

  function onexitcodefencedfence() {
    // Exit if this is the closing fence.
    if (getData('flowCodeInside')) return
    this.buffer();
    setData('flowCodeInside', true);
  }
  /** @type {Handle} */

  function onexitcodefenced() {
    const data = this.resume();
    const node =
      /** @type {Code} */
      this.stack[this.stack.length - 1];
    node.value = data.replace(/^(\r?\n|\r)|(\r?\n|\r)$/g, '');
    setData('flowCodeInside');
  }
  /** @type {Handle} */

  function onexitcodeindented() {
    const data = this.resume();
    const node =
      /** @type {Code} */
      this.stack[this.stack.length - 1];
    node.value = data.replace(/(\r?\n|\r)$/g, '');
  }
  /** @type {Handle} */

  function onexitdefinitionlabelstring(token) {
    // Discard label, use the source content instead.
    const label = this.resume();
    const node =
      /** @type {Definition} */
      this.stack[this.stack.length - 1];
    node.label = label;
    node.identifier = normalizeIdentifier(
      this.sliceSerialize(token)
    ).toLowerCase();
  }
  /** @type {Handle} */

  function onexitdefinitiontitlestring() {
    const data = this.resume();
    const node =
      /** @type {Definition} */
      this.stack[this.stack.length - 1];
    node.title = data;
  }
  /** @type {Handle} */

  function onexitdefinitiondestinationstring() {
    const data = this.resume();
    const node =
      /** @type {Definition} */
      this.stack[this.stack.length - 1];
    node.url = data;
  }
  /** @type {Handle} */

  function onexitatxheadingsequence(token) {
    const node =
      /** @type {Heading} */
      this.stack[this.stack.length - 1];

    if (!node.depth) {
      const depth = this.sliceSerialize(token).length;
      node.depth = depth;
    }
  }
  /** @type {Handle} */

  function onexitsetextheadingtext() {
    setData('setextHeadingSlurpLineEnding', true);
  }
  /** @type {Handle} */

  function onexitsetextheadinglinesequence(token) {
    const node =
      /** @type {Heading} */
      this.stack[this.stack.length - 1];
    node.depth = this.sliceSerialize(token).charCodeAt(0) === 61 ? 1 : 2;
  }
  /** @type {Handle} */

  function onexitsetextheading() {
    setData('setextHeadingSlurpLineEnding');
  }
  /** @type {Handle} */

  function onenterdata(token) {
    const parent =
      /** @type {Parent} */
      this.stack[this.stack.length - 1];
    /** @type {Node} */

    let tail = parent.children[parent.children.length - 1];

    if (!tail || tail.type !== 'text') {
      // Add a new text node.
      tail = text(); // @ts-expect-error: we’ll add `end` later.

      tail.position = {
        start: point(token.start)
      }; // @ts-expect-error: Assume `parent` accepts `text`.

      parent.children.push(tail);
    }

    this.stack.push(tail);
  }
  /** @type {Handle} */

  function onexitdata(token) {
    const tail = this.stack.pop();
    tail.value += this.sliceSerialize(token);
    tail.position.end = point(token.end);
  }
  /** @type {Handle} */

  function onexitlineending(token) {
    const context = this.stack[this.stack.length - 1];

    // If we’re at a hard break, include the line ending in there.
    if (getData('atHardBreak')) {
      const tail = context.children[context.children.length - 1];
      tail.position.end = point(token.end);
      setData('atHardBreak');
      return
    }

    if (
      !getData('setextHeadingSlurpLineEnding') &&
      config.canContainEols.includes(context.type)
    ) {
      onenterdata.call(this, token);
      onexitdata.call(this, token);
    }
  }
  /** @type {Handle} */

  function onexithardbreak() {
    setData('atHardBreak', true);
  }
  /** @type {Handle} */

  function onexithtmlflow() {
    const data = this.resume();
    const node =
      /** @type {HTML} */
      this.stack[this.stack.length - 1];
    node.value = data;
  }
  /** @type {Handle} */

  function onexithtmltext() {
    const data = this.resume();
    const node =
      /** @type {HTML} */
      this.stack[this.stack.length - 1];
    node.value = data;
  }
  /** @type {Handle} */

  function onexitcodetext() {
    const data = this.resume();
    const node =
      /** @type {InlineCode} */
      this.stack[this.stack.length - 1];
    node.value = data;
  }
  /** @type {Handle} */

  function onexitlink() {
    const context =
      /** @type {Link & {identifier: string, label: string}} */
      this.stack[this.stack.length - 1]; // To do: clean.

    if (getData('inReference')) {
      context.type += 'Reference'; // @ts-expect-error: mutate.

      context.referenceType = getData('referenceType') || 'shortcut'; // @ts-expect-error: mutate.

      delete context.url;
      delete context.title;
    } else {
      // @ts-expect-error: mutate.
      delete context.identifier; // @ts-expect-error: mutate.

      delete context.label;
    }

    setData('referenceType');
  }
  /** @type {Handle} */

  function onexitimage() {
    const context =
      /** @type {Image & {identifier: string, label: string}} */
      this.stack[this.stack.length - 1]; // To do: clean.

    if (getData('inReference')) {
      context.type += 'Reference'; // @ts-expect-error: mutate.

      context.referenceType = getData('referenceType') || 'shortcut'; // @ts-expect-error: mutate.

      delete context.url;
      delete context.title;
    } else {
      // @ts-expect-error: mutate.
      delete context.identifier; // @ts-expect-error: mutate.

      delete context.label;
    }

    setData('referenceType');
  }
  /** @type {Handle} */

  function onexitlabeltext(token) {
    const ancestor =
      /** @type {(Link|Image) & {identifier: string, label: string}} */
      this.stack[this.stack.length - 2];
    const string = this.sliceSerialize(token);
    ancestor.label = decodeString(string);
    ancestor.identifier = normalizeIdentifier(string).toLowerCase();
  }
  /** @type {Handle} */

  function onexitlabel() {
    const fragment =
      /** @type {Fragment} */
      this.stack[this.stack.length - 1];
    const value = this.resume();
    const node =
      /** @type {(Link|Image) & {identifier: string, label: string}} */
      this.stack[this.stack.length - 1]; // Assume a reference.

    setData('inReference', true);

    if (node.type === 'link') {
      // @ts-expect-error: Assume static phrasing content.
      node.children = fragment.children;
    } else {
      node.alt = value;
    }
  }
  /** @type {Handle} */

  function onexitresourcedestinationstring() {
    const data = this.resume();
    const node =
      /** @type {Link|Image} */
      this.stack[this.stack.length - 1];
    node.url = data;
  }
  /** @type {Handle} */

  function onexitresourcetitlestring() {
    const data = this.resume();
    const node =
      /** @type {Link|Image} */
      this.stack[this.stack.length - 1];
    node.title = data;
  }
  /** @type {Handle} */

  function onexitresource() {
    setData('inReference');
  }
  /** @type {Handle} */

  function onenterreference() {
    setData('referenceType', 'collapsed');
  }
  /** @type {Handle} */

  function onexitreferencestring(token) {
    const label = this.resume();
    const node =
      /** @type {LinkReference|ImageReference} */
      this.stack[this.stack.length - 1];
    node.label = label;
    node.identifier = normalizeIdentifier(
      this.sliceSerialize(token)
    ).toLowerCase();
    setData('referenceType', 'full');
  }
  /** @type {Handle} */

  function onexitcharacterreferencemarker(token) {
    setData('characterReferenceType', token.type);
  }
  /** @type {Handle} */

  function onexitcharacterreferencevalue(token) {
    const data = this.sliceSerialize(token);
    const type = getData('characterReferenceType');
    /** @type {string} */

    let value;

    if (type) {
      value = decodeNumericCharacterReference(
        data,
        type === 'characterReferenceMarkerNumeric' ? 10 : 16
      );
      setData('characterReferenceType');
    } else {
      // @ts-expect-error `decodeEntity` can return false for invalid named
      // character references, but everything we’ve tokenized is valid.
      value = decodeEntity(data);
    }

    const tail = this.stack.pop();
    tail.value += value;
    tail.position.end = point(token.end);
  }
  /** @type {Handle} */

  function onexitautolinkprotocol(token) {
    onexitdata.call(this, token);
    const node =
      /** @type {Link} */
      this.stack[this.stack.length - 1];
    node.url = this.sliceSerialize(token);
  }
  /** @type {Handle} */

  function onexitautolinkemail(token) {
    onexitdata.call(this, token);
    const node =
      /** @type {Link} */
      this.stack[this.stack.length - 1];
    node.url = 'mailto:' + this.sliceSerialize(token);
  } //
  // Creaters.
  //

  /** @returns {Blockquote} */

  function blockQuote() {
    return {
      type: 'blockquote',
      children: []
    }
  }
  /** @returns {Code} */

  function codeFlow() {
    return {
      type: 'code',
      lang: null,
      meta: null,
      value: ''
    }
  }
  /** @returns {InlineCode} */

  function codeText() {
    return {
      type: 'inlineCode',
      value: ''
    }
  }
  /** @returns {Definition} */

  function definition() {
    return {
      type: 'definition',
      identifier: '',
      label: null,
      title: null,
      url: ''
    }
  }
  /** @returns {Emphasis} */

  function emphasis() {
    return {
      type: 'emphasis',
      children: []
    }
  }
  /** @returns {Heading} */

  function heading() {
    // @ts-expect-error `depth` will be set later.
    return {
      type: 'heading',
      depth: undefined,
      children: []
    }
  }
  /** @returns {Break} */

  function hardBreak() {
    return {
      type: 'break'
    }
  }
  /** @returns {HTML} */

  function html() {
    return {
      type: 'html',
      value: ''
    }
  }
  /** @returns {Image} */

  function image() {
    return {
      type: 'image',
      title: null,
      url: '',
      alt: null
    }
  }
  /** @returns {Link} */

  function link() {
    return {
      type: 'link',
      title: null,
      url: '',
      children: []
    }
  }
  /**
   * @param {Token} token
   * @returns {List}
   */

  function list(token) {
    return {
      type: 'list',
      ordered: token.type === 'listOrdered',
      start: null,
      // @ts-expect-error Patched.
      spread: token._spread,
      children: []
    }
  }
  /**
   * @param {Token} token
   * @returns {ListItem}
   */

  function listItem(token) {
    return {
      type: 'listItem',
      // @ts-expect-error Patched.
      spread: token._spread,
      checked: null,
      children: []
    }
  }
  /** @returns {Paragraph} */

  function paragraph() {
    return {
      type: 'paragraph',
      children: []
    }
  }
  /** @returns {Strong} */

  function strong() {
    return {
      type: 'strong',
      children: []
    }
  }
  /** @returns {Text} */

  function text() {
    return {
      type: 'text',
      value: ''
    }
  }
  /** @returns {ThematicBreak} */

  function thematicBreak() {
    return {
      type: 'thematicBreak'
    }
  }
}
/**
 * @param {Extension} combined
 * @param {Array.<Extension|Array.<Extension>>} extensions
 * @returns {Extension}
 */

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

  return combined
}
/**
 * @param {Extension} combined
 * @param {Extension} extension
 * @returns {void}
 */

function extension(combined, extension) {
  /** @type {string} */
  let key;

  for (key in extension) {
    if (own$5.call(extension, key)) {
      const list = key === 'canContainEols' || key === 'transforms';
      const maybe = own$5.call(combined, key) ? combined[key] : undefined;
      /* c8 ignore next */

      const left = maybe || (combined[key] = list ? [] : {});
      const right = extension[key];

      if (right) {
        if (list) {
          // @ts-expect-error: `left` is an array.
          combined[key] = [...left, ...right];
        } else {
          Object.assign(left, right);
        }
      }
    }
  }
}

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast-util-from-markdown').Options} Options
 */

/** @type {import('unified').Plugin<[Options?] | void[], string, Root>} */
function remarkParse(options) {
  /** @type {import('unified').ParserFunction<Root>} */
  const parser = (doc) => {
    // Assume options.
    const settings = /** @type {Options} */ (this.data('settings'));

    return fromMarkdown(
      doc,
      Object.assign({}, settings, options, {
        // Note: these options are not in the readme.
        // The goal is for them to be set by plugins on `data` instead of being
        // passed by users.
        extensions: this.data('micromarkExtensions') || [],
        mdastExtensions: this.data('fromMarkdownExtensions') || []
      })
    )
  };

  Object.assign(this, {Parser: parser});
}

var own$4 = {}.hasOwnProperty;

/**
 * @callback Handler
 * @param {...unknown} value
 * @return {unknown}
 *
 * @typedef {Record<string, Handler>} Handlers
 *
 * @typedef {Object} Options
 * @property {Handler} [unknown]
 * @property {Handler} [invalid]
 * @property {Handlers} [handlers]
 */

/**
 * Handle values based on a property.
 *
 * @param {string} key
 * @param {Options} [options]
 */
function zwitch(key, options) {
  var settings = options || {};

  /**
   * Handle one value.
   * Based on the bound `key`, a respective handler will be called.
   * If `value` is not an object, or doesn’t have a `key` property, the special
   * “invalid” handler will be called.
   * If `value` has an unknown `key`, the special “unknown” handler will be
   * called.
   *
   * All arguments, and the context object, are passed through to the handler,
   * and it’s result is returned.
   *
   * @param {...unknown} [value]
   * @this {unknown}
   * @returns {unknown}
   * @property {Handler} invalid
   * @property {Handler} unknown
   * @property {Handlers} handlers
   */
  function one(value) {
    var fn = one.invalid;
    var handlers = one.handlers;

    if (value && own$4.call(value, key)) {
      fn = own$4.call(handlers, value[key]) ? handlers[value[key]] : one.unknown;
    }

    if (fn) {
      return fn.apply(this, arguments)
    }
  }

  one.handlers = settings.handlers || {};
  one.invalid = settings.invalid;
  one.unknown = settings.unknown;

  return one
}

/**
 * @typedef {import('./types.js').Options} Options
 * @typedef {import('./types.js').Context} Context
 */

/**
 * @param {Context} base
 * @param {Options} extension
 * @returns {Context}
 */
function configure(base, extension) {
  let index = -1;
  /** @type {string} */
  let key;

  // First do subextensions.
  if (extension.extensions) {
    while (++index < extension.extensions.length) {
      configure(base, extension.extensions[index]);
    }
  }

  for (key in extension) {
    if (key === 'extensions') ; else if (key === 'unsafe' || key === 'join') {
      /* c8 ignore next 2 */
      // @ts-expect-error: hush.
      base[key] = [...(base[key] || []), ...(extension[key] || [])];
    } else if (key === 'handlers') {
      base[key] = Object.assign(base[key], extension[key] || {});
    } else {
      // @ts-expect-error: hush.
      base.options[key] = extension[key];
    }
  }

  return base
}

/**
 * @typedef {import('../types.js').Node} Node
 * @typedef {import('../types.js').Parent} Parent
 * @typedef {import('../types.js').Join} Join
 * @typedef {import('../types.js').Context} Context
 */

/**
 * @param {Parent} parent
 * @param {Context} context
 * @returns {string}
 */
function containerFlow(parent, context) {
  const indexStack = context.indexStack;
  const children = parent.children || [];
  /** @type {Array.<string>} */
  const results = [];
  let index = -1;

  indexStack.push(-1);

  while (++index < children.length) {
    const child = children[index];

    indexStack[indexStack.length - 1] = index;

    results.push(
      context.handle(child, parent, context, {before: '\n', after: '\n'})
    );

    if (child.type !== 'list') {
      context.bulletLastUsed = undefined;
    }

    if (index < children.length - 1) {
      results.push(between(child, children[index + 1]));
    }
  }

  indexStack.pop();

  return results.join('')

  /**
   * @param {Node} left
   * @param {Node} right
   * @returns {string}
   */
  function between(left, right) {
    let index = context.join.length;

    while (index--) {
      const result = context.join[index](left, right, parent, context);

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
}

/**
 * @callback Map
 * @param {string} value
 * @param {number} line
 * @param {boolean} blank
 * @returns {string}
 */

const eol = /\r?\n|\r/g;

/**
 * @param {string} value
 * @param {Map} map
 * @returns {string}
 */
function indentLines(value, map) {
  /** @type {Array.<string>} */
  const result = [];
  let start = 0;
  let line = 0;
  /** @type {RegExpExecArray|null} */
  let match;

  while ((match = eol.exec(value))) {
    one(value.slice(start, match.index));
    result.push(match[0]);
    start = match.index + match[0].length;
    line++;
  }

  one(value.slice(start));

  return result.join('')

  /**
   * @param {string} value
   */
  function one(value) {
    result.push(map(value, line, !value));
  }
}

/**
 * @typedef {import('mdast').Blockquote} Blockquote
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('../util/indent-lines.js').Map} Map
 */

/**
 * @type {Handle}
 * @param {Blockquote} node
 */
function blockquote(node, _, context) {
  const exit = context.enter('blockquote');
  const value = indentLines(containerFlow(node, context), map$2);
  exit();
  return value
}

/** @type {Map} */
function map$2(line, _, blank) {
  return '>' + (blank ? '' : ' ') + line
}

/**
 * @typedef {import('../types.js').Unsafe} Unsafe
 */

/**
 * @param {Array.<string>} stack
 * @param {Unsafe} pattern
 * @returns {boolean}
 */
function patternInScope(stack, pattern) {
  return (
    listInScope(stack, pattern.inConstruct, true) &&
    !listInScope(stack, pattern.notInConstruct, false)
  )
}

/**
 * @param {Array.<string>} stack
 * @param {Unsafe['inConstruct']} list
 * @param {boolean} none
 * @returns {boolean}
 */
function listInScope(stack, list, none) {
  if (!list) {
    return none
  }

  if (typeof list === 'string') {
    list = [list];
  }

  let index = -1;

  while (++index < list.length) {
    if (stack.includes(list[index])) {
      return true
    }
  }

  return false
}

/**
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('mdast').Break} Break
 */

/**
 * @type {Handle}
 * @param {Break} _
 */
function hardBreak(_, _1, context, safe) {
  let index = -1;

  while (++index < context.unsafe.length) {
    // If we can’t put eols in this construct (setext headings, tables), use a
    // space instead.
    if (
      context.unsafe[index].character === '\n' &&
      patternInScope(context.stack, context.unsafe[index])
    ) {
      return /[ \t]/.test(safe.before) ? '' : ' '
    }
  }

  return '\\\n'
}

/**
 * Get the count of the longest repeating streak of `character` in `value`.
 *
 * @param {string} value Content.
 * @param {string} character Single character to look for
 * @returns {number} Count of most frequent adjacent `character`s in `value`
 */
function longestStreak(value, character) {
  var source = String(value);
  var index = source.indexOf(character);
  var expected = index;
  var count = 0;
  var max = 0;

  if (typeof character !== 'string' || character.length !== 1) {
    throw new Error('Expected character')
  }

  while (index !== -1) {
    if (index === expected) {
      if (++count > max) {
        max = count;
      }
    } else {
      count = 1;
    }

    expected = index + 1;
    index = source.indexOf(character, expected);
  }

  return max
}

/**
 * @typedef {import('mdast').Code} Code
 * @typedef {import('../types.js').Context} Context
 */

/**
 * @param {Code} node
 * @param {Context} context
 * @returns {boolean}
 */
function formatCodeAsIndented(node, context) {
  return Boolean(
    !context.options.fences &&
      node.value &&
      // If there’s no info…
      !node.lang &&
      // And there’s a non-whitespace character…
      /[^ \r\n]/.test(node.value) &&
      // And the value doesn’t start or end in a blank…
      !/^[\t ]*(?:[\r\n]|$)|(?:^|[\r\n])[\t ]*$/.test(node.value)
  )
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['fence'], undefined>}
 */
function checkFence(context) {
  const marker = context.options.fence || '`';

  if (marker !== '`' && marker !== '~') {
    throw new Error(
      'Cannot serialize code with `' +
        marker +
        '` for `options.fence`, expected `` ` `` or `~`'
    )
  }

  return marker
}

/**
 * @typedef {import('../types.js').Unsafe} Unsafe
 */

/**
 * @param {Unsafe} pattern
 * @returns {RegExp}
 */
function patternCompile(pattern) {
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

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').SafeOptions} SafeOptions
 */

/**
 * @param {Context} context
 * @param {string|null|undefined} input
 * @param {SafeOptions & {encode?: Array.<string>}} config
 * @returns {string}
 */
function safe(context, input, config) {
  const value = (config.before || '') + (input || '') + (config.after || '');
  /** @type {Array.<number>} */
  const positions = [];
  /** @type {Array.<string>} */
  const result = [];
  /** @type {Record<number, {before: boolean, after: boolean}>} */
  const infos = {};
  let index = -1;

  while (++index < context.unsafe.length) {
    const pattern = context.unsafe[index];

    if (!patternInScope(context.stack, pattern)) {
      continue
    }

    const expression = patternCompile(pattern);
    /** @type {RegExpExecArray|null} */
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

    // Character before or after matched:
    if (position < start || position >= end) {
      continue
    }

    // If this character is supposed to be escaped because it has a condition on
    // the next character, and the next character is definitly being escaped,
    // then skip this escape.
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
      // If we have to use a character reference, an ampersand would be more
      // correct, but as backslashes only care about punctuation, either will
      // do the trick
      result.push(escapeBackslashes(value.slice(start, position), '\\'));
    }

    start = position;

    if (
      /[!-/:-@[-`{-~]/.test(value.charAt(position)) &&
      (!config.encode || !config.encode.includes(value.charAt(position)))
    ) {
      // Character escape.
      result.push('\\');
    } else {
      // Character reference.
      result.push(
        '&#x' + value.charCodeAt(position).toString(16).toUpperCase() + ';'
      );
      start++;
    }
  }

  result.push(escapeBackslashes(value.slice(start, end), config.after));

  return result.join('')
}

/**
 * @param {number} a
 * @param {number} b
 * @returns {number}
 */
function numerical(a, b) {
  return a - b
}

/**
 * @param {string} value
 * @param {string} after
 * @returns {string}
 */
function escapeBackslashes(value, after) {
  const expression = /\\(?=[!-/:-@[-`{-~])/g;
  /** @type {Array.<number>} */
  const positions = [];
  /** @type {Array.<string>} */
  const results = [];
  const whole = value + after;
  let index = -1;
  let start = 0;
  /** @type {RegExpExecArray|null} */
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

/**
 * @typedef {import('mdast').Code} Code
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('../types.js').Exit} Exit
 * @typedef {import('../util/indent-lines.js').Map} Map
 */

/**
 * @type {Handle}
 * @param {Code} node
 */
function code$1(node, _, context) {
  const marker = checkFence(context);
  const raw = node.value || '';
  const suffix = marker === '`' ? 'GraveAccent' : 'Tilde';
  /** @type {string} */
  let value;
  /** @type {Exit} */
  let exit;

  if (formatCodeAsIndented(node, context)) {
    exit = context.enter('codeIndented');
    value = indentLines(raw, map$1);
  } else {
    const sequence = marker.repeat(Math.max(longestStreak(raw, marker) + 1, 3));
    /** @type {Exit} */
    let subexit;
    exit = context.enter('codeFenced');
    value = sequence;

    if (node.lang) {
      subexit = context.enter('codeFencedLang' + suffix);
      value += safe(context, node.lang, {
        before: '`',
        after: ' ',
        encode: ['`']
      });
      subexit();
    }

    if (node.lang && node.meta) {
      subexit = context.enter('codeFencedMeta' + suffix);
      value +=
        ' ' +
        safe(context, node.meta, {
          before: ' ',
          after: '\n',
          encode: ['`']
        });
      subexit();
    }

    value += '\n';

    if (raw) {
      value += raw + '\n';
    }

    value += sequence;
  }

  exit();
  return value
}

/** @type {Map} */
function map$1(line, _, blank) {
  return (blank ? '' : '    ') + line
}

/**
 * @typedef {import('mdast').Association} Association
 */

/**
 * The `label` of an association is the string value: character escapes and
 * references work, and casing is intact.
 * The `identifier` is used to match one association to another: controversially,
 * character escapes and references don’t work in this matching: `&copy;` does
 * not match `©`, and `\+` does not match `+`.
 * But casing is ignored (and whitespace) is trimmed and collapsed: ` A\nb`
 * matches `a b`.
 * So, we do prefer the label when figuring out how we’re going to serialize:
 * it has whitespace, casing, and we can ignore most useless character escapes
 * and all character references.
 *
 * @param {Association} node
 * @returns {string}
 */
function association(node) {
  if (node.label || !node.identifier) {
    return node.label || ''
  }

  return decodeString(node.identifier)
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['quote'], undefined>}
 */
function checkQuote(context) {
  const marker = context.options.quote || '"';

  if (marker !== '"' && marker !== "'") {
    throw new Error(
      'Cannot serialize title with `' +
        marker +
        '` for `options.quote`, expected `"`, or `\'`'
    )
  }

  return marker
}

/**
 * @typedef {import('mdast').Definition} Definition
 * @typedef {import('../types.js').Handle} Handle
 */

/**
 * @type {Handle}
 * @param {Definition} node
 */
function definition(node, _, context) {
  const marker = checkQuote(context);
  const suffix = marker === '"' ? 'Quote' : 'Apostrophe';
  const exit = context.enter('definition');
  let subexit = context.enter('label');
  let value =
    '[' + safe(context, association(node), {before: '[', after: ']'}) + ']: ';

  subexit();

  if (
    // If there’s no url, or…
    !node.url ||
    // If there’s whitespace, enclosed is prettier.
    /[ \t\r\n]/.test(node.url)
  ) {
    subexit = context.enter('destinationLiteral');
    value += '<' + safe(context, node.url, {before: '<', after: '>'}) + '>';
  } else {
    // No whitespace, raw is prettier.
    subexit = context.enter('destinationRaw');
    value += safe(context, node.url, {before: ' ', after: ' '});
  }

  subexit();

  if (node.title) {
    subexit = context.enter('title' + suffix);
    value +=
      ' ' +
      marker +
      safe(context, node.title, {before: marker, after: marker}) +
      marker;
    subexit();
  }

  exit();

  return value
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['emphasis'], undefined>}
 */
function checkEmphasis(context) {
  const marker = context.options.emphasis || '*';

  if (marker !== '*' && marker !== '_') {
    throw new Error(
      'Cannot serialize emphasis with `' +
        marker +
        '` for `options.emphasis`, expected `*`, or `_`'
    )
  }

  return marker
}

/**
 * @typedef {import('../types.js').Node} Node
 * @typedef {import('../types.js').Parent} Parent
 * @typedef {import('../types.js').SafeOptions} SafeOptions
 * @typedef {import('../types.js').Context} Context
 */

/**
 * @param {Parent} parent
 * @param {Context} context
 * @param {SafeOptions} safeOptions
 * @returns {string}
 */
function containerPhrasing(parent, context, safeOptions) {
  const indexStack = context.indexStack;
  const children = parent.children || [];
  /** @type {Array.<string>} */
  const results = [];
  let index = -1;
  let before = safeOptions.before;

  indexStack.push(-1);

  while (++index < children.length) {
    const child = children[index];
    /** @type {string} */
    let after;

    indexStack[indexStack.length - 1] = index;

    if (index + 1 < children.length) {
      // @ts-expect-error: hush, it’s actually a `zwitch`.
      let handle = context.handle.handlers[children[index + 1].type];
      if (handle && handle.peek) handle = handle.peek;
      after = handle
        ? handle(children[index + 1], parent, context, {
            before: '',
            after: ''
          }).charAt(0)
        : '';
    } else {
      after = safeOptions.after;
    }

    // In some cases, html (text) can be found in phrasing right after an eol.
    // When we’d serialize that, in most cases that would be seen as html
    // (flow).
    // As we can’t escape or so to prevent it from happening, we take a somewhat
    // reasonable approach: replace that eol with a space.
    // See: <https://github.com/syntax-tree/mdast-util-to-markdown/issues/15>
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
    }

    results.push(context.handle(child, parent, context, {before, after}));

    before = results[results.length - 1].slice(-1);
  }

  indexStack.pop();

  return results.join('')
}

/**
 * @typedef {import('mdast').Emphasis} Emphasis
 * @typedef {import('../types.js').Handle} Handle
 */

emphasis.peek = emphasisPeek;

// To do: there are cases where emphasis cannot “form” depending on the
// previous or next character of sequences.
// There’s no way around that though, except for injecting zero-width stuff.
// Do we need to safeguard against that?
/**
 * @type {Handle}
 * @param {Emphasis} node
 */
function emphasis(node, _, context) {
  const marker = checkEmphasis(context);
  const exit = context.enter('emphasis');
  const value = containerPhrasing(node, context, {
    before: marker,
    after: marker
  });
  exit();
  return marker + value + marker
}

/**
 * @type {Handle}
 * @param {Emphasis} _
 */
function emphasisPeek(_, _1, context) {
  return context.options.emphasis || '*'
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 *
 * @typedef {string} Type
 * @typedef {Object<string, unknown>} Props
 *
 * @typedef {null|undefined|Type|Props|TestFunctionAnything|Array.<Type|Props|TestFunctionAnything>} Test
 */

const convert =
  /**
   * @type {(
   *   (<T extends Node>(test: T['type']|Partial<T>|TestFunctionPredicate<T>) => AssertPredicate<T>) &
   *   ((test?: Test) => AssertAnything)
   * )}
   */
  (
    /**
     * Generate an assertion from a check.
     * @param {Test} [test]
     * When nullish, checks if `node` is a `Node`.
     * When `string`, works like passing `function (node) {return node.type === test}`.
     * When `function` checks if function passed the node is true.
     * When `object`, checks that all keys in test are in node, and that they have (strictly) equal values.
     * When `array`, checks any one of the subtests pass.
     * @returns {AssertAnything}
     */
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
/**
 * @param {Array.<Type|Props|TestFunctionAnything>} tests
 * @returns {AssertAnything}
 */
function anyFactory(tests) {
  /** @type {Array.<AssertAnything>} */
  const checks = [];
  let index = -1;

  while (++index < tests.length) {
    checks[index] = convert(tests[index]);
  }

  return castFactory(any)

  /**
   * @this {unknown}
   * @param {unknown[]} parameters
   * @returns {boolean}
   */
  function any(...parameters) {
    let index = -1;

    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) return true
    }

    return false
  }
}

/**
 * Utility to assert each property in `test` is represented in `node`, and each
 * values are strictly equal.
 *
 * @param {Props} check
 * @returns {AssertAnything}
 */
function propsFactory(check) {
  return castFactory(all)

  /**
   * @param {Node} node
   * @returns {boolean}
   */
  function all(node) {
    /** @type {string} */
    let key;

    for (key in check) {
      // @ts-expect-error: hush, it sure works as an index.
      if (node[key] !== check[key]) return false
    }

    return true
  }
}

/**
 * Utility to convert a string into a function which checks a given node’s type
 * for said string.
 *
 * @param {Type} check
 * @returns {AssertAnything}
 */
function typeFactory(check) {
  return castFactory(type)

  /**
   * @param {Node} node
   */
  function type(node) {
    return node && node.type === check
  }
}

/**
 * Utility to convert a string into a function which checks a given node’s type
 * for said string.
 * @param {TestFunctionAnything} check
 * @returns {AssertAnything}
 */
function castFactory(check) {
  return assertion

  /**
   * @this {unknown}
   * @param {Array.<unknown>} parameters
   * @returns {boolean}
   */
  function assertion(...parameters) {
    // @ts-expect-error: spreading is fine.
    return Boolean(check.call(this, ...parameters))
  }
}

// Utility to return true.
function ok() {
  return true
}

/**
 * @param {string} d
 * @returns {string}
 */
function color$1(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('unist-util-is').Test} Test
 * @typedef {import('./complex-types').Action} Action
 * @typedef {import('./complex-types').Index} Index
 * @typedef {import('./complex-types').ActionTuple} ActionTuple
 * @typedef {import('./complex-types').VisitorResult} VisitorResult
 * @typedef {import('./complex-types').Visitor} Visitor
 */

/**
 * Continue traversing as normal
 */
const CONTINUE$1 = true;
/**
 * Do not traverse this node’s children
 */
const SKIP$1 = 'skip';
/**
 * Stop traversing immediately
 */
const EXIT$1 = false;

/**
 * Visit children of tree which pass a test
 *
 * @param tree Abstract syntax tree to walk
 * @param test Test node, optional
 * @param visitor Function to run for each node
 * @param reverse Visit the tree in reverse order, defaults to false
 */
const visitParents$1 =
  /**
   * @type {(
   *   (<Tree extends Node, Check extends Test>(tree: Tree, test: Check, visitor: import('./complex-types').BuildVisitor<Tree, Check>, reverse?: boolean) => void) &
   *   (<Tree extends Node>(tree: Tree, visitor: import('./complex-types').BuildVisitor<Tree>, reverse?: boolean) => void)
   * )}
   */
  (
    /**
     * @param {Node} tree
     * @param {Test} test
     * @param {import('./complex-types').Visitor<Node>} visitor
     * @param {boolean} [reverse]
     */
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        // @ts-expect-error no visitor given, so `visitor` is test.
        visitor = test;
        test = null;
      }

      const is = convert(test);
      const step = reverse ? -1 : 1;

      factory(tree, null, [])();

      /**
       * @param {Node} node
       * @param {number?} index
       * @param {Array.<Parent>} parents
       */
      function factory(node, index, parents) {
        /** @type {Object.<string, unknown>} */
        // @ts-expect-error: hush
        const value = typeof node === 'object' && node !== null ? node : {};
        /** @type {string|undefined} */
        let name;

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
              color$1(value.type + (name ? '<' + name + '>' : '')) +
              ')'
          });
        }

        return visit

        function visit() {
          /** @type {ActionTuple} */
          let result = [];
          /** @type {ActionTuple} */
          let subresult;
          /** @type {number} */
          let offset;
          /** @type {Array.<Parent>} */
          let grandparents;

          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult$1(visitor(node, parents));

            if (result[0] === EXIT$1) {
              return result
            }
          }

          // @ts-expect-error looks like a parent.
          if (node.children && result[0] !== SKIP$1) {
            // @ts-expect-error looks like a parent.
            offset = (reverse ? node.children.length : -1) + step;
            // @ts-expect-error looks like a parent.
            grandparents = parents.concat(node);

            // @ts-expect-error looks like a parent.
            while (offset > -1 && offset < node.children.length) {
              // @ts-expect-error looks like a parent.
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

/**
 * @param {VisitorResult} value
 * @returns {ActionTuple}
 */
function toResult$1(value) {
  if (Array.isArray(value)) {
    return value
  }

  if (typeof value === 'number') {
    return [CONTINUE$1, value]
  }

  return [value]
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('unist-util-is').Test} Test
 * @typedef {import('unist-util-visit-parents').VisitorResult} VisitorResult
 * @typedef {import('./complex-types').Visitor} Visitor
 */

/**
 * Visit children of tree which pass a test
 *
 * @param tree Abstract syntax tree to walk
 * @param test Test, optional
 * @param visitor Function to run for each node
 * @param reverse Fisit the tree in reverse, defaults to false
 */
const visit$1 =
  /**
   * @type {(
   *   (<Tree extends Node, Check extends Test>(tree: Tree, test: Check, visitor: import('./complex-types').BuildVisitor<Tree, Check>, reverse?: boolean) => void) &
   *   (<Tree extends Node>(tree: Tree, visitor: import('./complex-types').BuildVisitor<Tree>, reverse?: boolean) => void)
   * )}
   */
  (
    /**
     * @param {Node} tree
     * @param {Test} test
     * @param {import('./complex-types').Visitor} visitor
     * @param {boolean} [reverse]
     */
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }

      visitParents$1(tree, test, overload, reverse);

      /**
       * @param {Node} node
       * @param {Array.<Parent>} parents
       */
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
 * @typedef {import('mdast').Heading} Heading
 * @typedef {import('../types.js').Context} Context
 */

/**
 * @param {Heading} node
 * @param {Context} context
 * @returns {boolean}
 */
function formatHeadingAsSetext(node, context) {
  let literalWithBreak = false;

  // Look for literals with a line break.
  // Note that this also
  visit$1(node, (node) => {
    if (
      ('value' in node && /\r?\n|\r/.test(node.value)) ||
      node.type === 'break'
    ) {
      literalWithBreak = true;
      return EXIT$1
    }
  });

  return Boolean(
    (!node.depth || node.depth < 3) &&
      toString(node) &&
      (context.options.setext || literalWithBreak)
  )
}

/**
 * @typedef {import('mdast').Heading} Heading
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('../types.js').Exit} Exit
 */

/**
 * @type {Handle}
 * @param {Heading} node
 */
function heading(node, _, context) {
  const rank = Math.max(Math.min(6, node.depth || 1), 1);

  if (formatHeadingAsSetext(node, context)) {
    const exit = context.enter('headingSetext');
    const subexit = context.enter('phrasing');
    const value = containerPhrasing(node, context, {before: '\n', after: '\n'});
    subexit();
    exit();

    return (
      value +
      '\n' +
      (rank === 1 ? '=' : '-').repeat(
        // The whole size…
        value.length -
          // Minus the position of the character after the last EOL (or
          // 0 if there is none)…
          (Math.max(value.lastIndexOf('\r'), value.lastIndexOf('\n')) + 1)
      )
    )
  }

  const sequence = '#'.repeat(rank);
  const exit = context.enter('headingAtx');
  const subexit = context.enter('phrasing');
  let value = containerPhrasing(node, context, {before: '# ', after: '\n'});

  if (/^[\t ]/.test(value)) {
    value =
      '&#x' +
      value.charCodeAt(0).toString(16).toUpperCase() +
      ';' +
      value.slice(1);
  }

  value = value ? sequence + ' ' + value : sequence;

  if (context.options.closeAtx) {
    value += ' ' + sequence;
  }

  subexit();
  exit();

  return value
}

/**
 * @typedef {import('mdast').HTML} HTML
 * @typedef {import('../types.js').Handle} Handle
 */

html.peek = htmlPeek;

/**
 * @type {Handle}
 * @param {HTML} node
 */
function html(node) {
  return node.value || ''
}

/**
 * @type {Handle}
 */
function htmlPeek() {
  return '<'
}

/**
 * @typedef {import('mdast').Image} Image
 * @typedef {import('../types.js').Handle} Handle
 */

image.peek = imagePeek;

/**
 * @type {Handle}
 * @param {Image} node
 */
function image(node, _, context) {
  const quote = checkQuote(context);
  const suffix = quote === '"' ? 'Quote' : 'Apostrophe';
  const exit = context.enter('image');
  let subexit = context.enter('label');
  let value = '![' + safe(context, node.alt, {before: '[', after: ']'}) + '](';

  subexit();

  if (
    // If there’s no url but there is a title…
    (!node.url && node.title) ||
    // Or if there’s markdown whitespace or an eol, enclose.
    /[ \t\r\n]/.test(node.url)
  ) {
    subexit = context.enter('destinationLiteral');
    value += '<' + safe(context, node.url, {before: '<', after: '>'}) + '>';
  } else {
    // No whitespace, raw is prettier.
    subexit = context.enter('destinationRaw');
    value += safe(context, node.url, {
      before: '(',
      after: node.title ? ' ' : ')'
    });
  }

  subexit();

  if (node.title) {
    subexit = context.enter('title' + suffix);
    value +=
      ' ' +
      quote +
      safe(context, node.title, {before: quote, after: quote}) +
      quote;
    subexit();
  }

  value += ')';
  exit();

  return value
}

/**
 * @type {Handle}
 */
function imagePeek() {
  return '!'
}

/**
 * @typedef {import('mdast').ImageReference} ImageReference
 * @typedef {import('../types.js').Handle} Handle
 */

imageReference.peek = imageReferencePeek;

/**
 * @type {Handle}
 * @param {ImageReference} node
 */
function imageReference(node, _, context) {
  const type = node.referenceType;
  const exit = context.enter('imageReference');
  let subexit = context.enter('label');
  const alt = safe(context, node.alt, {before: '[', after: ']'});
  let value = '![' + alt + ']';

  subexit();
  // Hide the fact that we’re in phrasing, because escapes don’t work.
  const stack = context.stack;
  context.stack = [];
  subexit = context.enter('reference');
  const reference = safe(context, association(node), {before: '[', after: ']'});
  subexit();
  context.stack = stack;
  exit();

  if (type === 'full' || !alt || alt !== reference) {
    value += '[' + reference + ']';
  } else if (type !== 'shortcut') {
    value += '[]';
  }

  return value
}

/**
 * @type {Handle}
 */
function imageReferencePeek() {
  return '!'
}

/**
 * @typedef {import('mdast').InlineCode} InlineCode
 * @typedef {import('../types.js').Handle} Handle
 */

inlineCode.peek = inlineCodePeek;

/**
 * @type {Handle}
 * @param {InlineCode} node
 */
function inlineCode(node, _, context) {
  let value = node.value || '';
  let sequence = '`';
  let index = -1;

  // If there is a single grave accent on its own in the code, use a fence of
  // two.
  // If there are two in a row, use one.
  while (new RegExp('(^|[^`])' + sequence + '([^`]|$)').test(value)) {
    sequence += '`';
  }

  // If this is not just spaces or eols (tabs don’t count), and either the
  // first or last character are a space, eol, or tick, then pad with spaces.
  if (
    /[^ \r\n]/.test(value) &&
    ((/^[ \r\n]/.test(value) && /[ \r\n]$/.test(value)) || /^`|`$/.test(value))
  ) {
    value = ' ' + value + ' ';
  }

  // We have a potential problem: certain characters after eols could result in
  // blocks being seen.
  // For example, if someone injected the string `'\n# b'`, then that would
  // result in an ATX heading.
  // We can’t escape characters in `inlineCode`, but because eols are
  // transformed to spaces when going from markdown to HTML anyway, we can swap
  // them out.
  while (++index < context.unsafe.length) {
    const pattern = context.unsafe[index];
    const expression = patternCompile(pattern);
    /** @type {RegExpExecArray|null} */
    let match;

    // Only look for `atBreak`s.
    // Btw: note that `atBreak` patterns will always start the regex at LF or
    // CR.
    if (!pattern.atBreak) continue

    while ((match = expression.exec(value))) {
      let position = match.index;

      // Support CRLF (patterns only look for one of the characters).
      if (
        value.charCodeAt(position) === 10 /* `\n` */ &&
        value.charCodeAt(position - 1) === 13 /* `\r` */
      ) {
        position--;
      }

      value = value.slice(0, position) + ' ' + value.slice(match.index + 1);
    }
  }

  return sequence + value + sequence
}

/**
 * @type {Handle}
 */
function inlineCodePeek() {
  return '`'
}

/**
 * @typedef {import('mdast').Link} Link
 * @typedef {import('../types.js').Context} Context
 */

/**
 * @param {Link} node
 * @param {Context} context
 * @returns {boolean}
 */
function formatLinkAsAutolink(node, context) {
  const raw = toString(node);

  return Boolean(
    !context.options.resourceLink &&
      // If there’s a url…
      node.url &&
      // And there’s a no title…
      !node.title &&
      // And the content of `node` is a single text node…
      node.children &&
      node.children.length === 1 &&
      node.children[0].type === 'text' &&
      // And if the url is the same as the content…
      (raw === node.url || 'mailto:' + raw === node.url) &&
      // And that starts w/ a protocol…
      /^[a-z][a-z+.-]+:/i.test(node.url) &&
      // And that doesn’t contain ASCII control codes (character escapes and
      // references don’t work) or angle brackets…
      !/[\0- <>\u007F]/.test(node.url)
  )
}

/**
 * @typedef {import('mdast').Link} Link
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('../types.js').Exit} Exit
 */

link.peek = linkPeek;

/**
 * @type {Handle}
 * @param {Link} node
 */
function link(node, _, context) {
  const quote = checkQuote(context);
  const suffix = quote === '"' ? 'Quote' : 'Apostrophe';
  /** @type {Exit} */
  let exit;
  /** @type {Exit} */
  let subexit;
  /** @type {string} */
  let value;

  if (formatLinkAsAutolink(node, context)) {
    // Hide the fact that we’re in phrasing, because escapes don’t work.
    const stack = context.stack;
    context.stack = [];
    exit = context.enter('autolink');
    value =
      '<' + containerPhrasing(node, context, {before: '<', after: '>'}) + '>';
    exit();
    context.stack = stack;
    return value
  }

  exit = context.enter('link');
  subexit = context.enter('label');
  value =
    '[' + containerPhrasing(node, context, {before: '[', after: ']'}) + '](';
  subexit();

  if (
    // If there’s no url but there is a title…
    (!node.url && node.title) ||
    // Or if there’s markdown whitespace or an eol, enclose.
    /[ \t\r\n]/.test(node.url)
  ) {
    subexit = context.enter('destinationLiteral');
    value += '<' + safe(context, node.url, {before: '<', after: '>'}) + '>';
  } else {
    // No whitespace, raw is prettier.
    subexit = context.enter('destinationRaw');
    value += safe(context, node.url, {
      before: '(',
      after: node.title ? ' ' : ')'
    });
  }

  subexit();

  if (node.title) {
    subexit = context.enter('title' + suffix);
    value +=
      ' ' +
      quote +
      safe(context, node.title, {before: quote, after: quote}) +
      quote;
    subexit();
  }

  value += ')';

  exit();
  return value
}

/**
 * @type {Handle}
 * @param {Link} node
 */
function linkPeek(node, _, context) {
  return formatLinkAsAutolink(node, context) ? '<' : '['
}

/**
 * @typedef {import('mdast').LinkReference} LinkReference
 * @typedef {import('../types.js').Handle} Handle
 */

linkReference.peek = linkReferencePeek;

/**
 * @type {Handle}
 * @param {LinkReference} node
 */
function linkReference(node, _, context) {
  const type = node.referenceType;
  const exit = context.enter('linkReference');
  let subexit = context.enter('label');
  const text = containerPhrasing(node, context, {before: '[', after: ']'});
  let value = '[' + text + ']';

  subexit();
  // Hide the fact that we’re in phrasing, because escapes don’t work.
  const stack = context.stack;
  context.stack = [];
  subexit = context.enter('reference');
  const reference = safe(context, association(node), {before: '[', after: ']'});
  subexit();
  context.stack = stack;
  exit();

  if (type === 'full' || !text || text !== reference) {
    value += '[' + reference + ']';
  } else if (type !== 'shortcut') {
    value += '[]';
  }

  return value
}

/**
 * @type {Handle}
 */
function linkReferencePeek() {
  return '['
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['bullet'], undefined>}
 */
function checkBullet(context) {
  const marker = context.options.bullet || '*';

  if (marker !== '*' && marker !== '+' && marker !== '-') {
    throw new Error(
      'Cannot serialize items with `' +
        marker +
        '` for `options.bullet`, expected `*`, `+`, or `-`'
    )
  }

  return marker
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['bullet'], undefined>}
 */
function checkBulletOther(context) {
  const bullet = checkBullet(context);
  const bulletOther = context.options.bulletOther;

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

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['bulletOrdered'], undefined>}
 */
function checkBulletOrdered(context) {
  const marker = context.options.bulletOrdered || '.';

  if (marker !== '.' && marker !== ')') {
    throw new Error(
      'Cannot serialize items with `' +
        marker +
        '` for `options.bulletOrdered`, expected `.` or `)`'
    )
  }

  return marker
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['bulletOrdered'], undefined>}
 */
function checkBulletOrderedOther(context) {
  const bulletOrdered = checkBulletOrdered(context);
  const bulletOrderedOther = context.options.bulletOrderedOther;

  if (!bulletOrderedOther) {
    return bulletOrdered === '.' ? ')' : '.'
  }

  if (bulletOrderedOther !== '.' && bulletOrderedOther !== ')') {
    throw new Error(
      'Cannot serialize items with `' +
        bulletOrderedOther +
        '` for `options.bulletOrderedOther`, expected `*`, `+`, or `-`'
    )
  }

  if (bulletOrderedOther === bulletOrdered) {
    throw new Error(
      'Expected `bulletOrdered` (`' +
        bulletOrdered +
        '`) and `bulletOrderedOther` (`' +
        bulletOrderedOther +
        '`) to be different'
    )
  }

  return bulletOrderedOther
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['rule'], undefined>}
 */
function checkRule(context) {
  const marker = context.options.rule || '*';

  if (marker !== '*' && marker !== '-' && marker !== '_') {
    throw new Error(
      'Cannot serialize rules with `' +
        marker +
        '` for `options.rule`, expected `*`, `-`, or `_`'
    )
  }

  return marker
}

/**
 * @typedef {import('mdast').List} List
 * @typedef {import('../types.js').Handle} Handle
 */

/**
 * @type {Handle}
 * @param {List} node
 */
function list(node, parent, context) {
  const exit = context.enter('list');
  const bulletCurrent = context.bulletCurrent;
  /** @type {string} */
  let bullet = node.ordered ? checkBulletOrdered(context) : checkBullet(context);
  /** @type {string} */
  const bulletOther = node.ordered
    ? checkBulletOrderedOther(context)
    : checkBulletOther(context);
  const bulletLastUsed = context.bulletLastUsed;
  let useDifferentMarker = false;

  if (
    parent &&
    // Explicit `other` set.
    (node.ordered
      ? context.options.bulletOrderedOther
      : context.options.bulletOther) &&
    bulletLastUsed &&
    bullet === bulletLastUsed
  ) {
    useDifferentMarker = true;
  }

  if (!node.ordered) {
    const firstListItem = node.children ? node.children[0] : undefined;

    // If there’s an empty first list item directly in two list items,
    // we have to use a different bullet:
    //
    // ```markdown
    // * - *
    // ```
    //
    // …because otherwise it would become one big thematic break.
    if (
      // Bullet could be used as a thematic break marker:
      (bullet === '*' || bullet === '-') &&
      // Empty first list item:
      firstListItem &&
      (!firstListItem.children || !firstListItem.children[0]) &&
      // Directly in two other list items:
      context.stack[context.stack.length - 1] === 'list' &&
      context.stack[context.stack.length - 2] === 'listItem' &&
      context.stack[context.stack.length - 3] === 'list' &&
      context.stack[context.stack.length - 4] === 'listItem' &&
      // That are each the first child.
      context.indexStack[context.indexStack.length - 1] === 0 &&
      context.indexStack[context.indexStack.length - 2] === 0 &&
      context.indexStack[context.indexStack.length - 3] === 0 &&
      context.indexStack[context.indexStack.length - 4] === 0
    ) {
      useDifferentMarker = true;
    }

    // If there’s a thematic break at the start of the first list item,
    // we have to use a different bullet:
    //
    // ```markdown
    // * ---
    // ```
    //
    // …because otherwise it would become one big thematic break.
    if (checkRule(context) === bullet && firstListItem) {
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

  context.bulletCurrent = bullet;
  const value = containerFlow(node, context);
  context.bulletLastUsed = bullet;
  context.bulletCurrent = bulletCurrent;
  exit();
  return value
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['listItemIndent'], undefined>}
 */
function checkListItemIndent(context) {
  const style = context.options.listItemIndent || 'tab';

  // To do: remove in a major.
  // @ts-expect-error: deprecated.
  if (style === 1 || style === '1') {
    return 'one'
  }

  if (style !== 'tab' && style !== 'one' && style !== 'mixed') {
    throw new Error(
      'Cannot serialize items with `' +
        style +
        '` for `options.listItemIndent`, expected `tab`, `one`, or `mixed`'
    )
  }

  return style
}

/**
 * @typedef {import('mdast').ListItem} ListItem
 * @typedef {import('mdast').List} List
 * @typedef {import('../util/indent-lines.js').Map} Map
 * @typedef {import('../types.js').Options} Options
 * @typedef {import('../types.js').Handle} Handle
 */

/**
 * @type {Handle}
 * @param {ListItem} node
 */
function listItem(node, parent, context) {
  const listItemIndent = checkListItemIndent(context);
  let bullet = context.bulletCurrent || checkBullet(context);

  // Add the marker value for ordered lists.
  if (parent && parent.type === 'list' && parent.ordered) {
    bullet =
      (typeof parent.start === 'number' && parent.start > -1
        ? parent.start
        : 1) +
      (context.options.incrementListMarker === false
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

  const exit = context.enter('listItem');
  const value = indentLines(containerFlow(node, context), map);
  exit();

  return value

  /** @type {Map} */
  function map(line, index, blank) {
    if (index) {
      return (blank ? '' : ' '.repeat(size)) + line
    }

    return (blank ? bullet : bullet + ' '.repeat(size - bullet.length)) + line
  }
}

/**
 * @typedef {import('mdast').Paragraph} Paragraph
 * @typedef {import('../types.js').Handle} Handle
 */

/**
 * @type {Handle}
 * @param {Paragraph} node
 */
function paragraph(node, _, context) {
  const exit = context.enter('paragraph');
  const subexit = context.enter('phrasing');
  const value = containerPhrasing(node, context, {before: '\n', after: '\n'});
  subexit();
  exit();
  return value
}

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('../types.js').Handle} Handle
 */

/**
 * @type {Handle}
 * @param {Root} node
 */
function root(node, _, context) {
  return containerFlow(node, context)
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['strong'], undefined>}
 */
function checkStrong(context) {
  const marker = context.options.strong || '*';

  if (marker !== '*' && marker !== '_') {
    throw new Error(
      'Cannot serialize strong with `' +
        marker +
        '` for `options.strong`, expected `*`, or `_`'
    )
  }

  return marker
}

/**
 * @typedef {import('mdast').Strong} Strong
 * @typedef {import('../types.js').Handle} Handle
 */

strong.peek = strongPeek;

// To do: there are cases where emphasis cannot “form” depending on the
// previous or next character of sequences.
// There’s no way around that though, except for injecting zero-width stuff.
// Do we need to safeguard against that?
/**
 * @type {Handle}
 * @param {Strong} node
 */
function strong(node, _, context) {
  const marker = checkStrong(context);
  const exit = context.enter('strong');
  const value = containerPhrasing(node, context, {
    before: marker,
    after: marker
  });
  exit();
  return marker + marker + value + marker + marker
}

/**
 * @type {Handle}
 * @param {Strong} _
 */
function strongPeek(_, _1, context) {
  return context.options.strong || '*'
}

/**
 * @typedef {import('mdast').Text} Text
 * @typedef {import('../types.js').Handle} Handle
 */

/**
 * @type {Handle}
 * @param {Text} node
 */
function text$1(node, _, context, safeOptions) {
  return safe(context, node.value, safeOptions)
}

/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */

/**
 * @param {Context} context
 * @returns {Exclude<Options['ruleRepetition'], undefined>}
 */
function checkRuleRepetition(context) {
  const repetition = context.options.ruleRepetition || 3;

  if (repetition < 3) {
    throw new Error(
      'Cannot serialize rules with repetition `' +
        repetition +
        '` for `options.ruleRepetition`, expected `3` or more'
    )
  }

  return repetition
}

/**
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('mdast').ThematicBreak} ThematicBreak
 */

/**
 * @type {Handle}
 * @param {ThematicBreak} _
 */
function thematicBreak(_, _1, context) {
  const value = (
    checkRule(context) + (context.options.ruleSpaces ? ' ' : '')
  ).repeat(checkRuleRepetition(context));

  return context.options.ruleSpaces ? value.slice(0, -1) : value
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

/**
 * @typedef {import('./types.js').Join} Join
 */

/** @type {Array.<Join>} */
const join = [joinDefaults];

/** @type {Join} */
function joinDefaults(left, right, parent, context) {
  // Indented code after list or another indented code.
  if (
    right.type === 'code' &&
    formatCodeAsIndented(right, context) &&
    (left.type === 'list' ||
      (left.type === right.type && formatCodeAsIndented(left, context)))
  ) {
    return false
  }

  // Two lists with the same marker.
  if (
    left.type === 'list' &&
    left.type === right.type &&
    Boolean(left.ordered) === Boolean(right.ordered) &&
    !(left.ordered
      ? context.options.bulletOrderedOther
      : context.options.bulletOther)
  ) {
    return false
  }

  // Join children of a list or an item.
  // In which case, `parent` has a `spread` field.
  if ('spread' in parent && typeof parent.spread === 'boolean') {
    if (
      left.type === 'paragraph' &&
      // Two paragraphs.
      (left.type === right.type ||
        right.type === 'definition' ||
        // Paragraph followed by a setext heading.
        (right.type === 'heading' && formatHeadingAsSetext(right, context)))
    ) {
      return
    }

    return parent.spread ? 1 : 0
  }
}

/**
 * @typedef {import('./types.js').Unsafe} Unsafe
 */

/**
 * List of constructs that occur in phrasing (paragraphs, headings), but cannot
 * contain things like attention (emphasis, strong), images, or links.
 * So they sort of cancel each other out.
 * Note: could use a better name.
 */
const fullPhrasingSpans = [
  'autolink',
  'destinationLiteral',
  'destinationRaw',
  'reference',
  'titleQuote',
  'titleApostrophe'
];

/** @type {Array.<Unsafe>} */
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
  // An exclamation mark can start an image, if it is followed by a link or
  // a link reference.
  {
    character: '!',
    after: '\\[',
    inConstruct: 'phrasing',
    notInConstruct: fullPhrasingSpans
  },
  // A quote can break out of a title.
  {character: '"', inConstruct: 'titleQuote'},
  // A number sign could start an ATX heading if it starts a line.
  {atBreak: true, character: '#'},
  {character: '#', inConstruct: 'headingAtx', after: '(?:[\r\n]|$)'},
  // Dollar sign and percentage are not used in markdown.
  // An ampersand could start a character reference.
  {character: '&', after: '[#A-Za-z]', inConstruct: 'phrasing'},
  // An apostrophe can break out of a title.
  {character: "'", inConstruct: 'titleApostrophe'},
  // A left paren could break out of a destination raw.
  {character: '(', inConstruct: 'destinationRaw'},
  // A left paren followed by `]` could make something into a link or image.
  {
    before: '\\]',
    character: '(',
    inConstruct: 'phrasing',
    notInConstruct: fullPhrasingSpans
  },
  // A right paren could start a list item or break out of a destination
  // raw.
  {atBreak: true, before: '\\d+', character: ')'},
  {character: ')', inConstruct: 'destinationRaw'},
  // An asterisk can start thematic breaks, list items, emphasis, strong.
  {atBreak: true, character: '*'},
  {character: '*', inConstruct: 'phrasing', notInConstruct: fullPhrasingSpans},
  // A plus sign could start a list item.
  {atBreak: true, character: '+'},
  // A dash can start thematic breaks, list items, and setext heading
  // underlines.
  {atBreak: true, character: '-'},
  // A dot could start a list item.
  {atBreak: true, before: '\\d+', character: '.', after: '(?:[ \t\r\n]|$)'},
  // Slash, colon, and semicolon are not used in markdown for constructs.
  // A less than can start html (flow or text) or an autolink.
  // HTML could start with an exclamation mark (declaration, cdata, comment),
  // slash (closing tag), question mark (instruction), or a letter (tag).
  // An autolink also starts with a letter.
  // Finally, it could break out of a destination literal.
  {atBreak: true, character: '<', after: '[!/?A-Za-z]'},
  {
    character: '<',
    after: '[!/?A-Za-z]',
    inConstruct: 'phrasing',
    notInConstruct: fullPhrasingSpans
  },
  {character: '<', inConstruct: 'destinationLiteral'},
  // An equals to can start setext heading underlines.
  {atBreak: true, character: '='},
  // A greater than can start block quotes and it can break out of a
  // destination literal.
  {atBreak: true, character: '>'},
  {character: '>', inConstruct: 'destinationLiteral'},
  // Question mark and at sign are not used in markdown for constructs.
  // A left bracket can start definitions, references, labels,
  {atBreak: true, character: '['},
  {character: '[', inConstruct: 'phrasing', notInConstruct: fullPhrasingSpans},
  {character: '[', inConstruct: ['label', 'reference']},
  // A backslash can start an escape (when followed by punctuation) or a
  // hard break (when followed by an eol).
  // Note: typical escapes are handled in `safe`!
  {character: '\\', after: '[\\r\\n]', inConstruct: 'phrasing'},
  // A right bracket can exit labels.
  {character: ']', inConstruct: ['label', 'reference']},
  // Caret is not used in markdown for constructs.
  // An underscore can start emphasis, strong, or a thematic break.
  {atBreak: true, character: '_'},
  {character: '_', inConstruct: 'phrasing', notInConstruct: fullPhrasingSpans},
  // A grave accent can start code (fenced or text), or it can break out of
  // a grave accent code fence.
  {atBreak: true, character: '`'},
  {
    character: '`',
    inConstruct: ['codeFencedLangGraveAccent', 'codeFencedMetaGraveAccent']
  },
  {character: '`', inConstruct: 'phrasing', notInConstruct: fullPhrasingSpans},
  // Left brace, vertical bar, right brace are not used in markdown for
  // constructs.
  // A tilde can start code (fenced).
  {atBreak: true, character: '~'}
];

/**
 * @typedef {import('./types.js').Node} Node
 * @typedef {import('./types.js').Options} Options
 * @typedef {import('./types.js').Context} Context
 * @typedef {import('./types.js').Handle} Handle
 * @typedef {import('./types.js').Join} Join
 * @typedef {import('./types.js').Unsafe} Unsafe
 */

/**
 * @param {Node} tree
 * @param {Options} [options]
 * @returns {string}
 */
function toMarkdown(tree, options = {}) {
  /** @type {Context} */
  // @ts-expect-error: we’ll add `handle` later.
  const context = {
    enter,
    stack: [],
    unsafe: [],
    join: [],
    handlers: {},
    options: {},
    indexStack: []
  };

  configure(context, {unsafe, join, handlers: handle});
  configure(context, options);

  if (context.options.tightDefinitions) {
    configure(context, {join: [joinDefinition]});
  }

  /** @type {Handle} */
  context.handle = zwitch('type', {
    invalid,
    // @ts-expect-error: hush.
    unknown,
    // @ts-expect-error: hush.
    handlers: context.handlers
  });

  let result = context.handle(tree, null, context, {before: '\n', after: '\n'});

  if (
    result &&
    result.charCodeAt(result.length - 1) !== 10 &&
    result.charCodeAt(result.length - 1) !== 13
  ) {
    result += '\n';
  }

  return result

  /** @type {Context['enter']} */
  function enter(name) {
    context.stack.push(name);
    return exit

    function exit() {
      context.stack.pop();
    }
  }
}

/**
 * @type {Handle}
 * @param {unknown} value
 */
function invalid(value) {
  throw new Error('Cannot handle value `' + value + '`, expected node')
}

/**
 * @type {Handle}
 * @param {Node} node
 */
function unknown(node) {
  throw new Error('Cannot handle unknown node `' + node.type + '`')
}

/** @type {Join} */
function joinDefinition(left, right) {
  // No blank line between adjacent definitions.
  if (left.type === 'definition' && left.type === right.type) {
    return 0
  }
}

/**
 * @typedef {import('mdast').Root|import('mdast').Content} Node
 * @typedef {import('mdast-util-to-markdown').Options} ToMarkdownOptions
 * @typedef {Omit<ToMarkdownOptions, 'extensions'>} Options
 */

/** @type {import('unified').Plugin<[Options]|void[], Node, string>} */
function remarkStringify(options) {
  /** @type {import('unified').CompilerFunction<Node, string>} */
  const compiler = (tree) => {
    // Assume options.
    const settings = /** @type {Options} */ (this.data('settings'));

    return toMarkdown(
      tree,
      Object.assign({}, settings, options, {
        // Note: this option is not in the readme.
        // The goal is for it to be set by plugins on `data` instead of being
        // passed by users.
        extensions:
          /** @type {ToMarkdownOptions['extensions']} */ (
            this.data('toMarkdownExtensions')
          ) || []
      })
    )
  };

  Object.assign(this, {Compiler: compiler});
}

/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').ConstructRecord} ConstructRecord
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Previous} Previous
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').Code} Code
 */
const www = {
  tokenize: tokenizeWww,
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
const punctuation = {
  tokenize: tokenizePunctuation,
  partial: true
};
const namedCharacterReference = {
  tokenize: tokenizeNamedCharacterReference,
  partial: true
};
const wwwAutolink = {
  tokenize: tokenizeWwwAutolink,
  previous: previousWww
};
const httpAutolink = {
  tokenize: tokenizeHttpAutolink,
  previous: previousHttp
};
const emailAutolink = {
  tokenize: tokenizeEmailAutolink,
  previous: previousEmail
};
/** @type {ConstructRecord} */

const text = {};
/** @type {Extension} */

const gfmAutolinkLiteral = {
  text
};
let code = 48; // Add alphanumerics.

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
text[72] = [emailAutolink, httpAutolink];
text[104] = [emailAutolink, httpAutolink];
text[87] = [emailAutolink, wwwAutolink];
text[119] = [emailAutolink, wwwAutolink];
/** @type {Tokenizer} */

function tokenizeEmailAutolink(effects, ok, nok) {
  const self = this;
  /** @type {boolean} */

  let hasDot;
  /** @type {boolean|undefined} */

  let hasDigitInLastSegment;
  return start
  /** @type {State} */

  function start(code) {
    if (
      !gfmAtext(code) ||
      !previousEmail(self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }

    effects.enter('literalAutolink');
    effects.enter('literalAutolinkEmail');
    return atext(code)
  }
  /** @type {State} */

  function atext(code) {
    if (gfmAtext(code)) {
      effects.consume(code);
      return atext
    }

    if (code === 64) {
      effects.consume(code);
      return label
    }

    return nok(code)
  }
  /** @type {State} */

  function label(code) {
    if (code === 46) {
      return effects.check(punctuation, done, dotContinuation)(code)
    }

    if (code === 45 || code === 95) {
      return effects.check(punctuation, nok, dashOrUnderscoreContinuation)(code)
    }

    if (asciiAlphanumeric(code)) {
      if (!hasDigitInLastSegment && asciiDigit(code)) {
        hasDigitInLastSegment = true;
      }

      effects.consume(code);
      return label
    }

    return done(code)
  }
  /** @type {State} */

  function dotContinuation(code) {
    effects.consume(code);
    hasDot = true;
    hasDigitInLastSegment = undefined;
    return label
  }
  /** @type {State} */

  function dashOrUnderscoreContinuation(code) {
    effects.consume(code);
    return afterDashOrUnderscore
  }
  /** @type {State} */

  function afterDashOrUnderscore(code) {
    if (code === 46) {
      return effects.check(punctuation, nok, dotContinuation)(code)
    }

    return label(code)
  }
  /** @type {State} */

  function done(code) {
    if (hasDot && !hasDigitInLastSegment) {
      effects.exit('literalAutolinkEmail');
      effects.exit('literalAutolink');
      return ok(code)
    }

    return nok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeWwwAutolink(effects, ok, nok) {
  const self = this;
  return start
  /** @type {State} */

  function start(code) {
    if (
      (code !== 87 && code !== 119) ||
      !previousWww(self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }

    effects.enter('literalAutolink');
    effects.enter('literalAutolinkWww'); // For `www.` we check instead of attempt, because when it matches, GH
    // treats it as part of a domain (yes, it says a valid domain must come
    // after `www.`, but that’s not how it’s implemented by them).

    return effects.check(
      www,
      effects.attempt(domain, effects.attempt(path, done), nok),
      nok
    )(code)
  }
  /** @type {State} */

  function done(code) {
    effects.exit('literalAutolinkWww');
    effects.exit('literalAutolink');
    return ok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeHttpAutolink(effects, ok, nok) {
  const self = this;
  return start
  /** @type {State} */

  function start(code) {
    if (
      (code !== 72 && code !== 104) ||
      !previousHttp(self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }

    effects.enter('literalAutolink');
    effects.enter('literalAutolinkHttp');
    effects.consume(code);
    return t1
  }
  /** @type {State} */

  function t1(code) {
    if (code === 84 || code === 116) {
      effects.consume(code);
      return t2
    }

    return nok(code)
  }
  /** @type {State} */

  function t2(code) {
    if (code === 84 || code === 116) {
      effects.consume(code);
      return p
    }

    return nok(code)
  }
  /** @type {State} */

  function p(code) {
    if (code === 80 || code === 112) {
      effects.consume(code);
      return s
    }

    return nok(code)
  }
  /** @type {State} */

  function s(code) {
    if (code === 83 || code === 115) {
      effects.consume(code);
      return colon
    }

    return colon(code)
  }
  /** @type {State} */

  function colon(code) {
    if (code === 58) {
      effects.consume(code);
      return slash1
    }

    return nok(code)
  }
  /** @type {State} */

  function slash1(code) {
    if (code === 47) {
      effects.consume(code);
      return slash2
    }

    return nok(code)
  }
  /** @type {State} */

  function slash2(code) {
    if (code === 47) {
      effects.consume(code);
      return after
    }

    return nok(code)
  }
  /** @type {State} */

  function after(code) {
    return code === null ||
      asciiControl(code) ||
      unicodeWhitespace(code) ||
      unicodePunctuation(code)
      ? nok(code)
      : effects.attempt(domain, effects.attempt(path, done), nok)(code)
  }
  /** @type {State} */

  function done(code) {
    effects.exit('literalAutolinkHttp');
    effects.exit('literalAutolink');
    return ok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeWww(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.consume(code);
    return w2
  }
  /** @type {State} */

  function w2(code) {
    if (code === 87 || code === 119) {
      effects.consume(code);
      return w3
    }

    return nok(code)
  }
  /** @type {State} */

  function w3(code) {
    if (code === 87 || code === 119) {
      effects.consume(code);
      return dot
    }

    return nok(code)
  }
  /** @type {State} */

  function dot(code) {
    if (code === 46) {
      effects.consume(code);
      return after
    }

    return nok(code)
  }
  /** @type {State} */

  function after(code) {
    return code === null || markdownLineEnding(code) ? nok(code) : ok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeDomain(effects, ok, nok) {
  /** @type {boolean|undefined} */
  let hasUnderscoreInLastSegment;
  /** @type {boolean|undefined} */

  let hasUnderscoreInLastLastSegment;
  return domain
  /** @type {State} */

  function domain(code) {
    if (code === 38) {
      return effects.check(
        namedCharacterReference,
        done,
        punctuationContinuation
      )(code)
    }

    if (code === 46 || code === 95) {
      return effects.check(punctuation, done, punctuationContinuation)(code)
    } // GH documents that only alphanumerics (other than `-`, `.`, and `_`) can
    // occur, which sounds like ASCII only, but they also support `www.點看.com`,
    // so that’s Unicode.
    // Instead of some new production for Unicode alphanumerics, markdown
    // already has that for Unicode punctuation and whitespace, so use those.

    if (
      code === null ||
      asciiControl(code) ||
      unicodeWhitespace(code) ||
      (code !== 45 && unicodePunctuation(code))
    ) {
      return done(code)
    }

    effects.consume(code);
    return domain
  }
  /** @type {State} */

  function punctuationContinuation(code) {
    if (code === 46) {
      hasUnderscoreInLastLastSegment = hasUnderscoreInLastSegment;
      hasUnderscoreInLastSegment = undefined;
      effects.consume(code);
      return domain
    }

    if (code === 95) hasUnderscoreInLastSegment = true;
    effects.consume(code);
    return domain
  }
  /** @type {State} */

  function done(code) {
    if (!hasUnderscoreInLastLastSegment && !hasUnderscoreInLastSegment) {
      return ok(code)
    }

    return nok(code)
  }
}
/** @type {Tokenizer} */

function tokenizePath(effects, ok) {
  let balance = 0;
  return inPath
  /** @type {State} */

  function inPath(code) {
    if (code === 38) {
      return effects.check(
        namedCharacterReference,
        ok,
        continuedPunctuation
      )(code)
    }

    if (code === 40) {
      balance++;
    }

    if (code === 41) {
      return effects.check(
        punctuation,
        parenAtPathEnd,
        continuedPunctuation
      )(code)
    }

    if (pathEnd(code)) {
      return ok(code)
    }

    if (trailingPunctuation(code)) {
      return effects.check(punctuation, ok, continuedPunctuation)(code)
    }

    effects.consume(code);
    return inPath
  }
  /** @type {State} */

  function continuedPunctuation(code) {
    effects.consume(code);
    return inPath
  }
  /** @type {State} */

  function parenAtPathEnd(code) {
    balance--;
    return balance < 0 ? ok(code) : continuedPunctuation(code)
  }
}
/** @type {Tokenizer} */

function tokenizeNamedCharacterReference(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.consume(code);
    return inside
  }
  /** @type {State} */

  function inside(code) {
    if (asciiAlpha(code)) {
      effects.consume(code);
      return inside
    }

    if (code === 59) {
      effects.consume(code);
      return after
    }

    return nok(code)
  }
  /** @type {State} */

  function after(code) {
    // If the named character reference is followed by the end of the path, it’s
    // not continued punctuation.
    return pathEnd(code) ? ok(code) : nok(code)
  }
}
/** @type {Tokenizer} */

function tokenizePunctuation(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    effects.consume(code);
    return after
  }
  /** @type {State} */

  function after(code) {
    // Check the next.
    if (trailingPunctuation(code)) {
      effects.consume(code);
      return after
    } // If the punctuation marker is followed by the end of the path, it’s not
    // continued punctuation.

    return pathEnd(code) ? ok(code) : nok(code)
  }
}
/**
 * @param {Code} code
 * @returns {boolean}
 */

function trailingPunctuation(code) {
  return (
    code === 33 ||
    code === 34 ||
    code === 39 ||
    code === 41 ||
    code === 42 ||
    code === 44 ||
    code === 46 ||
    code === 58 ||
    code === 59 ||
    code === 60 ||
    code === 63 ||
    code === 95 ||
    code === 126
  )
}
/**
 * @param {Code} code
 * @returns {boolean}
 */

function pathEnd(code) {
  return code === null || code === 60 || markdownLineEndingOrSpace(code)
}
/**
 * @param {Code} code
 * @returns {boolean}
 */

function gfmAtext(code) {
  return (
    code === 43 ||
    code === 45 ||
    code === 46 ||
    code === 95 ||
    asciiAlphanumeric(code)
  )
}
/** @type {Previous} */

function previousWww(code) {
  return (
    code === null ||
    code === 40 ||
    code === 42 ||
    code === 95 ||
    code === 126 ||
    markdownLineEndingOrSpace(code)
  )
}
/** @type {Previous} */

function previousHttp(code) {
  return code === null || !asciiAlpha(code)
}
/** @type {Previous} */

function previousEmail(code) {
  return code !== 47 && previousHttp(code)
}
/**
 * @param {Event[]} events
 * @returns {boolean}
 */

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
    } // @ts-expect-error If we’ve seen this token, and it was marked as not
    // having any unbalanced bracket before it, we can exit.

    if (token._gfmAutolinkLiteralWalkedInto) {
      result = false;
      break
    }
  }

  if (events.length > 0 && !result) {
    // @ts-expect-error Mark the last token as “walked into” w/o finding
    // anything.
    events[events.length - 1][1]._gfmAutolinkLiteralWalkedInto = true;
  }

  return result
}

/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Exiter} Exiter
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Event} Event
 */
const indent = {
  tokenize: tokenizeIndent,
  partial: true
};
/**
 * @returns {Extension}
 */

function gfmFootnote() {
  /** @type {Extension} */
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
/** @type {Tokenizer} */

function tokenizePotentialGfmFootnoteCall(effects, ok, nok) {
  const self = this;
  let index = self.events.length;
  /** @type {string[]} */
  // @ts-expect-error It’s fine!

  const defined = self.parser.gfmFootnotes || (self.parser.gfmFootnotes = []);
  /** @type {Token} */

  let labelStart; // Find an opening.

  while (index--) {
    const token = self.events[index][1];

    if (token.type === 'labelImage') {
      labelStart = token;
      break
    } // Exit if we’ve walked far enough.

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
  /** @type {State} */

  function start(code) {
    if (!labelStart || !labelStart._balanced) {
      return nok(code)
    }

    const id = normalizeIdentifier(
      self.sliceSerialize({
        start: labelStart.end,
        end: self.now()
      })
    );

    if (id.charCodeAt(0) !== 94 || !defined.includes(id.slice(1))) {
      return nok(code)
    }

    effects.enter('gfmFootnoteCallLabelMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteCallLabelMarker');
    return ok(code)
  }
}
/** @type {Resolver} */

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

  // Change the `labelImageMarker` to a `data`.
  events[index + 1][1].type = 'data';
  events[index + 3][1].type = 'gfmFootnoteCallLabelMarker'; // The whole (without `!`):

  const call = {
    type: 'gfmFootnoteCall',
    start: Object.assign({}, events[index + 3][1].start),
    end: Object.assign({}, events[events.length - 1][1].end)
  }; // The `^` marker

  const marker = {
    type: 'gfmFootnoteCallMarker',
    start: Object.assign({}, events[index + 3][1].end),
    end: Object.assign({}, events[index + 3][1].end)
  }; // Increment the end 1 character.

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
  /** @type {Event[]} */

  const replacement = [
    // Take the `labelImageMarker` (now `data`, the `!`)
    events[index + 1],
    events[index + 2],
    ['enter', call, context], // The `[`
    events[index + 3],
    events[index + 4], // The `^`.
    ['enter', marker, context],
    ['exit', marker, context], // Everything in between.
    ['enter', string, context],
    ['enter', chunk, context],
    ['exit', chunk, context],
    ['exit', string, context], // The ending (`]`, properly parsed and labelled).
    events[events.length - 2],
    events[events.length - 1],
    ['exit', call, context]
  ];
  events.splice(index, events.length - index + 1, ...replacement);
  return events
}
/** @type {Tokenizer} */

function tokenizeGfmFootnoteCall(effects, ok, nok) {
  const self = this;
  /** @type {string[]} */
  // @ts-expect-error It’s fine!

  const defined = self.parser.gfmFootnotes || (self.parser.gfmFootnotes = []);
  let size = 0;
  /** @type {boolean} */

  let data;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('gfmFootnoteCall');
    effects.enter('gfmFootnoteCallLabelMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteCallLabelMarker');
    return callStart
  }
  /** @type {State} */

  function callStart(code) {
    if (code !== 94) return nok(code)
    effects.enter('gfmFootnoteCallMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteCallMarker');
    effects.enter('gfmFootnoteCallString');
    effects.enter('chunkString').contentType = 'string';
    return callData
  }
  /** @type {State} */

  function callData(code) {
    /** @type {Token} */
    let token;

    if (code === null || code === 91 || size++ > 999) {
      return nok(code)
    }

    if (code === 93) {
      if (!data) {
        return nok(code)
      }

      effects.exit('chunkString');
      token = effects.exit('gfmFootnoteCallString');
      return defined.includes(normalizeIdentifier(self.sliceSerialize(token)))
        ? end(code)
        : nok(code)
    }

    effects.consume(code);

    if (!markdownLineEndingOrSpace(code)) {
      data = true;
    }

    return code === 92 ? callEscape : callData
  }
  /** @type {State} */

  function callEscape(code) {
    if (code === 91 || code === 92 || code === 93) {
      effects.consume(code);
      size++;
      return callData
    }

    return callData(code)
  }
  /** @type {State} */

  function end(code) {
    effects.enter('gfmFootnoteCallLabelMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteCallLabelMarker');
    effects.exit('gfmFootnoteCall');
    return ok
  }
}
/** @type {Tokenizer} */

function tokenizeDefinitionStart(effects, ok, nok) {
  const self = this;
  /** @type {string[]} */
  // @ts-expect-error It’s fine!

  const defined = self.parser.gfmFootnotes || (self.parser.gfmFootnotes = []);
  /** @type {string} */

  let identifier;
  let size = 0;
  /** @type {boolean|undefined} */

  let data;
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('gfmFootnoteDefinition')._container = true;
    effects.enter('gfmFootnoteDefinitionLabel');
    effects.enter('gfmFootnoteDefinitionLabelMarker');
    effects.consume(code);
    effects.exit('gfmFootnoteDefinitionLabelMarker');
    return labelStart
  }
  /** @type {State} */

  function labelStart(code) {
    if (code === 94) {
      effects.enter('gfmFootnoteDefinitionMarker');
      effects.consume(code);
      effects.exit('gfmFootnoteDefinitionMarker');
      effects.enter('gfmFootnoteDefinitionLabelString');
      return atBreak
    }

    return nok(code)
  }
  /** @type {State} */

  function atBreak(code) {
    /** @type {Token} */
    let token;

    if (code === null || code === 91 || size > 999) {
      return nok(code)
    }

    if (code === 93) {
      if (!data) {
        return nok(code)
      }

      token = effects.exit('gfmFootnoteDefinitionLabelString');
      identifier = normalizeIdentifier(self.sliceSerialize(token));
      effects.enter('gfmFootnoteDefinitionLabelMarker');
      effects.consume(code);
      effects.exit('gfmFootnoteDefinitionLabelMarker');
      effects.exit('gfmFootnoteDefinitionLabel');
      return labelAfter
    }

    if (markdownLineEnding(code)) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      size++;
      return atBreak
    }

    effects.enter('chunkString').contentType = 'string';
    return label(code)
  }
  /** @type {State} */

  function label(code) {
    if (
      code === null ||
      markdownLineEnding(code) ||
      code === 91 ||
      code === 93 ||
      size > 999
    ) {
      effects.exit('chunkString');
      return atBreak(code)
    }

    if (!markdownLineEndingOrSpace(code)) {
      data = true;
    }

    size++;
    effects.consume(code);
    return code === 92 ? labelEscape : label
  }
  /** @type {State} */

  function labelEscape(code) {
    if (code === 91 || code === 92 || code === 93) {
      effects.consume(code);
      size++;
      return label
    }

    return label(code)
  }
  /** @type {State} */

  function labelAfter(code) {
    if (code === 58) {
      effects.enter('definitionMarker');
      effects.consume(code);
      effects.exit('definitionMarker'); // Any whitespace after the marker is eaten, forming indented code
      // is not possible.
      // No space is also fine, just like a block quote marker.

      return factorySpace(effects, done, 'gfmFootnoteDefinitionWhitespace')
    }

    return nok(code)
  }
  /** @type {State} */

  function done(code) {
    if (!defined.includes(identifier)) {
      defined.push(identifier);
    }

    return ok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeDefinitionContinuation(effects, ok, nok) {
  // Either a blank line, which is okay, or an indented thing.
  return effects.check(blankLine, ok, effects.attempt(indent, ok, nok))
}
/** @type {Exiter} */

function gfmFootnoteDefinitionEnd(effects) {
  effects.exit('gfmFootnoteDefinition');
}
/** @type {Tokenizer} */

function tokenizeIndent(effects, ok, nok) {
  const self = this;
  return factorySpace(
    effects,
    afterPrefix,
    'gfmFootnoteDefinitionIndent',
    4 + 1
  )
  /** @type {State} */

  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'gfmFootnoteDefinitionIndent' &&
      tail[2].sliceSerialize(tail[1], true).length === 4
      ? ok(code)
      : nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').Event} Event
 */

/**
 * @param {Options} [options]
 * @returns {Extension}
 */
function gfmStrikethrough(options = {}) {
  let single = options.singleTilde;
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
  /**
   * Take events and resolve strikethrough.
   *
   * @type {Resolver}
   */

  function resolveAllStrikethrough(events, context) {
    let index = -1; // Walk through all events.

    while (++index < events.length) {
      // Find a token that can close.
      if (
        events[index][0] === 'enter' &&
        events[index][1].type === 'strikethroughSequenceTemporary' &&
        events[index][1]._close
      ) {
        let open = index; // Now walk back to find an opener.

        while (open--) {
          // Find a token that can open the closer.
          if (
            events[open][0] === 'exit' &&
            events[open][1].type === 'strikethroughSequenceTemporary' &&
            events[open][1]._open && // If the sizes are the same:
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
            }; // Opening.

            const nextEvents = [
              ['enter', strikethrough, context],
              ['enter', events[open][1], context],
              ['exit', events[open][1], context],
              ['enter', text, context]
            ]; // Between.

            splice(
              nextEvents,
              nextEvents.length,
              0,
              resolveAll(
                context.parser.constructs.insideSpan.null,
                events.slice(open + 1, index),
                context
              )
            ); // Closing.

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
  /** @type {Tokenizer} */

  function tokenizeStrikethrough(effects, ok, nok) {
    const previous = this.previous;
    const events = this.events;
    let size = 0;
    return start
    /** @type {State} */

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
    /** @type {State} */

    function more(code) {
      const before = classifyCharacter(previous);

      if (code === 126) {
        // If this is the third marker, exit.
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

/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Token} Token
 */

/** @type {Extension} */
const gfmTable = {
  flow: {
    null: {
      tokenize: tokenizeTable,
      resolve: resolveTable
    }
  }
};
const setextUnderlineMini = {
  tokenize: tokenizeSetextUnderlineMini,
  partial: true
};
const nextPrefixedOrBlank = {
  tokenize: tokenizeNextPrefixedOrBlank,
  partial: true
};
/** @type {Resolver} */

function resolveTable(events, context) {
  let index = -1;
  /** @type {boolean|undefined} */

  let inHead;
  /** @type {boolean|undefined} */

  let inDelimiterRow;
  /** @type {boolean|undefined} */

  let inRow;
  /** @type {number|undefined} */

  let contentStart;
  /** @type {number|undefined} */

  let contentEnd;
  /** @type {number|undefined} */

  let cellStart;

  while (++index < events.length) {
    const token = events[index][1];

    if (inRow) {
      if (token.type === 'temporaryTableCellContent') {
        contentStart = contentStart || index;
        contentEnd = index;
      }

      if (
        // Combine separate content parts into one.
        (token.type === 'tableCellDivider' || token.type === 'tableRow') &&
        contentEnd
      ) {
        const content = {
          type: 'tableContent',
          start: events[contentStart][1].start,
          end: events[contentEnd][1].end
        };
        /** @type {Token} */

        const text = {
          type: 'chunkText',
          start: content.start,
          end: content.end,
          // @ts-expect-error It’s fine.
          contentType: 'text'
        };
        events.splice(
          contentStart,
          contentEnd - contentStart + 1,
          ['enter', content, context],
          ['enter', text, context],
          ['exit', text, context],
          ['exit', content, context]
        );
        index -= contentEnd - contentStart - 3;
        contentStart = undefined;
        contentEnd = undefined;
      }
    }

    if (
      events[index][0] === 'exit' &&
      cellStart &&
      cellStart + 1 < index &&
      (token.type === 'tableCellDivider' ||
        (token.type === 'tableRow' &&
          (cellStart + 3 < index ||
            events[cellStart][1].type !== 'whitespace')))
    ) {
      const cell = {
        type: inDelimiterRow
          ? 'tableDelimiter'
          : inHead
          ? 'tableHeader'
          : 'tableData',
        start: events[cellStart][1].start,
        end: events[index][1].end
      };
      events.splice(index + (token.type === 'tableCellDivider' ? 1 : 0), 0, [
        'exit',
        cell,
        context
      ]);
      events.splice(cellStart, 0, ['enter', cell, context]);
      index += 2;
      cellStart = index + 1;
    }

    if (token.type === 'tableRow') {
      inRow = events[index][0] === 'enter';

      if (inRow) {
        cellStart = index + 1;
      }
    }

    if (token.type === 'tableDelimiterRow') {
      inDelimiterRow = events[index][0] === 'enter';

      if (inDelimiterRow) {
        cellStart = index + 1;
      }
    }

    if (token.type === 'tableHead') {
      inHead = events[index][0] === 'enter';
    }
  }

  return events
}
/** @type {Tokenizer} */

function tokenizeTable(effects, ok, nok) {
  const self = this;
  /** @type {Align[]} */

  const align = [];
  let tableHeaderCount = 0;
  /** @type {boolean|undefined} */

  let seenDelimiter;
  /** @type {boolean|undefined} */

  let hasDash;
  return start
  /** @type {State} */

  function start(code) {
    // @ts-expect-error Custom.
    effects.enter('table')._align = align;
    effects.enter('tableHead');
    effects.enter('tableRow'); // If we start with a pipe, we open a cell marker.

    if (code === 124) {
      return cellDividerHead(code)
    }

    tableHeaderCount++;
    effects.enter('temporaryTableCellContent'); // Can’t be space or eols at the start of a construct, so we’re in a cell.

    return inCellContentHead(code)
  }
  /** @type {State} */

  function cellDividerHead(code) {
    effects.enter('tableCellDivider');
    effects.consume(code);
    effects.exit('tableCellDivider');
    seenDelimiter = true;
    return cellBreakHead
  }
  /** @type {State} */

  function cellBreakHead(code) {
    if (code === null || markdownLineEnding(code)) {
      return atRowEndHead(code)
    }

    if (markdownSpace(code)) {
      effects.enter('whitespace');
      effects.consume(code);
      return inWhitespaceHead
    }

    if (seenDelimiter) {
      seenDelimiter = undefined;
      tableHeaderCount++;
    }

    if (code === 124) {
      return cellDividerHead(code)
    } // Anything else is cell content.

    effects.enter('temporaryTableCellContent');
    return inCellContentHead(code)
  }
  /** @type {State} */

  function inWhitespaceHead(code) {
    if (markdownSpace(code)) {
      effects.consume(code);
      return inWhitespaceHead
    }

    effects.exit('whitespace');
    return cellBreakHead(code)
  }
  /** @type {State} */

  function inCellContentHead(code) {
    // EOF, whitespace, pipe
    if (code === null || code === 124 || markdownLineEndingOrSpace(code)) {
      effects.exit('temporaryTableCellContent');
      return cellBreakHead(code)
    }

    effects.consume(code);
    return code === 92 ? inCellContentEscapeHead : inCellContentHead
  }
  /** @type {State} */

  function inCellContentEscapeHead(code) {
    if (code === 92 || code === 124) {
      effects.consume(code);
      return inCellContentHead
    } // Anything else.

    return inCellContentHead(code)
  }
  /** @type {State} */

  function atRowEndHead(code) {
    if (code === null) {
      return nok(code)
    }

    effects.exit('tableRow');
    effects.exit('tableHead');
    return effects.attempt(
      {
        tokenize: tokenizeRowEnd,
        partial: true
      },
      atDelimiterLineStart,
      nok
    )(code)
  }
  /** @type {State} */

  function atDelimiterLineStart(code) {
    return effects.check(
      setextUnderlineMini,
      nok, // Support an indent before the delimiter row.
      factorySpace(effects, rowStartDelimiter, 'linePrefix', 4)
    )(code)
  }
  /** @type {State} */

  function rowStartDelimiter(code) {
    // If there’s another space, or we’re at the EOL/EOF, exit.
    if (code === null || markdownLineEndingOrSpace(code)) {
      return nok(code)
    }

    effects.enter('tableDelimiterRow');
    return atDelimiterRowBreak(code)
  }
  /** @type {State} */

  function atDelimiterRowBreak(code) {
    if (code === null || markdownLineEnding(code)) {
      return rowEndDelimiter(code)
    }

    if (markdownSpace(code)) {
      effects.enter('whitespace');
      effects.consume(code);
      return inWhitespaceDelimiter
    }

    if (code === 45) {
      effects.enter('tableDelimiterFiller');
      effects.consume(code);
      hasDash = true;
      align.push(null);
      return inFillerDelimiter
    }

    if (code === 58) {
      effects.enter('tableDelimiterAlignment');
      effects.consume(code);
      effects.exit('tableDelimiterAlignment');
      align.push('left');
      return afterLeftAlignment
    } // If we start with a pipe, we open a cell marker.

    if (code === 124) {
      effects.enter('tableCellDivider');
      effects.consume(code);
      effects.exit('tableCellDivider');
      return atDelimiterRowBreak
    }

    return nok(code)
  }
  /** @type {State} */

  function inWhitespaceDelimiter(code) {
    if (markdownSpace(code)) {
      effects.consume(code);
      return inWhitespaceDelimiter
    }

    effects.exit('whitespace');
    return atDelimiterRowBreak(code)
  }
  /** @type {State} */

  function inFillerDelimiter(code) {
    if (code === 45) {
      effects.consume(code);
      return inFillerDelimiter
    }

    effects.exit('tableDelimiterFiller');

    if (code === 58) {
      effects.enter('tableDelimiterAlignment');
      effects.consume(code);
      effects.exit('tableDelimiterAlignment');
      align[align.length - 1] =
        align[align.length - 1] === 'left' ? 'center' : 'right';
      return afterRightAlignment
    }

    return atDelimiterRowBreak(code)
  }
  /** @type {State} */

  function afterLeftAlignment(code) {
    if (code === 45) {
      effects.enter('tableDelimiterFiller');
      effects.consume(code);
      hasDash = true;
      return inFillerDelimiter
    } // Anything else is not ok.

    return nok(code)
  }
  /** @type {State} */

  function afterRightAlignment(code) {
    if (code === null || markdownLineEnding(code)) {
      return rowEndDelimiter(code)
    }

    if (markdownSpace(code)) {
      effects.enter('whitespace');
      effects.consume(code);
      return inWhitespaceDelimiter
    } // `|`

    if (code === 124) {
      effects.enter('tableCellDivider');
      effects.consume(code);
      effects.exit('tableCellDivider');
      return atDelimiterRowBreak
    }

    return nok(code)
  }
  /** @type {State} */

  function rowEndDelimiter(code) {
    effects.exit('tableDelimiterRow'); // Exit if there was no dash at all, or if the header cell count is not the
    // delimiter cell count.

    if (!hasDash || tableHeaderCount !== align.length) {
      return nok(code)
    }

    if (code === null) {
      return tableClose(code)
    }

    return effects.check(
      nextPrefixedOrBlank,
      tableClose,
      effects.attempt(
        {
          tokenize: tokenizeRowEnd,
          partial: true
        },
        factorySpace(effects, bodyStart, 'linePrefix', 4),
        tableClose
      )
    )(code)
  }
  /** @type {State} */

  function tableClose(code) {
    effects.exit('table');
    return ok(code)
  }
  /** @type {State} */

  function bodyStart(code) {
    effects.enter('tableBody');
    return rowStartBody(code)
  }
  /** @type {State} */

  function rowStartBody(code) {
    effects.enter('tableRow'); // If we start with a pipe, we open a cell marker.

    if (code === 124) {
      return cellDividerBody(code)
    }

    effects.enter('temporaryTableCellContent'); // Can’t be space or eols at the start of a construct, so we’re in a cell.

    return inCellContentBody(code)
  }
  /** @type {State} */

  function cellDividerBody(code) {
    effects.enter('tableCellDivider');
    effects.consume(code);
    effects.exit('tableCellDivider');
    return cellBreakBody
  }
  /** @type {State} */

  function cellBreakBody(code) {
    if (code === null || markdownLineEnding(code)) {
      return atRowEndBody(code)
    }

    if (markdownSpace(code)) {
      effects.enter('whitespace');
      effects.consume(code);
      return inWhitespaceBody
    } // `|`

    if (code === 124) {
      return cellDividerBody(code)
    } // Anything else is cell content.

    effects.enter('temporaryTableCellContent');
    return inCellContentBody(code)
  }
  /** @type {State} */

  function inWhitespaceBody(code) {
    if (markdownSpace(code)) {
      effects.consume(code);
      return inWhitespaceBody
    }

    effects.exit('whitespace');
    return cellBreakBody(code)
  }
  /** @type {State} */

  function inCellContentBody(code) {
    // EOF, whitespace, pipe
    if (code === null || code === 124 || markdownLineEndingOrSpace(code)) {
      effects.exit('temporaryTableCellContent');
      return cellBreakBody(code)
    }

    effects.consume(code);
    return code === 92 ? inCellContentEscapeBody : inCellContentBody
  }
  /** @type {State} */

  function inCellContentEscapeBody(code) {
    if (code === 92 || code === 124) {
      effects.consume(code);
      return inCellContentBody
    } // Anything else.

    return inCellContentBody(code)
  }
  /** @type {State} */

  function atRowEndBody(code) {
    effects.exit('tableRow');

    if (code === null) {
      return tableBodyClose(code)
    }

    return effects.check(
      nextPrefixedOrBlank,
      tableBodyClose,
      effects.attempt(
        {
          tokenize: tokenizeRowEnd,
          partial: true
        },
        factorySpace(effects, rowStartBody, 'linePrefix', 4),
        tableBodyClose
      )
    )(code)
  }
  /** @type {State} */

  function tableBodyClose(code) {
    effects.exit('tableBody');
    return tableClose(code)
  }
  /** @type {Tokenizer} */

  function tokenizeRowEnd(effects, ok, nok) {
    return start
    /** @type {State} */

    function start(code) {
      effects.enter('lineEnding');
      effects.consume(code);
      effects.exit('lineEnding');
      return lineStart
    }
    /** @type {State} */

    function lineStart(code) {
      return self.parser.lazy[self.now().line] ? nok(code) : ok(code)
    }
  }
} // Based on micromark, but that won’t work as we’re in a table, and that expects
// content.
// <https://github.com/micromark/micromark/blob/main/lib/tokenize/setext-underline.js>

/** @type {Tokenizer} */

function tokenizeSetextUnderlineMini(effects, ok, nok) {
  return start
  /** @type {State} */

  function start(code) {
    if (code !== 45) {
      return nok(code)
    }

    effects.enter('setextUnderline');
    return sequence(code)
  }
  /** @type {State} */

  function sequence(code) {
    if (code === 45) {
      effects.consume(code);
      return sequence
    }

    return whitespace(code)
  }
  /** @type {State} */

  function whitespace(code) {
    if (code === null || markdownLineEnding(code)) {
      return ok(code)
    }

    if (markdownSpace(code)) {
      effects.consume(code);
      return whitespace
    }

    return nok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeNextPrefixedOrBlank(effects, ok, nok) {
  let size = 0;
  return start
  /** @type {State} */

  function start(code) {
    // This is a check, so we don’t care about tokens, but we open a bogus one
    // so we’re valid.
    effects.enter('check'); // EOL.

    effects.consume(code);
    return whitespace
  }
  /** @type {State} */

  function whitespace(code) {
    if (code === -1 || code === 32) {
      effects.consume(code);
      size++;
      return size === 4 ? ok : whitespace
    } // EOF or whitespace

    if (code === null || markdownLineEndingOrSpace(code)) {
      return ok(code)
    } // Anything else.

    return nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').ConstructRecord} ConstructRecord
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Previous} Previous
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').Code} Code
 */
const tasklistCheck = {
  tokenize: tokenizeTasklistCheck
};
const gfmTaskListItem = {
  text: {
    [91]: tasklistCheck
  }
};
/** @type {Tokenizer} */

function tokenizeTasklistCheck(effects, ok, nok) {
  const self = this;
  return open
  /** @type {State} */

  function open(code) {
    if (
      // Exit if there’s stuff before.
      self.previous !== null || // Exit if not in the first content that is the first child of a list
      // item.
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
  /** @type {State} */

  function inside(code) {
    if (markdownSpace(code)) {
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
  /** @type {State} */

  function close(code) {
    if (code === 93) {
      effects.enter('taskListCheckMarker');
      effects.consume(code);
      effects.exit('taskListCheckMarker');
      effects.exit('taskListCheck');
      return effects.check(
        {
          tokenize: spaceThenNonSpace
        },
        ok,
        nok
      )
    }

    return nok(code)
  }
}
/** @type {Tokenizer} */

function spaceThenNonSpace(effects, ok, nok) {
  const self = this;
  return factorySpace(effects, after, 'whitespace')
  /** @type {State} */

  function after(code) {
    const tail = self.events[self.events.length - 1];
    return tail &&
      tail[1].type === 'whitespace' &&
      code !== null &&
      !markdownLineEndingOrSpace(code)
      ? ok(code)
      : nok(code)
  }
}

/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').HtmlExtension} HtmlExtension
 * @typedef {import('micromark-extension-gfm-strikethrough').Options} Options
 * @typedef {import('micromark-extension-gfm-footnote').HtmlOptions} HtmlOptions
 */

/**
 * Support GFM or markdown on github.com.
 *
 * @param {Options} [options]
 * @returns {Extension}
 */
function gfm(options) {
  return combineExtensions([
    gfmAutolinkLiteral,
    gfmFootnote(),
    gfmStrikethrough(options),
    gfmTable,
    gfmTaskListItem
  ])
}

/**
 * Get the total count of `character` in `value`.
 *
 * @param {any} value Content, coerced to string
 * @param {string} character Single character to look for
 * @return {number} Number of times `character` occurred in `value`.
 */
function ccount(value, character) {
  var source = String(value);
  var count = 0;
  var index;

  if (typeof character !== 'string') {
    throw new Error('Expected character')
  }

  index = source.indexOf(character);

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

	// Escape characters with special meaning either inside or outside character sets.
	// Use a simple backslash escape when it’s always valid, and a `\xnn` escape when the simpler form would be disallowed by Unicode patterns’ stricter grammar.
	return string
		.replace(/[|\\{}()[\]^$+*?.]/g, '\\$&')
		.replace(/-/g, '\\x2d');
}

/**
 * @param {string} d
 * @returns {string}
 */
function color(d) {
  return '\u001B[33m' + d + '\u001B[39m'
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('unist-util-is').Test} Test
 */

/**
 * Continue traversing as normal
 */
const CONTINUE = true;
/**
 * Do not traverse this node’s children
 */
const SKIP = 'skip';
/**
 * Stop traversing immediately
 */
const EXIT = false;

const visitParents =
  /**
   * @type {(
   *   (<T extends Node>(tree: Node, test: T['type']|Partial<T>|import('unist-util-is').TestFunctionPredicate<T>|Array.<T['type']|Partial<T>|import('unist-util-is').TestFunctionPredicate<T>>, visitor: Visitor<T>, reverse?: boolean) => void) &
   *   ((tree: Node, test: Test, visitor: Visitor<Node>, reverse?: boolean) => void) &
   *   ((tree: Node, visitor: Visitor<Node>, reverse?: boolean) => void)
   * )}
   */
  (
    /**
     * Visit children of tree which pass a test
     *
     * @param {Node} tree Abstract syntax tree to walk
     * @param {Test} test test Test node
     * @param {Visitor<Node>} visitor Function to run for each node
     * @param {boolean} [reverse] Fisit the tree in reverse, defaults to false
     */
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        // @ts-ignore no visitor given, so `visitor` is test.
        visitor = test;
        test = null;
      }

      var is = convert(test);
      var step = reverse ? -1 : 1;

      factory(tree, null, [])();

      /**
       * @param {Node} node
       * @param {number?} index
       * @param {Array.<Parent>} parents
       */
      function factory(node, index, parents) {
        /** @type {Object.<string, unknown>} */
        var value = typeof node === 'object' && node !== null ? node : {};
        /** @type {string} */
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
              color(value.type + (name ? '<' + name + '>' : '')) +
              ')'
          });
        }

        return visit

        function visit() {
          /** @type {ActionTuple} */
          var result = [];
          /** @type {ActionTuple} */
          var subresult;
          /** @type {number} */
          var offset;
          /** @type {Array.<Parent>} */
          var grandparents;

          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult(visitor(node, parents));

            if (result[0] === EXIT) {
              return result
            }
          }

          if (node.children && result[0] !== SKIP) {
            // @ts-ignore looks like a parent.
            offset = (reverse ? node.children.length : -1) + step;
            // @ts-ignore looks like a parent.
            grandparents = parents.concat(node);

            // @ts-ignore looks like a parent.
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

/**
 * @param {VisitorResult} value
 * @returns {ActionTuple}
 */
function toResult(value) {
  if (Array.isArray(value)) {
    return value
  }

  if (typeof value === 'number') {
    return [CONTINUE, value]
  }

  return [value]
}

/**
 * @typedef Options Configuration.
 * @property {Test} [ignore] `unist-util-is` test used to assert parents
 *
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Content} Content
 * @typedef {import('mdast').PhrasingContent} PhrasingContent
 * @typedef {import('mdast').Text} Text
 * @typedef {Content|Root} Node
 * @typedef {Extract<Node, import('mdast').Parent>} Parent
 *
 * @typedef {import('unist-util-visit-parents').Test} Test
 * @typedef {import('unist-util-visit-parents').VisitorResult} VisitorResult
 *
 * @typedef RegExpMatchObject
 * @property {number} index
 * @property {string} input
 *
 * @typedef {string|RegExp} Find
 * @typedef {string|ReplaceFunction} Replace
 *
 * @typedef {[Find, Replace]} FindAndReplaceTuple
 * @typedef {Object.<string, Replace>} FindAndReplaceSchema
 * @typedef {Array.<FindAndReplaceTuple>} FindAndReplaceList
 *
 * @typedef {[RegExp, ReplaceFunction]} Pair
 * @typedef {Array.<Pair>} Pairs
 */

const own$3 = {}.hasOwnProperty;

/**
 * @param tree mdast tree
 * @param find Value to find and remove. When `string`, escaped and made into a global `RegExp`
 * @param [replace] Value to insert.
 *   * When `string`, turned into a Text node.
 *   * When `Function`, called with the results of calling `RegExp.exec` as
 *     arguments, in which case it can return a single or a list of `Node`,
 *     a `string` (which is wrapped in a `Text` node), or `false` to not replace
 * @param [options] Configuration.
 */
const findAndReplace =
  /**
   * @type {(
   *   ((tree: Node, find: Find, replace?: Replace, options?: Options) => Node) &
   *   ((tree: Node, schema: FindAndReplaceSchema|FindAndReplaceList, options?: Options) => Node)
   * )}
   **/
  (
    /**
     * @param {Node} tree
     * @param {Find|FindAndReplaceSchema|FindAndReplaceList} find
     * @param {Replace|Options} [replace]
     * @param {Options} [options]
     */
    function (tree, find, replace, options) {
      /** @type {Options|undefined} */
      let settings;
      /** @type {FindAndReplaceSchema|FindAndReplaceList} */
      let schema;

      if (typeof find === 'string' || find instanceof RegExp) {
        // @ts-expect-error don’t expect options twice.
        schema = [[find, replace]];
        settings = options;
      } else {
        schema = find;
        // @ts-expect-error don’t expect replace twice.
        settings = replace;
      }

      if (!settings) {
        settings = {};
      }

      const ignored = convert(settings.ignore || []);
      const pairs = toPairs(schema);
      let pairIndex = -1;

      while (++pairIndex < pairs.length) {
        visitParents(tree, 'text', visitor);
      }

      return tree

      /** @type {import('unist-util-visit-parents').Visitor<Text>} */
      function visitor(node, parents) {
        let index = -1;
        /** @type {Parent|undefined} */
        let grandparent;

        while (++index < parents.length) {
          const parent = /** @type {Parent} */ (parents[index]);

          if (
            ignored(
              parent,
              // @ts-expect-error mdast vs. unist parent.
              grandparent ? grandparent.children.indexOf(parent) : undefined,
              grandparent
            )
          ) {
            return
          }

          grandparent = parent;
        }

        if (grandparent) {
          return handler(node, grandparent)
        }
      }

      /**
       * @param {Text} node
       * @param {Parent} parent
       * @returns {VisitorResult}
       */
      function handler(node, parent) {
        const find = pairs[pairIndex][0];
        const replace = pairs[pairIndex][1];
        let start = 0;
        // @ts-expect-error: TS is wrong, some of these children can be text.
        let index = parent.children.indexOf(node);
        /** @type {Array.<PhrasingContent>} */
        let nodes = [];
        /** @type {number|undefined} */
        let position;

        find.lastIndex = 0;

        let match = find.exec(node.value);

        while (match) {
          position = match.index;
          // @ts-expect-error this is perfectly fine, typescript.
          let value = replace(...match, {
            index: match.index,
            input: match.input
          });

          if (typeof value === 'string') {
            value = value.length > 0 ? {type: 'text', value} : undefined;
          }

          if (value !== false) {
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
          }

          if (!find.global) {
            break
          }

          match = find.exec(node.value);
        }

        if (position === undefined) {
          nodes = [node];
          index--;
        } else {
          if (start < node.value.length) {
            nodes.push({type: 'text', value: node.value.slice(start)});
          }

          parent.children.splice(index, 1, ...nodes);
        }

        return index + nodes.length + 1
      }
    }
  );

/**
 * @param {FindAndReplaceSchema|FindAndReplaceList} schema
 * @returns {Pairs}
 */
function toPairs(schema) {
  /** @type {Pairs} */
  const result = [];

  if (typeof schema !== 'object') {
    throw new TypeError('Expected array or object as schema')
  }

  if (Array.isArray(schema)) {
    let index = -1;

    while (++index < schema.length) {
      result.push([
        toExpression(schema[index][0]),
        toFunction(schema[index][1])
      ]);
    }
  } else {
    /** @type {string} */
    let key;

    for (key in schema) {
      if (own$3.call(schema, key)) {
        result.push([toExpression(key), toFunction(schema[key])]);
      }
    }
  }

  return result
}

/**
 * @param {Find} find
 * @returns {RegExp}
 */
function toExpression(find) {
  return typeof find === 'string' ? new RegExp(escapeStringRegexp(find), 'g') : find
}

/**
 * @param {Replace} replace
 * @returns {ReplaceFunction}
 */
function toFunction(replace) {
  return typeof replace === 'function' ? replace : () => replace
}

/**
 * @typedef {import('mdast').Link} Link
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-from-markdown').Transform} FromMarkdownTransform
 * @typedef {import('mdast-util-from-markdown').Handle} FromMarkdownHandle
 * @typedef {import('mdast-util-to-markdown/lib/types.js').Options} ToMarkdownExtension
 * @typedef {import('mdast-util-find-and-replace').ReplaceFunction} ReplaceFunction
 * @typedef {import('mdast-util-find-and-replace').RegExpMatchObject} RegExpMatchObject
 * @typedef {import('mdast-util-find-and-replace').PhrasingContent} PhrasingContent
 */

const inConstruct = 'phrasing';
const notInConstruct = ['autolink', 'link', 'image', 'label'];

/** @type {FromMarkdownExtension} */
const gfmAutolinkLiteralFromMarkdown = {
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
};

/** @type {ToMarkdownExtension} */
const gfmAutolinkLiteralToMarkdown = {
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
    {character: ':', before: '[ps]', after: '\\/', inConstruct, notInConstruct}
  ]
};

/** @type {FromMarkdownHandle} */
function enterLiteralAutolink(token) {
  this.enter({type: 'link', title: null, url: '', children: []}, token);
}

/** @type {FromMarkdownHandle} */
function enterLiteralAutolinkValue(token) {
  this.config.enter.autolinkProtocol.call(this, token);
}

/** @type {FromMarkdownHandle} */
function exitLiteralAutolinkHttp(token) {
  this.config.exit.autolinkProtocol.call(this, token);
}

/** @type {FromMarkdownHandle} */
function exitLiteralAutolinkWww(token) {
  this.config.exit.data.call(this, token);
  const node = /** @type {Link} */ (this.stack[this.stack.length - 1]);
  node.url = 'http://' + this.sliceSerialize(token);
}

/** @type {FromMarkdownHandle} */
function exitLiteralAutolinkEmail(token) {
  this.config.exit.autolinkEmail.call(this, token);
}

/** @type {FromMarkdownHandle} */
function exitLiteralAutolink(token) {
  this.exit(token);
}

/** @type {FromMarkdownTransform} */
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

/**
 * @type {ReplaceFunction}
 * @param {string} _
 * @param {string} protocol
 * @param {string} domain
 * @param {string} path
 * @param {RegExpMatchObject} match
 */
// eslint-disable-next-line max-params
function findUrl(_, protocol, domain, path, match) {
  let prefix = '';

  // Not an expected previous character.
  if (!previous(match)) {
    return false
  }

  // Treat `www` as part of the domain.
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

  /** @type {PhrasingContent} */
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

/**
 * @type {ReplaceFunction}
 * @param {string} _
 * @param {string} atext
 * @param {string} label
 * @param {RegExpMatchObject} match
 */
function findEmail(_, atext, label, match) {
  if (
    // Not an expected previous character.
    !previous(match, true) ||
    // Label ends in not allowed character.
    /[_-\d]$/.test(label)
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

/**
 * @param {string} domain
 * @returns {boolean}
 */
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

/**
 * @param {string} url
 * @returns {[string, string|undefined]}
 */
function splitUrl(url) {
  const trailExec = /[!"&'),.:;<>?\]}]+$/.exec(url);
  /** @type {number} */
  let closingParenIndex;
  /** @type {number} */
  let openingParens;
  /** @type {number} */
  let closingParens;
  /** @type {string|undefined} */
  let trail;

  if (trailExec) {
    url = url.slice(0, trailExec.index);
    trail = trailExec[0];
    closingParenIndex = trail.indexOf(')');
    openingParens = ccount(url, '(');
    closingParens = ccount(url, ')');

    while (closingParenIndex !== -1 && openingParens > closingParens) {
      url += trail.slice(0, closingParenIndex + 1);
      trail = trail.slice(closingParenIndex + 1);
      closingParenIndex = trail.indexOf(')');
      closingParens++;
    }
  }

  return [url, trail]
}

/**
 * @param {RegExpMatchObject} match
 * @param {boolean} [email=false]
 * @returns {boolean}
 */
function previous(match, email) {
  const code = match.input.charCodeAt(match.index - 1);

  return (
    (match.index === 0 ||
      unicodeWhitespace(code) ||
      unicodePunctuation(code)) &&
    (!email || code !== 47)
  )
}

/**
 * @typedef {import('mdast').FootnoteReference} FootnoteReference
 * @typedef {import('mdast').FootnoteDefinition} FootnoteDefinition
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-from-markdown').Handle} FromMarkdownHandle
 * @typedef {import('mdast-util-to-markdown').Options} ToMarkdownExtension
 * @typedef {import('mdast-util-to-markdown').Handle} ToMarkdownHandle
 * @typedef {import('mdast-util-to-markdown').Map} Map
 */

let warningColonInFootnote = false;
let warningListInFootnote = false;

/**
 * @returns {FromMarkdownExtension}
 */
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

  /** @type {FromMarkdownHandle} */
  function enterFootnoteDefinition(token) {
    this.enter(
      {type: 'footnoteDefinition', identifier: '', label: '', children: []},
      token
    );
  }

  /** @type {FromMarkdownHandle} */
  function enterFootnoteDefinitionLabelString() {
    this.buffer();
  }

  /** @type {FromMarkdownHandle} */
  function exitFootnoteDefinitionLabelString(token) {
    const label = this.resume();
    const node = /** @type {FootnoteDefinition} */ (
      this.stack[this.stack.length - 1]
    );
    node.label = label;
    node.identifier = normalizeIdentifier(
      this.sliceSerialize(token)
    ).toLowerCase();
  }

  /** @type {FromMarkdownHandle} */
  function exitFootnoteDefinition(token) {
    this.exit(token);
  }

  /** @type {FromMarkdownHandle} */
  function enterFootnoteCall(token) {
    this.enter({type: 'footnoteReference', identifier: '', label: ''}, token);
  }

  /** @type {FromMarkdownHandle} */
  function enterFootnoteCallString() {
    this.buffer();
  }

  /** @type {FromMarkdownHandle} */
  function exitFootnoteCallString(token) {
    const label = this.resume();
    const node = /** @type {FootnoteDefinition} */ (
      this.stack[this.stack.length - 1]
    );
    node.label = label;
    node.identifier = normalizeIdentifier(
      this.sliceSerialize(token)
    ).toLowerCase();
  }

  /** @type {FromMarkdownHandle} */
  function exitFootnoteCall(token) {
    this.exit(token);
  }
}

/**
 * @returns {ToMarkdownExtension}
 */
function gfmFootnoteToMarkdown() {
  footnoteReference.peek = footnoteReferencePeek;

  return {
    // This is on by default already.
    unsafe: [{character: '[', inConstruct: ['phrasing', 'label', 'reference']}],
    handlers: {footnoteDefinition, footnoteReference}
  }

  /**
   * @type {ToMarkdownHandle}
   * @param {FootnoteReference} node
   */
  function footnoteReference(node, _, context) {
    const exit = context.enter('footnoteReference');
    const subexit = context.enter('reference');
    const reference = safe(context, association(node), {
      before: '^',
      after: ']'
    });
    subexit();
    exit();
    return '[^' + reference + ']'
  }

  /** @type {ToMarkdownHandle} */
  function footnoteReferencePeek() {
    return '['
  }

  /**
   * @type {ToMarkdownHandle}
   * @param {FootnoteDefinition} node
   */
  function footnoteDefinition(node, _, context) {
    const exit = context.enter('footnoteDefinition');
    const subexit = context.enter('label');
    const id = safe(context, association(node), {before: '^', after: ']'});
    const label = '[^' + id + ']:';
    subexit();
    const value = indentLines(containerFlow(node, context), map);
    exit();

    if (!warningColonInFootnote && id.includes(':')) {
      console.warn(
        '[mdast-util-gfm-footnote] Warning: Found a colon in footnote identifier `' +
          id +
          '`. GitHub currently crahes on colons in footnotes (see <https://github.com/github/cmark-gfm/issues/241> for more info)'
      );
      warningColonInFootnote = true;
    }

    if (!warningListInFootnote) {
      visit$1(node, 'list', () => {
        console.warn(
          '[mdast-util-gfm-footnote] Warning: Found a list in a footnote definition. GitHub currently crahes on lists in footnotes (see <https://github.com/github/cmark-gfm/issues/241> for more info)'
        );
        warningListInFootnote = true;
        return EXIT$1
      });
    }

    return value

    /** @type {Map} */
    function map(line, index, blank) {
      if (index) {
        return (blank ? '' : '    ') + line
      }

      return (blank ? label : label + ' ') + line
    }
  }
}

/**
 * @typedef {import('mdast').Delete} Delete
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-from-markdown').Handle} FromMarkdownHandle
 * @typedef {import('mdast-util-to-markdown').Options} ToMarkdownExtension
 * @typedef {import('mdast-util-to-markdown').Handle} ToMarkdownHandle
 */

/** @type {FromMarkdownExtension} */
const gfmStrikethroughFromMarkdown = {
  canContainEols: ['delete'],
  enter: {strikethrough: enterStrikethrough},
  exit: {strikethrough: exitStrikethrough}
};

/** @type {ToMarkdownExtension} */
const gfmStrikethroughToMarkdown = {
  unsafe: [{character: '~', inConstruct: 'phrasing'}],
  handlers: {delete: handleDelete}
};

handleDelete.peek = peekDelete;

/** @type {FromMarkdownHandle} */
function enterStrikethrough(token) {
  this.enter({type: 'delete', children: []}, token);
}

/** @type {FromMarkdownHandle} */
function exitStrikethrough(token) {
  this.exit(token);
}

/**
 * @type {ToMarkdownHandle}
 * @param {Delete} node
 */
function handleDelete(node, _, context) {
  const exit = context.enter('emphasis');
  const value = containerPhrasing(node, context, {before: '~', after: '~'});
  exit();
  return '~~' + value + '~~'
}

/** @type {ToMarkdownHandle} */
function peekDelete() {
  return '~'
}

/**
 * @typedef MarkdownTableOptions
 * @property {string|null|Array.<string|null|undefined>} [align]
 * @property {boolean} [padding=true]
 * @property {boolean} [delimiterStart=true]
 * @property {boolean} [delimiterStart=true]
 * @property {boolean} [delimiterEnd=true]
 * @property {boolean} [alignDelimiters=true]
 * @property {(value: string) => number} [stringLength]
 */

/**
 * Create a table from a matrix of strings.
 *
 * @param {Array.<Array.<string|null|undefined>>} table
 * @param {MarkdownTableOptions} [options]
 * @returns {string}
 */
function markdownTable(table, options) {
  const settings = options || {};
  const align = (settings.align || []).concat();
  const stringLength = settings.stringLength || defaultStringLength;
  /** @type {number[]} Character codes as symbols for alignment per column. */
  const alignments = [];
  let rowIndex = -1;
  /** @type {string[][]} Cells per row. */
  const cellMatrix = [];
  /** @type {number[][]} Sizes of each cell per row. */
  const sizeMatrix = [];
  /** @type {number[]} */
  const longestCellByColumn = [];
  let mostCellsPerRow = 0;
  /** @type {number} */
  let columnIndex;
  /** @type {string[]} Cells of current row */
  let row;
  /** @type {number[]} Sizes of current row */
  let sizes;
  /** @type {number} Sizes of current cell */
  let size;
  /** @type {string} Current cell */
  let cell;
  /** @type {string[]} Chunks of current line. */
  let line;
  /** @type {string} */
  let before;
  /** @type {string} */
  let after;
  /** @type {number} */
  let code;

  // This is a superfluous loop if we don’t align delimiters, but otherwise we’d
  // do superfluous work when aligning, so optimize for aligning.
  while (++rowIndex < table.length) {
    columnIndex = -1;
    row = [];
    sizes = [];

    if (table[rowIndex].length > mostCellsPerRow) {
      mostCellsPerRow = table[rowIndex].length;
    }

    while (++columnIndex < table[rowIndex].length) {
      cell = serialize(table[rowIndex][columnIndex]);

      if (settings.alignDelimiters !== false) {
        size = stringLength(cell);
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

  // Figure out which alignments to use.
  columnIndex = -1;

  if (typeof align === 'object' && 'length' in align) {
    while (++columnIndex < mostCellsPerRow) {
      alignments[columnIndex] = toAlignment(align[columnIndex]);
    }
  } else {
    code = toAlignment(align);

    while (++columnIndex < mostCellsPerRow) {
      alignments[columnIndex] = code;
    }
  }

  // Inject the alignment row.
  columnIndex = -1;
  row = [];
  sizes = [];

  while (++columnIndex < mostCellsPerRow) {
    code = alignments[columnIndex];
    before = '';
    after = '';

    if (code === 99 /* `c` */) {
      before = ':';
      after = ':';
    } else if (code === 108 /* `l` */) {
      before = ':';
    } else if (code === 114 /* `r` */) {
      after = ':';
    }

    // There *must* be at least one hyphen-minus in each alignment cell.
    size =
      settings.alignDelimiters === false
        ? 1
        : Math.max(
            1,
            longestCellByColumn[columnIndex] - before.length - after.length
          );

    cell = before + '-'.repeat(size) + after;

    if (settings.alignDelimiters !== false) {
      size = before.length + size + after.length;

      if (size > longestCellByColumn[columnIndex]) {
        longestCellByColumn[columnIndex] = size;
      }

      sizes[columnIndex] = size;
    }

    row[columnIndex] = cell;
  }

  // Inject the alignment row.
  cellMatrix.splice(1, 0, row);
  sizeMatrix.splice(1, 0, sizes);

  rowIndex = -1;
  /** @type {string[]} */
  const lines = [];

  while (++rowIndex < cellMatrix.length) {
    row = cellMatrix[rowIndex];
    sizes = sizeMatrix[rowIndex];
    columnIndex = -1;
    line = [];

    while (++columnIndex < mostCellsPerRow) {
      cell = row[columnIndex] || '';
      before = '';
      after = '';

      if (settings.alignDelimiters !== false) {
        size = longestCellByColumn[columnIndex] - (sizes[columnIndex] || 0);
        code = alignments[columnIndex];

        if (code === 114 /* `r` */) {
          before = ' '.repeat(size);
        } else if (code === 99 /* `c` */) {
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

      if (settings.delimiterStart !== false && !columnIndex) {
        line.push('|');
      }

      if (
        settings.padding !== false &&
        // Don’t add the opening space if we’re not aligning and the cell is
        // empty: there will be a closing space.
        !(settings.alignDelimiters === false && cell === '') &&
        (settings.delimiterStart !== false || columnIndex)
      ) {
        line.push(' ');
      }

      if (settings.alignDelimiters !== false) {
        line.push(before);
      }

      line.push(cell);

      if (settings.alignDelimiters !== false) {
        line.push(after);
      }

      if (settings.padding !== false) {
        line.push(' ');
      }

      if (
        settings.delimiterEnd !== false ||
        columnIndex !== mostCellsPerRow - 1
      ) {
        line.push('|');
      }
    }

    lines.push(
      settings.delimiterEnd === false
        ? line.join('').replace(/ +$/, '')
        : line.join('')
    );
  }

  return lines.join('\n')
}

/**
 * @param {string|null|undefined} [value]
 * @returns {string}
 */
function serialize(value) {
  return value === null || value === undefined ? '' : String(value)
}

/**
 * @param {string} value
 * @returns {number}
 */
function defaultStringLength(value) {
  return value.length
}

/**
 * @param {string|null|undefined} value
 * @returns {number}
 */
function toAlignment(value) {
  const code = typeof value === 'string' ? value.charCodeAt(0) : 0;

  return code === 67 /* `C` */ || code === 99 /* `c` */
    ? 99 /* `c` */
    : code === 76 /* `L` */ || code === 108 /* `l` */
    ? 108 /* `l` */
    : code === 82 /* `R` */ || code === 114 /* `r` */
    ? 114 /* `r` */
    : 0
}

/**
 * @typedef {import('mdast').AlignType} AlignType
 * @typedef {import('mdast').Table} Table
 * @typedef {import('mdast').TableRow} TableRow
 * @typedef {import('mdast').TableCell} TableCell
 * @typedef {import('mdast').InlineCode} InlineCode
 * @typedef {import('markdown-table').MarkdownTableOptions} MarkdownTableOptions
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-from-markdown').Handle} FromMarkdownHandle
 * @typedef {import('mdast-util-to-markdown').Options} ToMarkdownExtension
 * @typedef {import('mdast-util-to-markdown').Handle} ToMarkdownHandle
 * @typedef {import('mdast-util-to-markdown').Context} ToMarkdownContext
 *
 * @typedef Options
 * @property {boolean} [tableCellPadding=true]
 * @property {boolean} [tablePipeAlign=true]
 * @property {MarkdownTableOptions['stringLength']} [stringLength]
 */

/** @type {FromMarkdownExtension} */
const gfmTableFromMarkdown = {
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
};

/** @type {FromMarkdownHandle} */
function enterTable(token) {
  /** @type {AlignType[]} */
  // @ts-expect-error: `align` is custom.
  const align = token._align;
  this.enter({type: 'table', align, children: []}, token);
  this.setData('inTable', true);
}

/** @type {FromMarkdownHandle} */
function exitTable(token) {
  this.exit(token);
  this.setData('inTable');
}

/** @type {FromMarkdownHandle} */
function enterRow(token) {
  this.enter({type: 'tableRow', children: []}, token);
}

/** @type {FromMarkdownHandle} */
function exit(token) {
  this.exit(token);
}

/** @type {FromMarkdownHandle} */
function enterCell(token) {
  this.enter({type: 'tableCell', children: []}, token);
}

// Overwrite the default code text data handler to unescape escaped pipes when
// they are in tables.
/** @type {FromMarkdownHandle} */
function exitCodeText(token) {
  let value = this.resume();

  if (this.getData('inTable')) {
    value = value.replace(/\\([\\|])/g, replace);
  }

  const node = /** @type {InlineCode} */ (this.stack[this.stack.length - 1]);
  node.value = value;
  this.exit(token);
}

/**
 * @param {string} $0
 * @param {string} $1
 * @returns {string}
 */
function replace($0, $1) {
  // Pipes work, backslashes don’t (but can’t escape pipes).
  return $1 === '|' ? $1 : $0
}

/**
 * @param {Options} [options]
 * @returns {ToMarkdownExtension}
 */
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
      // A pipe, when followed by a tab or space (padding), or a dash or colon
      // (unpadded delimiter row), could result in a table.
      {atBreak: true, character: '|', after: '[\t :-]'},
      // A pipe in a cell must be encoded.
      {character: '|', inConstruct: 'tableCell'},
      // A colon must be followed by a dash, in which case it could start a
      // delimiter row.
      {atBreak: true, character: ':', after: '-'},
      // A delimiter row can also start with a dash, when followed by more
      // dashes, a colon, or a pipe.
      // This is a stricter version than the built in check for lists, thematic
      // breaks, and setex heading underlines though:
      // <https://github.com/syntax-tree/mdast-util-to-markdown/blob/51a2038/lib/unsafe.js#L57>
      {atBreak: true, character: '-', after: '[:|-]'}
    ],
    handlers: {
      table: handleTable,
      tableRow: handleTableRow,
      tableCell: handleTableCell,
      inlineCode: inlineCodeWithTable
    }
  }

  /**
   * @type {ToMarkdownHandle}
   * @param {Table} node
   */
  function handleTable(node, _, context) {
    // @ts-expect-error: fixed in `markdown-table@3.0.1`.
    return serializeData(handleTableAsData(node, context), node.align)
  }

  /**
   * This function isn’t really used normally, because we handle rows at the
   * table level.
   * But, if someone passes in a table row, this ensures we make somewhat sense.
   *
   * @type {ToMarkdownHandle}
   * @param {TableRow} node
   */
  function handleTableRow(node, _, context) {
    const row = handleTableRowAsData(node, context);
    // `markdown-table` will always add an align row
    const value = serializeData([row]);
    return value.slice(0, value.indexOf('\n'))
  }

  /**
   * @type {ToMarkdownHandle}
   * @param {TableCell} node
   */
  function handleTableCell(node, _, context) {
    const exit = context.enter('tableCell');
    const subexit = context.enter('phrasing');
    const value = containerPhrasing(node, context, {
      before: around,
      after: around
    });
    subexit();
    exit();
    return value
  }

  /**
   * @param {Array.<Array.<string>>} matrix
   * @param {Array.<string>} [align]
   */
  function serializeData(matrix, align) {
    return markdownTable(matrix, {
      align,
      alignDelimiters,
      padding,
      stringLength
    })
  }

  /**
   * @param {Table} node
   * @param {ToMarkdownContext} context
   */
  function handleTableAsData(node, context) {
    const children = node.children;
    let index = -1;
    /** @type {Array.<Array.<string>>} */
    const result = [];
    const subexit = context.enter('table');

    while (++index < children.length) {
      result[index] = handleTableRowAsData(children[index], context);
    }

    subexit();

    return result
  }

  /**
   * @param {TableRow} node
   * @param {ToMarkdownContext} context
   */
  function handleTableRowAsData(node, context) {
    const children = node.children;
    let index = -1;
    /** @type {Array.<string>} */
    const result = [];
    const subexit = context.enter('tableRow');

    while (++index < children.length) {
      result[index] = handleTableCell(children[index], node, context);
    }

    subexit();

    return result
  }

  /**
   * @type {ToMarkdownHandle}
   * @param {InlineCode} node
   */
  function inlineCodeWithTable(node, parent, context) {
    let value = inlineCode(node, parent, context);

    if (context.stack.includes('tableCell')) {
      value = value.replace(/\|/g, '\\$&');
    }

    return value
  }
}

/**
 * @typedef {import('mdast').ListItem} ListItem
 * @typedef {import('mdast').Paragraph} Paragraph
 * @typedef {import('mdast').BlockContent} BlockContent
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-from-markdown').Handle} FromMarkdownHandle
 * @typedef {import('mdast-util-to-markdown').Options} ToMarkdownExtension
 * @typedef {import('mdast-util-to-markdown').Handle} ToMarkdownHandle
 */

/** @type {FromMarkdownExtension} */
const gfmTaskListItemFromMarkdown = {
  exit: {
    taskListCheckValueChecked: exitCheck,
    taskListCheckValueUnchecked: exitCheck,
    paragraph: exitParagraphWithTaskListItem
  }
};

/** @type {ToMarkdownExtension} */
const gfmTaskListItemToMarkdown = {
  unsafe: [{atBreak: true, character: '-', after: '[:|-]'}],
  handlers: {listItem: listItemWithTaskListItem}
};

/** @type {FromMarkdownHandle} */
function exitCheck(token) {
  // We’re always in a paragraph, in a list item.
  this.stack[this.stack.length - 2].checked =
    token.type === 'taskListCheckValueChecked';
}

/** @type {FromMarkdownHandle} */
function exitParagraphWithTaskListItem(token) {
  const parent = this.stack[this.stack.length - 2];
  /** @type {Paragraph} */
  // @ts-expect-error: must be true.
  const node = this.stack[this.stack.length - 1];
  /** @type {BlockContent[]} */
  // @ts-expect-error: check whether `parent` is a `listItem` later.
  const siblings = parent.children;
  const head = node.children[0];
  let index = -1;
  /** @type {Paragraph|undefined} */
  let firstParaghraph;

  if (
    parent &&
    parent.type === 'listItem' &&
    typeof parent.checked === 'boolean' &&
    head &&
    head.type === 'text'
  ) {
    while (++index < siblings.length) {
      const sibling = siblings[index];
      if (sibling.type === 'paragraph') {
        firstParaghraph = sibling;
        break
      }
    }

    if (firstParaghraph === node) {
      // Must start with a space or a tab.
      head.value = head.value.slice(1);

      if (head.value.length === 0) {
        node.children.shift();
      } else {
        // @ts-expect-error: must be true.
        head.position.start.column++;
        // @ts-expect-error: must be true.
        head.position.start.offset++;
        // @ts-expect-error: must be true.
        node.position.start = Object.assign({}, head.position.start);
      }
    }
  }

  this.exit(token);
}

/**
 * @type {ToMarkdownHandle}
 * @param {ListItem} node
 */
function listItemWithTaskListItem(node, parent, context) {
  const head = node.children[0];
  let value = listItem(node, parent, context);

  if (typeof node.checked === 'boolean' && head && head.type === 'paragraph') {
    value = value.replace(/^(?:[*+-]|\d+\.)([\r\n]| {1,3})/, check);
  }

  return value

  /**
   * @param {string} $0
   * @returns {string}
   */
  function check($0) {
    return $0 + '[' + (node.checked ? 'x' : ' ') + '] '
  }
}

/**
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-to-markdown').Options} ToMarkdownExtension
 *
 * @typedef {import('mdast-util-gfm-table').Options} Options
 */

/**
 * @returns {Array.<FromMarkdownExtension>}
 */
function gfmFromMarkdown() {
  return [
    gfmAutolinkLiteralFromMarkdown,
    gfmFootnoteFromMarkdown(),
    gfmStrikethroughFromMarkdown,
    gfmTableFromMarkdown,
    gfmTaskListItemFromMarkdown
  ]
}

/**
 * @param {Options} [options]
 * @returns {ToMarkdownExtension}
 */
function gfmToMarkdown(options) {
  return {
    extensions: [
      gfmAutolinkLiteralToMarkdown,
      gfmFootnoteToMarkdown(),
      gfmStrikethroughToMarkdown,
      gfmTableToMarkdown(options),
      gfmTaskListItemToMarkdown
    ]
  }
}

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('micromark-extension-gfm').Options & import('mdast-util-gfm').Options} Options
 */

/**
 * Plugin to support GitHub Flavored Markdown (GFM).
 *
 * @type {import('unified').Plugin<[Options?]|void[], Root>}
 */
function remarkGfm(options = {}) {
  const data = this.data();

  add('micromarkExtensions', gfm(options));
  add('fromMarkdownExtensions', gfmFromMarkdown());
  add('toMarkdownExtensions', gfmToMarkdown(options));

  /**
   * @param {string} field
   * @param {unknown} value
   */
  function add(field, value) {
    const list = /** @type {unknown[]} */ (
      // Other extensions
      /* c8 ignore next 2 */
      data[field] ? data[field] : (data[field] = [])
    );

    list.push(value);
  }
}

/**
 * @typedef {import('unist').Point} Point
 * @typedef {import('vfile').VFile} VFile
 *
 * @typedef {Pick<Point, 'line'|'column'>} PositionalPoint
 * @typedef {Required<Point>} FullPoint
 * @typedef {NonNullable<Point['offset']>} Offset
 */

/**
 * Get transform functions for the given `document`.
 *
 * @param {string|Uint8Array|VFile} file
 */
function location(file) {
  var value = String(file);
  /** @type {Array.<number>} */
  var indices = [];
  var search = /\r?\n|\r/g;

  while (search.test(value)) {
    indices.push(search.lastIndex);
  }

  indices.push(value.length + 1);

  return {toPoint, toOffset}

  /**
   * Get the line and column-based `point` for `offset` in the bound indices.
   * Returns a point with `undefined` values when given invalid or out of bounds
   * input.
   *
   * @param {Offset} offset
   * @returns {FullPoint}
   */
  function toPoint(offset) {
    var index = -1;

    if (offset > -1 && offset < indices[indices.length - 1]) {
      while (++index < indices.length) {
        if (indices[index] > offset) {
          return {
            line: index + 1,
            column: offset - (indices[index - 1] || 0) + 1,
            offset
          }
        }
      }
    }

    return {line: undefined, column: undefined, offset: undefined}
  }

  /**
   * Get the `offset` for a line and column-based `point` in the bound indices.
   * Returns `-1` when given invalid or out of bounds input.
   *
   * @param {PositionalPoint} point
   * @returns {Offset}
   */
  function toOffset(point) {
    var line = point && point.line;
    var column = point && point.column;
    /** @type {number} */
    var offset;

    if (
      typeof line === 'number' &&
      typeof column === 'number' &&
      !Number.isNaN(line) &&
      !Number.isNaN(column) &&
      line - 1 in indices
    ) {
      offset = (indices[line - 2] || 0) + column - 1 || 0;
    }

    return offset > -1 && offset < indices[indices.length - 1] ? offset : -1
  }
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('unist-util-is').Test} Test
 * @typedef {import('unist-util-visit-parents').VisitorResult} VisitorResult
 */

const visit =
  /**
   * @type {(
   *   (<T extends Node>(tree: Node, test: T['type']|Partial<T>|import('unist-util-is').TestFunctionPredicate<T>|Array.<T['type']|Partial<T>|import('unist-util-is').TestFunctionPredicate<T>>, visitor: Visitor<T>, reverse?: boolean) => void) &
   *   ((tree: Node, test: Test, visitor: Visitor<Node>, reverse?: boolean) => void) &
   *   ((tree: Node, visitor: Visitor<Node>, reverse?: boolean) => void)
   * )}
   */
  (
    /**
     * Visit children of tree which pass a test
     *
     * @param {Node} tree Abstract syntax tree to walk
     * @param {Test} test test Test node
     * @param {Visitor<Node>} visitor Function to run for each node
     * @param {boolean} [reverse] Fisit the tree in reverse, defaults to false
     */
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor;
        visitor = test;
        test = null;
      }

      visitParents(tree, test, overload, reverse);

      /**
       * @param {Node} node
       * @param {Array.<Parent>} parents
       */
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

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('unist').Point} Point
 * @typedef {import('unist-util-is').Test} Test
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('vfile-message').VFileMessage} VFileMessage
 *
 * @typedef {OptionsWithoutReset|OptionsWithReset} Options
 * @typedef {OptionsBaseFields & OptionsWithoutResetFields} OptionsWithoutReset
 * @typedef {OptionsBaseFields & OptionsWithResetFields} OptionsWithReset
 *
 * @typedef OptionsWithoutResetFields
 * @property {false} [reset]
 *   Whether to treat all messages as turned off initially.
 * @property {string[]} [disable]
 *   List of `ruleId`s to turn off.
 *
 * @typedef OptionsWithResetFields
 * @property {true} reset
 *   Whether to treat all messages as turned off initially.
 * @property {string[]} [enable]
 *   List of `ruleId`s to initially turn on.
 *
 * @typedef OptionsBaseFields
 * @property {string} name
 *   Name of markers that can control the message sources.
 *
 *   For example, `{name: 'alpha'}` controls `alpha` markers:
 *
 *   ```html
 *   <!--alpha ignore-->
 *   ```
 * @property {MarkerParser} marker
 *   Parse a possible marker to a comment marker object (Marker).
 *   If the marker isn't a marker, should return `null`.
 * @property {Test} [test]
 *   Test for possible markers
 * @property {string[]} [known]
 *   List of allowed `ruleId`s. When given a warning is shown
 *   when someone tries to control an unknown rule.
 *
 *   For example, `{name: 'alpha', known: ['bravo']}` results in a warning if
 *   `charlie` is configured:
 *
 *   ```html
 *   <!--alpha ignore charlie-->
 *   ```
 * @property {string|string[]} [source]
 *   Sources that can be controlled with `name` markers.
 *   Defaults to `name`.
 *
 * @callback MarkerParser
 *   Parse a possible comment marker node to a Marker.
 * @param {Node} node
 *   Node to parse
 *
 * @typedef Marker
 *   A comment marker.
 * @property {string} name
 *   Name of marker.
 * @property {string} attributes
 *   Value after name.
 * @property {Record<string, string|number|boolean>} parameters
 *   Parsed attributes.
 * @property {Node} node
 *   Reference to given node.
 *
 * @typedef Mark
 * @property {Point|undefined} point
 * @property {boolean} state
 */

const own$2 = {}.hasOwnProperty;

/**
 * @type {import('unified').Plugin<[Options]>}
 * @returns {(tree: Node, file: VFile) => void}
 */
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

  /**
   * @param {Node} tree
   * @param {VFile} file
   */
  function transformer(tree, file) {
    const toOffset = location(file).toOffset;
    const initial = !reset;
    const gaps = detectGaps(tree, file);
    /** @type {Record<string, Mark[]>} */
    const scope = {};
    /** @type {Mark[]} */
    const globals = [];

    visit(tree, options.test, visitor);

    file.messages = file.messages.filter((m) => filter(m));

    /**
     * @param {Node} node
     * @param {number|null} position
     * @param {Parent|null} parent
     */
    function visitor(node, position, parent) {
      /** @type {Marker|null} */
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

      /** @type {string} */
      // @ts-expect-error: we’ll check for unknown values next.
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

      // Apply to all rules.
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

    /**
     * @param {VFileMessage} message
     * @returns {boolean}
     */
    function filter(message) {
      let gapIndex = gaps.length;

      // Keep messages from a different source.
      if (!message.source || !sources.includes(message.source)) {
        return true
      }

      // We only ignore messages if they‘re disabled, *not* when they’re not in
      // the document.
      if (!message.line) {
        message.line = 1;
      }

      if (!message.column) {
        message.column = 1;
      }

      // Check whether the warning is inside a gap.
      // @ts-expect-error: we just normalized `null` to `number`s.
      const offset = toOffset(message);

      while (gapIndex--) {
        if (gaps[gapIndex][0] <= offset && gaps[gapIndex][1] > offset) {
          return false
        }
      }

      // Check whether allowed by specific and global states.
      return (
        (!message.ruleId ||
          check(message, scope[message.ruleId], message.ruleId)) &&
        check(message, globals)
      )
    }

    /**
     * Helper to check (and possibly warn) if a `ruleId` is unknown.
     *
     * @param {string} ruleId
     * @param {string} verb
     * @param {Node} node
     * @returns {boolean}
     */
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

    /**
     * Get the latest state of a rule.
     * When without `ruleId`, gets global state.
     *
     * @param {string|undefined} ruleId
     * @returns {boolean}
     */
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

    /**
     * Handle a rule.
     *
     * @param {Point|undefined} point
     * @param {boolean} state
     * @param {string|undefined} [ruleId]
     * @returns {void}
     */
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

      // Toggle all known rules.
      if (!ruleId) {
        for (ruleId in scope) {
          if (own$2.call(scope, ruleId)) {
            toggle(point, state, ruleId);
          }
        }
      }
    }

    /**
     * Check all `ranges` for `message`.
     *
     * @param {VFileMessage} message
     * @param {Mark[]|undefined} ranges
     * @param {string|undefined} [ruleId]
     * @returns {boolean}
     */
    function check(message, ranges, ruleId) {
      if (ranges && ranges.length > 0) {
        // Check the state at the message’s position.
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

      // The first marker ocurred after the first message, so we check the
      // initial state.
      if (!ruleId) {
        return Boolean(initial || reset)
      }

      return reset ? enable.includes(ruleId) : !disable.includes(ruleId)
    }
  }
}

/**
 * Detect gaps in `tree`.
 *
 * @param {Node} tree
 * @param {VFile} file
 */
function detectGaps(tree, file) {
  /** @type {Node[]} */
  // @ts-expect-error: fine.
  const children = tree.children || [];
  const lastNode = children[children.length - 1];
  /** @type {[number, number][]} */
  const gaps = [];
  let offset = 0;
  /** @type {boolean|undefined} */
  let gap;

  // Find all gaps.
  visit(tree, one);

  // Get the end of the document.
  // This detects if the last node was the last node.
  // If not, there’s an extra gap between the last node and the end of the
  // document.
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

  /**
   * @param {Node} node
   */
  function one(node) {
    update(node.position && node.position.start && node.position.start.offset);

    if (!('children' in node)) {
      update(node.position && node.position.end && node.position.end.offset);
    }
  }

  /**
   * Detect a new position.
   *
   * @param {number|undefined} [latest]
   * @returns {void}
   */
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

/**
 * @typedef {string|number|boolean} MarkerParameterValue
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Content} Content
 * @typedef {import('mdast').HTML} HTML
 * @typedef {import('mdast-util-mdx-expression').MDXFlowExpression} MDXFlowExpression
 * @typedef {import('mdast-util-mdx-expression').MDXTextExpression} MDXTextExpression
 * @typedef {Root|Content} Node
 * @typedef {Object.<string, MarkerParameterValue>} MarkerParameters
 *
 * @typedef Mdx1CommentNode
 * @property {'comment'} type
 * @property {string} value
 *
 * @typedef Marker
 * @property {string} name
 * @property {string} attributes
 * @property {MarkerParameters|null} parameters
 * @property {HTML|Mdx1CommentNode|MDXFlowExpression|MDXTextExpression} node
 */

const commentExpression = /\s*([a-zA-Z\d-]+)(\s+([\s\S]*))?\s*/;
const esCommentExpression = new RegExp(
  '(\\s*\\/\\*' + commentExpression.source + '\\*\\/\\s*)'
);
const markerExpression = new RegExp(
  '(\\s*<!--' + commentExpression.source + '-->\\s*)'
);

/**
 * Parse a comment marker.
 * @param {unknown} value
 * @returns {Marker|null}
 */
function commentMarker(value) {
  if (
    isNode(value) &&
    (value.type === 'html' ||
      // @ts-expect-error: MDX@1
      value.type === 'comment' ||
      value.type === 'mdxFlowExpression' ||
      value.type === 'mdxTextExpression')
  ) {
    let offset = 2;
    /** @type {RegExpMatchArray|null|undefined} */
    let match;

    // @ts-expect-error: MDX@1
    if (value.type === 'comment') {
      // @ts-expect-error: MDX@1
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

/**
 * Parse `value` into an object.
 *
 * @param {string} value
 * @returns {MarkerParameters|null}
 */
function parseParameters(value) {
  /** @type {MarkerParameters} */
  const parameters = {};

  return value
    .replace(
      /\s+([-\w]+)(?:=(?:"((?:\\[\s\S]|[^"])+)"|'((?:\\[\s\S]|[^'])+)'|((?:\\[\s\S]|[^"'\s])+)))?/gi,
      replacer
    )
    .replace(/\s+/g, '')
    ? null
    : parameters

  /**
   * @param {string} _
   * @param {string} $1
   * @param {string} $2
   * @param {string} $3
   * @param {string} $4
   */
  // eslint-disable-next-line max-params
  function replacer(_, $1, $2, $3, $4) {
    /** @type {MarkerParameterValue} */
    let value = $2 || $3 || $4 || '';

    if (value === 'true' || value === '') {
      value = true;
    } else if (value === 'false') {
      value = false;
    } else if (!Number.isNaN(Number(value))) {
      value = Number(value);
    }

    parameters[$1] = value;

    return ''
  }
}

/**
 * @param {unknown} value
 * @returns {value is Node}
 */
function isNode(value) {
  return Boolean(value && typeof value === 'object' && 'type' in value)
}

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('unified-message-control')} MessageControl
 * @typedef {Omit<import('unified-message-control').OptionsWithoutReset, 'marker'>|Omit<import('unified-message-control').OptionsWithReset, 'marker'>} Options
 */

const test = [
  'html', // Comments are `html` nodes in mdast.
  'comment', // In MDX@1, comments have their own node.
  'mdxFlowExpression', // In MDX@2, comments exist in bracketed expressions.
  'mdxTextExpression'
];

/**
 * Plugin to enable, disable, and ignore messages.
 *
 * @type {import('unified').Plugin<[Options], Root>}
 * @returns {(node: Root, file: VFile) => void}
 */
function remarkMessageControl(options) {
  return messageControl(
    Object.assign({marker: commentMarker, test}, options)
  )
}

/**
 * @typedef {import('mdast').Root} Root
 */

/**
 * The core plugin for `remark-lint`.
 * This adds support for ignoring stuff from messages (`<!--lint ignore-->`).
 * All rules are in their own packages and presets.
 *
 * @type {import('unified').Plugin<void[], Root>}
 */
function remarkLint() {
  this.use(lintMessageControl);
}

/** @type {import('unified').Plugin<void[], Root>} */
function lintMessageControl() {
  return remarkMessageControl({name: 'lint', source: 'remark-lint'})
}

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('vfile').VFile} VFile
 *
 * @typedef {0|1|2} Severity
 * @typedef {'warn'|'on'|'off'|'error'} Label
 * @typedef {[Severity, ...unknown[]]} SeverityTuple
 *
 * @typedef RuleMeta
 * @property {string} origin name of the lint rule
 * @property {string} [url] link to documentation
 *
 * @callback Rule
 * @param {Node} tree
 * @param {VFile} file
 * @param {unknown} options
 * @returns {void}
 */

const primitives = new Set(['string', 'number', 'boolean']);

/**
 * @param {string|RuleMeta} meta
 * @param {Rule} rule
 */
function lintRule(meta, rule) {
  const id = typeof meta === 'string' ? meta : meta.origin;
  const url = typeof meta === 'string' ? undefined : meta.url;
  const parts = id.split(':');
  // Possibly useful if externalised later.
  /* c8 ignore next */
  const source = parts[1] ? parts[0] : undefined;
  const ruleId = parts[1];

  Object.defineProperty(plugin, 'name', {value: id});

  return plugin

  /** @type {import('unified').Plugin<[unknown]|void[]>} */
  function plugin(raw) {
    const [severity, options] = coerce$1(ruleId, raw);

    if (!severity) return

    const fatal = severity === 2;

    return (tree, file, next) => {
      let index = file.messages.length - 1;

      wrap(rule, (error) => {
        const messages = file.messages;

        // Add the error, if not already properly added.
        // Only happens for incorrect plugins.
        /* c8 ignore next 6 */
        // @ts-expect-error: errors could be `messages`.
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

/**
 * Coerce a value to a severity--options tuple.
 *
 * @param {string} name
 * @param {unknown} value
 * @returns {SeverityTuple}
 */
function coerce$1(name, value) {
  /** @type {unknown[]} */
  let result;

  if (typeof value === 'boolean') {
    result = [value];
  } else if (value === null || value === undefined) {
    result = [1];
  } else if (
    Array.isArray(value) &&
    // `isArray(unknown)` is turned into `any[]`:
    // type-coverage:ignore-next-line
    primitives.has(typeof value[0])
  ) {
    // `isArray(unknown)` is turned into `any[]`:
    // type-coverage:ignore-next-line
    result = [...value];
  } else {
    result = [1, value];
  }

  let level = result[0];

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

  if (typeof level !== 'number' || level < 0 || level > 2) {
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

  // @ts-expect-error: it’s now a valid tuple.
  return result
}

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module final-newline
 * @fileoverview
 *   Warn when a line feed at the end of a file is missing.
 *   Empty files are allowed.
 *
 *   See [StackExchange](https://unix.stackexchange.com/questions/18743) for why.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   always adds a final line feed to files.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 *   ## Example
 *
 *   ##### `ok.md`
 *
 *   ###### In
 *
 *   Note: `␊` represents LF.
 *
 *   ```markdown
 *   Alpha␊
 *   ```
 *
 *   ###### Out
 *
 *   No messages.
 *
 *   ##### `not-ok.md`
 *
 *   ###### In
 *
 *   Note: The below file does not have a final newline.
 *
 *   ```markdown
 *   Bravo
 *   ```
 *
 *   ###### Out
 *
 *   ```text
 *   1:1: Missing newline character at end of file
 *   ```
 */

const remarkLintFinalNewline = lintRule(
  {
    origin: 'remark-lint:final-newline',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-final-newline#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (_, file) => {
    const value = String(file);
    const last = value.length - 1;

    if (last > -1 && value.charAt(last) !== '\n') {
      file.message('Missing newline character at end of file');
    }
  }
);

var remarkLintFinalNewline$1 = remarkLintFinalNewline;

var pluralize = {exports: {}};

/* global define */

(function (module, exports) {
(function (root, pluralize) {
  /* istanbul ignore else */
  if (typeof commonjsRequire === 'function' && 'object' === 'object' && 'object' === 'object') {
    // Node.
    module.exports = pluralize();
  } else {
    // Browser global.
    root.pluralize = pluralize();
  }
})(commonjsGlobal, function () {
  // Rule storage - pluralize and singularize need to be run sequentially,
  // while other rules can be optimized using an object for instant lookups.
  var pluralRules = [];
  var singularRules = [];
  var uncountables = {};
  var irregularPlurals = {};
  var irregularSingles = {};

  /**
   * Sanitize a pluralization rule to a usable regular expression.
   *
   * @param  {(RegExp|string)} rule
   * @return {RegExp}
   */
  function sanitizeRule (rule) {
    if (typeof rule === 'string') {
      return new RegExp('^' + rule + '$', 'i');
    }

    return rule;
  }

  /**
   * Pass in a word token to produce a function that can replicate the case on
   * another word.
   *
   * @param  {string}   word
   * @param  {string}   token
   * @return {Function}
   */
  function restoreCase (word, token) {
    // Tokens are an exact match.
    if (word === token) return token;

    // Lower cased words. E.g. "hello".
    if (word === word.toLowerCase()) return token.toLowerCase();

    // Upper cased words. E.g. "WHISKY".
    if (word === word.toUpperCase()) return token.toUpperCase();

    // Title cased words. E.g. "Title".
    if (word[0] === word[0].toUpperCase()) {
      return token.charAt(0).toUpperCase() + token.substr(1).toLowerCase();
    }

    // Lower cased words. E.g. "test".
    return token.toLowerCase();
  }

  /**
   * Interpolate a regexp string.
   *
   * @param  {string} str
   * @param  {Array}  args
   * @return {string}
   */
  function interpolate (str, args) {
    return str.replace(/\$(\d{1,2})/g, function (match, index) {
      return args[index] || '';
    });
  }

  /**
   * Replace a word using a rule.
   *
   * @param  {string} word
   * @param  {Array}  rule
   * @return {string}
   */
  function replace (word, rule) {
    return word.replace(rule[0], function (match, index) {
      var result = interpolate(rule[1], arguments);

      if (match === '') {
        return restoreCase(word[index - 1], result);
      }

      return restoreCase(match, result);
    });
  }

  /**
   * Sanitize a word by passing in the word and sanitization rules.
   *
   * @param  {string}   token
   * @param  {string}   word
   * @param  {Array}    rules
   * @return {string}
   */
  function sanitizeWord (token, word, rules) {
    // Empty string or doesn't need fixing.
    if (!token.length || uncountables.hasOwnProperty(token)) {
      return word;
    }

    var len = rules.length;

    // Iterate over the sanitization rules and use the first one to match.
    while (len--) {
      var rule = rules[len];

      if (rule[0].test(word)) return replace(word, rule);
    }

    return word;
  }

  /**
   * Replace a word with the updated word.
   *
   * @param  {Object}   replaceMap
   * @param  {Object}   keepMap
   * @param  {Array}    rules
   * @return {Function}
   */
  function replaceWord (replaceMap, keepMap, rules) {
    return function (word) {
      // Get the correct token and case restoration functions.
      var token = word.toLowerCase();

      // Check against the keep object map.
      if (keepMap.hasOwnProperty(token)) {
        return restoreCase(word, token);
      }

      // Check against the replacement map for a direct word replacement.
      if (replaceMap.hasOwnProperty(token)) {
        return restoreCase(word, replaceMap[token]);
      }

      // Run all the rules against the word.
      return sanitizeWord(token, word, rules);
    };
  }

  /**
   * Check if a word is part of the map.
   */
  function checkWord (replaceMap, keepMap, rules, bool) {
    return function (word) {
      var token = word.toLowerCase();

      if (keepMap.hasOwnProperty(token)) return true;
      if (replaceMap.hasOwnProperty(token)) return false;

      return sanitizeWord(token, token, rules) === token;
    };
  }

  /**
   * Pluralize or singularize a word based on the passed in count.
   *
   * @param  {string}  word      The word to pluralize
   * @param  {number}  count     How many of the word exist
   * @param  {boolean} inclusive Whether to prefix with the number (e.g. 3 ducks)
   * @return {string}
   */
  function pluralize (word, count, inclusive) {
    var pluralized = count === 1
      ? pluralize.singular(word) : pluralize.plural(word);

    return (inclusive ? count + ' ' : '') + pluralized;
  }

  /**
   * Pluralize a word.
   *
   * @type {Function}
   */
  pluralize.plural = replaceWord(
    irregularSingles, irregularPlurals, pluralRules
  );

  /**
   * Check if a word is plural.
   *
   * @type {Function}
   */
  pluralize.isPlural = checkWord(
    irregularSingles, irregularPlurals, pluralRules
  );

  /**
   * Singularize a word.
   *
   * @type {Function}
   */
  pluralize.singular = replaceWord(
    irregularPlurals, irregularSingles, singularRules
  );

  /**
   * Check if a word is singular.
   *
   * @type {Function}
   */
  pluralize.isSingular = checkWord(
    irregularPlurals, irregularSingles, singularRules
  );

  /**
   * Add a pluralization rule to the collection.
   *
   * @param {(string|RegExp)} rule
   * @param {string}          replacement
   */
  pluralize.addPluralRule = function (rule, replacement) {
    pluralRules.push([sanitizeRule(rule), replacement]);
  };

  /**
   * Add a singularization rule to the collection.
   *
   * @param {(string|RegExp)} rule
   * @param {string}          replacement
   */
  pluralize.addSingularRule = function (rule, replacement) {
    singularRules.push([sanitizeRule(rule), replacement]);
  };

  /**
   * Add an uncountable word rule.
   *
   * @param {(string|RegExp)} word
   */
  pluralize.addUncountableRule = function (word) {
    if (typeof word === 'string') {
      uncountables[word.toLowerCase()] = true;
      return;
    }

    // Set singular and plural references for the word.
    pluralize.addPluralRule(word, '$0');
    pluralize.addSingularRule(word, '$0');
  };

  /**
   * Add an irregular word definition.
   *
   * @param {string} single
   * @param {string} plural
   */
  pluralize.addIrregularRule = function (single, plural) {
    plural = plural.toLowerCase();
    single = single.toLowerCase();

    irregularSingles[single] = plural;
    irregularPlurals[plural] = single;
  };

  /**
   * Irregular rules.
   */
  [
    // Pronouns.
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
    // Words ending in with a consonant and `o`.
    ['echo', 'echoes'],
    ['dingo', 'dingoes'],
    ['volcano', 'volcanoes'],
    ['tornado', 'tornadoes'],
    ['torpedo', 'torpedoes'],
    // Ends with `us`.
    ['genus', 'genera'],
    ['viscus', 'viscera'],
    // Ends with `ma`.
    ['stigma', 'stigmata'],
    ['stoma', 'stomata'],
    ['dogma', 'dogmata'],
    ['lemma', 'lemmata'],
    ['schema', 'schemata'],
    ['anathema', 'anathemata'],
    // Other irregular rules.
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

  /**
   * Pluralization rules.
   */
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

  /**
   * Singularization rules.
   */
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

  /**
   * Uncountable rules.
   */
  [
    // Singular words with no plurals.
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
    // Regexes.
    /[^aeiou]ese$/i, // "chinese", "japanese"
    /deer$/i, // "deer", "reindeer"
    /fish$/i, // "fish", "blowfish", "angelfish"
    /measles$/i,
    /o[iu]s$/i, // "carnivorous"
    /pox$/i, // "chickpox", "smallpox"
    /sheep$/i
  ].forEach(pluralize.addUncountableRule);

  return pluralize;
});
}(pluralize));

var plural = pluralize.exports;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module list-item-bullet-indent
 * @fileoverview
 *   Warn when list item bullets are indented.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   removes all indentation before bullets.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit$1(tree, 'list', (list, _, grandparent) => {
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

/**
 * @typedef {import('unist').Position} Position
 * @typedef {import('unist').Point} Point
 *
 * @typedef {Partial<Point>} PointLike
 *
 * @typedef {Object} PositionLike
 * @property {PointLike} [start]
 * @property {PointLike} [end]
 *
 * @typedef {Object} NodeLike
 * @property {PositionLike} [position]
 */

var pointStart = point('start');
var pointEnd = point('end');

/**
 * Get the positional info of `node`.
 *
 * @param {'start'|'end'} type
 */
function point(type) {
  return point

  /**
   * Get the positional info of `node`.
   *
   * @param {NodeLike} [node]
   * @returns {Point}
   */
  function point(node) {
    /** @type {Point} */
    // @ts-ignore looks like a point
    var point = (node && node.position && node.position[type]) || {};

    return {
      line: point.line || null,
      column: point.column || null,
      offset: point.offset > -1 ? point.offset : null
    }
  }
}

/**
 * @typedef {Object} PointLike
 * @property {number} [line]
 * @property {number} [column]
 * @property {number} [offset]
 *
 * @typedef {Object} PositionLike
 * @property {PointLike} [start]
 * @property {PointLike} [end]
 *
 * @typedef {Object} NodeLike
 * @property {PositionLike} [position]
 */

/**
 * Check if `node` is *generated*.
 *
 * @param {NodeLike} [node]
 * @returns {boolean}
 */
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
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module list-item-indent
 * @fileoverview
 *   Warn when the spacing between a list item’s bullet and its content violates
 *   a given style.
 *
 *   Options: `'tab-size'`, `'mixed'`, or `'space'`, default: `'tab-size'`.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   uses `'tab-size'` (named `'tab'` there) by default to ensure Markdown is
 *   seen the same way across vendors.
 *   This can be configured with the
 *   [`listItemIndent`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionslistitemindent)
 *   option.
 *   This rule’s `'space'` option is named `'1'` there.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
 *   {"name": "ok.md", "setting": "mixed"}
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
 *   {"name": "ok.md", "setting": "space"}
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
 *   {"name": "not-ok.md", "setting": "space", "label": "input"}
 *
 *   *···List
 *   ····item.
 *
 * @example
 *   {"name": "not-ok.md", "setting": "space", "label": "output"}
 *
 *    1:5: Incorrect list-item indent: remove 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "setting": "tab-size", "label": "input"}
 *
 *   *·List
 *   ··item.
 *
 * @example
 *   {"name": "not-ok.md", "setting": "tab-size", "label": "output"}
 *
 *    1:3: Incorrect list-item indent: add 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "setting": "mixed", "label": "input"}
 *
 *   *···List item.
 *
 * @example
 *   {"name": "not-ok.md", "setting": "mixed", "label": "output"}
 *
 *    1:5: Incorrect list-item indent: remove 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "setting": "💩", "label": "output", "positionless": true}
 *
 *    1:1: Incorrect list-item indent style `💩`: use either `'tab-size'`, `'space'`, or `'mixed'`
 */

const remarkLintListItemIndent = lintRule(
  {
    origin: 'remark-lint:list-item-indent',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-list-item-indent#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'tab-size') => {
    const value = String(file);

    if (option !== 'tab-size' && option !== 'space' && option !== 'mixed') {
      file.fail(
        'Incorrect list-item indent style `' +
          option +
          "`: use either `'tab-size'`, `'space'`, or `'mixed'`"
      );
    }

    visit$1(tree, 'list', (node) => {
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-blockquote-without-marker
 * @fileoverview
 *   Warn when blank lines without `>` (greater than) markers are found in a
 *   block quote.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   adds markers to every line in a block quote.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file);
    const loc = location(file);

    visit$1(tree, 'blockquote', (node) => {
      let index = -1;

      while (++index < node.children.length) {
        const child = node.children[index];

        if (child.type === 'paragraph' && !generated(child)) {
          const end = pointEnd(child).line;
          const column = pointStart(child).column;
          let line = pointStart(child).line;

          // Skip past the first line.
          while (++line <= end) {
            const offset = loc.toOffset({line, column});

            if (/>[\t ]+$/.test(value.slice(offset - 5, offset))) {
              continue
            }

            // Roughly here.
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-literal-urls
 * @fileoverview
 *   Warn for literal URLs in text.
 *   URLs are treated as links in some Markdown vendors, but not in others.
 *   To make sure they are always linked, wrap them in `<` (less than) and `>`
 *   (greater than).
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   never creates literal URLs and always uses `<` (less than) and `>`
 *   (greater than).
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit$1(tree, 'link', (node) => {
      const value = toString(node);

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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module ordered-list-marker-style
 * @fileoverview
 *   Warn when the list item marker style of ordered lists violate a given style.
 *
 *   Options: `'consistent'`, `'.'`, or `')'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used list style and warns when subsequent
 *   lists use different styles.
 *
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
 *   {"name": "ok.md", "setting": "."}
 *
 *   1.  Foo
 *
 *   2.  Bar
 *
 * @example
 *   {"name": "ok.md", "setting": ")"}
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
 *   {"name": "not-ok.md", "label": "output", "setting": "💩", "positionless": true}
 *
 *   1:1: Incorrect ordered list item marker style `💩`: use either `'.'` or `')'`
 */

const remarkLintOrderedListMarkerStyle = lintRule(
  {
    origin: 'remark-lint:ordered-list-marker-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-ordered-list-marker-style#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const value = String(file);

    if (option !== 'consistent' && option !== '.' && option !== ')') {
      file.fail(
        'Incorrect ordered list item marker style `' +
          option +
          "`: use either `'.'` or `')'`"
      );
    }

    visit$1(tree, 'list', (node) => {
      let index = -1;

      if (!node.ordered) return

      while (++index < node.children.length) {
        const child = node.children[index];

        if (!generated(child)) {
          const marker = /** @type {Marker} */ (
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module hard-break-spaces
 * @fileoverview
 *   Warn when too many spaces are used to create a hard break.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file);

    visit$1(tree, 'break', (node) => {
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-duplicate-definitions
 * @fileoverview
 *   Warn when duplicate definitions are found.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    /** @type {Record<string, string>} */
    const map = Object.create(null);

    visit$1(tree, (node) => {
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

        map[identifier] = stringifyPosition(pointStart(node));
      }
    });
  }
);

var remarkLintNoDuplicateDefinitions$1 = remarkLintNoDuplicateDefinitions;

/**
 * @typedef {import('mdast').Heading} Heading
 * @typedef {'atx'|'atx-closed'|'setext'} Style
 */

/**
 * @param {Heading} node
 * @param {Style} [relative]
 * @returns {Style|null}
 */
function headingStyle(node, relative) {
  var last = node.children[node.children.length - 1];
  var depth = node.depth;
  var pos = node && node.position && node.position.end;
  var final = last && last.position && last.position.end;

  if (!pos) {
    return null
  }

  // This can only occur for `'atx'` and `'atx-closed'` headings.
  // This might incorrectly match `'atx'` headings with lots of trailing white
  // space as an `'atx-closed'` heading.
  if (!last) {
    if (pos.column - 1 <= depth * 2) {
      return consolidate(depth, relative)
    }

    return 'atx-closed'
  }

  if (final.line + 1 === pos.line) {
    return 'setext'
  }

  if (final.column + depth < pos.column) {
    return 'atx-closed'
  }

  return consolidate(depth, relative)
}

/**
 * Get the probable style of an atx-heading, depending on preferred style.
 *
 * @param {number} depth
 * @param {Style} relative
 * @returns {Style|null}
 */
function consolidate(depth, relative) {
  return depth < 3
    ? 'atx'
    : relative === 'atx' || relative === 'setext'
    ? relative
    : null
}

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-heading-content-indent
 * @fileoverview
 *   Warn when content of headings is indented.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   removes all unneeded padding around content in headings.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit$1(tree, 'heading', (node) => {
      if (generated(node)) {
        return
      }

      const type = headingStyle(node, 'atx');

      if (type === 'atx' || type === 'atx-closed') {
        const head = pointStart(node.children[0]).column;

        // Ignore empty headings.
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

      // Closed ATX headings always must have a space between their content and
      // the final hashes, thus, there is no `add x spaces`.
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-inline-padding
 * @fileoverview
 *   Warn when phrasing content is padded with spaces between their markers and
 *   content.
 *
 *   Warns for emphasis, strong, delete, image, and link.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    // Note: `emphasis`, `strong`, `delete` (GFM) can’t have padding anymore
    // since CM.
    visit$1(tree, (node) => {
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shortcut-reference-image
 * @fileoverview
 *   Warn when shortcut reference images are used.
 *
 *   Shortcut references render as images when a definition is found, and as
 *   plain text without definition.
 *   Sometimes, you don’t intend to create an image from the reference, but this
 *   rule still warns anyway.
 *   In that case, you can escape the reference like so: `!\[foo]`.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit$1(tree, 'imageReference', (node) => {
      if (!generated(node) && node.referenceType === 'shortcut') {
        file.message('Use the trailing [] on reference images', node);
      }
    });
  }
);

var remarkLintNoShortcutReferenceImage$1 = remarkLintNoShortcutReferenceImage;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shortcut-reference-link
 * @fileoverview
 *   Warn when shortcut reference links are used.
 *
 *   Shortcut references render as links when a definition is found, and as
 *   plain text without definition.
 *   Sometimes, you don’t intend to create a link from the reference, but this
 *   rule still warns anyway.
 *   In that case, you can escape the reference like so: `\[foo]`.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit$1(tree, 'linkReference', (node) => {
      if (!generated(node) && node.referenceType === 'shortcut') {
        file.message('Use the trailing `[]` on reference links', node);
      }
    });
  }
);

var remarkLintNoShortcutReferenceLink$1 = remarkLintNoShortcutReferenceLink;

/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module no-undefined-references
 * @fileoverview
 *   Warn when references to undefined definitions are found.
 *
 *   Options: `Object`, optional.
 *
 *   The object can have an `allow` field, set to an array of strings that may
 *   appear between `[` and `]`, but that should not be treated as link
 *   identifiers.
 *
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
 *   {"name": "ok-allow.md", "setting": {"allow": ["...", "…"]}}
 *
 *   > Eliding a portion of a quoted passage […] is acceptable.
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
 */

const remarkLintNoUndefinedReferences = lintRule(
  {
    origin: 'remark-lint:no-undefined-references',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-undefined-references#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = {}) => {
    const contents = String(file);
    const loc = location(file);
    const lineEnding = /(\r?\n|\r)[\t ]*(>[\t ]*)*/g;
    const allow = new Set(
      (option.allow || []).map((d) => normalizeIdentifier(d))
    );
    /** @type {Record<string, boolean>} */
    const map = Object.create(null);

    visit$1(tree, (node) => {
      if (
        (node.type === 'definition' || node.type === 'footnoteDefinition') &&
        !generated(node)
      ) {
        map[normalizeIdentifier(node.identifier)] = true;
      }
    });

    visit$1(tree, (node) => {
      // CM specifiers that references only form when defined.
      // Still, they could be added by plugins, so let’s keep it.
      /* c8 ignore next 10 */
      if (
        (node.type === 'imageReference' ||
          node.type === 'linkReference' ||
          node.type === 'footnoteReference') &&
        !generated(node) &&
        !(normalizeIdentifier(node.identifier) in map) &&
        !allow.has(normalizeIdentifier(node.identifier))
      ) {
        file.message('Found reference to undefined definition', node);
      }

      if (node.type === 'paragraph' || node.type === 'heading') {
        findInPhrasing(node);
      }
    });

    /**
     * @param {Heading|Paragraph} node
     */
    function findInPhrasing(node) {
      /** @type {Range[]} */
      let ranges = [];

      visit$1(node, (child) => {
        // Ignore the node itself.
        if (child === node) return

        // Can’t have links in links, so reset ranges.
        if (child.type === 'link' || child.type === 'linkReference') {
          ranges = [];
          return SKIP$1
        }

        // Enter non-text.
        if (child.type !== 'text') return

        const start = pointStart(child).offset;
        const end = pointEnd(child).offset;

        // Bail if there’s no positional info.
        if (typeof start !== 'number' || typeof end !== 'number') {
          return EXIT$1
        }

        const source = contents.slice(start, end);
        /** @type {Array.<[number, string]>} */
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

            // Skip past escaped brackets.
            if (code === 92) {
              const next = line.charCodeAt(index + 1);
              index++;

              if (next === 91 || next === 93) {
                index++;
              }
            }
            // Opening bracket.
            else if (code === 91) {
              ranges.push([lines[lineIndex][0] + index]);
              index++;
            }
            // Close bracket.
            else if (code === 93) {
              // No opening.
              if (ranges.length === 0) {
                index++;
              } else if (line.charCodeAt(index + 1) === 91) {
                index++;

                // Collapsed or full.
                let range = ranges.pop();

                // Range should always exist.
                // eslint-disable-next-line max-depth
                if (range) {
                  range.push(lines[lineIndex][0] + index);

                  // This is the end of a reference already.
                  // eslint-disable-next-line max-depth
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

                // Shortcut or typical end of a reference.
                const range = ranges.pop();

                // Range should always exist.
                // eslint-disable-next-line max-depth
                if (range) {
                  range.push(lines[lineIndex][0] + index);
                  handleRange(range);
                }
              }
            }
            // Anything else.
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

      return SKIP$1

      /**
       * @param {Range} range
       */
      function handleRange(range) {
        if (range.length === 1) return
        if (range.length === 3) range.length = 2;

        // No need to warn for just `[]`.
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
          !allow.has(normalizeIdentifier(id))
        ) {
          file.message('Found reference to undefined definition', pos);
        }
      }
    }
  }
);

var remarkLintNoUndefinedReferences$1 = remarkLintNoUndefinedReferences;

/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module no-unused-definitions
 * @fileoverview
 *   Warn when unused definitions are found.
 *
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

const own$1 = {}.hasOwnProperty;

const remarkLintNoUnusedDefinitions = lintRule(
  {
    origin: 'remark-lint:no-unused-definitions',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-unused-definitions#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    /** @type {Record<string, {node: DefinitionContent, used: boolean}>} */
    const map = Object.create(null);

    visit$1(tree, (node) => {
      if (
        (node.type === 'definition' || node.type === 'footnoteDefinition') &&
        !generated(node)
      ) {
        map[node.identifier.toUpperCase()] = {node, used: false};
      }
    });

    visit$1(tree, (node) => {
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

    /** @type {string} */
    let identifier;

    for (identifier in map) {
      if (own$1.call(map, identifier)) {
        const entry = map[identifier];

        if (!entry.used) {
          file.message('Found unused definition', entry.node);
        }
      }
    }
  }
);

var remarkLintNoUnusedDefinitions$1 = remarkLintNoUnusedDefinitions;

/**
 * @fileoverview
 *   remark preset to configure `remark-lint` with settings that prevent
 *   mistakes or stuff that fails across vendors.
 */

/** @type {Preset} */
const remarkPresetLintRecommended = {
  plugins: [
    remarkLint,
    // Unix compatibility.
    remarkLintFinalNewline$1,
    // Rendering across vendors differs greatly if using other styles.
    remarkLintListItemBulletIndent$1,
    [remarkLintListItemIndent$1, 'tab-size'],
    remarkLintNoBlockquoteWithoutMarker$1,
    remarkLintNoLiteralUrls$1,
    [remarkLintOrderedListMarkerStyle$1, '.'],
    // Mistakes.
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module blockquote-indentation
 * @fileoverview
 *   Warn when block quotes are indented too much or too little.
 *
 *   Options: `number` or `'consistent'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used indentation and will warn when
 *   other block quotes use a different indentation.
 *
 * @example
 *   {"name": "ok.md", "setting": 4}
 *
 *   >   Hello
 *
 *   Paragraph.
 *
 *   >   World
 * @example
 *   {"name": "ok.md", "setting": 2}
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
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    visit$1(tree, 'blockquote', (node) => {
      if (generated(node) || node.children.length === 0) {
        return
      }

      if (option === 'consistent') {
        option = check$1(node);
      } else {
        const diff = option - check$1(node);

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

/**
 * @param {Blockquote} node
 * @returns {number}
 */
function check$1(node) {
  return pointStart(node.children[0]).column - pointStart(node).column
}

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module checkbox-character-style
 * @fileoverview
 *   Warn when list item checkboxes violate a given style.
 *
 *   Options: `Object` or `'consistent'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used checked and unchecked checkbox
 *   styles and warns when subsequent checkboxes use different styles.
 *
 *   Styles can also be passed in like so:
 *
 *   ```js
 *   {checked: 'x', unchecked: ' '}
 *   ```
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats checked checkboxes using `x` (lowercase X) and unchecked checkboxes
 *   as `·` (a single space).
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md", "setting": {"checked": "x"}, "gfm": true}
 *
 *   - [x] List item
 *   - [x] List item
 *
 * @example
 *   {"name": "ok.md", "setting": {"checked": "X"}, "gfm": true}
 *
 *   - [X] List item
 *   - [X] List item
 *
 * @example
 *   {"name": "ok.md", "setting": {"unchecked": " "}, "gfm": true}
 *
 *   - [ ] List item
 *   - [ ] List item
 *   - [ ]··
 *   - [ ]
 *
 * @example
 *   {"name": "ok.md", "setting": {"unchecked": "\t"}, "gfm": true}
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
 *   {"setting": {"unchecked": "💩"}, "name": "not-ok.md", "label": "output", "positionless": true, "gfm": true}
 *
 *   1:1: Incorrect unchecked checkbox marker `💩`: use either `'\t'`, or `' '`
 *
 * @example
 *   {"setting": {"checked": "💩"}, "name": "not-ok.md", "label": "output", "positionless": true, "gfm": true}
 *
 *   1:1: Incorrect checked checkbox marker `💩`: use either `'x'`, or `'X'`
 */

const remarkLintCheckboxCharacterStyle = lintRule(
  {
    origin: 'remark-lint:checkbox-character-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-checkbox-character-style#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const value = String(file);
    /** @type {'x'|'X'|'consistent'} */
    let checked = 'consistent';
    /** @type {' '|'\x09'|'consistent'} */
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

    visit$1(tree, 'listItem', (node) => {
      const head = node.children[0];
      const point = pointStart(head);

      // Exit early for items without checkbox.
      // A list item cannot be checked and empty, according to GFM.
      if (
        typeof node.checked !== 'boolean' ||
        !head ||
        typeof point.offset !== 'number'
      ) {
        return
      }

      // Move back to before `] `.
      point.offset -= 2;
      point.column -= 2;

      // Assume we start with a checkbox, because well, `checked` is set.
      const match = /\[([\t Xx])]/.exec(
        value.slice(point.offset - 2, point.offset + 1)
      );

      // Failsafe to make sure we don‘t crash if there actually isn’t a checkbox.
      /* c8 ignore next */
      if (!match) return

      const style = node.checked ? checked : unchecked;

      if (style === 'consistent') {
        if (node.checked) {
          // @ts-expect-error: valid marker.
          checked = match[1];
        } else {
          // @ts-expect-error: valid marker.
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module checkbox-content-indent
 * @fileoverview
 *   Warn when list item checkboxes are followed by too much whitespace.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file);
    const loc = location(file);

    visit$1(tree, 'listItem', (node) => {
      const head = node.children[0];
      const point = pointStart(head);

      // Exit early for items without checkbox.
      // A list item cannot be checked and empty, according to GFM.
      if (
        typeof node.checked !== 'boolean' ||
        !head ||
        typeof point.offset !== 'number'
      ) {
        return
      }

      // Assume we start with a checkbox, because well, `checked` is set.
      const match = /\[([\t xX])]/.exec(
        value.slice(point.offset - 4, point.offset + 1)
      );

      // Failsafe to make sure we don‘t crash if there actually isn’t a checkbox.
      /* c8 ignore next */
      if (!match) return

      // Move past checkbox.
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module code-block-style
 * @fileoverview
 *   Warn when code blocks do not adhere to a given style.
 *
 *   Options: `'consistent'`, `'fenced'`, or `'indented'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used code block style and warns when
 *   subsequent code blocks uses different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats code blocks using a fence if they have a language flag and
 *   indentation if not.
 *   Pass
 *   [`fences: true`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsfences)
 *   to always use fences for code blocks.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"setting": "indented", "name": "ok.md"}
 *
 *       alpha()
 *
 *   Paragraph.
 *
 *       bravo()
 *
 * @example
 *   {"setting": "indented", "name": "not-ok.md", "label": "input"}
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
 *   {"setting": "indented", "name": "not-ok.md", "label": "output"}
 *
 *   1:1-3:4: Code blocks should be indented
 *   7:1-9:4: Code blocks should be indented
 *
 * @example
 *   {"setting": "fenced", "name": "ok.md"}
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
 *   {"setting": "fenced", "name": "not-ok-fenced.md", "label": "input"}
 *
 *       alpha()
 *
 *   Paragraph.
 *
 *       bravo()
 *
 * @example
 *   {"setting": "fenced", "name": "not-ok-fenced.md", "label": "output"}
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
 *   {"setting": "💩", "name": "not-ok-incorrect.md", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect code block style `💩`: use either `'consistent'`, `'fenced'`, or `'indented'`
 */

const remarkLintCodeBlockStyle = lintRule(
  {
    origin: 'remark-lint:code-block-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-code-block-style#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
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

    visit$1(tree, 'code', (node) => {
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module definition-spacing
 * @fileoverview
 *   Warn when consecutive whitespace is used in a definition.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file);

    visit$1(tree, (node) => {
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module fenced-code-flag
 * @fileoverview
 *   Check fenced code block flags.
 *
 *   Options: `Array.<string>` or `Object`, optional.
 *
 *   Providing an array is as passing `{flags: Array}`.
 *
 *   The object can have an array of `'flags'` which are allowed: other flags
 *   will not be allowed.
 *   An `allowEmpty` field (`boolean`, default: `false`) can be set to allow
 *   code blocks without language flags.
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
 *   {"name": "ok.md", "setting": {"allowEmpty": true}}
 *
 *   ```
 *   alpha()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "setting": {"allowEmpty": false}, "label": "input"}
 *
 *   ```
 *   alpha()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "setting": {"allowEmpty": false}, "label": "output"}
 *
 *   1:1-3:4: Missing code language flag
 *
 * @example
 *   {"name": "ok.md", "setting": ["alpha"]}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "ok.md", "setting": {"flags":["alpha"]}}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "setting": ["charlie"], "label": "input"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "setting": ["charlie"], "label": "output"}
 *
 *   1:1-3:4: Incorrect code language flag
 */

const fence = /^ {0,3}([~`])\1{2,}/;

const remarkLintFencedCodeFlag = lintRule(
  {
    origin: 'remark-lint:fenced-code-flag',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-fenced-code-flag#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option) => {
    const value = String(file);
    let allowEmpty = false;
    /** @type {string[]} */
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

    visit$1(tree, 'code', (node) => {
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module fenced-code-marker
 * @fileoverview
 *   Warn for violating fenced code markers.
 *
 *   Options: `` '`' ``, `'~'`, or `'consistent'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used fenced code marker style and warns
 *   when subsequent fenced code blocks use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats fences using ``'`'`` (grave accent) by default.
 *   Pass
 *   [`fence: '~'`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsfence)
 *   to use `~` (tilde) instead.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   Indented code blocks are not affected by this rule:
 *
 *       bravo()
 *
 * @example
 *   {"name": "ok.md", "setting": "`"}
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
 *   {"name": "ok.md", "setting": "~"}
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
 *   {"name": "not-ok-incorrect.md", "setting": "💩", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect fenced code marker `💩`: use either `'consistent'`, `` '`' ``, or `'~'`
 */

const remarkLintFencedCodeMarker = lintRule(
  {
    origin: 'remark-lint:fenced-code-marker',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-fenced-code-marker#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const contents = String(file);

    if (option !== 'consistent' && option !== '~' && option !== '`') {
      file.fail(
        'Incorrect fenced code marker `' +
          option +
          "`: use either `'consistent'`, `` '`' ``, or `'~'`"
      );
    }

    visit$1(tree, 'code', (node) => {
      const start = pointStart(node).offset;

      if (typeof start === 'number') {
        const marker = contents
          .slice(start, start + 4)
          .replace(/^\s+/, '')
          .charAt(0);

        // Ignore unfenced code blocks.
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
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module file-extension
 * @fileoverview
 *   Warn when the file extension differ from the preferred extension.
 *
 *   Does not warn when given documents have no file extensions (such as
 *   `AUTHORS` or `LICENSE`).
 *
 *   Options: `string`, default: `'md'` — Expected file extension.
 *
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
 *   {"name": "readme.mkd", "setting": "mkd"}
 */

const remarkLintFileExtension = lintRule(
  {
    origin: 'remark-lint:file-extension',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-file-extension#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (_, file, option = 'md') => {
    const ext = file.extname;

    if (ext && ext.slice(1) !== option) {
      file.message('Incorrect extension: use `' + option + '`');
    }
  }
);

var remarkLintFileExtension$1 = remarkLintFileExtension;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module final-definition
 * @fileoverview
 *   Warn when definitions are placed somewhere other than at the end of
 *   the file.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    let last = 0;

    visit$1(
      tree,
      (node) => {
        // Ignore generated and HTML comment nodes.
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module first-heading-level
 * @fileoverview
 *   Warn when the first heading has a level other than a specified value.
 *
 *   Options: `number`, default: `1`.
 *
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
 *   {"name": "ok.md", "setting": 2}
 *
 *   ## Delta
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "ok-html.md", "setting": 2}
 *
 *   <h2>Echo</h2>
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "setting": 2, "label": "input"}
 *
 *   # Foxtrot
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "setting": 2, "label": "output"}
 *
 *   1:1-1:10: First heading level should be `2`
 *
 * @example
 *   {"name": "not-ok-html.md", "setting": 2, "label": "input"}
 *
 *   <h1>Golf</h1>
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok-html.md", "setting": 2, "label": "output"}
 *
 *   1:1-1:14: First heading level should be `2`
 */

const re$3 = /<h([1-6])/;

const remarkLintFirstHeadingLevel = lintRule(
  {
    origin: 'remark-lint:first-heading-level',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-first-heading-level#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 1) => {
    visit$1(tree, (node) => {
      if (!generated(node)) {
        /** @type {Depth|undefined} */
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

          return EXIT$1
        }
      }
    });
  }
);

var remarkLintFirstHeadingLevel$1 = remarkLintFirstHeadingLevel;

/**
 * @param {HTML} node
 * @returns {Depth|undefined}
 */
function infer(node) {
  const results = node.value.match(re$3);
  // @ts-expect-error: can be castes fine.
  return results ? Number(results[1]) : undefined
}

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module heading-style
 * @fileoverview
 *   Warn when a heading does not conform to a given style.
 *
 *   Options: `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`,
 *   default: `'consistent'`.
 *
 *   `'consistent'` detects the first used heading style and warns when
 *   subsequent headings use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats headings as ATX by default.
 *   This can be configured with the
 *   [`setext`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionssetext)
 *   and
 *   [`closeAtx`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionscloseatx)
 *   options.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md", "setting": "atx"}
 *
 *   # Alpha
 *
 *   ## Bravo
 *
 *   ### Charlie
 *
 * @example
 *   {"name": "ok.md", "setting": "atx-closed"}
 *
 *   # Delta ##
 *
 *   ## Echo ##
 *
 *   ### Foxtrot ###
 *
 * @example
 *   {"name": "ok.md", "setting": "setext"}
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
 *   {"name": "not-ok.md", "setting": "💩", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect heading style type `💩`: use either `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`
 */

const remarkLintHeadingStyle = lintRule(
  {
    origin: 'remark-lint:heading-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-heading-style#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
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

    visit$1(tree, 'heading', (node) => {
      if (!generated(node)) {
        if (option === 'consistent') {
          // Funky nodes perhaps cannot be detected.
          /* c8 ignore next */
          option = headingStyle(node) || 'consistent';
        } else if (headingStyle(node, option) !== option) {
          file.message('Headings should use ' + option, node);
        }
      }
    });
  }
);

var remarkLintHeadingStyle$1 = remarkLintHeadingStyle;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module maximum-line-length
 * @fileoverview
 *   Warn when lines are too long.
 *
 *   Options: `number`, default: `80`.
 *
 *   Ignores nodes that cannot be wrapped, such as headings, tables, code,
 *   definitions, HTML, and JSX.
 *
 *   Ignores images, links, and inline code if they start before the wrap, end
 *   after the wrap, and there’s no whitespace after them.
 *
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
 *   The following is also fine, because there is no whitespace.
 *
 *   <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>.
 *
 *   In addition, definitions are also fine:
 *
 *   [foo]: <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>
 *
 * @example
 *   {"name": "not-ok.md", "setting": 80, "label": "input", "positionless": true}
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
 *   {"name": "not-ok.md", "setting": 80, "label": "output", "positionless": true}
 *
 *   4:86: Line must be at most 80 characters
 *   6:99: Line must be at most 80 characters
 *   8:96: Line must be at most 80 characters
 *   10:97: Line must be at most 80 characters
 *   12:99: Line must be at most 80 characters
 *
 * @example
 *   {"name": "ok-mixed-line-endings.md", "setting": 10, "positionless": true}
 *
 *   0123456789␍␊
 *   0123456789␊
 *   01234␍␊
 *   01234␊
 *
 * @example
 *   {"name": "not-ok-mixed-line-endings.md", "setting": 10, "label": "input", "positionless": true}
 *
 *   012345678901␍␊
 *   012345678901␊
 *   01234567890␍␊
 *   01234567890␊
 *
 * @example
 *   {"name": "not-ok-mixed-line-endings.md", "setting": 10, "label": "output", "positionless": true}
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
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 80) => {
    const value = String(file);
    const lines = value.split(/\r?\n/);

    visit$1(tree, (node) => {
      if (
        (node.type === 'heading' ||
          node.type === 'table' ||
          node.type === 'code' ||
          node.type === 'definition' ||
          node.type === 'html' ||
          // @ts-expect-error: These are from MDX@1 and MDX@2: <https://github.com/mdx-js/specification>.
          node.type === 'jsx' ||
          // @ts-expect-error: MDX
          node.type === 'mdxFlowExpression' ||
          // @ts-expect-error: MDX
          node.type === 'mdxJsxFlowElement' ||
          // @ts-expect-error: MDX
          node.type === 'mdxJsxTextElement' ||
          // @ts-expect-error: MDX
          node.type === 'mdxTextExpression' ||
          // @ts-expect-error: MDX
          node.type === 'mdxjsEsm' ||
          node.type === 'yaml' ||
          // @ts-expect-error: YAML and TOML are from frontmatter.
          node.type === 'toml') &&
        !generated(node)
      ) {
        allowList(pointStart(node).line - 1, pointEnd(node).line);
      }
    });

    // Finally, allow some inline spans, but only if they occur at or after
    // the wrap.
    // However, when they do, and there’s whitespace after it, they are not
    // allowed.
    visit$1(tree, (node, pos, parent_) => {
      const parent = /** @type {Parent} */ (parent_);

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

        // Not allowing when starting after the border, or ending before it.
        if (initial.column > option || final.column < option) {
          return
        }

        const next = parent.children[pos + 1];

        // Not allowing when there’s whitespace after the link.
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

    // Iterate over every line, and warn for violating lines.
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

    /**
     * Allowlist from `initial` to `final`, zero-based.
     *
     * @param {number} initial
     * @param {number} final
     */
    function allowList(initial, final) {
      while (initial < final) {
        lines[initial++] = '';
      }
    }
  }
);

var remarkLintMaximumLineLength$1 = remarkLintMaximumLineLength;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-consecutive-blank-lines
 * @fileoverview
 *   Warn for too many consecutive blank lines.
 *   Knows about the extra line needed between a list and indented code, and two
 *   lists.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   always uses one blank line between blocks if possible, or two lines when
 *   needed.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit$1(tree, (node) => {
      if (!generated(node) && 'children' in node) {
        const head = node.children[0];

        if (head && !generated(head)) {
          if (!unknownContainerSize.has(node.type)) {
            // Compare parent and first child.
            compare(pointStart(node), pointStart(head), 0);
          }

          // Compare between each child.
          let index = -1;

          while (++index < node.children.length) {
            const previous = node.children[index - 1];
            const child = node.children[index];

            if (previous && !generated(previous) && !generated(child)) {
              compare(pointEnd(previous), pointStart(child), 2);
            }
          }

          const tail = node.children[node.children.length - 1];

          // Compare parent and last child.
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

    /**
     * Compare the difference between `start` and `end`, and warn when that
     * difference exceeds `max`.
     *
     * @param {Point} start
     * @param {Point} end
     * @param {0|1|2} max
     */
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
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-articles
 * @fileoverview
 *   Warn when file names start with an article.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (_, file) => {
    const match = file.stem && file.stem.match(/^(the|teh|an?)\b/i);

    if (match) {
      file.message('Do not start file names with `' + match[0] + '`');
    }
  }
);

var remarkLintNoFileNameArticles$1 = remarkLintNoFileNameArticles;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-consecutive-dashes
 * @fileoverview
 *   Warn when file names contain consecutive dashes.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (_, file) => {
    if (file.stem && /-{2,}/.test(file.stem)) {
      file.message('Do not use consecutive dashes in a file name');
    }
  }
);

var remarkLintNoFileNameConsecutiveDashes$1 = remarkLintNoFileNameConsecutiveDashes;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-outer-dashes
 * @fileoverview
 *   Warn when file names contain initial or final dashes (hyphen-minus, `-`).
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (_, file) => {
    if (file.stem && /^-|-$/.test(file.stem)) {
      file.message('Do not use initial or final dashes in a file name');
    }
  }
);

var remarkLintNofileNameOuterDashes$1 = remarkLintNofileNameOuterDashes;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-heading-indent
 * @fileoverview
 *   Warn when a heading is indented.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   removes all unneeded indentation before headings.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit$1(tree, 'heading', (node, _, parent) => {
      // Note: it’s rather complex to detect what the expected indent is in block
      // quotes and lists, so let’s only do directly in root for now.
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-multiple-toplevel-headings
 * @fileoverview
 *   Warn when multiple top level headings are used.
 *
 *   Options: `number`, default: `1`.
 *
 * @example
 *   {"name": "ok.md", "setting": 1}
 *
 *   # Foo
 *
 *   ## Bar
 *
 * @example
 *   {"name": "not-ok.md", "setting": 1, "label": "input"}
 *
 *   # Foo
 *
 *   # Bar
 *
 * @example
 *   {"name": "not-ok.md", "setting": 1, "label": "output"}
 *
 *   3:1-3:6: Don’t use multiple top level headings (1:1)
 */

const remarkLintNoMultipleToplevelHeadings = lintRule(
  {
    origin: 'remark-lint:no-multiple-toplevel-headings',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-multiple-toplevel-headings#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 1) => {
    /** @type {string|undefined} */
    let duplicate;

    visit$1(tree, 'heading', (node) => {
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shell-dollars
 * @fileoverview
 *   Warn when shell code is prefixed by `$` (dollar sign) characters.
 *
 *   Ignores indented code blocks and fenced code blocks without language flag.
 *
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

// List of shell script file extensions (also used as code flags for syntax
// highlighting on GitHub):
// See: <https://github.com/github/linguist/blob/40992ba/lib/linguist/languages.yml#L4984>
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit$1(tree, 'code', (node) => {
      // Check both known shell code and unknown code.
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-table-indentation
 * @fileoverview
 *   Warn when tables are indented.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   removes all unneeded indentation before tables.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file);
    const loc = location(value);

    visit$1(tree, 'table', (node, _, parent) => {
      const end = pointEnd(node).line;
      let line = pointStart(node).line;
      let column = 0;

      if (parent && parent.type === 'root') {
        column = 1;
      } else if (parent && parent.type === 'blockquote') {
        column = pointStart(parent).column + 2;
      } else if (parent && parent.type === 'listItem') {
        column = pointStart(parent.children[0]).column;

        // Skip past the first line if we’re the first child of a list item.
        /* c8 ignore next 3 */
        if (parent.children[0] === node) {
          line++;
        }
      }

      // In a parent we don’t know, exit.
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

      return SKIP$1
    });
  }
);

var remarkLintNoTableIndentation$1 = remarkLintNoTableIndentation;

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-tabs
 * @fileoverview
 *   Warn when hard tabs (`\t`) are used instead of spaces.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   uses spaces where tabs are used for indentation, but retains tabs used in
 *   content.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
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

/**
 * An Array.prototype.slice.call(arguments) alternative
 *
 * @param {Object} args something with a length
 * @param {Number} slice
 * @param {Number} sliceEnd
 * @api public
 */

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

/**
 * slice() reference.
 */

var slice = Array.prototype.slice;

/**
 * Expose `co`.
 */

var co_1 = co$1;

/**
 * Wrap the given generator `fn` and
 * return a thunk.
 *
 * @param {Function} fn
 * @return {Function}
 * @api public
 */

function co$1(fn) {
  var isGenFun = isGeneratorFunction(fn);

  return function (done) {
    var ctx = this;

    // in toThunk() below we invoke co()
    // with a generator, so optimize for
    // this case
    var gen = fn;

    // we only need to parse the arguments
    // if gen is a generator function.
    if (isGenFun) {
      var args = slice.call(arguments), len = args.length;
      var hasCallback = len && 'function' == typeof args[len - 1];
      done = hasCallback ? args.pop() : error;
      gen = fn.apply(this, args);
    } else {
      done = done || error;
    }

    next();

    // #92
    // wrap the callback in a setImmediate
    // so that any of its errors aren't caught by `co`
    function exit(err, res) {
      setImmediate(function(){
        done.call(ctx, err, res);
      });
    }

    function next(err, res) {
      var ret;

      // multiple args
      if (arguments.length > 2) res = slice.call(arguments, 1);

      // error
      if (err) {
        try {
          ret = gen.throw(err);
        } catch (e) {
          return exit(e);
        }
      }

      // ok
      if (!err) {
        try {
          ret = gen.next(res);
        } catch (e) {
          return exit(e);
        }
      }

      // done
      if (ret.done) return exit(null, ret.value);

      // normalize
      ret.value = toThunk(ret.value, ctx);

      // run
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

      // invalid
      next(new TypeError('You may only yield a function, promise, generator, array, or object, '
        + 'but the following was passed: "' + String(ret.value) + '"'));
    }
  }
}

/**
 * Convert `obj` into a normalized thunk.
 *
 * @param {Mixed} obj
 * @param {Mixed} ctx
 * @return {Function}
 * @api private
 */

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

/**
 * Convert an object of yieldables to a thunk.
 *
 * @param {Object} obj
 * @return {Function}
 * @api private
 */

function objectToThunk(obj){
  var ctx = this;
  var isArray = Array.isArray(obj);

  return function(done){
    var keys = Object.keys(obj);
    var pending = keys.length;
    var results = isArray
      ? new Array(pending) // predefine the array length
      : new obj.constructor();
    var finished;

    if (!pending) {
      setImmediate(function(){
        done(null, results);
      });
      return;
    }

    // prepopulate object keys to preserve key ordering
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

/**
 * Convert `promise` to a thunk.
 *
 * @param {Object} promise
 * @return {Function}
 * @api private
 */

function promiseToThunk(promise) {
  return function(fn){
    promise.then(function(res) {
      fn(null, res);
    }, fn);
  }
}

/**
 * Check if `obj` is a promise.
 *
 * @param {Object} obj
 * @return {Boolean}
 * @api private
 */

function isPromise(obj) {
  return obj && 'function' == typeof obj.then;
}

/**
 * Check if `obj` is a generator.
 *
 * @param {Mixed} obj
 * @return {Boolean}
 * @api private
 */

function isGenerator(obj) {
  return obj && 'function' == typeof obj.next && 'function' == typeof obj.throw;
}

/**
 * Check if `obj` is a generator function.
 *
 * @param {Mixed} obj
 * @return {Boolean}
 * @api private
 */

function isGeneratorFunction(obj) {
  return obj && obj.constructor && 'GeneratorFunction' == obj.constructor.name;
}

/**
 * Check for plain object.
 *
 * @param {Mixed} val
 * @return {Boolean}
 * @api private
 */

function isObject$1(val) {
  return val && Object == val.constructor;
}

/**
 * Throw `err` in a new stack.
 *
 * This is used when co() is invoked
 * without supplying a callback, which
 * should only be for demonstrational
 * purposes.
 *
 * @param {Error} err
 * @api private
 */

function error(err) {
  if (!err) return;
  setImmediate(function(){
    throw err;
  });
}

/**
 * Module Dependencies
 */

var sliced = sliced$1;
var noop = function(){};
var co = co_1;

/**
 * Export `wrapped`
 */

var wrapped_1 = wrapped$1;

/**
 * Wrap a function to support
 * sync, async, and gen functions.
 *
 * @param {Function} fn
 * @return {Function}
 * @api public
 */

function wrapped$1(fn) {
  function wrap() {
    var args = sliced(arguments);
    var last = args[args.length - 1];
    var ctx = this;

    // done
    var done = typeof last == 'function' ? args.pop() : noop;

    // nothing
    if (!fn) {
      return done.apply(ctx, [null].concat(args));
    }

    // generator
    if (generator(fn)) {
      return co(fn).apply(ctx, args.concat(done));
    }

    // async
    if (fn.length > args.length) {
      // NOTE: this only handles uncaught synchronous errors
      try {
        return fn.apply(ctx, args.concat(done));
      } catch (e) {
        return done(e);
      }
    }

    // sync
    return sync(fn, done).apply(ctx, args);
  }

  return wrap;
}

/**
 * Wrap a synchronous function execution.
 *
 * @param {Function} fn
 * @param {Function} done
 * @return {Function}
 * @api private
 */

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

/**
 * Is `value` a generator?
 *
 * @param {Mixed} value
 * @return {Boolean}
 * @api private
 */

function generator(value) {
  return value
    && value.constructor
    && 'GeneratorFunction' == value.constructor.name;
}


/**
 * Is `value` a promise?
 *
 * @param {Mixed} value
 * @return {Boolean}
 * @api private
 */

function promise(value) {
  return value && 'function' == typeof value.then;
}

var wrapped = wrapped_1;

var unifiedLintRule = factory;

function factory(id, rule) {
  var parts = id.split(':');
  var source = parts[0];
  var ruleId = parts[1];
  var fn = wrapped(rule);

  /* istanbul ignore if - possibly useful if externalised later. */
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

        // Add the error, if not already properly added.
        /* istanbul ignore if - only happens for incorrect plugins */
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

// Coerce a value to a severity--options tuple.
function coerce(name, value) {
  var def = 1;
  var result;
  var level;

  /* istanbul ignore if - Handled by unified in v6.0.0 */
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

var rule = unifiedLintRule;

var remarkLintNoTrailingSpaces = rule('remark-lint:no-trailing-spaces', noTrailingSpaces);

/**
 * Lines that are just space characters are not present in
 * the AST, which is why we loop through lines manually.
 */

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

function* getLinksRecursively(node) {
  if (node.url) {
    yield node;
  }
  for (const child of node.children || []) {
    yield* getLinksRecursively(child);
  }
}

function validateLinks(tree, vfile) {
  const currentFileURL = pathToFileURL(path$1.join(vfile.cwd, vfile.path));
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
          node
        );
      }
    }
    if (node.type === "definition") {
      if (previousDefinitionLabel && previousDefinitionLabel > node.label) {
        vfile.message(
          `Unordered reference ("${node.label}" should be before "${previousDefinitionLabel}")`,
          node
        );
      }
      previousDefinitionLabel = node.label;
    }
  }
}

const remarkLintNodejsLinks = lintRule(
  "remark-lint:nodejs-links",
  validateLinks
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

// YAML error class. http://stackoverflow.com/questions/8458984


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
  // Super constructor
  Error.call(this);

  this.name = 'YAMLException';
  this.reason = reason;
  this.mark = mark;
  this.message = formatError(this, false);

  // Include stack trace in error object
  if (Error.captureStackTrace) {
    // Chrome and NodeJS
    Error.captureStackTrace(this, this.constructor);
  } else {
    // FF, IE 10+ and Safari 6+. Fallback for others
    this.stack = (new Error()).stack || '';
  }
}


// Inherit from Error
YAMLException$1.prototype = Object.create(Error.prototype);
YAMLException$1.prototype.constructor = YAMLException$1;


YAMLException$1.prototype.toString = function toString(compact) {
  return this.name + ': ' + formatError(this, compact);
};


var exception = YAMLException$1;

// get snippet for a single line, respecting maxLength
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
    pos: position - lineStart + head.length // relative position
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

  // TODO: Add tag format check.
  this.options       = options; // keep original options in case user wants to extend this type later
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

/*eslint-disable max-len*/





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


function compileMap(/* lists... */) {
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
    // Schema.extend(type)
    explicit.push(definition);

  } else if (Array.isArray(definition)) {
    // Schema.extend([ type1, type2, ... ])
    explicit = explicit.concat(definition);

  } else if (definition && (Array.isArray(definition.implicit) || Array.isArray(definition.explicit))) {
    // Schema.extend({ explicit: [ type1, type2, ... ], implicit: [ type1, type2, ... ] })
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
  return ((0x30/* 0 */ <= c) && (c <= 0x39/* 9 */)) ||
         ((0x41/* A */ <= c) && (c <= 0x46/* F */)) ||
         ((0x61/* a */ <= c) && (c <= 0x66/* f */));
}

function isOctCode(c) {
  return ((0x30/* 0 */ <= c) && (c <= 0x37/* 7 */));
}

function isDecCode(c) {
  return ((0x30/* 0 */ <= c) && (c <= 0x39/* 9 */));
}

function resolveYamlInteger(data) {
  if (data === null) return false;

  var max = data.length,
      index = 0,
      hasDigits = false,
      ch;

  if (!max) return false;

  ch = data[index];

  // sign
  if (ch === '-' || ch === '+') {
    ch = data[++index];
  }

  if (ch === '0') {
    // 0
    if (index + 1 === max) return true;
    ch = data[++index];

    // base 2, base 8, base 16

    if (ch === 'b') {
      // base 2
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
      // base 16
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
      // base 8
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

  // base 10 (except 0)

  // value should not start with `_`;
  if (ch === '_') return false;

  for (; index < max; index++) {
    ch = data[index];
    if (ch === '_') continue;
    if (!isDecCode(data.charCodeAt(index))) {
      return false;
    }
    hasDigits = true;
  }

  // Should have digits and should not end with `_`
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
    /* eslint-disable max-len */
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
  // 2.5e4, 2.5 and integers
  '^(?:[-+]?(?:[0-9][0-9_]*)(?:\\.[0-9_]*)?(?:[eE][-+]?[0-9]+)?' +
  // .2e4, .2
  // special case, seems not from spec
  '|\\.[0-9_]+(?:[eE][-+]?[0-9]+)?' +
  // .inf
  '|[-+]?\\.(?:inf|Inf|INF)' +
  // .nan
  '|\\.(?:nan|NaN|NAN))$');

function resolveYamlFloat(data) {
  if (data === null) return false;

  if (!YAML_FLOAT_PATTERN.test(data) ||
      // Quick hack to not allow integers end with `_`
      // Probably should update regexp & check speed
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

  // JS stringifier can build scientific format without dots: 5e-100,
  // while YAML requres dot: 5.e-100. Fix it with simple hack

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
  '^([0-9][0-9][0-9][0-9])'          + // [1] year
  '-([0-9][0-9])'                    + // [2] month
  '-([0-9][0-9])$');                   // [3] day

var YAML_TIMESTAMP_REGEXP = new RegExp(
  '^([0-9][0-9][0-9][0-9])'          + // [1] year
  '-([0-9][0-9]?)'                   + // [2] month
  '-([0-9][0-9]?)'                   + // [3] day
  '(?:[Tt]|[ \\t]+)'                 + // ...
  '([0-9][0-9]?)'                    + // [4] hour
  ':([0-9][0-9])'                    + // [5] minute
  ':([0-9][0-9])'                    + // [6] second
  '(?:\\.([0-9]*))?'                 + // [7] fraction
  '(?:[ \\t]*(Z|([-+])([0-9][0-9]?)' + // [8] tz [9] tz_sign [10] tz_hour
  '(?::([0-9][0-9]))?))?$');           // [11] tz_minute

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

  // match: [1] year [2] month [3] day

  year = +(match[1]);
  month = +(match[2]) - 1; // JS month starts with 0
  day = +(match[3]);

  if (!match[4]) { // no hour
    return new Date(Date.UTC(year, month, day));
  }

  // match: [4] hour [5] minute [6] second [7] fraction

  hour = +(match[4]);
  minute = +(match[5]);
  second = +(match[6]);

  if (match[7]) {
    fraction = match[7].slice(0, 3);
    while (fraction.length < 3) { // milli-seconds
      fraction += '0';
    }
    fraction = +fraction;
  }

  // match: [8] tz [9] tz_sign [10] tz_hour [11] tz_minute

  if (match[9]) {
    tz_hour = +(match[10]);
    tz_minute = +(match[11] || 0);
    delta = (tz_hour * 60 + tz_minute) * 60000; // delta in mili-seconds
    if (match[9] === '-') delta = -delta;
  }

  date = new Date(Date.UTC(year, month, day, hour, minute, second, fraction));

  if (delta) date.setTime(date.getTime() - delta);

  return date;
}

function representYamlTimestamp(object /*, style*/) {
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

/*eslint-disable no-bitwise*/





// [ 64, 65, 66 ] -> [ padding, CR, LF ]
var BASE64_MAP = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n\r';


function resolveYamlBinary(data) {
  if (data === null) return false;

  var code, idx, bitlen = 0, max = data.length, map = BASE64_MAP;

  // Convert one by one.
  for (idx = 0; idx < max; idx++) {
    code = map.indexOf(data.charAt(idx));

    // Skip CR/LF
    if (code > 64) continue;

    // Fail on illegal characters
    if (code < 0) return false;

    bitlen += 6;
  }

  // If there are any bits left, source was corrupted
  return (bitlen % 8) === 0;
}

function constructYamlBinary(data) {
  var idx, tailbits,
      input = data.replace(/[\r\n=]/g, ''), // remove CR/LF & padding to simplify scan
      max = input.length,
      map = BASE64_MAP,
      bits = 0,
      result = [];

  // Collect by 6*4 bits (3 bytes)

  for (idx = 0; idx < max; idx++) {
    if ((idx % 4 === 0) && idx) {
      result.push((bits >> 16) & 0xFF);
      result.push((bits >> 8) & 0xFF);
      result.push(bits & 0xFF);
    }

    bits = (bits << 6) | map.indexOf(input.charAt(idx));
  }

  // Dump tail

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

function representYamlBinary(object /*, style*/) {
  var result = '', bits = 0, idx, tail,
      max = object.length,
      map = BASE64_MAP;

  // Convert every three bytes to 4 ASCII characters.

  for (idx = 0; idx < max; idx++) {
    if ((idx % 3 === 0) && idx) {
      result += map[(bits >> 18) & 0x3F];
      result += map[(bits >> 12) & 0x3F];
      result += map[(bits >> 6) & 0x3F];
      result += map[bits & 0x3F];
    }

    bits = (bits << 8) + object[idx];
  }

  // Dump tail

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

/*eslint-disable max-len,no-use-before-define*/







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
  return (c === 0x0A/* LF */) || (c === 0x0D/* CR */);
}

function is_WHITE_SPACE(c) {
  return (c === 0x09/* Tab */) || (c === 0x20/* Space */);
}

function is_WS_OR_EOL(c) {
  return (c === 0x09/* Tab */) ||
         (c === 0x20/* Space */) ||
         (c === 0x0A/* LF */) ||
         (c === 0x0D/* CR */);
}

function is_FLOW_INDICATOR(c) {
  return c === 0x2C/* , */ ||
         c === 0x5B/* [ */ ||
         c === 0x5D/* ] */ ||
         c === 0x7B/* { */ ||
         c === 0x7D/* } */;
}

function fromHexCode(c) {
  var lc;

  if ((0x30/* 0 */ <= c) && (c <= 0x39/* 9 */)) {
    return c - 0x30;
  }

  /*eslint-disable no-bitwise*/
  lc = c | 0x20;

  if ((0x61/* a */ <= lc) && (lc <= 0x66/* f */)) {
    return lc - 0x61 + 10;
  }

  return -1;
}

function escapedHexLen(c) {
  if (c === 0x78/* x */) { return 2; }
  if (c === 0x75/* u */) { return 4; }
  if (c === 0x55/* U */) { return 8; }
  return 0;
}

function fromDecimalCode(c) {
  if ((0x30/* 0 */ <= c) && (c <= 0x39/* 9 */)) {
    return c - 0x30;
  }

  return -1;
}

function simpleEscapeSequence(c) {
  /* eslint-disable indent */
  return (c === 0x30/* 0 */) ? '\x00' :
        (c === 0x61/* a */) ? '\x07' :
        (c === 0x62/* b */) ? '\x08' :
        (c === 0x74/* t */) ? '\x09' :
        (c === 0x09/* Tab */) ? '\x09' :
        (c === 0x6E/* n */) ? '\x0A' :
        (c === 0x76/* v */) ? '\x0B' :
        (c === 0x66/* f */) ? '\x0C' :
        (c === 0x72/* r */) ? '\x0D' :
        (c === 0x65/* e */) ? '\x1B' :
        (c === 0x20/* Space */) ? ' ' :
        (c === 0x22/* " */) ? '\x22' :
        (c === 0x2F/* / */) ? '/' :
        (c === 0x5C/* \ */) ? '\x5C' :
        (c === 0x4E/* N */) ? '\x85' :
        (c === 0x5F/* _ */) ? '\xA0' :
        (c === 0x4C/* L */) ? '\u2028' :
        (c === 0x50/* P */) ? '\u2029' : '';
}

function charFromCodepoint(c) {
  if (c <= 0xFFFF) {
    return String.fromCharCode(c);
  }
  // Encode UTF-16 surrogate pair
  // https://en.wikipedia.org/wiki/UTF-16#Code_points_U.2B010000_to_U.2B10FFFF
  return String.fromCharCode(
    ((c - 0x010000) >> 10) + 0xD800,
    ((c - 0x010000) & 0x03FF) + 0xDC00
  );
}

var simpleEscapeCheck = new Array(256); // integer, for fast access
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
  // (Hidden) Remove? makes the loader to expect YAML 1.1 documents
  // if such documents have no explicit %YAML directive
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

  // position of first leading tab in the current line,
  // used to make sure there are no tabs in the indentation
  this.firstTabInLine = -1;

  this.documents = [];

  /*
  this.version;
  this.checkLineBreaks;
  this.tagMap;
  this.anchorMap;
  this.tag;
  this.anchor;
  this.kind;
  this.result;*/

}


function generateError(state, message) {
  var mark = {
    name:     state.filename,
    buffer:   state.input.slice(0, -1), // omit trailing \0
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

  // The output is a plain object here, so keys can only be strings.
  // We need to convert keyNode to a string, but doing so can hang the process
  // (deeply nested arrays that explode exponentially using aliases).
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

  // Avoid code execution in load() via toString property
  // (still use its own toString for arrays, timestamps,
  // and whatever user schema extensions happen to have @@toStringTag)
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

    // used for this specific key only because Object.defineProperty is slow
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

  if (ch === 0x0A/* LF */) {
    state.position++;
  } else if (ch === 0x0D/* CR */) {
    state.position++;
    if (state.input.charCodeAt(state.position) === 0x0A/* LF */) {
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
      if (ch === 0x09/* Tab */ && state.firstTabInLine === -1) {
        state.firstTabInLine = state.position;
      }
      ch = state.input.charCodeAt(++state.position);
    }

    if (allowComments && ch === 0x23/* # */) {
      do {
        ch = state.input.charCodeAt(++state.position);
      } while (ch !== 0x0A/* LF */ && ch !== 0x0D/* CR */ && ch !== 0);
    }

    if (is_EOL(ch)) {
      readLineBreak(state);

      ch = state.input.charCodeAt(state.position);
      lineBreaks++;
      state.lineIndent = 0;

      while (ch === 0x20/* Space */) {
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

  // Condition state.position === state.lineStart is tested
  // in parent on each call, for efficiency. No needs to test here again.
  if ((ch === 0x2D/* - */ || ch === 0x2E/* . */) &&
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
      ch === 0x23/* # */    ||
      ch === 0x26/* & */    ||
      ch === 0x2A/* * */    ||
      ch === 0x21/* ! */    ||
      ch === 0x7C/* | */    ||
      ch === 0x3E/* > */    ||
      ch === 0x27/* ' */    ||
      ch === 0x22/* " */    ||
      ch === 0x25/* % */    ||
      ch === 0x40/* @ */    ||
      ch === 0x60/* ` */) {
    return false;
  }

  if (ch === 0x3F/* ? */ || ch === 0x2D/* - */) {
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
    if (ch === 0x3A/* : */) {
      following = state.input.charCodeAt(state.position + 1);

      if (is_WS_OR_EOL(following) ||
          withinFlowCollection && is_FLOW_INDICATOR(following)) {
        break;
      }

    } else if (ch === 0x23/* # */) {
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

  if (ch !== 0x27/* ' */) {
    return false;
  }

  state.kind = 'scalar';
  state.result = '';
  state.position++;
  captureStart = captureEnd = state.position;

  while ((ch = state.input.charCodeAt(state.position)) !== 0) {
    if (ch === 0x27/* ' */) {
      captureSegment(state, captureStart, state.position, true);
      ch = state.input.charCodeAt(++state.position);

      if (ch === 0x27/* ' */) {
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

  if (ch !== 0x22/* " */) {
    return false;
  }

  state.kind = 'scalar';
  state.result = '';
  state.position++;
  captureStart = captureEnd = state.position;

  while ((ch = state.input.charCodeAt(state.position)) !== 0) {
    if (ch === 0x22/* " */) {
      captureSegment(state, captureStart, state.position, true);
      state.position++;
      return true;

    } else if (ch === 0x5C/* \ */) {
      captureSegment(state, captureStart, state.position, true);
      ch = state.input.charCodeAt(++state.position);

      if (is_EOL(ch)) {
        skipSeparationSpace(state, false, nodeIndent);

        // TODO: rework to inline fn with no type cast?
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

  if (ch === 0x5B/* [ */) {
    terminator = 0x5D;/* ] */
    isMapping = false;
    _result = [];
  } else if (ch === 0x7B/* { */) {
    terminator = 0x7D;/* } */
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
    } else if (ch === 0x2C/* , */) {
      // "flow collection entries can never be completely empty", as per YAML 1.2, section 7.4
      throwError(state, "expected the node content, but found ','");
    }

    keyTag = keyNode = valueNode = null;
    isPair = isExplicitPair = false;

    if (ch === 0x3F/* ? */) {
      following = state.input.charCodeAt(state.position + 1);

      if (is_WS_OR_EOL(following)) {
        isPair = isExplicitPair = true;
        state.position++;
        skipSeparationSpace(state, true, nodeIndent);
      }
    }

    _line = state.line; // Save the current line.
    _lineStart = state.lineStart;
    _pos = state.position;
    composeNode(state, nodeIndent, CONTEXT_FLOW_IN, false, true);
    keyTag = state.tag;
    keyNode = state.result;
    skipSeparationSpace(state, true, nodeIndent);

    ch = state.input.charCodeAt(state.position);

    if ((isExplicitPair || state.line === _line) && ch === 0x3A/* : */) {
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

    if (ch === 0x2C/* , */) {
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

  if (ch === 0x7C/* | */) {
    folding = false;
  } else if (ch === 0x3E/* > */) {
    folding = true;
  } else {
    return false;
  }

  state.kind = 'scalar';
  state.result = '';

  while (ch !== 0) {
    ch = state.input.charCodeAt(++state.position);

    if (ch === 0x2B/* + */ || ch === 0x2D/* - */) {
      if (CHOMPING_CLIP === chomping) {
        chomping = (ch === 0x2B/* + */) ? CHOMPING_KEEP : CHOMPING_STRIP;
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

    if (ch === 0x23/* # */) {
      do { ch = state.input.charCodeAt(++state.position); }
      while (!is_EOL(ch) && (ch !== 0));
    }
  }

  while (ch !== 0) {
    readLineBreak(state);
    state.lineIndent = 0;

    ch = state.input.charCodeAt(state.position);

    while ((!detectedIndent || state.lineIndent < textIndent) &&
           (ch === 0x20/* Space */)) {
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

    // End of the scalar.
    if (state.lineIndent < textIndent) {

      // Perform the chomping.
      if (chomping === CHOMPING_KEEP) {
        state.result += common.repeat('\n', didReadContent ? 1 + emptyLines : emptyLines);
      } else if (chomping === CHOMPING_CLIP) {
        if (didReadContent) { // i.e. only if the scalar is not empty.
          state.result += '\n';
        }
      }

      // Break this `while` cycle and go to the funciton's epilogue.
      break;
    }

    // Folded style: use fancy rules to handle line breaks.
    if (folding) {

      // Lines starting with white space characters (more-indented lines) are not folded.
      if (is_WHITE_SPACE(ch)) {
        atMoreIndented = true;
        // except for the first content line (cf. Example 8.1)
        state.result += common.repeat('\n', didReadContent ? 1 + emptyLines : emptyLines);

      // End of more-indented block.
      } else if (atMoreIndented) {
        atMoreIndented = false;
        state.result += common.repeat('\n', emptyLines + 1);

      // Just one line break - perceive as the same line.
      } else if (emptyLines === 0) {
        if (didReadContent) { // i.e. only if we have already read some scalar content.
          state.result += ' ';
        }

      // Several line breaks - perceive as different lines.
      } else {
        state.result += common.repeat('\n', emptyLines);
      }

    // Literal style: just add exact number of line breaks between content lines.
    } else {
      // Keep all line breaks except the header line break.
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

  // there is a leading tab before this token, so it can't be a block sequence/mapping;
  // it can still be flow sequence/mapping or a scalar
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

    if (ch !== 0x2D/* - */) {
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

  // there is a leading tab before this token, so it can't be a block sequence/mapping;
  // it can still be flow sequence/mapping or a scalar
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
    _line = state.line; // Save the current line.

    //
    // Explicit notation case. There are two separate blocks:
    // first for the key (denoted by "?") and second for the value (denoted by ":")
    //
    if ((ch === 0x3F/* ? */ || ch === 0x3A/* : */) && is_WS_OR_EOL(following)) {

      if (ch === 0x3F/* ? */) {
        if (atExplicitKey) {
          storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, null, _keyLine, _keyLineStart, _keyPos);
          keyTag = keyNode = valueNode = null;
        }

        detected = true;
        atExplicitKey = true;
        allowCompact = true;

      } else if (atExplicitKey) {
        // i.e. 0x3A/* : */ === character after the explicit key.
        atExplicitKey = false;
        allowCompact = true;

      } else {
        throwError(state, 'incomplete explicit mapping pair; a key node is missed; or followed by a non-tabulated empty line');
      }

      state.position += 1;
      ch = following;

    //
    // Implicit notation case. Flow-style node as the key first, then ":", and the value.
    //
    } else {
      _keyLine = state.line;
      _keyLineStart = state.lineStart;
      _keyPos = state.position;

      if (!composeNode(state, flowIndent, CONTEXT_FLOW_OUT, false, true)) {
        // Neither implicit nor explicit notation.
        // Reading is done. Go to the epilogue.
        break;
      }

      if (state.line === _line) {
        ch = state.input.charCodeAt(state.position);

        while (is_WHITE_SPACE(ch)) {
          ch = state.input.charCodeAt(++state.position);
        }

        if (ch === 0x3A/* : */) {
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
          return true; // Keep the result of `composeNode`.
        }

      } else if (detected) {
        throwError(state, 'can not read a block mapping entry; a multiline key may not be an implicit key');

      } else {
        state.tag = _tag;
        state.anchor = _anchor;
        return true; // Keep the result of `composeNode`.
      }
    }

    //
    // Common reading code for both explicit and implicit notations.
    //
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

  //
  // Epilogue.
  //

  // Special case: last mapping's node contains only the key in explicit notation.
  if (atExplicitKey) {
    storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, null, _keyLine, _keyLineStart, _keyPos);
  }

  // Expose the resulting mapping.
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

  if (ch !== 0x21/* ! */) return false;

  if (state.tag !== null) {
    throwError(state, 'duplication of a tag property');
  }

  ch = state.input.charCodeAt(++state.position);

  if (ch === 0x3C/* < */) {
    isVerbatim = true;
    ch = state.input.charCodeAt(++state.position);

  } else if (ch === 0x21/* ! */) {
    isNamed = true;
    tagHandle = '!!';
    ch = state.input.charCodeAt(++state.position);

  } else {
    tagHandle = '!';
  }

  _position = state.position;

  if (isVerbatim) {
    do { ch = state.input.charCodeAt(++state.position); }
    while (ch !== 0 && ch !== 0x3E/* > */);

    if (state.position < state.length) {
      tagName = state.input.slice(_position, state.position);
      ch = state.input.charCodeAt(++state.position);
    } else {
      throwError(state, 'unexpected end of the stream within a verbatim tag');
    }
  } else {
    while (ch !== 0 && !is_WS_OR_EOL(ch)) {

      if (ch === 0x21/* ! */) {
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

  if (ch !== 0x26/* & */) return false;

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

  if (ch !== 0x2A/* * */) return false;

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
      indentStatus = 1, // 1: this>parent, 0: this=parent, -1: this<parent
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
      // Special case: block sequences are allowed to have same indentation level as the parent.
      // http://www.yaml.org/spec/1.2/spec.html#id2799784
      hasContent = allowBlockCollections && readBlockSequence(state, blockIndent);
    }
  }

  if (state.tag === null) {
    if (state.anchor !== null) {
      state.anchorMap[state.anchor] = state.result;
    }

  } else if (state.tag === '?') {
    // Implicit resolving is not allowed for non-scalar types, and '?'
    // non-specific tag is only automatically assigned to plain scalars.
    //
    // We only need to check kind conformity in case user explicitly assigns '?'
    // tag, for example like this: "!<?> [0]"
    //
    if (state.result !== null && state.kind !== 'scalar') {
      throwError(state, 'unacceptable node kind for !<?> tag; it should be "scalar", not "' + state.kind + '"');
    }

    for (typeIndex = 0, typeQuantity = state.implicitTypes.length; typeIndex < typeQuantity; typeIndex += 1) {
      type = state.implicitTypes[typeIndex];

      if (type.resolve(state.result)) { // `state.result` updated in resolver if matched
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
      // looking for multi type
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

    if (!type.resolve(state.result, state.tag)) { // `state.result` updated in resolver if matched
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

    if (state.lineIndent > 0 || ch !== 0x25/* % */) {
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

      if (ch === 0x23/* # */) {
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
      state.input.charCodeAt(state.position)     === 0x2D/* - */ &&
      state.input.charCodeAt(state.position + 1) === 0x2D/* - */ &&
      state.input.charCodeAt(state.position + 2) === 0x2D/* - */) {
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

    if (state.input.charCodeAt(state.position) === 0x2E/* . */) {
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

    // Add tailing `\n` if not exists
    if (input.charCodeAt(input.length - 1) !== 0x0A/* LF */ &&
        input.charCodeAt(input.length - 1) !== 0x0D/* CR */) {
      input += '\n';
    }

    // Strip BOM
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

  // Use 0 as string terminator. That significantly simplifies bounds check.
  state.input += '\0';

  while (state.input.charCodeAt(state.position) === 0x20/* Space */) {
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
    /*eslint-disable no-undefined*/
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

/*eslint-disable no-use-before-define*/





var _toString       = Object.prototype.toString;
var _hasOwnProperty = Object.prototype.hasOwnProperty;

var CHAR_BOM                  = 0xFEFF;
var CHAR_TAB                  = 0x09; /* Tab */
var CHAR_LINE_FEED            = 0x0A; /* LF */
var CHAR_CARRIAGE_RETURN      = 0x0D; /* CR */
var CHAR_SPACE                = 0x20; /* Space */
var CHAR_EXCLAMATION          = 0x21; /* ! */
var CHAR_DOUBLE_QUOTE         = 0x22; /* " */
var CHAR_SHARP                = 0x23; /* # */
var CHAR_PERCENT              = 0x25; /* % */
var CHAR_AMPERSAND            = 0x26; /* & */
var CHAR_SINGLE_QUOTE         = 0x27; /* ' */
var CHAR_ASTERISK             = 0x2A; /* * */
var CHAR_COMMA                = 0x2C; /* , */
var CHAR_MINUS                = 0x2D; /* - */
var CHAR_COLON                = 0x3A; /* : */
var CHAR_EQUALS               = 0x3D; /* = */
var CHAR_GREATER_THAN         = 0x3E; /* > */
var CHAR_QUESTION             = 0x3F; /* ? */
var CHAR_COMMERCIAL_AT        = 0x40; /* @ */
var CHAR_LEFT_SQUARE_BRACKET  = 0x5B; /* [ */
var CHAR_RIGHT_SQUARE_BRACKET = 0x5D; /* ] */
var CHAR_GRAVE_ACCENT         = 0x60; /* ` */
var CHAR_LEFT_CURLY_BRACKET   = 0x7B; /* { */
var CHAR_VERTICAL_LINE        = 0x7C; /* | */
var CHAR_RIGHT_CURLY_BRACKET  = 0x7D; /* } */

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

// Indents every line in a string. Empty lines (\n only) are not indented.
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

// [33] s-white ::= s-space | s-tab
function isWhitespace(c) {
  return c === CHAR_SPACE || c === CHAR_TAB;
}

// Returns true if the character can be printed without escaping.
// From YAML 1.2: "any allowed characters known to be non-printable
// should also be escaped. [However,] This isn’t mandatory"
// Derived from nb-char - \t - #x85 - #xA0 - #x2028 - #x2029.
function isPrintable(c) {
  return  (0x00020 <= c && c <= 0x00007E)
      || ((0x000A1 <= c && c <= 0x00D7FF) && c !== 0x2028 && c !== 0x2029)
      || ((0x0E000 <= c && c <= 0x00FFFD) && c !== CHAR_BOM)
      ||  (0x10000 <= c && c <= 0x10FFFF);
}

// [34] ns-char ::= nb-char - s-white
// [27] nb-char ::= c-printable - b-char - c-byte-order-mark
// [26] b-char  ::= b-line-feed | b-carriage-return
// Including s-white (for some reason, examples doesn't match specs in this aspect)
// ns-char ::= c-printable - b-line-feed - b-carriage-return - c-byte-order-mark
function isNsCharOrWhitespace(c) {
  return isPrintable(c)
    && c !== CHAR_BOM
    // - b-char
    && c !== CHAR_CARRIAGE_RETURN
    && c !== CHAR_LINE_FEED;
}

// [127]  ns-plain-safe(c) ::= c = flow-out  ⇒ ns-plain-safe-out
//                             c = flow-in   ⇒ ns-plain-safe-in
//                             c = block-key ⇒ ns-plain-safe-out
//                             c = flow-key  ⇒ ns-plain-safe-in
// [128] ns-plain-safe-out ::= ns-char
// [129]  ns-plain-safe-in ::= ns-char - c-flow-indicator
// [130]  ns-plain-char(c) ::=  ( ns-plain-safe(c) - “:” - “#” )
//                            | ( /* An ns-char preceding */ “#” )
//                            | ( “:” /* Followed by an ns-plain-safe(c) */ )
function isPlainSafe(c, prev, inblock) {
  var cIsNsCharOrWhitespace = isNsCharOrWhitespace(c);
  var cIsNsChar = cIsNsCharOrWhitespace && !isWhitespace(c);
  return (
    // ns-plain-safe
    inblock ? // c = flow-in
      cIsNsCharOrWhitespace
      : cIsNsCharOrWhitespace
        // - c-flow-indicator
        && c !== CHAR_COMMA
        && c !== CHAR_LEFT_SQUARE_BRACKET
        && c !== CHAR_RIGHT_SQUARE_BRACKET
        && c !== CHAR_LEFT_CURLY_BRACKET
        && c !== CHAR_RIGHT_CURLY_BRACKET
  )
    // ns-plain-char
    && c !== CHAR_SHARP // false on '#'
    && !(prev === CHAR_COLON && !cIsNsChar) // false on ': '
    || (isNsCharOrWhitespace(prev) && !isWhitespace(prev) && c === CHAR_SHARP) // change to true on '[^ ]#'
    || (prev === CHAR_COLON && cIsNsChar); // change to true on ':[^ ]'
}

// Simplified test for values allowed as the first character in plain style.
function isPlainSafeFirst(c) {
  // Uses a subset of ns-char - c-indicator
  // where ns-char = nb-char - s-white.
  // No support of ( ( “?” | “:” | “-” ) /* Followed by an ns-plain-safe(c)) */ ) part
  return isPrintable(c) && c !== CHAR_BOM
    && !isWhitespace(c) // - s-white
    // - (c-indicator ::=
    // “-” | “?” | “:” | “,” | “[” | “]” | “{” | “}”
    && c !== CHAR_MINUS
    && c !== CHAR_QUESTION
    && c !== CHAR_COLON
    && c !== CHAR_COMMA
    && c !== CHAR_LEFT_SQUARE_BRACKET
    && c !== CHAR_RIGHT_SQUARE_BRACKET
    && c !== CHAR_LEFT_CURLY_BRACKET
    && c !== CHAR_RIGHT_CURLY_BRACKET
    // | “#” | “&” | “*” | “!” | “|” | “=” | “>” | “'” | “"”
    && c !== CHAR_SHARP
    && c !== CHAR_AMPERSAND
    && c !== CHAR_ASTERISK
    && c !== CHAR_EXCLAMATION
    && c !== CHAR_VERTICAL_LINE
    && c !== CHAR_EQUALS
    && c !== CHAR_GREATER_THAN
    && c !== CHAR_SINGLE_QUOTE
    && c !== CHAR_DOUBLE_QUOTE
    // | “%” | “@” | “`”)
    && c !== CHAR_PERCENT
    && c !== CHAR_COMMERCIAL_AT
    && c !== CHAR_GRAVE_ACCENT;
}

// Simplified test for values allowed as the last character in plain style.
function isPlainSafeLast(c) {
  // just not whitespace or colon, it will be checked to be plain character later
  return !isWhitespace(c) && c !== CHAR_COLON;
}

// Same as 'string'.codePointAt(pos), but works in older browsers.
function codePointAt(string, pos) {
  var first = string.charCodeAt(pos), second;
  if (first >= 0xD800 && first <= 0xDBFF && pos + 1 < string.length) {
    second = string.charCodeAt(pos + 1);
    if (second >= 0xDC00 && second <= 0xDFFF) {
      // https://mathiasbynens.be/notes/javascript-encoding#surrogate-formulae
      return (first - 0xD800) * 0x400 + second - 0xDC00 + 0x10000;
    }
  }
  return first;
}

// Determines whether block indentation indicator is required.
function needIndentIndicator(string) {
  var leadingSpaceRe = /^\n* /;
  return leadingSpaceRe.test(string);
}

var STYLE_PLAIN   = 1,
    STYLE_SINGLE  = 2,
    STYLE_LITERAL = 3,
    STYLE_FOLDED  = 4,
    STYLE_DOUBLE  = 5;

// Determines which scalar styles are possible and returns the preferred style.
// lineWidth = -1 => no limit.
// Pre-conditions: str.length > 0.
// Post-conditions:
//    STYLE_PLAIN or STYLE_SINGLE => no \n are in the string.
//    STYLE_LITERAL => no lines are suitable for folding (or lineWidth is -1).
//    STYLE_FOLDED => a line > lineWidth and can be folded (and lineWidth != -1).
function chooseScalarStyle(string, singleLineOnly, indentPerLevel, lineWidth,
  testAmbiguousType, quotingType, forceQuotes, inblock) {

  var i;
  var char = 0;
  var prevChar = null;
  var hasLineBreak = false;
  var hasFoldableLine = false; // only checked if shouldTrackWidth
  var shouldTrackWidth = lineWidth !== -1;
  var previousLineBreak = -1; // count the first line correctly
  var plain = isPlainSafeFirst(codePointAt(string, 0))
          && isPlainSafeLast(codePointAt(string, string.length - 1));

  if (singleLineOnly || forceQuotes) {
    // Case: no block styles.
    // Check for disallowed characters to rule out plain and single.
    for (i = 0; i < string.length; char >= 0x10000 ? i += 2 : i++) {
      char = codePointAt(string, i);
      if (!isPrintable(char)) {
        return STYLE_DOUBLE;
      }
      plain = plain && isPlainSafe(char, prevChar, inblock);
      prevChar = char;
    }
  } else {
    // Case: block styles permitted.
    for (i = 0; i < string.length; char >= 0x10000 ? i += 2 : i++) {
      char = codePointAt(string, i);
      if (char === CHAR_LINE_FEED) {
        hasLineBreak = true;
        // Check if any line can be folded.
        if (shouldTrackWidth) {
          hasFoldableLine = hasFoldableLine ||
            // Foldable line = too long, and not more-indented.
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
    // in case the end is missing a \n
    hasFoldableLine = hasFoldableLine || (shouldTrackWidth &&
      (i - previousLineBreak - 1 > lineWidth &&
       string[previousLineBreak + 1] !== ' '));
  }
  // Although every style can represent \n without escaping, prefer block styles
  // for multiline, since they're more readable and they don't add empty lines.
  // Also prefer folding a super-long line.
  if (!hasLineBreak && !hasFoldableLine) {
    // Strings interpretable as another type have to be quoted;
    // e.g. the string 'true' vs. the boolean true.
    if (plain && !forceQuotes && !testAmbiguousType(string)) {
      return STYLE_PLAIN;
    }
    return quotingType === QUOTING_TYPE_DOUBLE ? STYLE_DOUBLE : STYLE_SINGLE;
  }
  // Edge case: block indentation indicator can only have one digit.
  if (indentPerLevel > 9 && needIndentIndicator(string)) {
    return STYLE_DOUBLE;
  }
  // At this point we know block styles are valid.
  // Prefer literal style unless we want to fold.
  if (!forceQuotes) {
    return hasFoldableLine ? STYLE_FOLDED : STYLE_LITERAL;
  }
  return quotingType === QUOTING_TYPE_DOUBLE ? STYLE_DOUBLE : STYLE_SINGLE;
}

// Note: line breaking/folding is implemented for only the folded style.
// NB. We drop the last trailing newline (if any) of a returned block scalar
//  since the dumper adds its own newline. This always works:
//    • No ending newline => unaffected; already using strip "-" chomping.
//    • Ending newline    => removed then restored.
//  Importantly, this keeps the "+" chomp indicator from gaining an extra line.
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

    var indent = state.indent * Math.max(1, level); // no 0-indent scalars
    // As indentation gets deeper, let the width decrease monotonically
    // to the lower bound min(state.lineWidth, 40).
    // Note that this implies
    //  state.lineWidth ≤ 40 + state.indent: width is fixed at the lower bound.
    //  state.lineWidth > 40 + state.indent: width decreases until the lower bound.
    // This behaves better than a constant minimum width which disallows narrower options,
    // or an indent threshold which causes the width to suddenly increase.
    var lineWidth = state.lineWidth === -1
      ? -1 : Math.max(Math.min(state.lineWidth, 40), state.lineWidth - indent);

    // Without knowing if keys are implicit/explicit, assume implicit for safety.
    var singleLineOnly = iskey
      // No block styles in flow mode.
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

// Pre-conditions: string is valid for a block scalar, 1 <= indentPerLevel <= 9.
function blockHeader(string, indentPerLevel) {
  var indentIndicator = needIndentIndicator(string) ? String(indentPerLevel) : '';

  // note the special case: the string '\n' counts as a "trailing" empty line.
  var clip =          string[string.length - 1] === '\n';
  var keep = clip && (string[string.length - 2] === '\n' || string === '\n');
  var chomp = keep ? '+' : (clip ? '' : '-');

  return indentIndicator + chomp + '\n';
}

// (See the note for writeScalar.)
function dropEndingNewline(string) {
  return string[string.length - 1] === '\n' ? string.slice(0, -1) : string;
}

// Note: a long line without a suitable break point will exceed the width limit.
// Pre-conditions: every char in str isPrintable, str.length > 0, width > 0.
function foldString(string, width) {
  // In folded style, $k$ consecutive newlines output as $k+1$ newlines—
  // unless they're before or after a more-indented line, or at the very
  // beginning or end, in which case $k$ maps to $k$.
  // Therefore, parse each chunk as newline(s) followed by a content line.
  var lineRe = /(\n+)([^\n]*)/g;

  // first line (possibly an empty line)
  var result = (function () {
    var nextLF = string.indexOf('\n');
    nextLF = nextLF !== -1 ? nextLF : string.length;
    lineRe.lastIndex = nextLF;
    return foldLine(string.slice(0, nextLF), width);
  }());
  // If we haven't reached the first content line yet, don't add an extra \n.
  var prevMoreIndented = string[0] === '\n' || string[0] === ' ';
  var moreIndented;

  // rest of the lines
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

// Greedy line breaking.
// Picks the longest line under the limit each time,
// otherwise settles for the shortest line over the limit.
// NB. More-indented lines *cannot* be folded, as that would add an extra \n.
function foldLine(line, width) {
  if (line === '' || line[0] === ' ') return line;

  // Since a more-indented line adds a \n, breaks can't be followed by a space.
  var breakRe = / [^ ]/g; // note: the match index will always be <= length-2.
  var match;
  // start is an inclusive index. end, curr, and next are exclusive.
  var start = 0, end, curr = 0, next = 0;
  var result = '';

  // Invariants: 0 <= start <= length-1.
  //   0 <= curr <= next <= max(0, length-2). curr - start <= width.
  // Inside the loop:
  //   A match implies length >= 2, so curr and next are <= length-2.
  while ((match = breakRe.exec(line))) {
    next = match.index;
    // maintain invariant: curr - start <= width
    if (next - start > width) {
      end = (curr > start) ? curr : next; // derive end <= length-2
      result += '\n' + line.slice(start, end);
      // skip the space that was output as \n
      start = end + 1;                    // derive start <= length-1
    }
    curr = next;
  }

  // By the invariants, start <= length-1, so there is something left over.
  // It is either the whole string or a part starting from non-whitespace.
  result += '\n';
  // Insert a break if the remainder is too long and there is a break available.
  if (line.length - start > width && curr > start) {
    result += line.slice(start, curr) + '\n' + line.slice(curr + 1);
  } else {
    result += line.slice(start);
  }

  return result.slice(1); // drop extra \n joiner
}

// Escapes a double-quoted string.
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

    // Write only valid elements, put null instead of invalid elements.
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

    // Write only valid elements, put null instead of invalid elements.
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
  state.dump = _result || '[]'; // Empty sequence if no valid values.
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
      continue; // Skip this pair because of invalid key;
    }

    if (state.dump.length > 1024) pairBuffer += '? ';

    pairBuffer += state.dump + (state.condenseFlow ? '"' : '') + ':' + (state.condenseFlow ? '' : ' ');

    if (!writeNode(state, level, objectValue, false, false)) {
      continue; // Skip this pair because of invalid value.
    }

    pairBuffer += state.dump;

    // Both key and value are valid.
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

  // Allow sorting keys so that the output file is deterministic
  if (state.sortKeys === true) {
    // Default sorting
    objectKeyList.sort();
  } else if (typeof state.sortKeys === 'function') {
    // Custom sort function
    objectKeyList.sort(state.sortKeys);
  } else if (state.sortKeys) {
    // Something is wrong
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
      continue; // Skip this pair because of invalid key.
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
      continue; // Skip this pair because of invalid value.
    }

    if (state.dump && CHAR_LINE_FEED === state.dump.charCodeAt(0)) {
      pairBuffer += ':';
    } else {
      pairBuffer += ': ';
    }

    pairBuffer += state.dump;

    // Both key and value are valid.
    _result += pairBuffer;
  }

  state.tag = _tag;
  state.dump = _result || '{}'; // Empty mapping if no valid pairs.
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

// Serializes `object` and writes it to global `result`.
// Returns true on success, or false on invalid object.
//
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
      // Need to encode all characters except those allowed by the spec:
      //
      // [35] ns-dec-digit    ::=  [#x30-#x39] /* 0-9 */
      // [36] ns-hex-digit    ::=  ns-dec-digit
      //                         | [#x41-#x46] /* A-F */ | [#x61-#x66] /* a-f */
      // [37] ns-ascii-letter ::=  [#x41-#x5A] /* A-Z */ | [#x61-#x7A] /* a-z */
      // [38] ns-word-char    ::=  ns-dec-digit | ns-ascii-letter | “-”
      // [39] ns-uri-char     ::=  “%” ns-hex-digit ns-hex-digit | ns-word-char | “#”
      //                         | “;” | “/” | “?” | “:” | “@” | “&” | “=” | “+” | “$” | “,”
      //                         | “_” | “.” | “!” | “~” | “*” | “'” | “(” | “)” | “[” | “]”
      //
      // Also need to encode '!' because it has special meaning (end of tag prefix).
      //
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

// Re-export all types in case user wants to create custom schema
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

// Removed functions from JS-YAML 3.0.x
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

// Note: this is the semver.org version of the spec that it implements
// Not necessarily the package version of this code.
const SEMVER_SPEC_VERSION = '2.0.0';

const MAX_LENGTH$2 = 256;
const MAX_SAFE_INTEGER$1 = Number.MAX_SAFE_INTEGER ||
  /* istanbul ignore next */ 9007199254740991;

// Max safe segment length for coercion.
const MAX_SAFE_COMPONENT_LENGTH = 16;

var constants = {
  SEMVER_SPEC_VERSION,
  MAX_LENGTH: MAX_LENGTH$2,
  MAX_SAFE_INTEGER: MAX_SAFE_INTEGER$1,
  MAX_SAFE_COMPONENT_LENGTH
};

var re$2 = {exports: {}};

const debug$1 = (
  typeof process === 'object' &&
  process.env &&
  process.env.NODE_DEBUG &&
  /\bsemver\b/i.test(process.env.NODE_DEBUG)
) ? (...args) => console.error('SEMVER', ...args)
  : () => {};

var debug_1 = debug$1;

(function (module, exports) {
const { MAX_SAFE_COMPONENT_LENGTH } = constants;
const debug = debug_1;
exports = module.exports = {};

// The actual regexps go on exports.re
const re = exports.re = [];
const src = exports.src = [];
const t = exports.t = {};
let R = 0;

const createToken = (name, value, isGlobal) => {
  const index = R++;
  debug(index, value);
  t[name] = index;
  src[index] = value;
  re[index] = new RegExp(value, isGlobal ? 'g' : undefined);
};

// The following Regular Expressions can be used for tokenizing,
// validating, and parsing SemVer version strings.

// ## Numeric Identifier
// A single `0`, or a non-zero digit followed by zero or more digits.

createToken('NUMERICIDENTIFIER', '0|[1-9]\\d*');
createToken('NUMERICIDENTIFIERLOOSE', '[0-9]+');

// ## Non-numeric Identifier
// Zero or more digits, followed by a letter or hyphen, and then zero or
// more letters, digits, or hyphens.

createToken('NONNUMERICIDENTIFIER', '\\d*[a-zA-Z-][a-zA-Z0-9-]*');

// ## Main Version
// Three dot-separated numeric identifiers.

createToken('MAINVERSION', `(${src[t.NUMERICIDENTIFIER]})\\.` +
                   `(${src[t.NUMERICIDENTIFIER]})\\.` +
                   `(${src[t.NUMERICIDENTIFIER]})`);

createToken('MAINVERSIONLOOSE', `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.` +
                        `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.` +
                        `(${src[t.NUMERICIDENTIFIERLOOSE]})`);

// ## Pre-release Version Identifier
// A numeric identifier, or a non-numeric identifier.

createToken('PRERELEASEIDENTIFIER', `(?:${src[t.NUMERICIDENTIFIER]
}|${src[t.NONNUMERICIDENTIFIER]})`);

createToken('PRERELEASEIDENTIFIERLOOSE', `(?:${src[t.NUMERICIDENTIFIERLOOSE]
}|${src[t.NONNUMERICIDENTIFIER]})`);

// ## Pre-release Version
// Hyphen, followed by one or more dot-separated pre-release version
// identifiers.

createToken('PRERELEASE', `(?:-(${src[t.PRERELEASEIDENTIFIER]
}(?:\\.${src[t.PRERELEASEIDENTIFIER]})*))`);

createToken('PRERELEASELOOSE', `(?:-?(${src[t.PRERELEASEIDENTIFIERLOOSE]
}(?:\\.${src[t.PRERELEASEIDENTIFIERLOOSE]})*))`);

// ## Build Metadata Identifier
// Any combination of digits, letters, or hyphens.

createToken('BUILDIDENTIFIER', '[0-9A-Za-z-]+');

// ## Build Metadata
// Plus sign, followed by one or more period-separated build metadata
// identifiers.

createToken('BUILD', `(?:\\+(${src[t.BUILDIDENTIFIER]
}(?:\\.${src[t.BUILDIDENTIFIER]})*))`);

// ## Full Version String
// A main version, followed optionally by a pre-release version and
// build metadata.

// Note that the only major, minor, patch, and pre-release sections of
// the version string are capturing groups.  The build metadata is not a
// capturing group, because it should not ever be used in version
// comparison.

createToken('FULLPLAIN', `v?${src[t.MAINVERSION]
}${src[t.PRERELEASE]}?${
  src[t.BUILD]}?`);

createToken('FULL', `^${src[t.FULLPLAIN]}$`);

// like full, but allows v1.2.3 and =1.2.3, which people do sometimes.
// also, 1.0.0alpha1 (prerelease without the hyphen) which is pretty
// common in the npm registry.
createToken('LOOSEPLAIN', `[v=\\s]*${src[t.MAINVERSIONLOOSE]
}${src[t.PRERELEASELOOSE]}?${
  src[t.BUILD]}?`);

createToken('LOOSE', `^${src[t.LOOSEPLAIN]}$`);

createToken('GTLT', '((?:<|>)?=?)');

// Something like "2.*" or "1.2.x".
// Note that "x.x" is a valid xRange identifer, meaning "any version"
// Only the first item is strictly required.
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

// Coercion.
// Extract anything that could conceivably be a part of a valid semver
createToken('COERCE', `${'(^|[^\\d])' +
              '(\\d{1,'}${MAX_SAFE_COMPONENT_LENGTH}})` +
              `(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?` +
              `(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?` +
              `(?:$|[^\\d])`);
createToken('COERCERTL', src[t.COERCE], true);

// Tilde ranges.
// Meaning is "reasonably at or greater than"
createToken('LONETILDE', '(?:~>?)');

createToken('TILDETRIM', `(\\s*)${src[t.LONETILDE]}\\s+`, true);
exports.tildeTrimReplace = '$1~';

createToken('TILDE', `^${src[t.LONETILDE]}${src[t.XRANGEPLAIN]}$`);
createToken('TILDELOOSE', `^${src[t.LONETILDE]}${src[t.XRANGEPLAINLOOSE]}$`);

// Caret ranges.
// Meaning is "at least and backwards compatible with"
createToken('LONECARET', '(?:\\^)');

createToken('CARETTRIM', `(\\s*)${src[t.LONECARET]}\\s+`, true);
exports.caretTrimReplace = '$1^';

createToken('CARET', `^${src[t.LONECARET]}${src[t.XRANGEPLAIN]}$`);
createToken('CARETLOOSE', `^${src[t.LONECARET]}${src[t.XRANGEPLAINLOOSE]}$`);

// A simple gt/lt/eq thing, or just "" to indicate "any version"
createToken('COMPARATORLOOSE', `^${src[t.GTLT]}\\s*(${src[t.LOOSEPLAIN]})$|^$`);
createToken('COMPARATOR', `^${src[t.GTLT]}\\s*(${src[t.FULLPLAIN]})$|^$`);

// An expression to strip any whitespace between the gtlt and the thing
// it modifies, so that `> 1.2.3` ==> `>1.2.3`
createToken('COMPARATORTRIM', `(\\s*)${src[t.GTLT]
}\\s*(${src[t.LOOSEPLAIN]}|${src[t.XRANGEPLAIN]})`, true);
exports.comparatorTrimReplace = '$1$2$3';

// Something like `1.2.3 - 1.2.4`
// Note that these all use the loose form, because they'll be
// checked against either the strict or loose comparator form
// later.
createToken('HYPHENRANGE', `^\\s*(${src[t.XRANGEPLAIN]})` +
                   `\\s+-\\s+` +
                   `(${src[t.XRANGEPLAIN]})` +
                   `\\s*$`);

createToken('HYPHENRANGELOOSE', `^\\s*(${src[t.XRANGEPLAINLOOSE]})` +
                        `\\s+-\\s+` +
                        `(${src[t.XRANGEPLAINLOOSE]})` +
                        `\\s*$`);

// Star ranges basically just allow anything at all.
createToken('STAR', '(<|>)?=?\\s*\\*');
// >=0.0.0 is like a star
createToken('GTE0', '^\\s*>=\\s*0\.0\.0\\s*$');
createToken('GTE0PRE', '^\\s*>=\\s*0\.0\.0-0\\s*$');
}(re$2, re$2.exports));

// parse out just the options we care about so we always get a consistent
// obj with keys in a consistent order.
const opts = ['includePrerelease', 'loose', 'rtl'];
const parseOptions$2 = options =>
  !options ? {}
  : typeof options !== 'object' ? { loose: true }
  : opts.filter(k => options[k]).reduce((options, k) => {
    options[k] = true;
    return options
  }, {});
var parseOptions_1 = parseOptions$2;

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
  rcompareIdentifiers
};

const debug = debug_1;
const { MAX_LENGTH: MAX_LENGTH$1, MAX_SAFE_INTEGER } = constants;
const { re: re$1, t: t$1 } = re$2.exports;

const parseOptions$1 = parseOptions_1;
const { compareIdentifiers } = identifiers;
class SemVer$2 {
  constructor (version, options) {
    options = parseOptions$1(options);

    if (version instanceof SemVer$2) {
      if (version.loose === !!options.loose &&
          version.includePrerelease === !!options.includePrerelease) {
        return version
      } else {
        version = version.version;
      }
    } else if (typeof version !== 'string') {
      throw new TypeError(`Invalid Version: ${version}`)
    }

    if (version.length > MAX_LENGTH$1) {
      throw new TypeError(
        `version is longer than ${MAX_LENGTH$1} characters`
      )
    }

    debug('SemVer', version, options);
    this.options = options;
    this.loose = !!options.loose;
    // this isn't actually relevant for versions, but keep it so that we
    // don't run into trouble passing this.options around.
    this.includePrerelease = !!options.includePrerelease;

    const m = version.trim().match(options.loose ? re$1[t$1.LOOSE] : re$1[t$1.FULL]);

    if (!m) {
      throw new TypeError(`Invalid Version: ${version}`)
    }

    this.raw = version;

    // these are actually numbers
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

    // numberify any prerelease numeric ids
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
    if (!(other instanceof SemVer$2)) {
      if (typeof other === 'string' && other === this.version) {
        return 0
      }
      other = new SemVer$2(other, this.options);
    }

    if (other.version === this.version) {
      return 0
    }

    return this.compareMain(other) || this.comparePre(other)
  }

  compareMain (other) {
    if (!(other instanceof SemVer$2)) {
      other = new SemVer$2(other, this.options);
    }

    return (
      compareIdentifiers(this.major, other.major) ||
      compareIdentifiers(this.minor, other.minor) ||
      compareIdentifiers(this.patch, other.patch)
    )
  }

  comparePre (other) {
    if (!(other instanceof SemVer$2)) {
      other = new SemVer$2(other, this.options);
    }

    // NOT having a prerelease is > having one
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
    if (!(other instanceof SemVer$2)) {
      other = new SemVer$2(other, this.options);
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

  // preminor will bump the version up to the next minor release, and immediately
  // down to pre-release. premajor and prepatch work the same way.
  inc (release, identifier) {
    switch (release) {
      case 'premajor':
        this.prerelease.length = 0;
        this.patch = 0;
        this.minor = 0;
        this.major++;
        this.inc('pre', identifier);
        break
      case 'preminor':
        this.prerelease.length = 0;
        this.patch = 0;
        this.minor++;
        this.inc('pre', identifier);
        break
      case 'prepatch':
        // If this is already a prerelease, it will bump to the next version
        // drop any prereleases that might already exist, since they are not
        // relevant at this point.
        this.prerelease.length = 0;
        this.inc('patch', identifier);
        this.inc('pre', identifier);
        break
      // If the input is a non-prerelease version, this acts the same as
      // prepatch.
      case 'prerelease':
        if (this.prerelease.length === 0) {
          this.inc('patch', identifier);
        }
        this.inc('pre', identifier);
        break

      case 'major':
        // If this is a pre-major version, bump up to the same major version.
        // Otherwise increment major.
        // 1.0.0-5 bumps to 1.0.0
        // 1.1.0 bumps to 2.0.0
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
        // If this is a pre-minor version, bump up to the same minor version.
        // Otherwise increment minor.
        // 1.2.0-5 bumps to 1.2.0
        // 1.2.1 bumps to 1.3.0
        if (this.patch !== 0 || this.prerelease.length === 0) {
          this.minor++;
        }
        this.patch = 0;
        this.prerelease = [];
        break
      case 'patch':
        // If this is not a pre-release version, it will increment the patch.
        // If it is a pre-release it will bump up to the same patch version.
        // 1.2.0-5 patches to 1.2.0
        // 1.2.0 patches to 1.2.1
        if (this.prerelease.length === 0) {
          this.patch++;
        }
        this.prerelease = [];
        break
      // This probably shouldn't be used publicly.
      // 1.0.0 'pre' would become 1.0.0-0 which is the wrong direction.
      case 'pre':
        if (this.prerelease.length === 0) {
          this.prerelease = [0];
        } else {
          let i = this.prerelease.length;
          while (--i >= 0) {
            if (typeof this.prerelease[i] === 'number') {
              this.prerelease[i]++;
              i = -2;
            }
          }
          if (i === -1) {
            // didn't increment anything
            this.prerelease.push(0);
          }
        }
        if (identifier) {
          // 1.2.0-beta.1 bumps to 1.2.0-beta.2,
          // 1.2.0-beta.fooblz or 1.2.0-beta bumps to 1.2.0-beta.0
          if (this.prerelease[0] === identifier) {
            if (isNaN(this.prerelease[1])) {
              this.prerelease = [identifier, 0];
            }
          } else {
            this.prerelease = [identifier, 0];
          }
        }
        break

      default:
        throw new Error(`invalid increment argument: ${release}`)
    }
    this.format();
    this.raw = this.version;
    return this
  }
}

var semver = SemVer$2;

const {MAX_LENGTH} = constants;
const { re, t } = re$2.exports;
const SemVer$1 = semver;

const parseOptions = parseOptions_1;
const parse = (version, options) => {
  options = parseOptions(options);

  if (version instanceof SemVer$1) {
    return version
  }

  if (typeof version !== 'string') {
    return null
  }

  if (version.length > MAX_LENGTH) {
    return null
  }

  const r = options.loose ? re[t.LOOSE] : re[t.FULL];
  if (!r.test(version)) {
    return null
  }

  try {
    return new SemVer$1(version, options)
  } catch (er) {
    return null
  }
};

var parse_1 = parse;

const SemVer = semver;
const compare$2 = (a, b, loose) =>
  new SemVer(a, loose).compare(new SemVer(b, loose));

var compare_1 = compare$2;

const compare$1 = compare_1;
const lt = (a, b, loose) => compare$1(a, b, loose) < 0;
var lt_1 = lt;

const allowedKeys = [
  "added",
  "napiVersion",
  "deprecated",
  "removed",
  "changes",
];
const changesExpectedKeys = ["version", "pr-url", "description"];
const VERSION_PLACEHOLDER = "REPLACEME";
const MAX_SAFE_SEMVER_VERSION = parse_1(
  Array.from({ length: 3 }, () => Number.MAX_SAFE_INTEGER).join(".")
);
const validVersionNumberRegex = /^v\d+\.\d+\.\d+$/;
const prUrlRegex = new RegExp("^https://github.com/nodejs/node/pull/\\d+$");
const privatePRUrl = "https://github.com/nodejs-private/node-private/pull/";

let releasedVersions;
let invalidVersionMessage = "version(s) must respect the pattern `vx.x.x` or";
if (process.env.NODE_RELEASED_VERSIONS) {
  console.log("Using release list from env...");
  releasedVersions = process.env.NODE_RELEASED_VERSIONS.split(",").map(
    (v) => `v${v}`
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
    // Always ignore 0.0.x and 0.1.x release numbers:
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
      lt_1(
        getValidSemver(versions[index - 1]),
        getValidSemver(versions[index])
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
        node
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
      node
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
        node
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
        node
      );
    }

    if (typeof change.description !== "string" || !change.description.length) {
      file.message(
        `changes[${index}]: must contain a non-empty description`,
        node
      );
    } else if (!change.description.endsWith(".")) {
      file.message(
        `changes[${index}]: description must end with a period`,
        node
      );
    }

    changesVersions.push(
      Array.isArray(change.version) ? change.version[0] : change.version
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
        node
      );
      break;

    case kWrongKeyOrder:
      file.message(
        "YAML dictionary keys should be in this order: " +
          allowedKeys.join(", "),
        node
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
      node
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
  visit$1(tree, "html", function visitor(node) {
    if (node.value.startsWith("<!--YAML\n"))
      file.message(
        "Expected `<!-- YAML`, found `<!--YAML`. Please add a space",
        node
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
  validateYAMLComments
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

  // If it starts with a letter, make sure it is a word break.
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

  // If it ends with a letter, make sure it is a word break.
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

  visit$1(ast, 'text', checkText);

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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module rule-style
 * @fileoverview
 *   Warn when the thematic breaks (horizontal rules) violate a given or
 *   detected style.
 *
 *   Options: `string`, either a corect thematic breaks such as `***`, or
 *   `'consistent'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used thematic break style and warns when
 *   subsequent rules use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   has three settings that define how rules are created:
 *
 *   *   [`rule`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsrule)
 *       (default: `*`) — Marker to use
 *   *   [`ruleRepetition`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsrulerepetition)
 *       (default: `3`) — Number of markers to use
 *   *   [`ruleSpaces`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsrulespaces)
 *       (default: `true`) — Whether to pad markers with spaces
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md", "setting": "* * *"}
 *
 *   * * *
 *
 *   * * *
 *
 * @example
 *   {"name": "ok.md", "setting": "_______"}
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
 *   {"name": "not-ok.md", "label": "output", "setting": "💩", "positionless": true}
 *
 *   1:1: Incorrect preferred rule style: provide a correct markdown rule or `'consistent'`
 */

const remarkLintRuleStyle = lintRule(
  {
    origin: 'remark-lint:rule-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-rule-style#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const value = String(file);

    if (option !== 'consistent' && /[^-_* ]/.test(option)) {
      file.fail(
        "Incorrect preferred rule style: provide a correct markdown rule or `'consistent'`"
      );
    }

    visit$1(tree, 'thematicBreak', (node) => {
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module strong-marker
 * @fileoverview
 *   Warn for violating importance (strong) markers.
 *
 *   Options: `'consistent'`, `'*'`, or `'_'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used importance style and warns when
 *   subsequent importance sequences use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats importance using an `*` (asterisk) by default.
 *   Pass
 *   [`strong: '_'`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsstrong)
 *   to use `_` (underscore) instead.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
 *   {"name": "ok.md", "setting": "*"}
 *
 *   **foo**.
 *
 * @example
 *   {"name": "ok.md", "setting": "_"}
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
 *   {"name": "not-ok.md", "label": "output", "setting": "💩", "positionless": true}
 *
 *   1:1: Incorrect strong marker `💩`: use either `'consistent'`, `'*'`, or `'_'`
 */

const remarkLintStrongMarker = lintRule(
  {
    origin: 'remark-lint:strong-marker',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-strong-marker#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const value = String(file);

    if (option !== '*' && option !== '_' && option !== 'consistent') {
      file.fail(
        'Incorrect strong marker `' +
          option +
          "`: use either `'consistent'`, `'*'`, or `'_'`"
      );
    }

    visit$1(tree, 'strong', (node) => {
      const start = pointStart(node).offset;

      if (typeof start === 'number') {
        const marker = /** @type {Marker} */ (value.charAt(start));

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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module table-cell-padding
 * @fileoverview
 *   Warn when table cells are incorrectly padded.
 *
 *   Options: `'consistent'`, `'padded'`, or `'compact'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used cell padding style and warns when
 *   subsequent cells use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats tables with padding by default.
 *   Pass
 *   [`spacedTable: false`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsspacedtable)
 *   to not use padding.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md", "setting": "padded", "gfm": true}
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "setting": "padded", "gfm": true}
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
 *   {"name": "not-ok.md", "label": "output", "setting": "padded", "gfm": true}
 *
 *   3:8: Cell should be padded
 *   3:9: Cell should be padded
 *   7:2: Cell should be padded
 *   7:17: Cell should be padded
 *   13:9: Cell should be padded with 1 space, not 2
 *   13:20: Cell should be padded with 1 space, not 2
 *   13:21: Cell should be padded with 1 space, not 2
 *   13:29: Cell should be padded with 1 space, not 2
 *   13:30: Cell should be padded with 1 space, not 2
 *
 * @example
 *   {"name": "ok.md", "setting": "compact", "gfm": true}
 *
 *   |A    |B    |
 *   |-----|-----|
 *   |Alpha|Bravo|
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "setting": "compact", "gfm": true}
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
 *   {"name": "not-ok.md", "label": "output", "setting": "compact", "gfm": true}
 *
 *   3:2: Cell should be compact
 *   3:11: Cell should be compact
 *   7:16: Cell should be compact
 *
 * @example
 *   {"name": "ok-padded.md", "setting": "consistent", "gfm": true}
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
 *   {"name": "not-ok-padded.md", "label": "input", "setting": "consistent", "gfm": true}
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
 *   {"name": "not-ok-padded.md", "label": "output", "setting": "consistent", "gfm": true}
 *
 *   7:2: Cell should be padded
 *
 * @example
 *   {"name": "ok-compact.md", "setting": "consistent", "gfm": true}
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
 *   {"name": "not-ok-compact.md", "label": "input", "setting": "consistent", "gfm": true}
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
 *   {"name": "not-ok-compact.md", "label": "output", "setting": "consistent", "gfm": true}
 *
 *   7:16: Cell should be compact
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "setting": "💩", "positionless": true, "gfm": true}
 *
 *   1:1: Incorrect table cell padding style `💩`, expected `'padded'`, `'compact'`, or `'consistent'`
 *
 * @example
 *   {"name": "empty.md", "label": "input", "setting": "padded", "gfm": true}
 *
 *   <!-- Empty cells are OK, but those surrounding them may not be. -->
 *
 *   |        | Alpha | Bravo|
 *   | ------ | ----- | ---: |
 *   | Charlie|       |  Echo|
 *
 * @example
 *   {"name": "empty.md", "label": "output", "setting": "padded", "gfm": true}
 *
 *   3:25: Cell should be padded
 *   5:10: Cell should be padded
 *   5:25: Cell should be padded
 *
 * @example
 *   {"name": "missing-body.md", "setting": "padded", "gfm": true}
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
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
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

    visit$1(tree, 'table', (node) => {
      const rows = node.children;
      // To do: fix types to always have `align` defined.
      /* c8 ignore next */
      const align = node.align || [];
      /** @type {number[]} */
      const sizes = [];
      /** @type {Entry[]} */
      const entries = [];
      let index = -1;

      // Check align row.
      // Because there’s zero to two `:`, and there must be one `-`.
      while (++index < align.length) {
        const alignment = align[index];
        sizes[index] = alignment === 'center' ? 3 : alignment ? 2 : 1;
      }

      index = -1;

      // Check rows.
      while (++index < rows.length) {
        const row = rows[index];
        let column = -1;

        // Check fences (before, between, and after cells).
        while (++column < row.children.length) {
          const cell = row.children[column];

          if (cell.children.length > 0) {
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
              start: contentStart - cellStart - (column ? 0 : 1),
              end: cellEnd - contentEnd - 1,
              column
            });

            // Detect max space per column.
            sizes[column] = Math.max(
              // More cells could exist than the align row for generated tables.
              /* c8 ignore next */
              sizes[column] || 0,
              contentEnd - contentStart
            );
          }
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

      return SKIP$1
    });

    /**
     * @param {'start'|'end'} side
     * @param {Entry} entry
     * @param {0|1} style
     * @param {number[]} sizes
     */
    function checkSide(side, entry, style, sizes) {
      const cell = entry.node;
      const column = entry.column;
      const spacing = entry[side];

      if (spacing === undefined || spacing === style) {
        return
      }

      let reason = 'Cell should be ';

      if (style === 0) {
        // Ignore every cell except the biggest in the column.
        if (size$1(cell) < sizes[column]) {
          return
        }

        reason += 'compact';
      } else {
        reason += 'padded';

        if (spacing > style) {
          // May be right or center aligned.
          if (size$1(cell) < sizes[column]) {
            return
          }

          reason += ' with 1 space, not ' + spacing;
        }
      }

      /** @type {Point} */
      let point;

      if (side === 'start') {
        point = pointStart(cell);
        if (!column) {
          point.column++;

          if (typeof point.offset === 'number') {
            point.offset++;
          }
        }
      } else {
        point = pointEnd(cell);
        point.column--;

        if (typeof point.offset === 'number') {
          point.offset--;
        }
      }

      file.message(reason, point);
    }
  }
);

var remarkLintTableCellPadding$1 = remarkLintTableCellPadding;

/**
 * @param {TableCell} node
 * @returns {number}
 */
function size$1(node) {
  const head = pointStart(node.children[0]).offset;
  const tail = pointEnd(node.children[node.children.length - 1]).offset;
  // Only called when we’re sure offsets exist.
  /* c8 ignore next */
  return typeof head === 'number' && typeof tail === 'number' ? tail - head : 0
}

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module table-pipes
 * @fileoverview
 *   Warn when table rows are not fenced with pipes.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   creates fenced rows with initial and final pipes by default.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
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

/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module unordered-list-marker-style
 * @fileoverview
 *   Warn when the list item marker style of unordered lists violate a given
 *   style.
 *
 *   Options: `'consistent'`, `'-'`, `'*'`, or `'+'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used list style and warns when subsequent
 *   lists use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats unordered lists using `-` (hyphen-minus) by default.
 *   Pass
 *   [`bullet: '*'` or `bullet: '+'`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsbullet)
 *   to use `*` (asterisk) or `+` (plus sign) instead.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
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
 *   {"name": "ok.md", "setting": "*"}
 *
 *   * Foo
 *
 * @example
 *   {"name": "ok.md", "setting": "-"}
 *
 *   - Foo
 *
 * @example
 *   {"name": "ok.md", "setting": "+"}
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
 *   {"name": "not-ok.md", "label": "output", "setting": "💩", "positionless": true}
 *
 *   1:1: Incorrect unordered list item marker style `💩`: use either `'-'`, `'*'`, or `'+'`
 */

const markers = new Set(['-', '*', '+']);

const remarkLintUnorderedListMarkerStyle = lintRule(
  {
    origin: 'remark-lint:unordered-list-marker-style',
    url: 'https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-unordered-list-marker-style#readme'
  },
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const value = String(file);

    if (option !== 'consistent' && !markers.has(option)) {
      file.fail(
        'Incorrect unordered list item marker style `' +
          option +
          "`: use either `'-'`, `'*'`, or `'+'`"
      );
    }

    visit$1(tree, 'list', (node) => {
      if (node.ordered) return

      let index = -1;

      while (++index < node.children.length) {
        const child = node.children[index];

        if (!generated(child)) {
          const marker = /** @type {Marker} */ (
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

// @see https://github.com/nodejs/node/blob/HEAD/doc/guides/doc-style-guide.md

// Add in rules alphabetically after Gfm and PresetLintRecommended.
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
  remarkLintNoTrailingSpaces,
  remarkLintNodejsLinks,
  remarkLintNodejsYamlComments,
  [
    remarkLintProhibitedStrings,
    [
      { yes: "End-of-Life" },
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
  listItemIndent: 1,
  tightDefinitions: true,
};

const remarkPresetLintNode = { plugins, settings };

/**
 * @typedef {import('vfile').VFileValue} Value
 * @typedef {import('vfile').VFileOptions} Options
 * @typedef {import('vfile').BufferEncoding} BufferEncoding
 *
 * @typedef {number|string} Mode
 * @typedef {BufferEncoding|{encoding?: null|BufferEncoding, flag?: string}} ReadOptions
 * @typedef {BufferEncoding|{encoding?: null|BufferEncoding, mode: Mode?, flag?: string}} WriteOptions
 *
 * @typedef {string|Uint8Array} Path Path of the file.
 * @typedef {Path|URL|Options|VFile} Compatible Things that can be
 *   passed to the function.
 */

/**
 * Create a virtual file from a description.
 * If `options` is a string or a buffer, it’s used as the path.
 * If it’s a VFile itself, it’s returned instead.
 * In all other cases, the options are passed through to `vfile()`.
 *
 * @param {Compatible} [options]
 * @returns {VFile}
 */
function toVFile(options) {
  if (typeof options === 'string' || options instanceof URL$1) {
    options = {path: options};
  } else if (isBuffer(options)) {
    options = {path: String(options)};
  }

  return looksLikeAVFile(options) ? options : new VFile(options)
}

/**
 * Create a virtual file and read it in, synchronously.
 *
 * @param {Compatible} description
 * @param {ReadOptions} [options]
 * @returns {VFile}
 */
function readSync(description, options) {
  const file = toVFile(description);
  file.value = fs.readFileSync(path$1.resolve(file.cwd, file.path), options);
  return file
}

/**
 * Create a virtual file and write it in, synchronously.
 *
 * @param {Compatible} description
 * @param {WriteOptions} [options]
 * @returns {VFile}
 */
function writeSync(description, options) {
  const file = toVFile(description);
  fs.writeFileSync(path$1.resolve(file.cwd, file.path), file.value || '', options);
  return file
}

const read =
  /**
   * @type {{
   *   (description: Compatible, options: ReadOptions, callback: Callback): void
   *   (description: Compatible, callback: Callback): void
   *   (description: Compatible, options?: ReadOptions): Promise<VFile>
   * }}
   */
  (
    /**
     * Create a virtual file and read it in, asynchronously.
     *
     * @param {Compatible} description
     * @param {ReadOptions} [options]
     * @param {Callback} [callback]
     */
    function (description, options, callback) {
      const file = toVFile(description);

      if (!callback && typeof options === 'function') {
        callback = options;
        options = null;
      }

      if (!callback) {
        return new Promise(executor)
      }

      executor(resolve, callback);

      /**
       * @param {VFile} result
       */
      function resolve(result) {
        callback(null, result);
      }

      /**
       * @param {(x: VFile) => void} resolve
       * @param {(x: Error, y?: VFile) => void} reject
       */
      function executor(resolve, reject) {
        /** @type {string} */
        let fp;

        try {
          fp = path$1.resolve(file.cwd, file.path);
        } catch (error) {
          return reject(error)
        }

        fs.readFile(fp, options, done);

        /**
         * @param {Error} error
         * @param {Value} result
         */
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
  );

const write =
  /**
   * @type {{
   *   (description: Compatible, options: WriteOptions, callback: Callback): void
   *   (description: Compatible, callback: Callback): void
   *   (description: Compatible, options?: WriteOptions): Promise<VFile>
   * }}
   */
  (
    /**
     * Create a virtual file and write it in, asynchronously.
     *
     * @param {Compatible} description
     * @param {WriteOptions} [options]
     * @param {Callback} [callback]
     */
    function (description, options, callback) {
      const file = toVFile(description);

      // Weird, right? Otherwise `fs` doesn’t accept it.
      if (!callback && typeof options === 'function') {
        callback = options;
        options = undefined;
      }

      if (!callback) {
        return new Promise(executor)
      }

      executor(resolve, callback);

      /**
       * @param {VFile} result
       */
      function resolve(result) {
        callback(null, result);
      }

      /**
       * @param {(x: VFile) => void} resolve
       * @param {(x: Error, y?: VFile) => void} reject
       */
      function executor(resolve, reject) {
        /** @type {string} */
        let fp;

        try {
          fp = path$1.resolve(file.cwd, file.path);
        } catch (error) {
          return reject(error)
        }

        fs.writeFile(fp, file.value || '', options, done);

        /**
         * @param {Error} error
         */
        function done(error) {
          if (error) {
            reject(error);
          } else {
            resolve(file);
          }
        }
      }
    }
  );

/**
 * @param {Compatible} value
 * @returns {value is VFile}
 */
function looksLikeAVFile(value) {
  return (
    value &&
    typeof value === 'object' &&
    'message' in value &&
    'messages' in value
  )
}

toVFile.readSync = readSync;
toVFile.writeSync = writeSync;
toVFile.read = read;
toVFile.write = write;

function hasFlag(flag, argv = process$1.argv) {
	const prefix = flag.startsWith('-') ? '' : (flag.length === 1 ? '-' : '--');
	const position = argv.indexOf(prefix + flag);
	const terminatorPosition = argv.indexOf('--');
	return position !== -1 && (terminatorPosition === -1 || position < terminatorPosition);
}

const {env} = process$1;

let flagForceColor;
if (hasFlag('no-color') ||
	hasFlag('no-colors') ||
	hasFlag('color=false') ||
	hasFlag('color=never')) {
	flagForceColor = 0;
} else if (hasFlag('color') ||
	hasFlag('colors') ||
	hasFlag('color=true') ||
	hasFlag('color=always')) {
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
		has16m: level >= 3
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
		if (hasFlag('color=16m') ||
			hasFlag('color=full') ||
			hasFlag('color=truecolor')) {
			return 3;
		}

		if (hasFlag('color=256')) {
			return 2;
		}
	}

	if (haveStream && !streamIsTTY && forceColor === undefined) {
		return 0;
	}

	const min = forceColor || 0;

	if (env.TERM === 'dumb') {
		return min;
	}

	if (process$1.platform === 'win32') {
		// Windows 10 build 10586 is the first Windows release that supports 256 colors.
		// Windows 10 build 14931 is the first release that supports 16m/TrueColor.
		const osRelease = os.release().split('.');
		if (
			Number(osRelease[0]) >= 10 &&
			Number(osRelease[2]) >= 10586
		) {
			return Number(osRelease[2]) >= 14931 ? 3 : 2;
		}

		return 1;
	}

	if ('CI' in env) {
		if (['TRAVIS', 'CIRCLECI', 'APPVEYOR', 'GITLAB_CI', 'GITHUB_ACTIONS', 'BUILDKITE', 'DRONE'].some(sign => sign in env) || env.CI_NAME === 'codeship') {
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

	if ('TERM_PROGRAM' in env) {
		const version = Number.parseInt((env.TERM_PROGRAM_VERSION || '').split('.')[0], 10);

		switch (env.TERM_PROGRAM) {
			case 'iTerm.app':
				return version >= 3 ? 3 : 2;
			case 'Apple_Terminal':
				return 2;
			// No default
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
		...options
	});

	return translateLevel(level);
}

const supportsColor = {
	stdout: createSupportsColor({isTTY: tty.isatty(1)}),
	stderr: createSupportsColor({isTTY: tty.isatty(2)})
};

function ansiRegex({onlyFirst = false} = {}) {
	const pattern = [
	    '[\\u001B\\u009B][[\\]()#;?]*(?:(?:(?:(?:;[-a-zA-Z\\d\\/#&.:=?%@~_]+)*|[a-zA-Z\\d]+(?:;[-a-zA-Z\\d\\/#&.:=?%@~_]*)*)?\\u0007)',
		'(?:(?:\\d{1,4}(?:;\\d{0,4})*)?[\\dA-PR-TZcf-ntqry=><~]))'
	].join('|');

	return new RegExp(pattern, onlyFirst ? undefined : 'g');
}

function stripAnsi(string) {
	if (typeof string !== 'string') {
		throw new TypeError(`Expected a \`string\`, got \`${typeof string}\``);
	}

	return string.replace(ansiRegex(), '');
}

/* eslint-disable yoda */

function isFullwidthCodePoint(codePoint) {
	if (!Number.isInteger(codePoint)) {
		return false;
	}

	// Code points are derived from:
	// https://unicode.org/Public/UNIDATA/EastAsianWidth.txt
	return codePoint >= 0x1100 && (
		codePoint <= 0x115F || // Hangul Jamo
		codePoint === 0x2329 || // LEFT-POINTING ANGLE BRACKET
		codePoint === 0x232A || // RIGHT-POINTING ANGLE BRACKET
		// CJK Radicals Supplement .. Enclosed CJK Letters and Months
		(0x2E80 <= codePoint && codePoint <= 0x3247 && codePoint !== 0x303F) ||
		// Enclosed CJK Letters and Months .. CJK Unified Ideographs Extension A
		(0x3250 <= codePoint && codePoint <= 0x4DBF) ||
		// CJK Unified Ideographs .. Yi Radicals
		(0x4E00 <= codePoint && codePoint <= 0xA4C6) ||
		// Hangul Jamo Extended-A
		(0xA960 <= codePoint && codePoint <= 0xA97C) ||
		// Hangul Syllables
		(0xAC00 <= codePoint && codePoint <= 0xD7A3) ||
		// CJK Compatibility Ideographs
		(0xF900 <= codePoint && codePoint <= 0xFAFF) ||
		// Vertical Forms
		(0xFE10 <= codePoint && codePoint <= 0xFE19) ||
		// CJK Compatibility Forms .. Small Form Variants
		(0xFE30 <= codePoint && codePoint <= 0xFE6B) ||
		// Halfwidth and Fullwidth Forms
		(0xFF01 <= codePoint && codePoint <= 0xFF60) ||
		(0xFFE0 <= codePoint && codePoint <= 0xFFE6) ||
		// Kana Supplement
		(0x1B000 <= codePoint && codePoint <= 0x1B001) ||
		// Enclosed Ideographic Supplement
		(0x1F200 <= codePoint && codePoint <= 0x1F251) ||
		// CJK Unified Ideographs Extension B .. Tertiary Ideographic Plane
		(0x20000 <= codePoint && codePoint <= 0x3FFFD)
	);
}

var emojiRegex = function () {
  // https://mths.be/emoji
  return /\uD83C\uDFF4\uDB40\uDC67\uDB40\uDC62(?:\uDB40\uDC77\uDB40\uDC6C\uDB40\uDC73|\uDB40\uDC73\uDB40\uDC63\uDB40\uDC74|\uDB40\uDC65\uDB40\uDC6E\uDB40\uDC67)\uDB40\uDC7F|(?:\uD83E\uDDD1\uD83C\uDFFF\u200D\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1|\uD83D\uDC69\uD83C\uDFFF\u200D\uD83E\uDD1D\u200D(?:\uD83D[\uDC68\uDC69]))(?:\uD83C[\uDFFB-\uDFFE])|(?:\uD83E\uDDD1\uD83C\uDFFE\u200D\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1|\uD83D\uDC69\uD83C\uDFFE\u200D\uD83E\uDD1D\u200D(?:\uD83D[\uDC68\uDC69]))(?:\uD83C[\uDFFB-\uDFFD\uDFFF])|(?:\uD83E\uDDD1\uD83C\uDFFD\u200D\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1|\uD83D\uDC69\uD83C\uDFFD\u200D\uD83E\uDD1D\u200D(?:\uD83D[\uDC68\uDC69]))(?:\uD83C[\uDFFB\uDFFC\uDFFE\uDFFF])|(?:\uD83E\uDDD1\uD83C\uDFFC\u200D\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1|\uD83D\uDC69\uD83C\uDFFC\u200D\uD83E\uDD1D\u200D(?:\uD83D[\uDC68\uDC69]))(?:\uD83C[\uDFFB\uDFFD-\uDFFF])|(?:\uD83E\uDDD1\uD83C\uDFFB\u200D\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D)?\uD83E\uDDD1|\uD83D\uDC69\uD83C\uDFFB\u200D\uD83E\uDD1D\u200D(?:\uD83D[\uDC68\uDC69]))(?:\uD83C[\uDFFC-\uDFFF])|\uD83D\uDC68(?:\uD83C\uDFFB(?:\u200D(?:\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D\uD83D\uDC68(?:\uD83C[\uDFFB-\uDFFF])|\uD83D\uDC68(?:\uD83C[\uDFFB-\uDFFF]))|\uD83E\uDD1D\u200D\uD83D\uDC68(?:\uD83C[\uDFFC-\uDFFF])|[\u2695\u2696\u2708]\uFE0F|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD]))?|(?:\uD83C[\uDFFC-\uDFFF])\u200D\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D\uD83D\uDC68(?:\uD83C[\uDFFB-\uDFFF])|\uD83D\uDC68(?:\uD83C[\uDFFB-\uDFFF]))|\u200D(?:\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D)?\uD83D\uDC68|(?:\uD83D[\uDC68\uDC69])\u200D(?:\uD83D\uDC66\u200D\uD83D\uDC66|\uD83D\uDC67\u200D(?:\uD83D[\uDC66\uDC67]))|\uD83D\uDC66\u200D\uD83D\uDC66|\uD83D\uDC67\u200D(?:\uD83D[\uDC66\uDC67])|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFF\u200D(?:\uD83E\uDD1D\u200D\uD83D\uDC68(?:\uD83C[\uDFFB-\uDFFE])|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFE\u200D(?:\uD83E\uDD1D\u200D\uD83D\uDC68(?:\uD83C[\uDFFB-\uDFFD\uDFFF])|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFD\u200D(?:\uD83E\uDD1D\u200D\uD83D\uDC68(?:\uD83C[\uDFFB\uDFFC\uDFFE\uDFFF])|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFC\u200D(?:\uD83E\uDD1D\u200D\uD83D\uDC68(?:\uD83C[\uDFFB\uDFFD-\uDFFF])|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|(?:\uD83C\uDFFF\u200D[\u2695\u2696\u2708]|\uD83C\uDFFE\u200D[\u2695\u2696\u2708]|\uD83C\uDFFD\u200D[\u2695\u2696\u2708]|\uD83C\uDFFC\u200D[\u2695\u2696\u2708]|\u200D[\u2695\u2696\u2708])\uFE0F|\u200D(?:(?:\uD83D[\uDC68\uDC69])\u200D(?:\uD83D[\uDC66\uDC67])|\uD83D[\uDC66\uDC67])|\uD83C\uDFFF|\uD83C\uDFFE|\uD83C\uDFFD|\uD83C\uDFFC)?|(?:\uD83D\uDC69(?:\uD83C\uDFFB\u200D\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D(?:\uD83D[\uDC68\uDC69])|\uD83D[\uDC68\uDC69])|(?:\uD83C[\uDFFC-\uDFFF])\u200D\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D(?:\uD83D[\uDC68\uDC69])|\uD83D[\uDC68\uDC69]))|\uD83E\uDDD1(?:\uD83C[\uDFFB-\uDFFF])\u200D\uD83E\uDD1D\u200D\uD83E\uDDD1)(?:\uD83C[\uDFFB-\uDFFF])|\uD83D\uDC69\u200D\uD83D\uDC69\u200D(?:\uD83D\uDC66\u200D\uD83D\uDC66|\uD83D\uDC67\u200D(?:\uD83D[\uDC66\uDC67]))|\uD83D\uDC69(?:\u200D(?:\u2764\uFE0F\u200D(?:\uD83D\uDC8B\u200D(?:\uD83D[\uDC68\uDC69])|\uD83D[\uDC68\uDC69])|\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFF\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFE\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFD\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFC\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFB\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD]))|\uD83E\uDDD1(?:\u200D(?:\uD83E\uDD1D\u200D\uD83E\uDDD1|\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFF\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFE\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFD\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFC\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD])|\uD83C\uDFFB\u200D(?:\uD83C[\uDF3E\uDF73\uDF7C\uDF84\uDF93\uDFA4\uDFA8\uDFEB\uDFED]|\uD83D[\uDCBB\uDCBC\uDD27\uDD2C\uDE80\uDE92]|\uD83E[\uDDAF-\uDDB3\uDDBC\uDDBD]))|\uD83D\uDC69\u200D\uD83D\uDC66\u200D\uD83D\uDC66|\uD83D\uDC69\u200D\uD83D\uDC69\u200D(?:\uD83D[\uDC66\uDC67])|\uD83D\uDC69\u200D\uD83D\uDC67\u200D(?:\uD83D[\uDC66\uDC67])|(?:\uD83D\uDC41\uFE0F\u200D\uD83D\uDDE8|\uD83E\uDDD1(?:\uD83C\uDFFF\u200D[\u2695\u2696\u2708]|\uD83C\uDFFE\u200D[\u2695\u2696\u2708]|\uD83C\uDFFD\u200D[\u2695\u2696\u2708]|\uD83C\uDFFC\u200D[\u2695\u2696\u2708]|\uD83C\uDFFB\u200D[\u2695\u2696\u2708]|\u200D[\u2695\u2696\u2708])|\uD83D\uDC69(?:\uD83C\uDFFF\u200D[\u2695\u2696\u2708]|\uD83C\uDFFE\u200D[\u2695\u2696\u2708]|\uD83C\uDFFD\u200D[\u2695\u2696\u2708]|\uD83C\uDFFC\u200D[\u2695\u2696\u2708]|\uD83C\uDFFB\u200D[\u2695\u2696\u2708]|\u200D[\u2695\u2696\u2708])|\uD83D\uDE36\u200D\uD83C\uDF2B|\uD83C\uDFF3\uFE0F\u200D\u26A7|\uD83D\uDC3B\u200D\u2744|(?:(?:\uD83C[\uDFC3\uDFC4\uDFCA]|\uD83D[\uDC6E\uDC70\uDC71\uDC73\uDC77\uDC81\uDC82\uDC86\uDC87\uDE45-\uDE47\uDE4B\uDE4D\uDE4E\uDEA3\uDEB4-\uDEB6]|\uD83E[\uDD26\uDD35\uDD37-\uDD39\uDD3D\uDD3E\uDDB8\uDDB9\uDDCD-\uDDCF\uDDD4\uDDD6-\uDDDD])(?:\uD83C[\uDFFB-\uDFFF])|\uD83D\uDC6F|\uD83E[\uDD3C\uDDDE\uDDDF])\u200D[\u2640\u2642]|(?:\u26F9|\uD83C[\uDFCB\uDFCC]|\uD83D\uDD75)(?:\uFE0F|\uD83C[\uDFFB-\uDFFF])\u200D[\u2640\u2642]|\uD83C\uDFF4\u200D\u2620|(?:\uD83C[\uDFC3\uDFC4\uDFCA]|\uD83D[\uDC6E\uDC70\uDC71\uDC73\uDC77\uDC81\uDC82\uDC86\uDC87\uDE45-\uDE47\uDE4B\uDE4D\uDE4E\uDEA3\uDEB4-\uDEB6]|\uD83E[\uDD26\uDD35\uDD37-\uDD39\uDD3D\uDD3E\uDDB8\uDDB9\uDDCD-\uDDCF\uDDD4\uDDD6-\uDDDD])\u200D[\u2640\u2642]|[\xA9\xAE\u203C\u2049\u2122\u2139\u2194-\u2199\u21A9\u21AA\u2328\u23CF\u23ED-\u23EF\u23F1\u23F2\u23F8-\u23FA\u24C2\u25AA\u25AB\u25B6\u25C0\u25FB\u25FC\u2600-\u2604\u260E\u2611\u2618\u2620\u2622\u2623\u2626\u262A\u262E\u262F\u2638-\u263A\u2640\u2642\u265F\u2660\u2663\u2665\u2666\u2668\u267B\u267E\u2692\u2694-\u2697\u2699\u269B\u269C\u26A0\u26A7\u26B0\u26B1\u26C8\u26CF\u26D1\u26D3\u26E9\u26F0\u26F1\u26F4\u26F7\u26F8\u2702\u2708\u2709\u270F\u2712\u2714\u2716\u271D\u2721\u2733\u2734\u2744\u2747\u2763\u27A1\u2934\u2935\u2B05-\u2B07\u3030\u303D\u3297\u3299]|\uD83C[\uDD70\uDD71\uDD7E\uDD7F\uDE02\uDE37\uDF21\uDF24-\uDF2C\uDF36\uDF7D\uDF96\uDF97\uDF99-\uDF9B\uDF9E\uDF9F\uDFCD\uDFCE\uDFD4-\uDFDF\uDFF5\uDFF7]|\uD83D[\uDC3F\uDCFD\uDD49\uDD4A\uDD6F\uDD70\uDD73\uDD76-\uDD79\uDD87\uDD8A-\uDD8D\uDDA5\uDDA8\uDDB1\uDDB2\uDDBC\uDDC2-\uDDC4\uDDD1-\uDDD3\uDDDC-\uDDDE\uDDE1\uDDE3\uDDE8\uDDEF\uDDF3\uDDFA\uDECB\uDECD-\uDECF\uDEE0-\uDEE5\uDEE9\uDEF0\uDEF3])\uFE0F|\uD83C\uDFF3\uFE0F\u200D\uD83C\uDF08|\uD83D\uDC69\u200D\uD83D\uDC67|\uD83D\uDC69\u200D\uD83D\uDC66|\uD83D\uDE35\u200D\uD83D\uDCAB|\uD83D\uDE2E\u200D\uD83D\uDCA8|\uD83D\uDC15\u200D\uD83E\uDDBA|\uD83E\uDDD1(?:\uD83C\uDFFF|\uD83C\uDFFE|\uD83C\uDFFD|\uD83C\uDFFC|\uD83C\uDFFB)?|\uD83D\uDC69(?:\uD83C\uDFFF|\uD83C\uDFFE|\uD83C\uDFFD|\uD83C\uDFFC|\uD83C\uDFFB)?|\uD83C\uDDFD\uD83C\uDDF0|\uD83C\uDDF6\uD83C\uDDE6|\uD83C\uDDF4\uD83C\uDDF2|\uD83D\uDC08\u200D\u2B1B|\u2764\uFE0F\u200D(?:\uD83D\uDD25|\uD83E\uDE79)|\uD83D\uDC41\uFE0F|\uD83C\uDFF3\uFE0F|\uD83C\uDDFF(?:\uD83C[\uDDE6\uDDF2\uDDFC])|\uD83C\uDDFE(?:\uD83C[\uDDEA\uDDF9])|\uD83C\uDDFC(?:\uD83C[\uDDEB\uDDF8])|\uD83C\uDDFB(?:\uD83C[\uDDE6\uDDE8\uDDEA\uDDEC\uDDEE\uDDF3\uDDFA])|\uD83C\uDDFA(?:\uD83C[\uDDE6\uDDEC\uDDF2\uDDF3\uDDF8\uDDFE\uDDFF])|\uD83C\uDDF9(?:\uD83C[\uDDE6\uDDE8\uDDE9\uDDEB-\uDDED\uDDEF-\uDDF4\uDDF7\uDDF9\uDDFB\uDDFC\uDDFF])|\uD83C\uDDF8(?:\uD83C[\uDDE6-\uDDEA\uDDEC-\uDDF4\uDDF7-\uDDF9\uDDFB\uDDFD-\uDDFF])|\uD83C\uDDF7(?:\uD83C[\uDDEA\uDDF4\uDDF8\uDDFA\uDDFC])|\uD83C\uDDF5(?:\uD83C[\uDDE6\uDDEA-\uDDED\uDDF0-\uDDF3\uDDF7-\uDDF9\uDDFC\uDDFE])|\uD83C\uDDF3(?:\uD83C[\uDDE6\uDDE8\uDDEA-\uDDEC\uDDEE\uDDF1\uDDF4\uDDF5\uDDF7\uDDFA\uDDFF])|\uD83C\uDDF2(?:\uD83C[\uDDE6\uDDE8-\uDDED\uDDF0-\uDDFF])|\uD83C\uDDF1(?:\uD83C[\uDDE6-\uDDE8\uDDEE\uDDF0\uDDF7-\uDDFB\uDDFE])|\uD83C\uDDF0(?:\uD83C[\uDDEA\uDDEC-\uDDEE\uDDF2\uDDF3\uDDF5\uDDF7\uDDFC\uDDFE\uDDFF])|\uD83C\uDDEF(?:\uD83C[\uDDEA\uDDF2\uDDF4\uDDF5])|\uD83C\uDDEE(?:\uD83C[\uDDE8-\uDDEA\uDDF1-\uDDF4\uDDF6-\uDDF9])|\uD83C\uDDED(?:\uD83C[\uDDF0\uDDF2\uDDF3\uDDF7\uDDF9\uDDFA])|\uD83C\uDDEC(?:\uD83C[\uDDE6\uDDE7\uDDE9-\uDDEE\uDDF1-\uDDF3\uDDF5-\uDDFA\uDDFC\uDDFE])|\uD83C\uDDEB(?:\uD83C[\uDDEE-\uDDF0\uDDF2\uDDF4\uDDF7])|\uD83C\uDDEA(?:\uD83C[\uDDE6\uDDE8\uDDEA\uDDEC\uDDED\uDDF7-\uDDFA])|\uD83C\uDDE9(?:\uD83C[\uDDEA\uDDEC\uDDEF\uDDF0\uDDF2\uDDF4\uDDFF])|\uD83C\uDDE8(?:\uD83C[\uDDE6\uDDE8\uDDE9\uDDEB-\uDDEE\uDDF0-\uDDF5\uDDF7\uDDFA-\uDDFF])|\uD83C\uDDE7(?:\uD83C[\uDDE6\uDDE7\uDDE9-\uDDEF\uDDF1-\uDDF4\uDDF6-\uDDF9\uDDFB\uDDFC\uDDFE\uDDFF])|\uD83C\uDDE6(?:\uD83C[\uDDE8-\uDDEC\uDDEE\uDDF1\uDDF2\uDDF4\uDDF6-\uDDFA\uDDFC\uDDFD\uDDFF])|[#\*0-9]\uFE0F\u20E3|\u2764\uFE0F|(?:\uD83C[\uDFC3\uDFC4\uDFCA]|\uD83D[\uDC6E\uDC70\uDC71\uDC73\uDC77\uDC81\uDC82\uDC86\uDC87\uDE45-\uDE47\uDE4B\uDE4D\uDE4E\uDEA3\uDEB4-\uDEB6]|\uD83E[\uDD26\uDD35\uDD37-\uDD39\uDD3D\uDD3E\uDDB8\uDDB9\uDDCD-\uDDCF\uDDD4\uDDD6-\uDDDD])(?:\uD83C[\uDFFB-\uDFFF])|(?:\u26F9|\uD83C[\uDFCB\uDFCC]|\uD83D\uDD75)(?:\uFE0F|\uD83C[\uDFFB-\uDFFF])|\uD83C\uDFF4|(?:[\u270A\u270B]|\uD83C[\uDF85\uDFC2\uDFC7]|\uD83D[\uDC42\uDC43\uDC46-\uDC50\uDC66\uDC67\uDC6B-\uDC6D\uDC72\uDC74-\uDC76\uDC78\uDC7C\uDC83\uDC85\uDC8F\uDC91\uDCAA\uDD7A\uDD95\uDD96\uDE4C\uDE4F\uDEC0\uDECC]|\uD83E[\uDD0C\uDD0F\uDD18-\uDD1C\uDD1E\uDD1F\uDD30-\uDD34\uDD36\uDD77\uDDB5\uDDB6\uDDBB\uDDD2\uDDD3\uDDD5])(?:\uD83C[\uDFFB-\uDFFF])|(?:[\u261D\u270C\u270D]|\uD83D[\uDD74\uDD90])(?:\uFE0F|\uD83C[\uDFFB-\uDFFF])|[\u270A\u270B]|\uD83C[\uDF85\uDFC2\uDFC7]|\uD83D[\uDC08\uDC15\uDC3B\uDC42\uDC43\uDC46-\uDC50\uDC66\uDC67\uDC6B-\uDC6D\uDC72\uDC74-\uDC76\uDC78\uDC7C\uDC83\uDC85\uDC8F\uDC91\uDCAA\uDD7A\uDD95\uDD96\uDE2E\uDE35\uDE36\uDE4C\uDE4F\uDEC0\uDECC]|\uD83E[\uDD0C\uDD0F\uDD18-\uDD1C\uDD1E\uDD1F\uDD30-\uDD34\uDD36\uDD77\uDDB5\uDDB6\uDDBB\uDDD2\uDDD3\uDDD5]|\uD83C[\uDFC3\uDFC4\uDFCA]|\uD83D[\uDC6E\uDC70\uDC71\uDC73\uDC77\uDC81\uDC82\uDC86\uDC87\uDE45-\uDE47\uDE4B\uDE4D\uDE4E\uDEA3\uDEB4-\uDEB6]|\uD83E[\uDD26\uDD35\uDD37-\uDD39\uDD3D\uDD3E\uDDB8\uDDB9\uDDCD-\uDDCF\uDDD4\uDDD6-\uDDDD]|\uD83D\uDC6F|\uD83E[\uDD3C\uDDDE\uDDDF]|[\u231A\u231B\u23E9-\u23EC\u23F0\u23F3\u25FD\u25FE\u2614\u2615\u2648-\u2653\u267F\u2693\u26A1\u26AA\u26AB\u26BD\u26BE\u26C4\u26C5\u26CE\u26D4\u26EA\u26F2\u26F3\u26F5\u26FA\u26FD\u2705\u2728\u274C\u274E\u2753-\u2755\u2757\u2795-\u2797\u27B0\u27BF\u2B1B\u2B1C\u2B50\u2B55]|\uD83C[\uDC04\uDCCF\uDD8E\uDD91-\uDD9A\uDE01\uDE1A\uDE2F\uDE32-\uDE36\uDE38-\uDE3A\uDE50\uDE51\uDF00-\uDF20\uDF2D-\uDF35\uDF37-\uDF7C\uDF7E-\uDF84\uDF86-\uDF93\uDFA0-\uDFC1\uDFC5\uDFC6\uDFC8\uDFC9\uDFCF-\uDFD3\uDFE0-\uDFF0\uDFF8-\uDFFF]|\uD83D[\uDC00-\uDC07\uDC09-\uDC14\uDC16-\uDC3A\uDC3C-\uDC3E\uDC40\uDC44\uDC45\uDC51-\uDC65\uDC6A\uDC79-\uDC7B\uDC7D-\uDC80\uDC84\uDC88-\uDC8E\uDC90\uDC92-\uDCA9\uDCAB-\uDCFC\uDCFF-\uDD3D\uDD4B-\uDD4E\uDD50-\uDD67\uDDA4\uDDFB-\uDE2D\uDE2F-\uDE34\uDE37-\uDE44\uDE48-\uDE4A\uDE80-\uDEA2\uDEA4-\uDEB3\uDEB7-\uDEBF\uDEC1-\uDEC5\uDED0-\uDED2\uDED5-\uDED7\uDEEB\uDEEC\uDEF4-\uDEFC\uDFE0-\uDFEB]|\uD83E[\uDD0D\uDD0E\uDD10-\uDD17\uDD1D\uDD20-\uDD25\uDD27-\uDD2F\uDD3A\uDD3F-\uDD45\uDD47-\uDD76\uDD78\uDD7A-\uDDB4\uDDB7\uDDBA\uDDBC-\uDDCB\uDDD0\uDDE0-\uDDFF\uDE70-\uDE74\uDE78-\uDE7A\uDE80-\uDE86\uDE90-\uDEA8\uDEB0-\uDEB6\uDEC0-\uDEC2\uDED0-\uDED6]|(?:[\u231A\u231B\u23E9-\u23EC\u23F0\u23F3\u25FD\u25FE\u2614\u2615\u2648-\u2653\u267F\u2693\u26A1\u26AA\u26AB\u26BD\u26BE\u26C4\u26C5\u26CE\u26D4\u26EA\u26F2\u26F3\u26F5\u26FA\u26FD\u2705\u270A\u270B\u2728\u274C\u274E\u2753-\u2755\u2757\u2795-\u2797\u27B0\u27BF\u2B1B\u2B1C\u2B50\u2B55]|\uD83C[\uDC04\uDCCF\uDD8E\uDD91-\uDD9A\uDDE6-\uDDFF\uDE01\uDE1A\uDE2F\uDE32-\uDE36\uDE38-\uDE3A\uDE50\uDE51\uDF00-\uDF20\uDF2D-\uDF35\uDF37-\uDF7C\uDF7E-\uDF93\uDFA0-\uDFCA\uDFCF-\uDFD3\uDFE0-\uDFF0\uDFF4\uDFF8-\uDFFF]|\uD83D[\uDC00-\uDC3E\uDC40\uDC42-\uDCFC\uDCFF-\uDD3D\uDD4B-\uDD4E\uDD50-\uDD67\uDD7A\uDD95\uDD96\uDDA4\uDDFB-\uDE4F\uDE80-\uDEC5\uDECC\uDED0-\uDED2\uDED5-\uDED7\uDEEB\uDEEC\uDEF4-\uDEFC\uDFE0-\uDFEB]|\uD83E[\uDD0C-\uDD3A\uDD3C-\uDD45\uDD47-\uDD78\uDD7A-\uDDCB\uDDCD-\uDDFF\uDE70-\uDE74\uDE78-\uDE7A\uDE80-\uDE86\uDE90-\uDEA8\uDEB0-\uDEB6\uDEC0-\uDEC2\uDED0-\uDED6])|(?:[#\*0-9\xA9\xAE\u203C\u2049\u2122\u2139\u2194-\u2199\u21A9\u21AA\u231A\u231B\u2328\u23CF\u23E9-\u23F3\u23F8-\u23FA\u24C2\u25AA\u25AB\u25B6\u25C0\u25FB-\u25FE\u2600-\u2604\u260E\u2611\u2614\u2615\u2618\u261D\u2620\u2622\u2623\u2626\u262A\u262E\u262F\u2638-\u263A\u2640\u2642\u2648-\u2653\u265F\u2660\u2663\u2665\u2666\u2668\u267B\u267E\u267F\u2692-\u2697\u2699\u269B\u269C\u26A0\u26A1\u26A7\u26AA\u26AB\u26B0\u26B1\u26BD\u26BE\u26C4\u26C5\u26C8\u26CE\u26CF\u26D1\u26D3\u26D4\u26E9\u26EA\u26F0-\u26F5\u26F7-\u26FA\u26FD\u2702\u2705\u2708-\u270D\u270F\u2712\u2714\u2716\u271D\u2721\u2728\u2733\u2734\u2744\u2747\u274C\u274E\u2753-\u2755\u2757\u2763\u2764\u2795-\u2797\u27A1\u27B0\u27BF\u2934\u2935\u2B05-\u2B07\u2B1B\u2B1C\u2B50\u2B55\u3030\u303D\u3297\u3299]|\uD83C[\uDC04\uDCCF\uDD70\uDD71\uDD7E\uDD7F\uDD8E\uDD91-\uDD9A\uDDE6-\uDDFF\uDE01\uDE02\uDE1A\uDE2F\uDE32-\uDE3A\uDE50\uDE51\uDF00-\uDF21\uDF24-\uDF93\uDF96\uDF97\uDF99-\uDF9B\uDF9E-\uDFF0\uDFF3-\uDFF5\uDFF7-\uDFFF]|\uD83D[\uDC00-\uDCFD\uDCFF-\uDD3D\uDD49-\uDD4E\uDD50-\uDD67\uDD6F\uDD70\uDD73-\uDD7A\uDD87\uDD8A-\uDD8D\uDD90\uDD95\uDD96\uDDA4\uDDA5\uDDA8\uDDB1\uDDB2\uDDBC\uDDC2-\uDDC4\uDDD1-\uDDD3\uDDDC-\uDDDE\uDDE1\uDDE3\uDDE8\uDDEF\uDDF3\uDDFA-\uDE4F\uDE80-\uDEC5\uDECB-\uDED2\uDED5-\uDED7\uDEE0-\uDEE5\uDEE9\uDEEB\uDEEC\uDEF0\uDEF3-\uDEFC\uDFE0-\uDFEB]|\uD83E[\uDD0C-\uDD3A\uDD3C-\uDD45\uDD47-\uDD78\uDD7A-\uDDCB\uDDCD-\uDDFF\uDE70-\uDE74\uDE78-\uDE7A\uDE80-\uDE86\uDE90-\uDEA8\uDEB0-\uDEB6\uDEC0-\uDEC2\uDED0-\uDED6])\uFE0F|(?:[\u261D\u26F9\u270A-\u270D]|\uD83C[\uDF85\uDFC2-\uDFC4\uDFC7\uDFCA-\uDFCC]|\uD83D[\uDC42\uDC43\uDC46-\uDC50\uDC66-\uDC78\uDC7C\uDC81-\uDC83\uDC85-\uDC87\uDC8F\uDC91\uDCAA\uDD74\uDD75\uDD7A\uDD90\uDD95\uDD96\uDE45-\uDE47\uDE4B-\uDE4F\uDEA3\uDEB4-\uDEB6\uDEC0\uDECC]|\uD83E[\uDD0C\uDD0F\uDD18-\uDD1F\uDD26\uDD30-\uDD39\uDD3C-\uDD3E\uDD77\uDDB5\uDDB6\uDDB8\uDDB9\uDDBB\uDDCD-\uDDCF\uDDD1-\uDDDD])/g;
};

function stringWidth(string) {
	if (typeof string !== 'string' || string.length === 0) {
		return 0;
	}

	string = stripAnsi(string);

	if (string.length === 0) {
		return 0;
	}

	string = string.replace(emojiRegex(), '  ');

	let width = 0;

	for (let index = 0; index < string.length; index++) {
		const codePoint = string.codePointAt(index);

		// Ignore control characters
		if (codePoint <= 0x1F || (codePoint >= 0x7F && codePoint <= 0x9F)) {
			continue;
		}

		// Ignore combining characters
		if (codePoint >= 0x300 && codePoint <= 0x36F) {
			continue;
		}

		// Surrogates
		if (codePoint > 0xFFFF) {
			index++;
		}

		width += isFullwidthCodePoint(codePoint) ? 2 : 1;
	}

	return width;
}

/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('vfile-message').VFileMessage} VFileMessage
 *
 * @typedef Statistics
 * @property {number} fatal Fatal errors (`fatal: true`)
 * @property {number} warn warning errors (`fatal: false`)
 * @property {number} info informational messages (`fatal: null|undefined`)
 * @property {number} nonfatal warning + info
 * @property {number} total nonfatal + fatal
 */

/**
 * Get stats for a file, list of files, or list of messages.
 *
 * @param {Array.<VFile|VFileMessage>|VFile|VFileMessage} [value]
 * @returns {Statistics}
 */
function statistics(value) {
  var result = {true: 0, false: 0, null: 0};

  if (value) {
    if (Array.isArray(value)) {
      list(value);
    } else {
      one(value);
    }
  }

  return {
    fatal: result.true,
    nonfatal: result.false + result.null,
    warn: result.false,
    info: result.null,
    total: result.true + result.false + result.null
  }

  /**
   * @param {Array.<VFile|VFileMessage>} value
   * @returns {void}
   */
  function list(value) {
    var index = -1;

    while (++index < value.length) {
      one(value[index]);
    }
  }

  /**
   * @param {VFile|VFileMessage} value
   * @returns {void}
   */
  function one(value) {
    if ('messages' in value) return list(value.messages)

    result[
      value.fatal === undefined || value.fatal === null
        ? null
        : Boolean(value.fatal)
    ]++;
  }
}

/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('vfile-message').VFileMessage} VFileMessage
 */

var severities = {true: 2, false: 1, null: 0, undefined: 0};

/**
 * @template {VFile} F
 * @param {F} file
 * @returns {F}
 */
function sort(file) {
  file.messages.sort(comparator);
  return file
}

/**
 * @param {VFileMessage} a
 * @param {VFileMessage} b
 * @returns {number}
 */
function comparator(a, b) {
  return (
    check(a, b, 'line') ||
    check(a, b, 'column') ||
    severities[b.fatal] - severities[a.fatal] ||
    compare(a, b, 'source') ||
    compare(a, b, 'ruleId') ||
    compare(a, b, 'reason') ||
    0
  )
}

/**
 * @param {VFileMessage} a
 * @param {VFileMessage} b
 * @param {string} property
 * @returns {number}
 */
function check(a, b, property) {
  return (a[property] || 0) - (b[property] || 0)
}

/**
 * @param {VFileMessage} a
 * @param {VFileMessage} b
 * @param {string} property
 * @returns {number}
 */
function compare(a, b, property) {
  return String(a[property] || '').localeCompare(b[property] || '')
}

/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('vfile-message').VFileMessage} VFileMessage
 * @typedef {import('vfile-statistics').Statistics} Statistics
 *
 * @typedef Options
 * @property {boolean} [color]
 * @property {boolean} [silent=false]
 * @property {boolean} [quiet=false]
 * @property {boolean} [verbose=false]
 * @property {string} [defaultName='<stdin>']
 *
 * @typedef _Row
 * @property {string} place
 * @property {string} label
 * @property {string} reason
 * @property {string} ruleId
 * @property {string} source
 *
 * @typedef _FileRow
 * @property {'file'} type
 * @property {VFile} file
 * @property {Statistics} stats
 *
 * @typedef {{[x: string]: number}} _Sizes
 *
 * @typedef _Info
 * @property {Array.<_FileRow|_Row>} rows
 * @property {Statistics} stats
 * @property {_Sizes} sizes
 */

const own = {}.hasOwnProperty;

// @ts-expect-error Types are incorrect.
const supported = supportsColor.stderr.hasBasic;

// `log-symbols` without chalk, ignored for Windows:
/* c8 ignore next 4 */
const chars =
  process.platform === 'win32'
    ? {error: '×', warning: '‼'}
    : {error: '✖', warning: '⚠'};

const labels = {
  true: 'error',
  false: 'warning',
  null: 'info',
  undefined: 'info'
};

/**
 * Report a file’s messages.
 *
 * @param {Error|VFile|Array.<VFile>} [files]
 * @param {Options} [options]
 * @returns {string}
 */
function reporter(files, options = {}) {
  /** @type {boolean|undefined} */
  let one;

  if (!files) {
    return ''
  }

  // Error.
  if ('name' in files && 'message' in files) {
    return String(files.stack || files)
  }

  // One file.
  if (!Array.isArray(files)) {
    one = true;
    files = [files];
  }

  return format$1(transform(files, options), one, options)
}

/**
 * @param {Array.<VFile>} files
 * @param {Options} options
 * @returns {_Info}
 */
function transform(files, options) {
  /** @type {Array.<_FileRow|_Row>} */
  const rows = [];
  /** @type {Array.<VFileMessage>} */
  const all = [];
  /** @type {_Sizes} */
  const sizes = {};
  let index = -1;

  while (++index < files.length) {
    // @ts-expect-error it works fine.
    const messages = sort({messages: [...files[index].messages]}).messages;
    /** @type {Array.<_Row>} */
    const messageRows = [];
    let offset = -1;

    while (++offset < messages.length) {
      const message = messages[offset];

      if (!options.silent || message.fatal) {
        all.push(message);

        const row = {
          place: stringifyPosition(
            message.position
              ? message.position.end.line && message.position.end.column
                ? message.position
                : message.position.start
              : undefined
          ),
          label: labels[/** @type {keyof labels} */ (String(message.fatal))],
          reason:
            (message.stack || message.message) +
            (options.verbose && message.note ? '\n' + message.note : ''),
          ruleId: message.ruleId || '',
          source: message.source || ''
        };

        /** @type {keyof row} */
        let key;

        for (key in row) {
          // eslint-disable-next-line max-depth
          if (own.call(row, key)) {
            sizes[key] = Math.max(size(row[key]), sizes[key] || 0);
          }
        }

        messageRows.push(row);
      }
    }

    if ((!options.quiet && !options.silent) || messageRows.length > 0) {
      rows.push(
        {type: 'file', file: files[index], stats: statistics(messages)},
        ...messageRows
      );
    }
  }

  return {rows, stats: statistics(all), sizes}
}

/**
 * @param {_Info} map
 * @param {boolean|undefined} one
 * @param {Options} options
 */
// eslint-disable-next-line complexity
function format$1(map, one, options) {
  /** @type {boolean} */
  const enabled =
    options.color === undefined || options.color === null
      ? supported
      : options.color;
  /** @type {Array.<string>} */
  const lines = [];
  let index = -1;

  while (++index < map.rows.length) {
    const row = map.rows[index];

    if ('type' in row) {
      const stats = row.stats;
      let line = row.file.history[0] || options.defaultName || '<stdin>';

      line =
        one && !options.defaultName && !row.file.history[0]
          ? ''
          : (enabled
              ? '\u001B[4m' /* Underline. */ +
                (stats.fatal
                  ? '\u001B[31m' /* Red. */
                  : stats.total
                  ? '\u001B[33m' /* Yellow. */
                  : '\u001B[32m') /* Green. */ +
                line +
                '\u001B[39m\u001B[24m'
              : line) +
            (row.file.stored && row.file.path !== row.file.history[0]
              ? ' > ' + row.file.path
              : '');

      if (!stats.total) {
        line =
          (line ? line + ': ' : '') +
          (row.file.stored
            ? enabled
              ? '\u001B[33mwritten\u001B[39m' /* Yellow. */
              : 'written'
            : 'no issues found');
      }

      if (line) {
        if (index && !('type' in map.rows[index - 1])) {
          lines.push('');
        }

        lines.push(line);
      }
    } else {
      let reason = row.reason;
      const match = /\r?\n|\r/.exec(reason);
      /** @type {string} */
      let rest;

      if (match) {
        rest = reason.slice(match.index);
        reason = reason.slice(0, match.index);
      } else {
        rest = '';
      }

      lines.push(
        (
          '  ' +
          ' '.repeat(map.sizes.place - size(row.place)) +
          row.place +
          '  ' +
          (enabled
            ? (row.label === 'error'
                ? '\u001B[31m' /* Red. */
                : '\u001B[33m') /* Yellow. */ +
              row.label +
              '\u001B[39m'
            : row.label) +
          ' '.repeat(map.sizes.label - size(row.label)) +
          '  ' +
          reason +
          ' '.repeat(map.sizes.reason - size(reason)) +
          '  ' +
          row.ruleId +
          ' '.repeat(map.sizes.ruleId - size(row.ruleId)) +
          '  ' +
          (row.source || '')
        ).replace(/ +$/, '') + rest
      );
    }
  }

  const stats = map.stats;

  if (stats.fatal || stats.warn) {
    let line = '';

    if (stats.fatal) {
      line =
        (enabled
          ? '\u001B[31m' /* Red. */ + chars.error + '\u001B[39m'
          : chars.error) +
        ' ' +
        stats.fatal +
        ' ' +
        (labels.true + (stats.fatal === 1 ? '' : 's'));
    }

    if (stats.warn) {
      line =
        (line ? line + ', ' : '') +
        (enabled
          ? '\u001B[33m' /* Yellow. */ + chars.warning + '\u001B[39m'
          : chars.warning) +
        ' ' +
        stats.warn +
        ' ' +
        (labels.false + (stats.warn === 1 ? '' : 's'));
    }

    if (stats.total !== stats.fatal && stats.total !== stats.warn) {
      line = stats.total + ' messages (' + line + ')';
    }

    lines.push('', line);
  }

  return lines.join('\n')
}

/**
 * Get the length of `value`, ignoring ANSI sequences.
 *
 * @param {string} value
 * @returns {number}
 */
function size(value) {
  const match = /\r?\n|\r/.exec(value);
  return stringWidth(match ? value.slice(0, match.index) : value)
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
  // We need to calculate `fileContents` before running `linter.process(files)`
  // because `linter.process(files)` mutates `file` and returns it as `result`.
  // So we won't be able to use `file` after that to see if its contents have
  // changed as they will have been altered to the changed version.
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
