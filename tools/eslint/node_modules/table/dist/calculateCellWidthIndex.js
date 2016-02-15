'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _map2 = require('lodash/map');

var _map3 = _interopRequireDefault(_map2);

var _stringWidth = require('string-width');

var _stringWidth2 = _interopRequireDefault(_stringWidth);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Calculates width of each cell contents.
 *
 * @param {string[]} cells
 * @return {number[]}
 */

exports.default = function (cells) {
    return (0, _map3.default)(cells, function (value) {
        return (0, _stringWidth2.default)(value);
    });
};

module.exports = exports['default'];
//# sourceMappingURL=calculateCellWidthIndex.js.map
