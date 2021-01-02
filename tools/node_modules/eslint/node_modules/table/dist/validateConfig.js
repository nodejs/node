"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _validators = _interopRequireDefault(require("../dist/validators"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

// eslint-disable-next-line import/default

/**
 * @param {string} schemaId
 * @param {formatData~config} config
 * @returns {undefined}
 */
const validateConfig = (schemaId, config = {}) => {
  const validate = _validators.default[schemaId];

  if (!validate(config)) {
    const errors = validate.errors.map(error => {
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

var _default = validateConfig;
exports.default = _default;
//# sourceMappingURL=validateConfig.js.map