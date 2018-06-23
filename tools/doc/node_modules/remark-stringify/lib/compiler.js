/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:compiler
 * @fileoverview Markdown compiler
 */

'use strict';

/* eslint-env commonjs */

/*
 * Dependencies.
 */

var decode = require('parse-entities');
var encode = require('stringify-entities');
var table = require('markdown-table');
var repeat = require('repeat-string');
var extend = require('extend');
var ccount = require('ccount');
var longestStreak = require('longest-streak');
var defaultOptions = require('./defaults.js');

/*
 * Constants.
 */

var INDENT = 4;
var MINIMUM_CODE_FENCE_LENGTH = 3;
var YAML_FENCE_LENGTH = 3;
var MINIMUM_RULE_LENGTH = 3;
var MAILTO = 'mailto:';
var ERROR_LIST_ITEM_INDENT = 'Cannot indent code properly. See ' +
    'http://git.io/vgFvT';

/*
 * Expressions.
 */

var EXPRESSIONS_WHITE_SPACE = /\s/;

/*
 * Naive fence expression.
 */

var FENCE = /([`~])\1{2}/;

/*
 * Expression for a protocol.
 *
 * @see http://en.wikipedia.org/wiki/URI_scheme#Generic_syntax
 */

var PROTOCOL = /^[a-z][a-z+.-]+:\/?/i;

/*
 * Punctuation characters.
 */

var PUNCTUATION = /[-!"#$%&'()*+,.\/:;<=>?@\[\\\]^`{|}~_]/;

/*
 * Characters.
 */

var ANGLE_BRACKET_CLOSE = '>';
var ANGLE_BRACKET_OPEN = '<';
var ASTERISK = '*';
var BACKSLASH = '\\';
var CARET = '^';
var COLON = ':';
var SEMICOLON = ';';
var DASH = '-';
var DOT = '.';
var EMPTY = '';
var EQUALS = '=';
var EXCLAMATION_MARK = '!';
var HASH = '#';
var AMPERSAND = '&';
var LINE = '\n';
var CARRIAGE = '\r';
var FORM_FEED = '\f';
var PARENTHESIS_OPEN = '(';
var PARENTHESIS_CLOSE = ')';
var PIPE = '|';
var PLUS = '+';
var QUOTE_DOUBLE = '"';
var QUOTE_SINGLE = '\'';
var SPACE = ' ';
var TAB = '\t';
var VERTICAL_TAB = '\u000B';
var SQUARE_BRACKET_OPEN = '[';
var SQUARE_BRACKET_CLOSE = ']';
var TICK = '`';
var TILDE = '~';
var UNDERSCORE = '_';

/**
 * Check whether `character` is numeric.
 *
 * @param {string} character - Single character to check.
 * @return {boolean} - Whether `character` is numeric.
 */
function isNumeric(character) {
    return /[0-9]/.test(character);
}

/**
 * Check whether `character` is alphanumeric.
 *
 * @param {string} character - Single character to check.
 * @return {boolean} - Whether `character` is alphanumeric.
 */
function isAlphanumeric(character) {
    return /\w/.test(character) && character !== UNDERSCORE;
}

/*
 * Entities.
 */

var ENTITY_AMPERSAND = AMPERSAND + 'amp' + SEMICOLON;
var ENTITY_ANGLE_BRACKET_OPEN = AMPERSAND + 'lt' + SEMICOLON;
var ENTITY_COLON = AMPERSAND + '#x3A' + SEMICOLON;

/*
 * Character combinations.
 */

var BREAK = LINE + LINE;
var GAP = BREAK + LINE;
var DOUBLE_TILDE = TILDE + TILDE;

/*
 * Allowed entity options.
 */

var ENTITY_OPTIONS = {};

ENTITY_OPTIONS.true = true;
ENTITY_OPTIONS.false = true;
ENTITY_OPTIONS.numbers = true;
ENTITY_OPTIONS.escape = true;

/*
 * Allowed list-bullet characters.
 */

var LIST_BULLETS = {};

LIST_BULLETS[ASTERISK] = true;
LIST_BULLETS[DASH] = true;
LIST_BULLETS[PLUS] = true;

/*
 * Allowed horizontal-rule bullet characters.
 */

var THEMATIC_BREAK_BULLETS = {};

THEMATIC_BREAK_BULLETS[ASTERISK] = true;
THEMATIC_BREAK_BULLETS[DASH] = true;
THEMATIC_BREAK_BULLETS[UNDERSCORE] = true;

/*
 * Allowed emphasis characters.
 */

var EMPHASIS_MARKERS = {};

EMPHASIS_MARKERS[UNDERSCORE] = true;
EMPHASIS_MARKERS[ASTERISK] = true;

/*
 * Allowed fence markers.
 */

var FENCE_MARKERS = {};

FENCE_MARKERS[TICK] = true;
FENCE_MARKERS[TILDE] = true;

/*
 * Which method to use based on `list.ordered`.
 */

var ORDERED_MAP = {};

ORDERED_MAP.true = 'visitOrderedItems';
ORDERED_MAP.false = 'visitUnorderedItems';

/*
 * Allowed list-item-indent's.
 */

var LIST_ITEM_INDENTS = {};

var LIST_ITEM_TAB = 'tab';
var LIST_ITEM_ONE = '1';
var LIST_ITEM_MIXED = 'mixed';

LIST_ITEM_INDENTS[LIST_ITEM_ONE] = true;
LIST_ITEM_INDENTS[LIST_ITEM_TAB] = true;
LIST_ITEM_INDENTS[LIST_ITEM_MIXED] = true;

/*
 * Which checkbox to use.
 */

var CHECKBOX_MAP = {};

CHECKBOX_MAP.null = EMPTY;
CHECKBOX_MAP.undefined = EMPTY;
CHECKBOX_MAP.true = SQUARE_BRACKET_OPEN + 'x' + SQUARE_BRACKET_CLOSE + SPACE;
CHECKBOX_MAP.false = SQUARE_BRACKET_OPEN + SPACE + SQUARE_BRACKET_CLOSE +
    SPACE;

/**
 * Throw an exception with in its `message` `value`
 * and `name`.
 *
 * @param {*} value - Invalid value.
 * @param {string} name - Setting name.
 */
function raise(value, name) {
    throw new Error(
        'Invalid value `' + value + '` ' +
        'for setting `' + name + '`'
    );
}

/**
 * Validate a value to be boolean. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * a boolean.
 *
 * @example
 *   validateBoolean({foo: null}, 'foo', true) // true
 *   validateBoolean({foo: false}, 'foo', true) // false
 *   validateBoolean({foo: 'bar'}, 'foo', true) // Throws
 *
 * @throws {Error} - When a setting is neither omitted nor
 *   a boolean.
 * @param {Object} context - Settings.
 * @param {string} name - Setting name.
 * @param {boolean} def - Default value.
 */
function validateBoolean(context, name, def) {
    var value = context[name];

    if (value === null || value === undefined) {
        value = def;
    }

    if (typeof value !== 'boolean') {
        raise(value, 'options.' + name);
    }

    context[name] = value;
}

/**
 * Validate a value to be boolean. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * a boolean.
 *
 * @example
 *   validateNumber({foo: null}, 'foo', 1) // 1
 *   validateNumber({foo: 2}, 'foo', 1) // 2
 *   validateNumber({foo: 'bar'}, 'foo', 1) // Throws
 *
 * @throws {Error} - When a setting is neither omitted nor
 *   a number.
 * @param {Object} context - Settings.
 * @param {string} name - Setting name.
 * @param {number} def - Default value.
 */
function validateNumber(context, name, def) {
    var value = context[name];

    if (value === null || value === undefined) {
        value = def;
    }

    if (typeof value !== 'number' || value !== value) {
        raise(value, 'options.' + name);
    }

    context[name] = value;
}

/**
 * Validate a value to be in `map`. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * in `map`.
 *
 * @example
 *   var map = {bar: true, baz: true};
 *   validateString({foo: null}, 'foo', 'bar', map) // 'bar'
 *   validateString({foo: 'baz'}, 'foo', 'bar', map) // 'baz'
 *   validateString({foo: true}, 'foo', 'bar', map) // Throws
 *
 * @throws {Error} - When a setting is neither omitted nor
 *   in `map`.
 * @param {Object} context - Settings.
 * @param {string} name - Setting name.
 * @param {string} def - Default value.
 * @param {Object} map - Enum.
 */
function validateString(context, name, def, map) {
    var value = context[name];

    if (value === null || value === undefined) {
        value = def;
    }

    if (!(value in map)) {
        raise(value, 'options.' + name);
    }

    context[name] = value;
}

/*
 * Expose `validate`.
 */

var validate = {
    'boolean': validateBoolean,
    'string': validateString,
    'number': validateNumber
};

/**
 * Construct a state `toggler`: a function which inverses
 * `property` in context based on its current value.
 * The by `toggler` returned function restores that value.
 *
 * @example
 *   var context = {};
 *   var key = 'foo';
 *   var val = true;
 *   context[key] = val;
 *   context.enter = toggler(key, val);
 *   context[key]; // true
 *   var exit = context.enter();
 *   context[key]; // false
 *   var nested = context.enter();
 *   context[key]; // false
 *   nested();
 *   context[key]; // false
 *   exit();
 *   context[key]; // true
 *
 * @param {string} key - Property to toggle.
 * @param {boolean} state - It's default state.
 * @return {function(): function()} - Enter.
 */
function toggler(key, state) {
    /**
     * Construct a toggler for the bound `key`.
     *
     * @return {Function} - Exit state.
     */
    function enter() {
        var self = this;
        var current = self[key];

        self[key] = !state;

        /**
         * State canceler, cancels the state, if allowed.
         */
        function exit() {
            self[key] = current;
        }

        return exit;
    }

    return enter;
}

/**
 * Check whether a node is mergeable with adjacent nodes.
 *
 * @param {Object} node - Node to check.
 * @return {boolean} - Whether `node` is mergable.
 */
function mergeable(node) {
    var start;
    var end;

    if (node.type !== 'text' || !node.position) {
        return true;
    }

    start = node.position.start;
    end = node.position.end;

    /*
     * Only merge nodes which occupy the same size as their
     * `value`.
     */

    return start.line !== end.line ||
        end.column - start.column === node.value.length;
}

/**
 * Encode noop.
 * Simply returns the given value.
 *
 * @example
 *   var encode = encodeNoop();
 *   encode('AT&T') // 'AT&T'
 *
 * @param {string} value - Content.
 * @return {string} - Content, without any modifications.
 */
function encodeNoop(value) {
    return value;
}

/**
 * Factory to encode HTML entities.
 * Creates a no-operation function when `type` is
 * `'false'`, a function which encodes using named
 * references when `type` is `'true'`, and a function
 * which encodes using numbered references when `type` is
 * `'numbers'`.
 *
 * @example
 *   encodeFactory('false')('AT&T') // 'AT&T'
 *   encodeFactory('true')('AT&T') // 'AT&amp;T'
 *   encodeFactory('numbers')('AT&T') // 'ATT&#x26;T'
 *
 * @param {string} type - Either `'true'`, `'false'`, or
 *   `'numbers'`.
 * @return {function(string): string} - Function which
 *   takes a value and returns its encoded version.
 */
function encodeFactory(type) {
    var options = {};

    if (type === 'false') {
        return encodeNoop;
    }

    if (type === 'true') {
        options.useNamedReferences = true;
    }

    if (type === 'escape') {
        options.escapeOnly = options.useNamedReferences = true;
    }

    /**
     * Encode HTML entities using the bound options.
     *
     * @example
     *   // When `type` is `'true'`.
     *   encode('AT&T'); // 'AT&amp;T'
     *
     *   // When `type` is `'numbers'`.
     *   encode('AT&T'); // 'ATT&#x26;T'
     *
     * @param {string} value - Content.
     * @param {Object} [node] - Node which is compiled.
     * @return {string} - Encoded content.
     */
    function encoder(value) {
        return encode(value, options);
    }

    return encoder;
}

/**
 * Returns the length of HTML entity that is a prefix of
 * the given string (excluding the ampersand), 0 if it
 * does not start with an entity.
 *
 * @example
 *   entityPrefixLength('&copycat') // 4
 *   entityPrefixLength('&foo &amp &bar') // 0
 *
 * @param {string} value - Input string.
 * @return {number} - Length of an entity.
 */
function entityPrefixLength(value) {
    var prefix;

    /* istanbul ignore if - Currently also tested for at
     * implemention, but we keep it here because that’s
     * proper. */
    if (value.charAt(0) !== AMPERSAND) {
        return 0;
    }

    prefix = value.split(AMPERSAND, 2).join(AMPERSAND);

    return prefix.length - decode(prefix).length;
}

/**
 * Checks if a string starts with HTML entity.
 *
 * @example
 *   startsWithEntity('&copycat') // true
 *   startsWithEntity('&foo &amp &bar') // false
 *
 * @param {string} value - Value to check.
 * @return {number} - Whether `value` starts an entity.
 */
function startsWithEntity(value) {
    return entityPrefixLength(value) > 0;
}

/**
 * Check if `character` is a valid alignment row character.
 *
 * @example
 *   isAlignmentRowCharacter(':') // true
 *   isAlignmentRowCharacter('=') // false
 *
 * @param {string} character - Character to check.
 * @return {boolean} - Whether `character` is a valid
 *   alignment row character.
 */
function isAlignmentRowCharacter(character) {
    return character === COLON ||
        character === DASH ||
        character === SPACE ||
        character === PIPE;
}

/**
 * Check if `index` in `value` is inside an alignment row.
 *
 * @example
 *   isInAlignmentRow(':--:', 2) // true
 *   isInAlignmentRow(':--:\n:-*-:', 9) // false
 *
 * @param {string} value - Value to check.
 * @param {number} index - Position in `value` to check.
 * @return {boolean} - Whether `index` in `value` is in
 *   an alignment row.
 */
function isInAlignmentRow(value, index) {
    var length = value.length;
    var start = index;
    var character;

    while (++index < length) {
        character = value.charAt(index);

        if (character === LINE) {
            break;
        }

        if (!isAlignmentRowCharacter(character)) {
            return false;
        }
    }

    index = start;

    while (--index > -1) {
        character = value.charAt(index);

        if (character === LINE) {
            break;
        }

        if (!isAlignmentRowCharacter(character)) {
            return false;
        }
    }

    return true;
}

/**
 * Factory to escape characters.
 *
 * @example
 *   var escape = escapeFactory({ commonmark: true });
 *   escape('x*x', { type: 'text', value: 'x*x' }) // 'x\\*x'
 *
 * @param {Object} options - Compiler options.
 * @return {function(value, node, parent): string} - Function which
 *   takes a value and a node and (optionally) its parent and returns
 *   its escaped value.
 */
function escapeFactory(options) {
    /**
     * Escape punctuation characters in a node's value.
     *
     * @param {string} value - Value to escape.
     * @param {Object} node - Node in which `value` exists.
     * @param {Object} [parent] - Parent of `node`.
     * @return {string} - Escaped `value`.
     */
    return function escape(value, node, parent) {
        var self = this;
        var gfm = options.gfm;
        var commonmark = options.commonmark;
        var pedantic = options.pedantic;
        var siblings = parent && parent.children;
        var index = siblings && siblings.indexOf(node);
        var prev = siblings && siblings[index - 1];
        var next = siblings && siblings[index + 1];
        var length = value.length;
        var position = -1;
        var queue = [];
        var escaped = queue;
        var afterNewLine;
        var character;
        var wordCharBefore;
        var wordCharAfter;
        var offset;

        if (prev) {
            afterNewLine = prev.type === 'text' && /\n\s*$/.test(prev.value);
        } else if (parent) {
            afterNewLine = parent.type === 'paragraph';
        }

        while (++position < length) {
            character = value.charAt(position);

            if (
                character === BACKSLASH ||
                character === TICK ||
                character === ASTERISK ||
                character === SQUARE_BRACKET_OPEN ||
                (
                    character === UNDERSCORE &&
                    /*
                     * Delegate leading/trailing underscores
                     * to the multinode version below.
                     */
                    0 < position &&
                    position < length - 1 &&
                    (
                        pedantic ||
                        !isAlphanumeric(value.charAt(position - 1)) ||
                        !isAlphanumeric(value.charAt(position + 1))
                    )
                ) ||
                (self.inLink && character === SQUARE_BRACKET_CLOSE) ||
                (
                    gfm &&
                    character === PIPE &&
                    (
                        self.inTable ||
                        isInAlignmentRow(value, position)
                    )
                )
            ) {
                afterNewLine = false;
                queue.push(BACKSLASH);
            } else if (character === ANGLE_BRACKET_OPEN) {
                afterNewLine = false;

                if (commonmark) {
                    queue.push(BACKSLASH);
                } else {
                    queue.push(ENTITY_ANGLE_BRACKET_OPEN);
                    continue;
                }
            } else if (
                gfm &&
                !self.inLink &&
                character === COLON &&
                (
                    queue.slice(-6).join(EMPTY) === 'mailto' ||
                    queue.slice(-5).join(EMPTY) === 'https' ||
                    queue.slice(-4).join(EMPTY) === 'http'
                )
            ) {
                afterNewLine = false;

                if (commonmark) {
                    queue.push(BACKSLASH);
                } else {
                    queue.push(ENTITY_COLON);
                    continue;
                }
            /* istanbul ignore if - Impossible to test with
             * the current set-up.  We need tests which try
             * to force markdown content into the tree. */
            } else if (
                character === AMPERSAND &&
                startsWithEntity(value.slice(position))
            ) {
                afterNewLine = false;

                if (commonmark) {
                    queue.push(BACKSLASH);
                } else {
                    queue.push(ENTITY_AMPERSAND);
                    continue;
                }
            } else if (
                gfm &&
                character === TILDE &&
                value.charAt(position + 1) === TILDE
            ) {
                queue.push(BACKSLASH, TILDE);
                afterNewLine = false;
                position += 1;
            } else if (character === LINE) {
                afterNewLine = true;
            } else if (afterNewLine) {
                if (
                    character === ANGLE_BRACKET_CLOSE ||
                    character === HASH ||
                    LIST_BULLETS[character]
                ) {
                    queue.push(BACKSLASH);
                    afterNewLine = false;
                } else if (isNumeric(character)) {
                    offset = position + 1;

                    while (offset < length) {
                        if (!isNumeric(value.charAt(offset))) {
                            break;
                        }

                        offset++;
                    }

                    if (
                        value.charAt(offset) === DOT ||
                        (
                            commonmark &&
                            /* istanbul ignore next - hard to test :( */
                            value.charAt(offset) === PARENTHESIS_CLOSE
                        )
                    ) {
                        queue.push(value.slice(position, offset), BACKSLASH);
                        position = offset;
                        character = value.charAt(position);
                    }

                    afterNewLine = false;
                } else if (
                    character !== SPACE &&
                    character !== TAB &&
                    character !== CARRIAGE &&
                    character !== VERTICAL_TAB &&
                    character !== FORM_FEED
                ) {
                    afterNewLine = false;
                }
            }

            queue.push(character);
        }

        /*
         * Multi-node versions.
         */

        if (siblings && node.type === 'text') {
            /*
             * Check for an opening parentheses after a
             * link-reference (which can be joined by
             * white-space).
             */

            if (
                prev &&
                prev.referenceType === 'shortcut'
            ) {
                position = -1;
                length = escaped.length;

                while (++position < length) {
                    character = escaped[position];

                    if (character === SPACE || character === TAB) {
                        continue;
                    }

                    if (character === PARENTHESIS_OPEN) {
                        escaped[position] = BACKSLASH + character;
                    }

                    if (character === COLON) {
                        if (commonmark) {
                            escaped[position] = BACKSLASH + character;
                        } else {
                            escaped[position] = ENTITY_COLON;
                        }
                    }

                    break;
                }

                /*
                 * If the current node is all spaces / tabs,
                 * preceded by a shortcut, and followed by
                 * a text starting with `(`, escape it.
                 */

                if (
                    next &&
                    position === length &&
                    next.type === 'text' &&
                    next.value.charAt(0) === PARENTHESIS_OPEN
                ) {
                    escaped.push(BACKSLASH);
                }
            }

            /*
             * Ensure non-auto-links are not seen as links.
             * This pattern needs to check the preceding
             * nodes too.
             */

            if (
                gfm &&
                !self.inLink &&
                prev &&
                prev.type === 'text' &&
                value.charAt(0) === COLON
            ) {
                queue = prev.value.slice(-6);

                if (
                    queue === 'mailto' ||
                    queue.slice(-5) === 'https' ||
                    queue.slice(-4) === 'http'
                ) {
                    if (commonmark) {
                        escaped.unshift(BACKSLASH);
                    } else {
                        escaped.splice(0, 1, ENTITY_COLON);
                    }
                }
            }

            /*
             * Escape ampersand if it would otherwise
             * start an entity.
             */

            if (
                next &&
                next.type === 'text' &&
                value.slice(-1) === AMPERSAND &&
                startsWithEntity(AMPERSAND + next.value)
            ) {
                if (commonmark) {
                    escaped.splice(escaped.length - 1, 0, BACKSLASH);
                } else {
                    escaped.push('amp', SEMICOLON);
                }
            }

            /*
             * Escape double tildes in GFM.
             */

            if (
                gfm &&
                next &&
                next.type === 'text' &&
                value.slice(-1) === TILDE &&
                next.value.charAt(0) === TILDE
            ) {
                escaped.splice(escaped.length - 1, 0, BACKSLASH);
            }

            /*
             * Escape underscores, but not mid-word (unless
             * in pedantic mode).
             */

            wordCharBefore = prev &&
                prev.type === 'text' &&
                isAlphanumeric(prev.value.slice(-1))

            wordCharAfter = next &&
                next.type === 'text' &&
                isAlphanumeric(next.value.charAt(0))

            if (length <= 1) {
                if (
                    value === UNDERSCORE &&
                    (
                        pedantic ||
                        !wordCharBefore ||
                        !wordCharAfter
                    )
                ) {
                    escaped.unshift(BACKSLASH);
                }
            } else {
                if (
                    value.charAt(0) === UNDERSCORE &&
                    (
                        pedantic ||
                        !wordCharBefore ||
                        /* istanbul ignore next - only for trees */
                        !isAlphanumeric(value.charAt(1))
                    )
                ) {
                    escaped.unshift(BACKSLASH);
                }

                if (
                    value.slice(-1) === UNDERSCORE &&
                    (
                        pedantic ||
                        !wordCharAfter ||
                        /* istanbul ignore next - only for trees */
                        !isAlphanumeric(value.slice(-2).charAt(0))
                    )
                ) {
                    escaped.splice(escaped.length - 1, 0, BACKSLASH);
                }
            }
        }

        return escaped.join(EMPTY);
    };
}

/**
 * Wrap `url` in angle brackets when needed, or when
 * forced.
 *
 * In links, images, and definitions, the URL part needs
 * to be enclosed when it:
 *
 * - has a length of `0`;
 * - contains white-space;
 * - has more or less opening than closing parentheses.
 *
 * @example
 *   encloseURI('foo bar') // '<foo bar>'
 *   encloseURI('foo(bar(baz)') // '<foo(bar(baz)>'
 *   encloseURI('') // '<>'
 *   encloseURI('example.com') // 'example.com'
 *   encloseURI('example.com', true) // '<example.com>'
 *
 * @param {string} uri - URI to enclose.
 * @param {boolean?} [always] - Force enclosing.
 * @return {boolean} - Properly enclosed `uri`.
 */
function encloseURI(uri, always) {
    if (
        always ||
        !uri.length ||
        EXPRESSIONS_WHITE_SPACE.test(uri) ||
        ccount(uri, PARENTHESIS_OPEN) !== ccount(uri, PARENTHESIS_CLOSE)
    ) {
        return ANGLE_BRACKET_OPEN + uri + ANGLE_BRACKET_CLOSE;
    }

    return uri;
}

/**
 * There is currently no way to support nested delimiters
 * across Markdown.pl, CommonMark, and GitHub (RedCarpet).
 * The following code supports Markdown.pl and GitHub.
 * CommonMark is not supported when mixing double- and
 * single quotes inside a title.
 *
 * @see https://github.com/vmg/redcarpet/issues/473
 * @see https://github.com/jgm/CommonMark/issues/308
 *
 * @example
 *   encloseTitle('foo') // '"foo"'
 *   encloseTitle('foo \'bar\' baz') // '"foo \'bar\' baz"'
 *   encloseTitle('foo "bar" baz') // '\'foo "bar" baz\''
 *   encloseTitle('foo "bar" \'baz\'') // '"foo "bar" \'baz\'"'
 *
 * @param {string} title - Content.
 * @return {string} - Properly enclosed title.
 */
function encloseTitle(title) {
    var delimiter = QUOTE_DOUBLE;

    if (title.indexOf(delimiter) !== -1) {
        delimiter = QUOTE_SINGLE;
    }

    return delimiter + title + delimiter;
}

/**
 * Pad `value` with `level * INDENT` spaces.  Respects
 * lines. Ignores empty lines.
 *
 * @example
 *   pad('foo', 1) // '    foo'
 *
 * @param {string} value - Content.
 * @param {number} level - Indentation level.
 * @return {string} - Padded `value`.
 */
function pad(value, level) {
    var index;
    var padding;

    value = value.split(LINE);

    index = value.length;
    padding = repeat(SPACE, level * INDENT);

    while (index--) {
        if (value[index].length !== 0) {
            value[index] = padding + value[index];
        }
    }

    return value.join(LINE);
}

/**
 * Construct a new compiler.
 *
 * @example
 *   var compiler = new Compiler(new File('> foo.'));
 *
 * @constructor
 * @class {Compiler}
 * @param {File} file - Virtual file.
 * @param {Object?} [options] - Passed to
 *   `Compiler#setOptions()`.
 */
function Compiler(file, options) {
    var self = this;

    self.file = file;

    self.options = extend({}, self.options);

    self.setOptions(options);
}

/*
 * Cache prototype.
 */

var compilerPrototype = Compiler.prototype;

/*
 * Expose defaults.
 */

compilerPrototype.options = defaultOptions;

/*
 * Expose visitors.
 */

var visitors = compilerPrototype.visitors = {};

/*
 * Map of applicable enum's.
 */

var maps = {
    'entities': ENTITY_OPTIONS,
    'bullet': LIST_BULLETS,
    'rule': THEMATIC_BREAK_BULLETS,
    'listItemIndent': LIST_ITEM_INDENTS,
    'emphasis': EMPHASIS_MARKERS,
    'strong': EMPHASIS_MARKERS,
    'fence': FENCE_MARKERS
};

/**
 * Set options.  Does not overwrite previously set
 * options.
 *
 * @example
 *   var compiler = new Compiler();
 *   compiler.setOptions({bullet: '*'});
 *
 * @this {Compiler}
 * @throws {Error} - When an option is invalid.
 * @param {Object?} [options] - Stringify settings.
 * @return {Compiler} - `self`.
 */
compilerPrototype.setOptions = function (options) {
    var self = this;
    var current = self.options;
    var ruleRepetition;
    var key;

    if (options === null || options === undefined) {
        options = {};
    } else if (typeof options === 'object') {
        options = extend({}, options);
    } else {
        throw new Error(
            'Invalid value `' + options + '` ' +
            'for setting `options`'
        );
    }

    for (key in defaultOptions) {
        validate[typeof current[key]](
            options, key, current[key], maps[key]
        );
    }

    ruleRepetition = options.ruleRepetition;

    if (ruleRepetition && ruleRepetition < MINIMUM_RULE_LENGTH) {
        raise(ruleRepetition, 'options.ruleRepetition');
    }

    self.encode = encodeFactory(String(options.entities));
    self.escape = escapeFactory(options);

    self.options = options;

    return self;
};

/*
 * Enter and exit helpers.
 */

compilerPrototype.enterLink = toggler('inLink', false);
compilerPrototype.enterTable = toggler('inTable', false);

/**
 * Shortcut and collapsed link references need no escaping
 * and encoding during the processing of child nodes (it
 * must be implied from identifier).
 *
 * This toggler turns encoding and escaping off for shortcut
 * and collapsed references.
 *
 * Implies `enterLink`.
 *
 * @param {Compiler} compiler - Compiler instance.
 * @param {LinkReference} node - LinkReference node.
 * @return {Function} - Exit state.
 */
compilerPrototype.enterLinkReference = function (compiler, node) {
    var encode = compiler.encode;
    var escape = compiler.escape;
    var exitLink = compiler.enterLink();

    if (
        node.referenceType === 'shortcut' ||
        node.referenceType === 'collapsed'
    ) {
        compiler.encode = compiler.escape = encodeNoop;
        return function () {
            compiler.encode = encode;
            compiler.escape = escape;
            exitLink();
        };
    } else {
        return exitLink;
    }
};

/**
 * Visit a node.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.visit({
 *     type: 'strong',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     }]
 *   });
 *   // '**Foo**'
 *
 * @param {Object} node - Node.
 * @param {Object?} [parent] - `node`s parent.
 * @return {string} - Compiled `node`.
 */
compilerPrototype.visit = function (node, parent) {
    var self = this;

    /*
     * Fail on unknown nodes.
     */

    if (typeof self.visitors[node.type] !== 'function') {
        self.file.fail(
            'Missing compiler for node of type `' +
            node.type + '`: `' + node + '`',
            node
        );
    }

    return self.visitors[node.type].call(self, node, parent);
};

/**
 * Visit all children of `parent`.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.all({
 *     type: 'strong',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     },
 *     {
 *       type: 'text',
 *       value: 'Bar'
 *     }]
 *   });
 *   // ['Foo', 'Bar']
 *
 * @param {Object} parent - Parent node of children.
 * @return {Array.<string>} - List of compiled children.
 */
compilerPrototype.all = function (parent) {
    var self = this;
    var children = parent.children;
    var values = [];
    var results = [];
    var length = children.length;
    var index = 0;
    var node = children[0];
    var next;

    if (!length) {
        return values;
    }

    length++;

    while (++index < length) {
        next = children[index];

        if (
            next &&
            node.type === 'text' &&
            node.type === next.type &&
            mergeable(node) &&
            mergeable(next)
        ) {
            node.value += next.value;
        } else {
            values.push(node);
            node = next;
        }
    }

    index = -1;
    length = values.length;
    parent.children = values;

    while (++index < length) {
        results[index] = self.visit(values[index], parent);
    }

    return results;
};

/**
 * Visit ordered list items.
 *
 * Starts the list with
 * `node.start` and increments each following list item
 * bullet by one:
 *
 *     2. foo
 *     3. bar
 *
 * In `incrementListMarker: false` mode, does not increment
 * each marker and stays on `node.start`:
 *
 *     1. foo
 *     1. bar
 *
 * Adds an extra line after an item if it has
 * `loose: true`.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.visitOrderedItems({
 *     type: 'list',
 *     ordered: true,
 *     children: [{
 *       type: 'listItem',
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   });
 *   // '1.  bar'
 *
 * @param {Object} node - `list` node with
 *   `ordered: true`.
 * @return {string} - Markdown list.
 */
compilerPrototype.visitOrderedItems = function (node) {
    var self = this;
    var increment = self.options.incrementListMarker;
    var values = [];
    var start = node.start;
    var children = node.children;
    var length = children.length;
    var index = -1;
    var bullet;
    var fn = self.visitors.listItem;

    while (++index < length) {
        bullet = (increment ? start + index : start) + DOT;
        values[index] = fn.call(self, children[index], node, index, bullet);
    }

    return values.join(LINE);
};

/**
 * Visit unordered list items.
 *
 * Uses `options.bullet` as each item's bullet.
 *
 * Adds an extra line after an item if it has
 * `loose: true`.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.visitUnorderedItems({
 *     type: 'list',
 *     ordered: false,
 *     children: [{
 *       type: 'listItem',
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   });
 *   // '-   bar'
 *
 * @param {Object} node - `list` node with
 *   `ordered: false`.
 * @return {string} - Markdown list.
 */
compilerPrototype.visitUnorderedItems = function (node) {
    var self = this;
    var values = [];
    var children = node.children;
    var length = children.length;
    var index = -1;
    var bullet = self.options.bullet;
    var fn = self.visitors.listItem;

    while (++index < length) {
        values[index] = fn.call(self, children[index], node, index, bullet);
    }

    return values.join(LINE);
};

/**
 * Stringify a block node with block children (e.g., `root`
 * or `blockquote`).
 *
 * Knows about code following a list, or adjacent lists
 * with similar bullets, and places an extra newline
 * between them.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.block({
 *     type: 'root',
 *     children: [{
 *       type: 'paragraph',
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   });
 *   // 'bar'
 *
 * @param {Object} node - `root` node.
 * @return {string} - Markdown block content.
 */
compilerPrototype.block = function (node) {
    var self = this;
    var values = [];
    var children = node.children;
    var length = children.length;
    var index = -1;
    var child;
    var prev;

    while (++index < length) {
        child = children[index];

        if (prev) {
            /*
             * Duplicate nodes, such as a list
             * directly following another list,
             * often need multiple new lines.
             *
             * Additionally, code blocks following a list
             * might easily be mistaken for a paragraph
             * in the list itself.
             */

            if (child.type === prev.type && prev.type === 'list') {
                values.push(prev.ordered === child.ordered ? GAP : BREAK);
            } else if (
                prev.type === 'list' &&
                child.type === 'code' &&
                !child.lang
            ) {
                values.push(GAP);
            } else {
                values.push(BREAK);
            }
        }

        values.push(self.visit(child, node));

        prev = child;
    }

    return values.join(EMPTY);
};

/**
 * Stringify a root.
 *
 * Adds a final newline to ensure valid POSIX files.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.root({
 *     type: 'root',
 *     children: [{
 *       type: 'paragraph',
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   });
 *   // 'bar'
 *
 * @param {Object} node - `root` node.
 * @return {string} - Markdown document.
 */
visitors.root = function (node) {
    return this.block(node) + LINE;
};

/**
 * Stringify a heading.
 *
 * In `setext: true` mode and when `depth` is smaller than
 * three, creates a setext header:
 *
 *     Foo
 *     ===
 *
 * Otherwise, an ATX header is generated:
 *
 *     ### Foo
 *
 * In `closeAtx: true` mode, the header is closed with
 * hashes:
 *
 *     ### Foo ###
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.heading({
 *     type: 'heading',
 *     depth: 2,
 *     children: [{
 *       type: 'strong',
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   });
 *   // '## **bar**'
 *
 * @param {Object} node - `heading` node.
 * @return {string} - Markdown heading.
 */
visitors.heading = function (node) {
    var self = this;
    var setext = self.options.setext;
    var closeAtx = self.options.closeAtx;
    var depth = node.depth;
    var content = self.all(node).join(EMPTY);
    var prefix;

    if (setext && depth < 3) {
        return content + LINE +
            repeat(depth === 1 ? EQUALS : DASH, content.length);
    }

    prefix = repeat(HASH, node.depth);
    content = prefix + SPACE + content;

    if (closeAtx) {
        content += SPACE + prefix;
    }

    return content;
};

/**
 * Stringify text.
 *
 * Supports named entities in `settings.encode: true` mode:
 *
 *     AT&amp;T
 *
 * Supports numbered entities in `settings.encode: numbers`
 * mode:
 *
 *     AT&#x26;T
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.text({
 *     type: 'text',
 *     value: 'foo'
 *   });
 *   // 'foo'
 *
 * @param {Object} node - `text` node.
 * @param {Object} parent - Parent of `node`.
 * @return {string} - Raw markdown text.
 */
visitors.text = function (node, parent) {
    return this.encode(this.escape(node.value, node, parent), node);
};

/**
 * Stringify a paragraph.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.paragraph({
 *     type: 'paragraph',
 *     children: [{
 *       type: 'strong',
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   });
 *   // '**bar**'
 *
 * @param {Object} node - `paragraph` node.
 * @return {string} - Markdown paragraph.
 */
visitors.paragraph = function (node) {
    return this.all(node).join(EMPTY);
};

/**
 * Stringify a block quote.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.paragraph({
 *     type: 'blockquote',
 *     children: [{
 *       type: 'paragraph',
 *       children: [{
 *         type: 'strong',
 *         children: [{
 *           type: 'text',
 *           value: 'bar'
 *         }]
 *       }]
 *     }]
 *   });
 *   // '> **bar**'
 *
 * @param {Object} node - `blockquote` node.
 * @return {string} - Markdown block quote.
 */
visitors.blockquote = function (node) {
    var values = this.block(node).split(LINE);
    var result = [];
    var length = values.length;
    var index = -1;
    var value;

    while (++index < length) {
        value = values[index];
        result[index] = (value ? SPACE : EMPTY) + value;
    }

    return ANGLE_BRACKET_CLOSE + result.join(LINE + ANGLE_BRACKET_CLOSE);
};

/**
 * Stringify a list. See `Compiler#visitOrderedList()` and
 * `Compiler#visitUnorderedList()` for internal working.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.visitUnorderedItems({
 *     type: 'list',
 *     ordered: false,
 *     children: [{
 *       type: 'listItem',
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   });
 *   // '-   bar'
 *
 * @param {Object} node - `list` node.
 * @return {string} - Markdown list.
 */
visitors.list = function (node) {
    return this[ORDERED_MAP[node.ordered]](node);
};

/**
 * Stringify a list item.
 *
 * Prefixes the content with a checked checkbox when
 * `checked: true`:
 *
 *     [x] foo
 *
 * Prefixes the content with an unchecked checkbox when
 * `checked: false`:
 *
 *     [ ] foo
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.listItem({
 *     type: 'listItem',
 *     checked: true,
 *     children: [{
 *       type: 'text',
 *       value: 'bar'
 *     }]
 *   }, {
 *     type: 'list',
 *     ordered: false,
 *     children: [{
 *       type: 'listItem',
 *       checked: true,
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   }, 0, '*');
 *   '-   [x] bar'
 *
 * @param {Object} node - `listItem` node.
 * @param {Object} parent - `list` node.
 * @param {number} position - Index of `node` in `parent`.
 * @param {string} bullet - Bullet to use.  This, and the
 *   `listItemIndent` setting define the used indent.
 * @return {string} - Markdown list item.
 */
visitors.listItem = function (node, parent, position, bullet) {
    var self = this;
    var style = self.options.listItemIndent;
    var children = node.children;
    var values = [];
    var index = -1;
    var length = children.length;
    var loose = node.loose;
    var value;
    var indent;
    var spacing;

    while (++index < length) {
        values[index] = self.visit(children[index], node);
    }

    value = CHECKBOX_MAP[node.checked] + values.join(loose ? BREAK : LINE);

    if (
        style === LIST_ITEM_ONE ||
        (style === LIST_ITEM_MIXED && value.indexOf(LINE) === -1)
    ) {
        indent = bullet.length + 1;
        spacing = SPACE;
    } else {
        indent = Math.ceil((bullet.length + 1) / INDENT) * INDENT;
        spacing = repeat(SPACE, indent - bullet.length);
    }

    value = bullet + spacing + pad(value, indent / INDENT).slice(indent);

    if (loose && parent.children.length - 1 !== position) {
        value += LINE;
    }

    return value;
};

/**
 * Stringify inline code.
 *
 * Knows about internal ticks (`\``), and ensures one more
 * tick is used to enclose the inline code:
 *
 *     ```foo ``bar`` baz```
 *
 * Even knows about inital and final ticks:
 *
 *     `` `foo ``
 *     `` foo` ``
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.inlineCode({
 *     type: 'inlineCode',
 *     value: 'foo(); `bar`; baz()'
 *   });
 *   // '``foo(); `bar`; baz()``'
 *
 * @param {Object} node - `inlineCode` node.
 * @return {string} - Markdown inline code.
 */
visitors.inlineCode = function (node) {
    var value = node.value;
    var ticks = repeat(TICK, longestStreak(value, TICK) + 1);
    var start = ticks;
    var end = ticks;

    if (value.charAt(0) === TICK) {
        start += SPACE;
    }

    if (value.charAt(value.length - 1) === TICK) {
        end = SPACE + end;
    }

    return start + node.value + end;
};

/**
 * Stringify YAML front matter.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.yaml({
 *     type: 'yaml',
 *     value: 'foo: bar'
 *   });
 *   // '---\nfoo: bar\n---'
 *
 * @param {Object} node - `yaml` node.
 * @return {string} - Markdown YAML document.
 */
visitors.yaml = function (node) {
    var delimiter = repeat(DASH, YAML_FENCE_LENGTH);
    var value = node.value ? LINE + node.value : EMPTY;

    return delimiter + value + LINE + delimiter;
};

/**
 * Stringify a code block.
 *
 * Creates indented code when:
 *
 * - No language tag exists;
 * - Not in `fences: true` mode;
 * - A non-empty value exists.
 *
 * Otherwise, GFM fenced code is created:
 *
 *     ```js
 *     foo();
 *     ```
 *
 * When in ``fence: `~` `` mode, uses tildes as fences:
 *
 *     ~~~js
 *     foo();
 *     ~~~
 *
 * Knows about internal fences (Note: GitHub/Kramdown does
 * not support this):
 *
 *     ````javascript
 *     ```markdown
 *     foo
 *     ```
 *     ````
 *
 * Supports named entities in the language flag with
 * `settings.encode` mode.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.code({
 *     type: 'code',
 *     lang: 'js',
 *     value: 'fooo();'
 *   });
 *   // '```js\nfooo();\n```'
 *
 * @param {Object} node - `code` node.
 * @param {Object} parent - Parent of `node`.
 * @return {string} - Markdown code block.
 */
visitors.code = function (node, parent) {
    var self = this;
    var value = node.value;
    var options = self.options;
    var marker = options.fence;
    var language = self.encode(node.lang || EMPTY, node);
    var fence;

    /*
     * Without (needed) fences.
     */

    if (!language && !options.fences && value) {
        /*
         * Throw when pedantic, in a list item which
         * isn’t compiled using a tab.
         */

        if (
            parent &&
            parent.type === 'listItem' &&
            options.listItemIndent !== LIST_ITEM_TAB &&
            options.pedantic
        ) {
            self.file.fail(ERROR_LIST_ITEM_INDENT, node.position);
        }

        return pad(value, 1);
    }

    fence = longestStreak(value, marker) + 1;

    /*
     * Fix GFM / RedCarpet bug, where fence-like characters
     * inside fenced code can exit a code-block.
     * Yes, even when the outer fence uses different
     * characters, or is longer.
     * Thus, we can only pad the code to make it work.
     */

    if (FENCE.test(value)) {
        value = pad(value, 1);
    }

    fence = repeat(marker, Math.max(fence, MINIMUM_CODE_FENCE_LENGTH));

    return fence + language + LINE + value + LINE + fence;
};

/**
 * Stringify HTML.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.html({
 *     type: 'html',
 *     value: '<div>bar</div>'
 *   });
 *   // '<div>bar</div>'
 *
 * @param {Object} node - `html` node.
 * @return {string} - Markdown HTML.
 */
visitors.html = function (node) {
    return node.value;
};

/**
 * Stringify a horizontal rule.
 *
 * The character used is configurable by `rule`: (`'_'`)
 *
 *     ___
 *
 * The number of repititions is defined through
 * `ruleRepetition`: (`6`)
 *
 *     ******
 *
 * Whether spaces delimit each character, is configured
 * through `ruleSpaces`: (`true`)
 *
 *     * * *
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.thematicBreak({
 *     type: 'thematicBreak'
 *   });
 *   // '***'
 *
 * @return {string} - Markdown rule.
 */
visitors.thematicBreak = function () {
    var options = this.options;
    var rule = repeat(options.rule, options.ruleRepetition);

    if (options.ruleSpaces) {
        rule = rule.split(EMPTY).join(SPACE);
    }

    return rule;
};

/**
 * Stringify a strong.
 *
 * The marker used is configurable by `strong`, which
 * defaults to an asterisk (`'*'`) but also accepts an
 * underscore (`'_'`):
 *
 *     _foo_
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.strong({
 *     type: 'strong',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     }]
 *   });
 *   // '**Foo**'
 *
 * @param {Object} node - `strong` node.
 * @return {string} - Markdown strong-emphasised text.
 */
visitors.strong = function (node) {
    var marker = this.options.strong;

    marker = marker + marker;

    return marker + this.all(node).join(EMPTY) + marker;
};

/**
 * Stringify an emphasis.
 *
 * The marker used is configurable by `emphasis`, which
 * defaults to an underscore (`'_'`) but also accepts an
 * asterisk (`'*'`):
 *
 *     *foo*
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.emphasis({
 *     type: 'emphasis',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     }]
 *   });
 *   // '_Foo_'
 *
 * @param {Object} node - `emphasis` node.
 * @return {string} - Markdown emphasised text.
 */
visitors.emphasis = function (node) {
    var marker = this.options.emphasis;

    return marker + this.all(node).join(EMPTY) + marker;
};

/**
 * Stringify a hard break.
 *
 * In Commonmark mode, trailing backslash form is used in order
 * to preserve trailing whitespace that the line may end with,
 * and also for better visibility.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.break({
 *     type: 'break'
 *   });
 *   // '  \n'
 *
 * @return {string} - Hard markdown break.
 */
visitors.break = function () {
    return this.options.commonmark ? BACKSLASH + LINE : SPACE + SPACE + LINE;
};

/**
 * Stringify a delete.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.delete({
 *     type: 'delete',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     }]
 *   });
 *   // '~~Foo~~'
 *
 * @param {Object} node - `delete` node.
 * @return {string} - Markdown strike-through.
 */
visitors.delete = function (node) {
    return DOUBLE_TILDE + this.all(node).join(EMPTY) + DOUBLE_TILDE;
};

/**
 * Stringify a link.
 *
 * When no title exists, the compiled `children` equal
 * `url`, and `url` starts with a protocol, an auto
 * link is created:
 *
 *     <http://example.com>
 *
 * Otherwise, is smart about enclosing `url` (see
 * `encloseURI()`) and `title` (see `encloseTitle()`).
 *
 *    [foo](<foo at bar dot com> 'An "example" e-mail')
 *
 * Supports named entities in the `url` and `title` when
 * in `settings.encode` mode.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.link({
 *     type: 'link',
 *     url: 'http://example.com',
 *     title: 'Example Domain',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     }]
 *   });
 *   // '[Foo](http://example.com "Example Domain")'
 *
 * @param {Object} node - `link` node.
 * @return {string} - Markdown link.
 */
visitors.link = function (node) {
    var self = this;
    var url = self.encode(node.url, node);
    var exit = self.enterLink();
    var escapedURL = self.encode(self.escape(node.url, node));
    var value = self.all(node).join(EMPTY);

    exit();

    if (
        node.title === null &&
        PROTOCOL.test(url) &&
        (escapedURL === value || escapedURL === MAILTO + value)
    ) {
        /*
         * Backslash escapes do not work in autolinks,
         * so we do not escape.
         */

        return encloseURI(self.encode(node.url), true);
    }

    url = encloseURI(url);

    if (node.title) {
        url += SPACE + encloseTitle(self.encode(self.escape(
            node.title, node
        ), node));
    }

    value = SQUARE_BRACKET_OPEN + value + SQUARE_BRACKET_CLOSE;

    value += PARENTHESIS_OPEN + url + PARENTHESIS_CLOSE;

    return value;
};

/**
 * Stringify a link label.
 *
 * Because link references are easily, mistakingly,
 * created (for example, `[foo]`), reference nodes have
 * an extra property depicting how it looked in the
 * original document, so stringification can cause minimal
 * changes.
 *
 * @example
 *   label({
 *     type: 'referenceImage',
 *     referenceType: 'full',
 *     identifier: 'foo'
 *   });
 *   // '[foo]'
 *
 *   label({
 *     type: 'referenceImage',
 *     referenceType: 'collapsed',
 *     identifier: 'foo'
 *   });
 *   // '[]'
 *
 *   label({
 *     type: 'referenceImage',
 *     referenceType: 'shortcut',
 *     identifier: 'foo'
 *   });
 *   // ''
 *
 * @param {Object} node - `linkReference` or
 *   `imageReference` node.
 * @return {string} - Markdown label reference.
 */
function label(node) {
    var value = EMPTY;
    var type = node.referenceType;

    if (type === 'full') {
        value = node.identifier;
    }

    if (type !== 'shortcut') {
        value = SQUARE_BRACKET_OPEN + value + SQUARE_BRACKET_CLOSE;
    }

    return value;
}

/**
 * For shortcut and collapsed reference links, the contents
 * is also an identifier, so we need to restore the original
 * encoding and escaping that were present in the source
 * string.
 *
 * This function takes the unescaped & unencoded value from
 * shortcut's child nodes and the identifier and encodes
 * the former according to the latter.
 *
 * @example
 *   copyIdentifierEncoding('a*b', 'a\\*b*c')
 *   // 'a\\*b*c'
 *
 * @param {string} value - Unescaped and unencoded stringified
 *   link value.
 * @param {string} identifier - Link identifier.
 * @return {string} - Encoded link value.
 */
function copyIdentifierEncoding(value, identifier) {
    var index = 0;
    var position = 0;
    var length = value.length;
    var count = identifier.length;
    var result = [];
    var start;

    while (index < length) {
        /*
         * Take next non-punctuation characters from `value`.
         */

        start = index;

        while (
            index < length &&
            !PUNCTUATION.test(value.charAt(index))
        ) {
            index += 1;
        }

        result.push(value.slice(start, index));

        /*
         * Advance `position` to the next punctuation character.
         */
        while (
            position < count &&
            !PUNCTUATION.test(identifier.charAt(position))
        ) {
            position += 1;
        }

        /*
         * Take next punctuation characters from `identifier`.
         */
        start = position;

        while (
            position < count &&
            PUNCTUATION.test(identifier.charAt(position))
        ) {
            if (identifier.charAt(position) === AMPERSAND) {
                position += entityPrefixLength(identifier.slice(position));
            }
            position += 1;
        }

        result.push(identifier.slice(start, position));

        /*
         * Advance `index` to the next non-punctuation character.
         */
        while (index < length && PUNCTUATION.test(value.charAt(index))) {
            index += 1;
        }
    }

    return result.join(EMPTY);
}

/**
 * Stringify a link reference.
 *
 * See `label()` on how reference labels are created.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.linkReference({
 *     type: 'linkReference',
 *     referenceType: 'collapsed',
 *     identifier: 'foo',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     }]
 *   });
 *   // '[Foo][]'
 *
 * @param {Object} node - `linkReference` node.
 * @return {string} - Markdown link reference.
 */
visitors.linkReference = function (node) {
    var self = this;
    var exitLinkReference = self.enterLinkReference(self, node);
    var value = self.all(node).join(EMPTY);

    exitLinkReference();

    if (
        node.referenceType === 'shortcut' ||
        node.referenceType === 'collapsed'
    ) {
        value = copyIdentifierEncoding(value, node.identifier);
    }

    return SQUARE_BRACKET_OPEN + value + SQUARE_BRACKET_CLOSE + label(node);
};

/**
 * Stringify an image reference.
 *
 * See `label()` on how reference labels are created.
 *
 * Supports named entities in the `alt` when
 * in `settings.encode` mode.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.imageReference({
 *     type: 'imageReference',
 *     referenceType: 'full',
 *     identifier: 'foo',
 *     alt: 'Foo'
 *   });
 *   // '![Foo][foo]'
 *
 * @param {Object} node - `imageReference` node.
 * @return {string} - Markdown image reference.
 */
visitors.imageReference = function (node) {
    var alt = this.encode(node.alt, node) || EMPTY;

    return EXCLAMATION_MARK +
        SQUARE_BRACKET_OPEN + alt + SQUARE_BRACKET_CLOSE +
        label(node);
};

/**
 * Stringify a footnote reference.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.footnoteReference({
 *     type: 'footnoteReference',
 *     identifier: 'foo'
 *   });
 *   // '[^foo]'
 *
 * @param {Object} node - `footnoteReference` node.
 * @return {string} - Markdown footnote reference.
 */
visitors.footnoteReference = function (node) {
    return SQUARE_BRACKET_OPEN + CARET + node.identifier +
        SQUARE_BRACKET_CLOSE;
};

/**
 * Stringify a link- or image definition.
 *
 * Is smart about enclosing `url` (see `encloseURI()`) and
 * `title` (see `encloseTitle()`).
 *
 *    [foo]: <foo at bar dot com> 'An "example" e-mail'
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.definition({
 *     type: 'definition',
 *     url: 'http://example.com',
 *     title: 'Example Domain',
 *     identifier: 'foo'
 *   });
 *   // '[foo]: http://example.com "Example Domain"'
 *
 * @param {Object} node - `definition` node.
 * @return {string} - Markdown link- or image definition.
 */
visitors.definition = function (node) {
    var value = SQUARE_BRACKET_OPEN + node.identifier + SQUARE_BRACKET_CLOSE;
    var url = encloseURI(node.url);

    if (node.title) {
        url += SPACE + encloseTitle(node.title);
    }

    return value + COLON + SPACE + url;
};

/**
 * Stringify an image.
 *
 * Is smart about enclosing `url` (see `encloseURI()`) and
 * `title` (see `encloseTitle()`).
 *
 *    ![foo](</fav icon.png> 'My "favourite" icon')
 *
 * Supports named entities in `url`, `alt`, and `title`
 * when in `settings.encode` mode.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.image({
 *     type: 'image',
 *     url: 'http://example.png/favicon.png',
 *     title: 'Example Icon',
 *     alt: 'Foo'
 *   });
 *   // '![Foo](http://example.png/favicon.png "Example Icon")'
 *
 * @param {Object} node - `image` node.
 * @return {string} - Markdown image.
 */
visitors.image = function (node) {
    var url = encloseURI(this.encode(node.url, node));
    var value;

    if (node.title) {
        url += SPACE + encloseTitle(this.encode(node.title, node));
    }

    value = EXCLAMATION_MARK +
        SQUARE_BRACKET_OPEN + this.encode(node.alt || EMPTY, node) +
        SQUARE_BRACKET_CLOSE;

    value += PARENTHESIS_OPEN + url + PARENTHESIS_CLOSE;

    return value;
};

/**
 * Stringify a footnote.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.footnote({
 *     type: 'footnote',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     }]
 *   });
 *   // '[^Foo]'
 *
 * @param {Object} node - `footnote` node.
 * @return {string} - Markdown footnote.
 */
visitors.footnote = function (node) {
    return SQUARE_BRACKET_OPEN + CARET + this.all(node).join(EMPTY) +
        SQUARE_BRACKET_CLOSE;
};

/**
 * Stringify a footnote definition.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.footnoteDefinition({
 *     type: 'footnoteDefinition',
 *     identifier: 'foo',
 *     children: [{
 *       type: 'paragraph',
 *       children: [{
 *         type: 'text',
 *         value: 'bar'
 *       }]
 *     }]
 *   });
 *   // '[^foo]: bar'
 *
 * @param {Object} node - `footnoteDefinition` node.
 * @return {string} - Markdown footnote definition.
 */
visitors.footnoteDefinition = function (node) {
    var id = node.identifier.toLowerCase();

    return SQUARE_BRACKET_OPEN + CARET + id +
        SQUARE_BRACKET_CLOSE + COLON + SPACE +
        this.all(node).join(BREAK + repeat(SPACE, INDENT));
};

/**
 * Stringify table.
 *
 * Creates a fenced table by default, but not in
 * `looseTable: true` mode:
 *
 *     Foo | Bar
 *     :-: | ---
 *     Baz | Qux
 *
 * NOTE: Be careful with `looseTable: true` mode, as a
 * loose table inside an indented code block on GitHub
 * renders as an actual table!
 *
 * Creates a spaces table by default, but not in
 * `spacedTable: false`:
 *
 *     |Foo|Bar|
 *     |:-:|---|
 *     |Baz|Qux|
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.table({
 *     type: 'table',
 *     align: ['center', null],
 *     children: [
 *       {
 *         type: 'tableRow',
 *         children: [
 *           {
 *             type: 'tableCell'
 *             children: [{
 *               type: 'text'
 *               value: 'Foo'
 *             }]
 *           },
 *           {
 *             type: 'tableCell'
 *             children: [{
 *               type: 'text'
 *               value: 'Bar'
 *             }]
 *           }
 *         ]
 *       },
 *       {
 *         type: 'tableRow',
 *         children: [
 *           {
 *             type: 'tableCell'
 *             children: [{
 *               type: 'text'
 *               value: 'Baz'
 *             }]
 *           },
 *           {
 *             type: 'tableCell'
 *             children: [{
 *               type: 'text'
 *               value: 'Qux'
 *             }]
 *           }
 *         ]
 *       }
 *     ]
 *   });
 *   // '| Foo | Bar |\n| :-: | --- |\n| Baz | Qux |'
 *
 * @param {Object} node - `table` node.
 * @return {string} - Markdown table.
 */
visitors.table = function (node) {
    var self = this;
    var loose = self.options.looseTable;
    var spaced = self.options.spacedTable;
    var rows = node.children;
    var index = rows.length;
    var exit = self.enterTable();
    var result = [];
    var start;

    while (index--) {
        result[index] = self.all(rows[index]);
    }

    exit();

    start = loose ? EMPTY : spaced ? PIPE + SPACE : PIPE;

    return table(result, {
        'align': node.align,
        'start': start,
        'end': start.split(EMPTY).reverse().join(EMPTY),
        'delimiter': spaced ? SPACE + PIPE + SPACE : PIPE
    });
};

/**
 * Stringify a table cell.
 *
 * @example
 *   var compiler = new Compiler();
 *
 *   compiler.tableCell({
 *     type: 'tableCell',
 *     children: [{
 *       type: 'text'
 *       value: 'Qux'
 *     }]
 *   });
 *   // 'Qux'
 *
 * @param {Object} node - `tableCell` node.
 * @return {string} - Markdown table cell.
 */
visitors.tableCell = function (node) {
    return this.all(node).join(EMPTY);
};

/**
 * Stringify the bound file.
 *
 * @example
 *   var file = new VFile('__Foo__');
 *
 *   file.namespace('mdast').tree = {
 *     type: 'strong',
 *     children: [{
 *       type: 'text',
 *       value: 'Foo'
 *     }]
 *   });
 *
 *   new Compiler(file).compile();
 *   // '**Foo**'
 *
 * @this {Compiler}
 * @param {Node} node - Syntax tree.
 * @return {string} - Markdown document.
 */
compilerPrototype.compile = function (node) {
    return this.visit(node);
};

/*
 * Expose `stringify` on `module.exports`.
 */

module.exports = Compiler;
