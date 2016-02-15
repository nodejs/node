'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});

var _isNumber2 = require('lodash/isNumber');

var _isNumber3 = _interopRequireDefault(_isNumber2);

var _isString2 = require('lodash/isString');

var _isString3 = _interopRequireDefault(_isString2);

var _floor2 = require('lodash/floor');

var _floor3 = _interopRequireDefault(_floor2);

var _repeat2 = require('lodash/repeat');

var _repeat3 = _interopRequireDefault(_repeat2);

var _stringWidth = require('string-width');

var _stringWidth2 = _interopRequireDefault(_stringWidth);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var alignCenter = undefined,
    alignLeft = undefined,
    alignRight = undefined,
    alignments = undefined;

alignments = ['left', 'right', 'center'];

/**
 * @param {string} subject
 * @param {number} width
 * @returns {string}
 */
alignLeft = function alignLeft(subject, width) {
    return subject + (0, _repeat3.default)(' ', width);
};

/**
 * @param {string} subject
 * @param {number} width
 * @returns {string}
 */
alignRight = function alignRight(subject, width) {
    return (0, _repeat3.default)(' ', width) + subject;
};

/**
 * @param {string} subject
 * @param {number} width
 * @returns {string}
 */
alignCenter = function alignCenter(subject, width) {
    var halfWidth = undefined;

    halfWidth = width / 2;

    if (halfWidth % 2 === 0) {
        return (0, _repeat3.default)(' ', halfWidth) + subject + (0, _repeat3.default)(' ', halfWidth);
    } else {
        halfWidth = (0, _floor3.default)(halfWidth);

        return (0, _repeat3.default)(' ', halfWidth) + subject + (0, _repeat3.default)(' ', halfWidth + 1);
    }
};

/**
 * Pads a string to the left and/or right to position the subject
 * text in a desired alignment within a container.
 *
 * @param {string} subject
 * @param {number} containerWidth
 * @param {string} alignment One of the valid options (left, right, center).
 * @returns {string}
 */

exports.default = function (subject, containerWidth, alignment) {
    var availableWidth = undefined,
        subjectWidth = undefined;

    if (!(0, _isString3.default)(subject)) {
        throw new Error('Subject parameter value must be a string.');
    }

    if (!(0, _isNumber3.default)(containerWidth)) {
        throw new Error('Container width parameter value must be a number.');
    }

    subjectWidth = (0, _stringWidth2.default)(subject);

    if (subjectWidth > containerWidth) {
        // console.log('subjectWidth', subjectWidth, 'containerWidth', containerWidth, 'subject', subject);

        throw new Error('Subject parameter value width cannot be greater than the container width.');
    }

    if (!(0, _isString3.default)(alignment)) {
        throw new Error('Alignment parameter value must be a string.');
    }

    if (alignments.indexOf(alignment) === -1) {
        throw new Error('Alignment parameter value must be a known alignment parameter value (left, right, center).');
    }

    if (subjectWidth === 0) {
        return (0, _repeat3.default)(' ', containerWidth);
    }

    availableWidth = containerWidth - subjectWidth;

    if (alignment === 'left') {
        return alignLeft(subject, availableWidth);
    }

    if (alignment === 'right') {
        return alignRight(subject, availableWidth);
    }

    return alignCenter(subject, availableWidth);
};

module.exports = exports['default'];
//# sourceMappingURL=alignString.js.map
