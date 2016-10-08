'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _ajv = require('ajv');

var _ajv2 = _interopRequireDefault(_ajv);

var _ajvKeywords = require('ajv-keywords');

var _ajvKeywords2 = _interopRequireDefault(_ajvKeywords);

var _config = require('./schemas/config.json');

var _config2 = _interopRequireDefault(_config);

var _streamConfig = require('./schemas/streamConfig.json');

var _streamConfig2 = _interopRequireDefault(_streamConfig);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const ajv = new _ajv2.default({
  allErrors: true
});

(0, _ajvKeywords2.default)(ajv, 'typeof');

ajv.addSchema(_config2.default);
ajv.addSchema(_streamConfig2.default);

/**
 * @param {string} schemaId
 * @param {formatData~config} config
 * @returns {undefined}
 */

exports.default = function (schemaId) {
  let config = arguments.length <= 1 || arguments[1] === undefined ? {} : arguments[1];

  if (!ajv.validate(schemaId, config)) {
    const errors = ajv.errors.map(error => {
      return {
        dataPath: error.dataPath,
        message: error.message,
        params: error.params,
        schemaPath: error.schemaPath
      };
    });

    /* eslint-disable no-console */
    console.log('config', config);
    console.log('errors', errors);
    /* eslint-enable no-console */

    throw new Error('Invalid config.');
  }
};

module.exports = exports['default'];