"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var _default = (0, _iterateJsdoc.default)(({
  report,
  utils
}) => {
  utils.forEachPreferredTag('property', (jsdoc, targetTagName) => {
    if (jsdoc.tag && jsdoc.name === '') {
      report(`There must be an identifier after @${targetTagName} ${jsdoc.type === '' ? 'type' : 'tag'}.`, null, jsdoc);
    }
  });
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: 'Requires that all function `@property` tags have names.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-require-property-name'
    },
    type: 'suggestion'
  }
});

exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=requirePropertyName.js.map