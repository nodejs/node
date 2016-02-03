/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module vfile
 * @fileoverview Virtual file format to attach additional
 *   information related to processed input.  Similar to
 *   `wearefractal/vinyl`.  Additionally, `VFile` can be
 *   passed directly to ESLint formatters to visualise
 *   warnings and errors relating to a file.
 * @example
 *   var VFile = require('vfile');
 *
 *   var file = new VFile({
 *     'directory': '~',
 *     'filename': 'example',
 *     'extension': 'txt',
 *     'contents': 'Foo *bar* baz'
 *   });
 *
 *   file.toString(); // 'Foo *bar* baz'
 *   file.filePath(); // '~/example.txt'
 *
 *   file.move({'extension': 'md'});
 *   file.filePath(); // '~/example.md'
 *
 *   file.warn('Something went wrong', {'line': 2, 'column': 3});
 *   // { [~/example.md:2:3: Something went wrong]
 *   //   name: '~/example.md:2:3',
 *   //   file: '~/example.md',
 *   //   reason: 'Something went wrong',
 *   //   line: 2,
 *   //   column: 3,
 *   //   fatal: false }
 */

'use strict';

/* eslint-env commonjs */

var SEPARATOR = '/';

try {
    SEPARATOR = require('pa' + 'th').sep;
} catch (e) { /* empty */ }

/**
 * Construct a new file message.
 *
 * Note: We cannot invoke `Error` on the created context,
 * as that adds readonly `line` and `column` attributes on
 * Safari 9, thus throwing and failing the data.
 *
 * @example
 *   var message = new VFileMessage('Whoops!');
 *
 *   message instanceof Error // true
 *
 * @constructor
 * @class {VFileMessage}
 * @param {string} reason - Reason for messaging.
 * @property {boolean} [fatal=null] - Whether the message
 *   is fatal.
 * @property {string} [name=''] - File-name and positional
 *   information.
 * @property {string} [file=''] - File-path.
 * @property {string} [reason=''] - Reason for messaging.
 * @property {number} [line=null] - Start of message.
 * @property {number} [column=null] - Start of message.
 * @property {Position|Location} [location=null] - Place of
 *   message.
 * @property {string} [stack] - Stack-trace of warning.
 */
function VFileMessage(reason) {
    this.message = reason;
}

/**
 * Inherit from `Error#`.
 */
function VFileMessagePrototype() {}

VFileMessagePrototype.prototype = Error.prototype;

var proto = new VFileMessagePrototype();

VFileMessage.prototype = proto;

/*
 * Expose defaults.
 */

proto.file = proto.name = proto.reason = proto.message = proto.stack = '';
proto.fatal = proto.column = proto.line = null;

/**
 * File-related message with location information.
 *
 * @typedef {Error} VFileMessage
 * @property {string} name - (Starting) location of the
 *   message, preceded by its file-path when available,
 *   and joined by `:`. Used internally by the native
 *   `Error#toString()`.
 * @property {string} file - File-path.
 * @property {string} reason - Reason for message.
 * @property {number?} line - Line of message, when
 *   available.
 * @property {number?} column - Column of message, when
 *   available.
 * @property {string?} stack - Stack of message, when
 *   available.
 * @property {boolean?} fatal - Whether the associated file
 *   is still processable.
 */

/**
 * Stringify a position.
 *
 * @example
 *   stringify({'line': 1, 'column': 3}) // '1:3'
 *   stringify({'line': 1}) // '1:1'
 *   stringify({'column': 3}) // '1:3'
 *   stringify() // '1:1'
 *
 * @private
 * @param {Object?} [position] - Single position, like
 *   those available at `node.position.start`.
 * @return {string} - Compiled location.
 */
function stringify(position) {
    if (!position) {
        position = {};
    }

    return (position.line || 1) + ':' + (position.column || 1);
}

/**
 * ESLint's formatter API expects `filePath` to be a
 * string.  This hack supports invocation as well as
 * implicit coercion.
 *
 * @example
 *   var file = new VFile({
 *     'filename': 'example',
 *     'extension': 'txt'
 *   });
 *
 *   filePath = filePathFactory(file);
 *
 *   String(filePath); // 'example.txt'
 *   filePath(); // 'example.txt'
 *
 * @private
 * @param {VFile} file - Virtual file.
 * @return {Function} - `filePath` getter.
 */
function filePathFactory(file) {
    /**
     * Get the filename, with extension and directory, if applicable.
     *
     * @example
     *   var file = new VFile({
     *     'directory': '~',
     *     'filename': 'example',
     *     'extension': 'txt'
     *   });
     *
     *   String(file.filePath); // ~/example.txt
     *   file.filePath() // ~/example.txt
     *
     * @memberof {VFile}
     * @property {Function} toString - Itself. ESLint's
     *   formatter API expects `filePath` to be `string`.
     *   This hack supports invocation as well as implicit
     *   coercion.
     * @return {string} - If the `vFile` has a `filename`,
     *   it will be prefixed with the directory (slashed),
     *   if applicable, and suffixed with the (dotted)
     *   extension (if applicable).  Otherwise, an empty
     *   string is returned.
     */
    function filePath() {
        var directory = file.directory;
        var separator;

        if (file.filename || file.extension) {
            separator = directory.charAt(directory.length - 1);

            if (separator === '/' || separator === '\\') {
                directory = directory.slice(0, -1);
            }

            if (directory === '.') {
                directory = '';
            }

            return (directory ? directory + SEPARATOR : '') +
                file.filename +
                (file.extension ? '.' + file.extension : '');
        }

        return '';
    }

    filePath.toString = filePath;

    return filePath;
}

/**
* Get the filename with extantion.
*
* @example
*   var file = new VFile({
*     'directory': '~/foo/bar'
*     'filename': 'example',
*     'extension': 'txt'
*   });
*
*   file.basename() // example.txt
*
* @memberof {VFile}
* @return {string} - name of file with extantion.
*/
function basename() {
    var self = this;
    var extension = self.extension;

    if (self.filename || extension) {
        return self.filename + (extension ? '.' + extension : '');
    }

    return '';
}

/**
 * Construct a new file.
 *
 * @example
 *   var file = new VFile({
 *     'directory': '~',
 *     'filename': 'example',
 *     'extension': 'txt',
 *     'contents': 'Foo *bar* baz'
 *   });
 *
 *   file === VFile(file) // true
 *   file === new VFile(file) // true
 *   VFile('foo') instanceof VFile // true
 *
 * @constructor
 * @class {VFile}
 * @param {Object|VFile|string} [options] - either an
 *   options object, or the value of `contents` (both
 *   optional).  When a `file` is passed in, it's
 *   immediately returned.
 * @property {string} [contents=''] - Content of file.
 * @property {string} [directory=''] - Path to parent
 *   directory.
 * @property {string} [filename=''] - Filename.
 *   A file-path can still be generated when no filename
 *   exists.
 * @property {string} [extension=''] - Extension.
 *   A file-path can still be generated when no extension
 *   exists.
 * @property {boolean?} quiet - Whether an error created by
 *   `VFile#fail()` is returned (when truthy) or thrown
 *   (when falsey). Ensure all `messages` associated with
 *   a file are handled properly when setting this to
 *   `true`.
 * @property {Array.<VFileMessage>} messages - List of associated
 *   messages.
 */
function VFile(options) {
    var self = this;

    /*
     * No `new` operator.
     */

    if (!(self instanceof VFile)) {
        return new VFile(options);
    }

    /*
     * Given file.
     */

    if (
        options &&
        typeof options.message === 'function' &&
        typeof options.hasFailed === 'function'
    ) {
        return options;
    }

    if (!options) {
        options = {};
    } else if (typeof options === 'string') {
        options = {
            'contents': options
        };
    }

    self.contents = options.contents || '';

    self.messages = [];

    /*
     * Make sure eslint’s formatters stringify `filePath`
     * properly.
     */

    self.filePath = filePathFactory(self);

    self.history = [];

    self.move({
        'filename': options.filename,
        'directory': options.directory,
        'extension': options.extension
    });
}

/**
 * Get the value of the file.
 *
 * @example
 *   var vFile = new VFile('Foo');
 *   String(vFile); // 'Foo'
 *
 * @this {VFile}
 * @memberof {VFile}
 * @return {string} - value at the `contents` property
 *   in context.
 */
function toString() {
    return this.contents;
}

/**
 * Move a file by passing a new directory, filename,
 * and extension.  When these are not given, the default
 * values are kept.
 *
 * @example
 *   var file = new VFile({
 *     'directory': '~',
 *     'filename': 'example',
 *     'extension': 'txt',
 *     'contents': 'Foo *bar* baz'
 *   });
 *
 *   file.move({'directory': '/var/www'});
 *   file.filePath(); // '/var/www/example.txt'
 *
 *   file.move({'extension': 'md'});
 *   file.filePath(); // '/var/www/example.md'
 *
 * @this {VFile}
 * @memberof {VFile}
 * @param {Object?} [options] - Configuration.
 * @return {VFile} - Context object.
 */
function move(options) {
    var self = this;
    var before = self.filePath();
    var after;

    if (!options) {
        options = {};
    }

    self.directory = options.directory || self.directory || '';
    self.filename = options.filename || self.filename || '';
    self.extension = options.extension || self.extension || '';

    after = self.filePath();

    if (after && before !== after) {
        self.history.push(after);
    }

    return self;
}

/**
 * Create a message with `reason` at `position`.
 * When an error is passed in as `reason`, copies the
 * stack.  This does not add a message to `messages`.
 *
 * @example
 *   var file = new VFile();
 *
 *   file.message('Something went wrong');
 *   // { [1:1: Something went wrong]
 *   //   name: '1:1',
 *   //   file: '',
 *   //   reason: 'Something went wrong',
 *   //   line: null,
 *   //   column: null }
 *
 * @this {VFile}
 * @memberof {VFile}
 * @param {string|Error} reason - Reason for message.
 * @param {Node|Location|Position} [position] - Location
 *   of message in file.
 * @return {VFileMessage} - File-related message with
 *   location information.
 */
function message(reason, position) {
    var filePath = this.filePath();
    var range;
    var err;
    var location = {
        'start': {
            'line': null,
            'column': null
        },
        'end': {
            'line': null,
            'column': null
        }
    };

    /*
     * Node / location / position.
     */

    if (position && position.position) {
        position = position.position;
    }

    if (position && position.start) {
        range = stringify(position.start) + '-' + stringify(position.end);
        location = position;
        position = position.start;
    } else {
        range = stringify(position);

        if (position) {
            location.start = position;
            location.end.line = null;
            location.end.column = null;
        }
    }

    err = new VFileMessage(reason.message || reason);

    err.name = (filePath ? filePath + ':' : '') + range;
    err.file = filePath;
    err.reason = reason.message || reason;
    err.line = position ? position.line : null;
    err.column = position ? position.column : null;
    err.location = location;

    if (reason.stack) {
        err.stack = reason.stack;
    }

    return err;
}

/**
 * Warn. Creates a non-fatal message (see `VFile#message()`),
 * and adds it to the file's `messages` list.
 *
 * @example
 *   var file = new VFile();
 *
 *   file.warn('Something went wrong');
 *   // { [1:1: Something went wrong]
 *   //   name: '1:1',
 *   //   file: '',
 *   //   reason: 'Something went wrong',
 *   //   line: null,
 *   //   column: null,
 *   //   fatal: false }
 *
 * @see VFile#message
 * @this {VFile}
 * @memberof {VFile}
 */
function warn() {
    var err = this.message.apply(this, arguments);

    err.fatal = false;

    this.messages.push(err);

    return err;
}

/**
 * Fail. Creates a fatal message (see `VFile#message()`),
 * sets `fatal: true`, adds it to the file's
 * `messages` list.
 *
 * If `quiet` is not `true`, throws the error.
 *
 * @example
 *   var file = new VFile();
 *
 *   file.fail('Something went wrong');
 *   // 1:1: Something went wrong
 *   //     at VFile.exception (vfile/index.js:296:11)
 *   //     at VFile.fail (vfile/index.js:360:20)
 *   //     at repl:1:6
 *
 *   file.quiet = true;
 *   file.fail('Something went wrong');
 *   // { [1:1: Something went wrong]
 *   //   name: '1:1',
 *   //   file: '',
 *   //   reason: 'Something went wrong',
 *   //   line: null,
 *   //   column: null,
 *   //   fatal: true }
 *
 * @this {VFile}
 * @memberof {VFile}
 * @throws {VFileMessage} - When not `quiet: true`.
 * @param {string|Error} reason - Reason for failure.
 * @param {Node|Location|Position} [position] - Place
 *   of failure in file.
 * @return {VFileMessage} - Unless thrown, of course.
 */
function fail(reason, position) {
    var err = this.message(reason, position);

    err.fatal = true;

    this.messages.push(err);

    if (!this.quiet) {
        throw err;
    }

    return err;
}

/**
 * Check if a fatal message occurred making the file no
 * longer processable.
 *
 * @example
 *   var file = new VFile();
 *   file.quiet = true;
 *
 *   file.hasFailed(); // false
 *
 *   file.fail('Something went wrong');
 *   file.hasFailed(); // true
 *
 * @this {VFile}
 * @memberof {VFile}
 * @return {boolean} - `true` if at least one of file's
 *   `messages` has a `fatal` property set to `true`
 */
function hasFailed() {
    var messages = this.messages;
    var index = -1;
    var length = messages.length;

    while (++index < length) {
        if (messages[index].fatal) {
            return true;
        }
    }

    return false;
}

/**
 * Access metadata.
 *
 * @example
 *   var file = new VFile('Foo');
 *
 *   file.namespace('foo').bar = 'baz';
 *
 *   console.log(file.namespace('foo').bar) // 'baz';
 *
 * @this {VFile}
 * @memberof {VFile}
 * @param {string} key - Namespace key.
 * @return {Object} - Private space.
 */
function namespace(key) {
    var self = this;
    var space = self.data;

    if (!space) {
        space = self.data = {};
    }

    if (!space[key]) {
        space[key] = {};
    }

    return space[key];
}

/*
 * Methods.
 */

var vFilePrototype = VFile.prototype;

vFilePrototype.basename = basename;
vFilePrototype.move = move;
vFilePrototype.toString = toString;
vFilePrototype.message = message;
vFilePrototype.warn = warn;
vFilePrototype.fail = fail;
vFilePrototype.hasFailed = hasFailed;
vFilePrototype.namespace = namespace;

/*
 * Expose.
 */

module.exports = VFile;
