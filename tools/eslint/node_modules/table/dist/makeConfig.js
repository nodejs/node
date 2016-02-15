'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _cloneDeep2 = require('lodash/cloneDeep');

var _cloneDeep3 = _interopRequireDefault(_cloneDeep2);

var _isUndefined2 = require('lodash/isUndefined');

var _isUndefined3 = _interopRequireDefault(_isUndefined2);

var _times2 = require('lodash/times');

var _times3 = _interopRequireDefault(_times2);

var _assign2 = require('lodash/assign');

var _assign3 = _interopRequireDefault(_assign2);

var _getBorderCharacters = require('./getBorderCharacters');

var _getBorderCharacters2 = _interopRequireDefault(_getBorderCharacters);

var _validateConfig = require('./validateConfig');

var _validateConfig2 = _interopRequireDefault(_validateConfig);

var _calculateMaximumColumnWidthIndex = require('./calculateMaximumColumnWidthIndex');

var _calculateMaximumColumnWidthIndex2 = _interopRequireDefault(_calculateMaximumColumnWidthIndex);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var makeBorder = undefined,
    makeColumns = undefined;

/**
 * Merges user provided border characters with the default border ("honeywell") characters.
 *
 * @param {Object} border
 * @returns {Object}
 */
makeBorder = function makeBorder() {
    var border = arguments.length <= 0 || arguments[0] === undefined ? {} : arguments[0];

    return (0, _assign3.default)({}, (0, _getBorderCharacters2.default)('honeywell'), border);
};

/**
 * Creates a configuration for every column using default
 * values for the missing configuration properties.
 *
 * @param {Array[]} rows
 * @param {Object} columns
 * @param {Object} columnDefault
 * @returns {Object}
 */
makeColumns = function makeColumns(rows) {
    var columns = arguments.length <= 1 || arguments[1] === undefined ? {} : arguments[1];
    var columnDefault = arguments.length <= 2 || arguments[2] === undefined ? {} : arguments[2];

    var maximumColumnWidthIndex = undefined;

    maximumColumnWidthIndex = (0, _calculateMaximumColumnWidthIndex2.default)(rows);

    (0, _times3.default)(rows[0].length, function (index) {
        if ((0, _isUndefined3.default)(columns[index])) {
            columns[index] = {};
        }

        columns[index] = (0, _assign3.default)({
            alignment: 'left',
            width: maximumColumnWidthIndex[index],
            wrapWord: false,
            truncate: Infinity,
            paddingLeft: 1,
            paddingRight: 1
        }, columnDefault, columns[index]);
    });

    return columns;
};

/**
 * Makes a new configuration object out of the userConfig object
 * using default values for the missing configuration properties.
 *
 * @param {Array[]} rows
 * @param {Object} userConfig
 * @returns {Object}
 */

exports.default = function (rows) {
    var userConfig = arguments.length <= 1 || arguments[1] === undefined ? {} : arguments[1];

    var config = undefined;

    (0, _validateConfig2.default)(userConfig);

    config = (0, _cloneDeep3.default)(userConfig);

    config.border = makeBorder(config.border);
    config.columns = makeColumns(rows, config.columns, config.columnDefault);

    if (!config.drawHorizontalLine) {
        /**
         * @returns {boolean}
         */
        config.drawHorizontalLine = function () {
            return true;
        };
    }

    return config;
};

module.exports = exports['default'];
//# sourceMappingURL=makeConfig.js.map
