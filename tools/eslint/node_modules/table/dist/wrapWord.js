'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _trim2 = require('lodash/trim');

var _trim3 = _interopRequireDefault(_trim2);

var _sliceAnsi = require('slice-ansi');

var _sliceAnsi2 = _interopRequireDefault(_sliceAnsi);

var _stringWidth = require('string-width');

var _stringWidth2 = _interopRequireDefault(_stringWidth);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {string} input
 * @param {number} size
 * @returns {Array}
 */

exports.default = function (input, size) {
    var chunk = undefined,
        chunks = undefined,
        re = undefined,
        subject = undefined;

    subject = input;

    chunks = [];

    // https://regex101.com/r/gY5kZ1/1
    re = new RegExp('(^.{1,' + size + '}(\\s+|$))|(^.{1,' + (size - 1) + '}(\\\\|/|_|\\.|,|;|\-))');

    do {
        chunk = subject.match(re);

        // console.log('chunk', chunk, re);

        if (chunk) {
            chunk = chunk[0];

            subject = (0, _sliceAnsi2.default)(subject, (0, _stringWidth2.default)(chunk));

            chunk = (0, _trim3.default)(chunk);
        } else {
            chunk = (0, _sliceAnsi2.default)(subject, 0, size);
            subject = (0, _sliceAnsi2.default)(subject, size);
        }

        chunks.push(chunk);
    } while ((0, _stringWidth2.default)(subject));

    return chunks;
};

module.exports = exports['default'];
//# sourceMappingURL=wrapWord.js.map
