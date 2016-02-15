'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _ceil2 = require('lodash/ceil');

var _ceil3 = _interopRequireDefault(_ceil2);

var _isInteger2 = require('lodash/isInteger');

var _isInteger3 = _interopRequireDefault(_isInteger2);

var _isString2 = require('lodash/isString');

var _isString3 = _interopRequireDefault(_isString2);

var _stringWidth = require('string-width');

var _stringWidth2 = _interopRequireDefault(_stringWidth);

var _wrapWord = require('./wrapWord');

var _wrapWord2 = _interopRequireDefault(_wrapWord);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {string} value
 * @param {number} columnWidth
 * @param {boolean} useWrapWord
 * @returns {number}
 */

exports.default = function (value, columnWidth) {
    var useWrapWord = arguments.length <= 2 || arguments[2] === undefined ? false : arguments[2];

    if (!(0, _isString3.default)(value)) {
        throw new Error('Value must be a string.');
    }

    if (!(0, _isInteger3.default)(columnWidth)) {
        throw new Error('Column width must be an integer.');
    }

    if (columnWidth < 1) {
        throw new Error('Column width must be greater than 0.');
    }

    if (useWrapWord) {
        return (0, _wrapWord2.default)(value, columnWidth).length;
    }

    return (0, _ceil3.default)((0, _stringWidth2.default)(value) / columnWidth);
};

module.exports = exports['default'];
//# sourceMappingURL=calculateCellHeight.js.map
