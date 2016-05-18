'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _forEach2 = require('lodash/forEach');

var _forEach3 = _interopRequireDefault(_forEach2);

var _fill2 = require('lodash/fill');

var _fill3 = _interopRequireDefault(_fill2);

var _calculateCellWidthIndex = require('./calculateCellWidthIndex');

var _calculateCellWidthIndex2 = _interopRequireDefault(_calculateCellWidthIndex);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Produces an array of values that describe the largest value length (width) in every column.
 *
 * @param {Array[]} rows
 * @return {number[]}
 */

exports.default = function (rows) {
    var columns = undefined;

    if (!rows[0]) {
        throw new Error('Dataset must have at least one row.');
    }

    columns = (0, _fill3.default)(Array(rows[0].length), 0);

    (0, _forEach3.default)(rows, function (row) {
        var columnWidthIndex = undefined;

        columnWidthIndex = (0, _calculateCellWidthIndex2.default)(row);

        (0, _forEach3.default)(columnWidthIndex, function (valueWidth, index0) {
            if (columns[index0] < valueWidth) {
                columns[index0] = valueWidth;
            }
        });
    });

    return columns;
};

module.exports = exports['default'];
//# sourceMappingURL=calculateMaximumColumnWidthIndex.js.map
