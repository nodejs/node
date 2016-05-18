'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _map2 = require('lodash/map');

var _map3 = _interopRequireDefault(_map2);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Casts all cell values to a string.
 *
 * @param {table~row[]} rows
 * @return {table~row[]}
 */

exports.default = function (rows) {
    return (0, _map3.default)(rows, function (cells) {
        return (0, _map3.default)(cells, String);
    });
};

module.exports = exports['default'];
//# sourceMappingURL=stringifyTableData.js.map
