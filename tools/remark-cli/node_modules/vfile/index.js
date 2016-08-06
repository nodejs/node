/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module vfile
 * @fileoverview Virtual file format to attach additional
 *   information related to processed input.  Similar to
 *   `wearefractal/vinyl`.
 */

'use strict';

/* Dependencies. */
var path = require('path');
var has = require('has');
var replace = require('replace-ext');
var stringify = require('unist-util-stringify-position');
var buffer = require('is-buffer');
var string = require('x-is-string');

/* Expose. */
module.exports = VFile;

/* Methods. */
var proto = VFile.prototype;

proto.toString = toString;
proto.message = message;
proto.fail = fail;

/* Slight backwards compatibility.  Remove in the future. */
proto.warn = message;

/* Order of setting (least specific to most). */
var order = [
  'history',
  'path',
  'basename',
  'stem',
  'extname',
  'dirname'
];

/**
 * Construct a new file.
 *
 * @constructor
 * @param {Object|VFile|string} [options] - File, contents, or config.
 */
function VFile(options) {
  var prop;
  var index;
  var length;

  if (!options) {
    options = {};
  } else if (string(options) || buffer(options)) {
    options = {contents: options};
  } else if ('message' in options && 'messages' in options) {
    return options;
  }

  if (!(this instanceof VFile)) {
    return new VFile(options);
  }

  this.data = {};
  this.messages = [];
  this.history = [];
  this.cwd = process.cwd();

  /* Set path related properties in the correct order. */
  index = -1;
  length = order.length;

  while (++index < length) {
    prop = order[index];

    if (has(options, prop)) {
      this[prop] = options[prop];
    }
  }

  /* Set non-path related properties. */
  for (prop in options) {
    if (order.indexOf(prop) === -1) {
      this[prop] = options[prop];
    }
  }
}

/**
 * Access complete path (`~/index.min.js`).
 */
Object.defineProperty(proto, 'path', {
  get: function () {
    return this.history[this.history.length - 1];
  },
  set: function (path) {
    assertNonEmpty(path, 'path');

    if (path !== this.path) {
      this.history.push(path);
    }
  }
});

/**
 * Access parent path (`~`).
 */
Object.defineProperty(proto, 'dirname', {
  get: function () {
    return string(this.path) ? path.dirname(this.path) : undefined;
  },
  set: function (dirname) {
    assertPath(this.path, 'dirname');
    this.path = path.join(dirname || '', this.basename);
  }
});

/**
 * Access basename (`index.min.js`).
 */
Object.defineProperty(proto, 'basename', {
  get: function () {
    return string(this.path) ? path.basename(this.path) : undefined;
  },
  set: function (basename) {
    assertNonEmpty(basename, 'basename');
    assertPart(basename, 'basename');
    this.path = path.join(this.dirname || '', basename);
  }
});

/**
 * Access extname (`.js`).
 */
Object.defineProperty(proto, 'extname', {
  get: function () {
    return string(this.path) ? path.extname(this.path) : undefined;
  },
  set: function (extname) {
    var ext = extname || '';

    assertPart(ext, 'extname');
    assertPath(this.path, 'extname');

    if (ext) {
      if (ext.charAt(0) !== '.') {
        throw new Error('`extname` must start with `.`');
      }

      if (ext.indexOf('.', 1) !== -1) {
        throw new Error('`extname` cannot contain multiple dots');
      }
    }

    this.path = replace(this.path, ext);
  }
});

/**
 * Access stem (`index.min`).
 */
Object.defineProperty(proto, 'stem', {
  get: function () {
    return string(this.path) ? path.basename(this.path, this.extname) : undefined;
  },
  set: function (stem) {
    assertNonEmpty(stem, 'stem');
    assertPart(stem, 'stem');
    this.path = path.join(this.dirname || '', stem + (this.extname || ''));
  }
});

/**
 * Get the value of the file.
 *
 * @return {string} - Contents.
 */
function toString(encoding) {
  var value = this.contents || '';
  return buffer(value) ? value.toString(encoding) : String(value);
}

/**
 * Create a message with `reason` at `position`.
 * When an error is passed in as `reason`, copies the
 * stack.  This does not add a message to `messages`.
 *
 * @param {string|Error} reason - Reason for message.
 * @param {Node|Location|Position} [position] - Place of message.
 * @param {string} [ruleId] - Category of message.
 * @return {VMessage} - Message.
 */
function message(reason, position, ruleId) {
  var filePath = this.path;
  var range = stringify(position) || '1:1';
  var location;
  var err;

  location = {
    start: {line: null, column: null},
    end: {line: null, column: null}
  };

  if (position && position.position) {
    position = position.position;
  }

  if (position) {
    /* Location. */
    if (position.start) {
      location = position;
      position = position.start;
    } else {
      /* Position. */
      location.start = position;
      location.end.line = null;
      location.end.column = null;
    }
  }

  err = new VMessage(reason.message || reason);

  err.name = (filePath ? filePath + ':' : '') + range;
  err.file = filePath || '';
  err.reason = reason.message || reason;
  err.line = position ? position.line : null;
  err.column = position ? position.column : null;
  err.location = location;
  err.ruleId = ruleId || null;
  err.source = null;
  err.fatal = false;

  if (reason.stack) {
    err.stack = reason.stack;
  }

  this.messages.push(err);

  return err;
}

/**
 * Fail. Creates a vmessage, associates it with the file,
 * and throws it.
 *
 * @throws {VMessage} - Fatal exception.
 */
function fail() {
  var message = this.message.apply(this, arguments);

  message.fatal = true;

  throw message;
}

/* Inherit from `Error#`. */
function VMessagePrototype() {}
VMessagePrototype.prototype = Error.prototype;
VMessage.prototype = new VMessagePrototype();

/* Message properties. */
proto = VMessage.prototype;

proto.file = proto.name = proto.reason = proto.message = proto.stack = '';
proto.fatal = proto.column = proto.line = null;

/**
 * Construct a new file message.
 *
 * Note: We cannot invoke `Error` on the created context,
 * as that adds readonly `line` and `column` attributes on
 * Safari 9, thus throwing and failing the data.
 *
 * @constructor
 * @param {string} reason - Reason for messaging.
 */
function VMessage(reason) {
  this.message = reason;
}

/* Assert that `part` is not a path (i.e., does
 * not contain `path.sep`). */
function assertPart(part, name) {
  if (part.indexOf(path.sep) !== -1) {
    throw new Error(
      '`' + name + '` cannot be a path: did not expect `' + path.sep + '`'
    );
  }
}

/* Assert that `part` is not empty. */
function assertNonEmpty(part, name) {
  if (!part) {
    throw new Error('`' + name + '` cannot be empty');
  }
}

/* Assert `path` exists. */
function assertPath(path, name) {
  if (!path) {
    throw new Error(
      'Setting `' + name + '` requires `path` to be set too'
    );
  }
}
