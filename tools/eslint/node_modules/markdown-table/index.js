'use strict';

/*
 * Useful expressions.
 */

var EXPRESSION_DOT = /\./;
var EXPRESSION_LAST_DOT = /\.[^.]*$/;

/*
 * Allowed alignment values.
 */

var LEFT = 'l';
var RIGHT = 'r';
var CENTER = 'c';
var DOT = '.';
var NULL = '';

var ALLIGNMENT = [LEFT, RIGHT, CENTER, DOT, NULL];

/*
 * Characters.
 */

var COLON = ':';
var DASH = '-';
var PIPE = '|';
var SPACE = ' ';
var NEW_LINE = '\n';

/**
 * Get the length of `value`.
 *
 * @param {string} value
 * @return {number}
 */
function lengthNoop(value) {
    return String(value).length;
}

/**
 * Get a string consisting of `length` `character`s.
 *
 * @param {number} length
 * @param {string} [character=' ']
 * @return {string}
 */
function pad(length, character) {
    return Array(length + 1).join(character || SPACE);
}

/**
 * Get the position of the last dot in `value`.
 *
 * @param {string} value
 * @return {number}
 */
function dotindex(value) {
    var match = EXPRESSION_LAST_DOT.exec(value);

    return match ? match.index + 1 : value.length;
}

/**
 * Create a table from a matrix of strings.
 *
 * @param {Array.<Array.<string>>} table
 * @param {Object?} options
 * @param {boolean?} [options.rule=true]
 * @param {string?} [options.delimiter=" | "]
 * @param {string?} [options.start="| "]
 * @param {string?} [options.end=" |"]
 * @param {Array.<string>?} options.align
 * @param {function(string)?} options.stringLength
 * @return {string} Pretty table
 */
function markdownTable(table, options) {
    var settings = options || {};
    var delimiter = settings.delimiter;
    var start = settings.start;
    var end = settings.end;
    var alignment = settings.align;
    var calculateStringLength = settings.stringLength || lengthNoop;
    var cellCount = 0;
    var rowIndex = -1;
    var rowLength = table.length;
    var sizes = [];
    var align;
    var rule;
    var rows;
    var row;
    var cells;
    var index;
    var position;
    var size;
    var value;
    var spacing;
    var before;
    var after;

    alignment = alignment ? alignment.concat() : [];

    if (delimiter === null || delimiter === undefined) {
        delimiter = SPACE + PIPE + SPACE;
    }

    if (start === null || start === undefined) {
        start = PIPE + SPACE;
    }

    if (end === null || end === undefined) {
        end = SPACE + PIPE;
    }

    while (++rowIndex < rowLength) {
        row = table[rowIndex];

        index = -1;

        if (row.length > cellCount) {
            cellCount = row.length;
        }

        while (++index < cellCount) {
            position = row[index] ? dotindex(row[index]) : null;

            if (!sizes[index]) {
                sizes[index] = 3;
            }

            if (position > sizes[index]) {
                sizes[index] = position;
            }
        }
    }

    if (typeof alignment === 'string') {
        alignment = pad(cellCount, alignment).split('');
    }

    /*
     * Make sure only valid alignments are used.
     */

    index = -1;

    while (++index < cellCount) {
        align = alignment[index];

        if (typeof align === 'string') {
            align = align.charAt(0).toLowerCase();
        }

        if (ALLIGNMENT.indexOf(align) === -1) {
            align = NULL;
        }

        alignment[index] = align;
    }

    rowIndex = -1;
    rows = [];

    while (++rowIndex < rowLength) {
        row = table[rowIndex];

        index = -1;
        cells = [];

        while (++index < cellCount) {
            value = row[index];

            if (value === null || value === undefined) {
                value = '';
            } else {
                value = String(value);
            }

            if (alignment[index] !== DOT) {
                cells[index] = value;
            } else {
                position = dotindex(value);

                size = sizes[index] +
                    (EXPRESSION_DOT.test(value) ? 0 : 1) -
                    (calculateStringLength(value) - position);

                cells[index] = value + pad(size - 1);
            }
        }

        rows[rowIndex] = cells;
    }

    sizes = [];
    rowIndex = -1;

    while (++rowIndex < rowLength) {
        cells = rows[rowIndex];

        index = -1;

        while (++index < cellCount) {
            value = cells[index];

            if (!sizes[index]) {
                sizes[index] = 3;
            }

            size = calculateStringLength(value);

            if (size > sizes[index]) {
                sizes[index] = size;
            }
        }
    }

    rowIndex = -1;

    while (++rowIndex < rowLength) {
        cells = rows[rowIndex];

        index = -1;

        while (++index < cellCount) {
            value = cells[index];

            position = sizes[index] - (calculateStringLength(value) || 0);
            spacing = pad(position);

            if (alignment[index] === RIGHT || alignment[index] === DOT) {
                value = spacing + value;
            } else if (alignment[index] !== CENTER) {
                value = value + spacing;
            } else {
                position = position / 2;

                if (position % 1 === 0) {
                    before = position;
                    after = position;
                } else {
                    before = position + 0.5;
                    after = position - 0.5;
                }

                value = pad(before) + value + pad(after);
            }

            cells[index] = value;
        }

        rows[rowIndex] = cells.join(delimiter);
    }

    if (settings.rule !== false) {
        index = -1;
        rule = [];

        while (++index < cellCount) {
            align = alignment[index];

            /*
             * When `align` is left, don't add colons.
             */

            value = align === RIGHT || align === NULL ? DASH : COLON;
            value += pad(sizes[index] - 2, DASH);
            value += align !== LEFT && align !== NULL ? COLON : DASH;

            rule[index] = value;
        }

        rows.splice(1, 0, rule.join(delimiter));
    }

    return start + rows.join(end + NEW_LINE + start) + end;
}

/*
 * Expose `markdownTable`.
 */

module.exports = markdownTable;
