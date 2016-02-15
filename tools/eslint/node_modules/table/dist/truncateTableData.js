'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _truncate2 = require('lodash/truncate');

var _truncate3 = _interopRequireDefault(_truncate2);

var _map2 = require('lodash/map');

var _map3 = _interopRequireDefault(_map2);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @todo Make it work with ASCII content.
 * @param {table~row[]} rows
 * @param {Object} config
 * @return {table~row[]}
 */

exports.default = function (rows, config) {
    return (0, _map3.default)(rows, function (cells) {
        return (0, _map3.default)(cells, function (content, index) {
            return (0, _truncate3.default)(content, {
                length: config.columns[index].truncate
            });
        });
    });
};

module.exports = exports['default'];
//# sourceMappingURL=truncateTableData.js.map
