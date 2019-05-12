"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _lodash = _interopRequireDefault(require("lodash"));

var _getBorderCharacters = _interopRequireDefault(require("./getBorderCharacters"));

var _validateConfig = _interopRequireDefault(require("./validateConfig"));

var _calculateMaximumColumnWidthIndex = _interopRequireDefault(require("./calculateMaximumColumnWidthIndex"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Merges user provided border characters with the default border ("honeywell") characters.
 *
 * @param {Object} border
 * @returns {Object}
 */
const makeBorder = (border = {}) => {
  return Object.assign({}, (0, _getBorderCharacters.default)('honeywell'), border);
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


const makeColumns = (rows, columns = {}, columnDefault = {}) => {
  const maximumColumnWidthIndex = (0, _calculateMaximumColumnWidthIndex.default)(rows);

  _lodash.default.times(rows[0].length, index => {
    if (_lodash.default.isUndefined(columns[index])) {
      columns[index] = {};
    }

    columns[index] = Object.assign({
      alignment: 'left',
      paddingLeft: 1,
      paddingRight: 1,
      truncate: Infinity,
      width: maximumColumnWidthIndex[index],
      wrapWord: false
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


const makeConfig = (rows, userConfig = {}) => {
  (0, _validateConfig.default)('config.json', userConfig);

  const config = _lodash.default.cloneDeep(userConfig);

  config.border = makeBorder(config.border);
  config.columns = makeColumns(rows, config.columns, config.columnDefault);

  if (!config.drawHorizontalLine) {
    /**
         * @returns {boolean}
         */
    config.drawHorizontalLine = () => {
      return true;
    };
  }

  if (config.singleLine === undefined) {
    config.singleLine = false;
  }

  return config;
};

var _default = makeConfig;
exports.default = _default;
//# sourceMappingURL=makeConfig.js.map