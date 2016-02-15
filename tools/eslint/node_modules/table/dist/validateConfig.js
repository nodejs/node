'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _config = require('./schemas/config.json');

var _config2 = _interopRequireDefault(_config);

var _tv = require('tv4');

var _tv2 = _interopRequireDefault(_tv);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @typedef {string} cell
 */

/**
 * @typedef {cell[]} validateData~column
 */

/**
 * @param {formatData~config} config
 * @returns {undefined}
 */

exports.default = function () {
    var config = arguments.length <= 0 || arguments[0] === undefined ? {} : arguments[0];

    var result = undefined;

    result = _tv2.default.validateResult(config, _config2.default);

    if (!result.valid) {
        /* eslint-disable no-console */
        console.log('config', config);
        console.log('error', {
            message: result.error.message,
            params: result.error.params,
            dataPath: result.error.dataPath,
            schemaPath: result.error.schemaPath
        });
        /* eslint-enable no-console */

        throw new Error('Invalid config.');
    }
};

module.exports = exports['default'];
//# sourceMappingURL=validateConfig.js.map
