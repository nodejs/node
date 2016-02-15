'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _max2 = require('lodash/max');

var _max3 = _interopRequireDefault(_max2);

var _isBoolean2 = require('lodash/isBoolean');

var _isBoolean3 = _interopRequireDefault(_isBoolean2);

var _isNumber2 = require('lodash/isNumber');

var _isNumber3 = _interopRequireDefault(_isNumber2);

var _fill2 = require('lodash/fill');

var _fill3 = _interopRequireDefault(_fill2);

var _forEach2 = require('lodash/forEach');

var _forEach3 = _interopRequireDefault(_forEach2);

var _calculateCellHeight = require('./calculateCellHeight');

var _calculateCellHeight2 = _interopRequireDefault(_calculateCellHeight);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Calculates the vertical row span index.
 *
 * @param {Array[]} rows
 * @param {Object} config
 * @return {number[]}
 */

exports.default = function (rows, config) {
    var rowSpanIndex = undefined,
        tableWidth = undefined;

    tableWidth = rows[0].length;

    rowSpanIndex = [];

    (0, _forEach3.default)(rows, function (cells) {
        var cellHeightIndex = undefined;

        cellHeightIndex = (0, _fill3.default)(Array(tableWidth), 1);

        (0, _forEach3.default)(cells, function (value, index1) {
            if (!(0, _isNumber3.default)(config.columns[index1].width)) {
                throw new Error('column[index].width must be a number.');
            }

            if (!(0, _isBoolean3.default)(config.columns[index1].wrapWord)) {
                throw new Error('column[index].wrapWord must be a boolean.');
            }

            cellHeightIndex[index1] = (0, _calculateCellHeight2.default)(value, config.columns[index1].width, config.columns[index1].wrapWord);
        });

        rowSpanIndex.push((0, _max3.default)(cellHeightIndex));
    });

    return rowSpanIndex;
};

module.exports = exports['default'];
//# sourceMappingURL=calculateRowHeightIndex.js.map
