'use strict';

Object.defineProperty(exports, "__esModule", {
    value: true
});
exports.drawBorderBottom = exports.drawBorderJoin = exports.drawBorderTop = exports.drawBorder = undefined;

var _repeat2 = require('lodash/repeat');

var _repeat3 = _interopRequireDefault(_repeat2);

var _map2 = require('lodash/map');

var _map3 = _interopRequireDefault(_map2);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var drawBorder = undefined,
    drawBorderBottom = undefined,
    drawBorderJoin = undefined,
    drawBorderTop = undefined;

/**
 * @typedef drawBorder~parts
 * @property {string} left
 * @property {string} right
 * @property {string} body
 * @property {string} join
 */

/**
 * @param {number[]} columnSizeIndex
 * @param {drawBorder~parts} parts
 * @returns {string}
 */
exports.drawBorder = drawBorder = function drawBorder(columnSizeIndex, parts) {
    var columns = undefined;

    columns = (0, _map3.default)(columnSizeIndex, function (size) {
        return (0, _repeat3.default)(parts.body, size);
    });

    columns = columns.join(parts.join);

    return parts.left + columns + parts.right + '\n';
};

/**
 * @typedef drawBorderTop~parts
 * @property {string} topLeft
 * @property {string} topRight
 * @property {string} topBody
 * @property {string} topJoin
 */

/**
 * @param {number[]} columnSizeIndex
 * @param {drawBorderTop~parts} parts
 * @return {string}
 */
exports.drawBorderTop = drawBorderTop = function drawBorderTop(columnSizeIndex, parts) {
    return drawBorder(columnSizeIndex, {
        left: parts.topLeft,
        right: parts.topRight,
        body: parts.topBody,
        join: parts.topJoin
    });
};

/**
 * @typedef drawBorderJoin~parts
 * @property {string} joinLeft
 * @property {string} joinRight
 * @property {string} joinBody
 * @property {string} joinJoin
 */

/**
 * @param {number[]} columnSizeIndex
 * @param {drawBorderJoin~parts} parts
 * @returns {string}
 */
exports.drawBorderJoin = drawBorderJoin = function drawBorderJoin(columnSizeIndex, parts) {
    return drawBorder(columnSizeIndex, {
        left: parts.joinLeft,
        right: parts.joinRight,
        body: parts.joinBody,
        join: parts.joinJoin
    });
};

/**
 * @typedef drawBorderBottom~parts
 * @property {string} topLeft
 * @property {string} topRight
 * @property {string} topBody
 * @property {string} topJoin
 */

/**
 * @param {number[]} columnSizeIndex
 * @param {drawBorderBottom~parts} parts
 * @returns {string}
 */
exports.drawBorderBottom = drawBorderBottom = function drawBorderBottom(columnSizeIndex, parts) {
    return drawBorder(columnSizeIndex, {
        left: parts.bottomLeft,
        right: parts.bottomRight,
        body: parts.bottomBody,
        join: parts.bottomJoin
    });
};

exports.drawBorder = drawBorder;
exports.drawBorderTop = drawBorderTop;
exports.drawBorderJoin = drawBorderJoin;
exports.drawBorderBottom = drawBorderBottom;
//# sourceMappingURL=drawBorder.js.map
