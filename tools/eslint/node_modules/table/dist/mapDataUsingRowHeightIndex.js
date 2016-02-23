'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _flatten2 = require('lodash/flatten');

var _flatten3 = _interopRequireDefault(_flatten2);

var _forEach2 = require('lodash/forEach');

var _forEach3 = _interopRequireDefault(_forEach2);

var _fill2 = require('lodash/fill');

var _fill3 = _interopRequireDefault(_fill2);

var _map2 = require('lodash/map');

var _map3 = _interopRequireDefault(_map2);

var _wrapString = require('./wrapString');

var _wrapString2 = _interopRequireDefault(_wrapString);

var _wrapWord = require('./wrapWord');

var _wrapWord2 = _interopRequireDefault(_wrapWord);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {Array} unmappedRows
 * @param {number[]} rowHeightIndex
 * @param {Object} config
 * @return {Array}
 */

exports.default = function (unmappedRows, rowHeightIndex, config) {
    var mappedRows = undefined,
        tableWidth = undefined;

    tableWidth = unmappedRows[0].length;

    // console.log('unmappedRows', unmappedRows, 'rowHeightIndex', rowHeightIndex, 'config', config, 'tableWidth', tableWidth);

    mappedRows = (0, _map3.default)(unmappedRows, function (cells, index0) {
        var rowHeight = undefined;

        rowHeight = (0, _map3.default)(Array(rowHeightIndex[index0]), function () {
            return (0, _fill3.default)(Array(tableWidth), '');
        });

        // console.log('rowHeight', rowHeight);

        // rowHeight
        //     [{row index within rowSaw; index2}]
        //     [{cell index within a virtual row; index1}]

        (0, _forEach3.default)(cells, function (value, index1) {
            var chunkedValue = undefined;

            if (config.columns[index1].wrapWord) {
                chunkedValue = (0, _wrapWord2.default)(value, config.columns[index1].width);
            } else {
                chunkedValue = (0, _wrapString2.default)(value, config.columns[index1].width);
            }

            // console.log('chunkedValue', chunkedValue.length, 'rowHeight', rowHeight.length);

            (0, _forEach3.default)(chunkedValue, function (part, index2) {
                // console.log(rowHeight[index2]);

                rowHeight[index2][index1] = part;
            });
        });

        return rowHeight;
    });

    return (0, _flatten3.default)(mappedRows);
};

module.exports = exports['default'];
//# sourceMappingURL=mapDataUsingRowHeightIndex.js.map
