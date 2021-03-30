"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _lodash = _interopRequireDefault(require("lodash.clonedeep"));

var _calculateMaximumColumnWidthIndex = _interopRequireDefault(require("./calculateMaximumColumnWidthIndex"));

var _getBorderCharacters = _interopRequireDefault(require("./getBorderCharacters"));

var _validateConfig = _interopRequireDefault(require("./validateConfig"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Merges user provided border characters with the default border ("honeywell") characters.
 *
 * @param {object} border
 * @returns {object}
 */
const makeBorder = (border = {}) => {
  return Object.assign({}, (0, _getBorderCharacters.default)('honeywell'), border);
};
/**
 * Creates a configuration for every column using default
 * values for the missing configuration properties.
 *
 * @param {Array[]} rows
 * @param {object} columns
 * @param {object} columnDefault
 * @returns {object}
 */


const makeColumns = (rows, columns = {}, columnDefault = {}) => {
  const maximumColumnWidthIndex = (0, _calculateMaximumColumnWidthIndex.default)(rows);

  for (let index = 0; index < rows[0].length; index++) {
    if (typeof columns[index] === 'undefined') {
      columns[index] = {};
    }

    columns[index] = Object.assign({
      alignment: 'left',
      paddingLeft: 1,
      paddingRight: 1,
      truncate: Number.POSITIVE_INFINITY,
      width: maximumColumnWidthIndex[index],
      wrapWord: false
    }, columnDefault, columns[index]);
  }

  return columns;
};
/**
 * Makes a new configuration object out of the userConfig object
 * using default values for the missing configuration properties.
 *
 * @param {Array[]} rows
 * @param {object} userConfig
 * @returns {object}
 */


const makeConfig = (rows, userConfig = {}) => {
  (0, _validateConfig.default)('config.json', userConfig);
  const config = (0, _lodash.default)(userConfig);
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