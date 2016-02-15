'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _forEach2 = require('lodash/forEach');

var _forEach3 = _interopRequireDefault(_forEach2);

var _isArray2 = require('lodash/isArray');

var _isArray3 = _interopRequireDefault(_isArray2);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @typedef {string} cell
 */

/**
 * @typedef {cell[]} validateData~column
 */

/**
 * @param {column[]} rows
 * @returns {undefined}
 */

exports.default = function (rows) {
    var columnNumber = undefined;

    if (!(0, _isArray3.default)(rows)) {
        throw new Error('Table data must be an array.');
    }

    if (rows.length === 0) {
        throw new Error('Table must define at least one row.');
    }

    if (rows[0].length === 0) {
        throw new Error('Table must define at least one column.');
    }

    columnNumber = rows[0].length;

    (0, _forEach3.default)(rows, function (cells) {
        if (!(0, _isArray3.default)(cells)) {
            throw new Error('Table row data must be an array.');
        }

        if (cells.length !== columnNumber) {
            throw new Error('Table must have a consistent number of cells.');
        }

        // @todo Make an exception for newline characters.
        // @see https://github.com/gajus/table/issues/9
        (0, _forEach3.default)(cells, function (cell) {
            if (/[\x01-\x1A]/.test(cell)) {
                throw new Error('Table data must not contain control characters.');
            }
        });
    });
};

module.exports = exports['default'];
//# sourceMappingURL=validateTableData.js.map
