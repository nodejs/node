'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _map2 = require('lodash/map');

var _map3 = _interopRequireDefault(_map2);

var _alignString = require('./alignString');

var _alignString2 = _interopRequireDefault(_alignString);

var _stringWidth = require('string-width');

var _stringWidth2 = _interopRequireDefault(_stringWidth);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {table~row[]} rows
 * @param {Object} config
 * @return {table~row[]}
 */

exports.default = function (rows, config) {
    return (0, _map3.default)(rows, function (cells) {
        return (0, _map3.default)(cells, function (value, index1) {
            var column = undefined;

            column = config.columns[index1];

            if ((0, _stringWidth2.default)(value) === column.width) {
                return value;
            } else {
                return (0, _alignString2.default)(value, column.width, column.alignment);
            }
        });
    });
};

module.exports = exports['default'];
//# sourceMappingURL=alignTableData.js.map
