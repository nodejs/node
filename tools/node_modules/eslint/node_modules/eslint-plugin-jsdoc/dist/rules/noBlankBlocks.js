"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc.js"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
var _default = exports.default = (0, _iterateJsdoc.default)(({
  context,
  jsdoc,
  utils
}) => {
  if (jsdoc.tags.length) {
    return;
  }
  const {
    description,
    lastDescriptionLine
  } = utils.getDescription();
  if (description.trim()) {
    return;
  }
  const {
    enableFixer
  } = context.options[0] || {};
  utils.reportJSDoc('No empty blocks', {
    line: lastDescriptionLine
  }, enableFixer ? () => {
    jsdoc.source.splice(0, jsdoc.source.length);
  } : null);
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: 'Removes empty blocks with nothing but possibly line breaks',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc/blob/main/docs/rules/no-blank-blocks.md#repos-sticky-header'
    },
    fixable: 'code',
    schema: [{
      additionalProperties: false,
      properties: {
        enableFixer: {
          type: 'boolean'
        }
      }
    }],
    type: 'suggestion'
  }
});
module.exports = exports.default;
//# sourceMappingURL=noBlankBlocks.js.map