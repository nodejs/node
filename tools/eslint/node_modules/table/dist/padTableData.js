'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _repeat2 = require('lodash/repeat');

var _repeat3 = _interopRequireDefault(_repeat2);

var _map2 = require('lodash/map');

var _map3 = _interopRequireDefault(_map2);

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

            return (0, _repeat3.default)(' ', column.paddingLeft) + value + (0, _repeat3.default)(' ', column.paddingRight);
        });
    });
};

module.exports = exports['default'];
//# sourceMappingURL=padTableData.js.map
