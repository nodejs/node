"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
const defaultEmptyTags = new Set(['abstract', 'async', 'generator', 'global', 'hideconstructor', 'ignore', 'inner', 'instance', 'override', 'readonly',
// jsdoc doesn't use this form in its docs, but allow for compatibility with
//  TypeScript which allows and Closure which requires
'inheritDoc',
// jsdoc doesn't use but allow for TypeScript
'internal']);
const emptyIfNotClosure = new Set(['package', 'private', 'protected', 'public', 'static',
// Closure doesn't allow with this casing
'inheritdoc']);
const emptyIfClosure = new Set(['interface']);
var _default = (0, _iterateJsdoc.default)(({
  settings,
  jsdoc,
  utils
}) => {
  const emptyTags = utils.filterTags(({
    tag: tagName
  }) => {
    return defaultEmptyTags.has(tagName) || utils.hasOptionTag(tagName) && jsdoc.tags.some(({
      tag
    }) => {
      return tag === tagName;
    }) || settings.mode === 'closure' && emptyIfClosure.has(tagName) || settings.mode !== 'closure' && emptyIfNotClosure.has(tagName);
  });
  for (const tag of emptyTags) {
    const content = tag.name || tag.description || tag.type;
    if (content.trim()) {
      const fix = () => {
        utils.setTag(tag);
      };
      utils.reportJSDoc(`@${tag.tag} should be empty.`, tag, fix, true);
    }
  }
}, {
  checkInternal: true,
  checkPrivate: true,
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: 'Expects specific tags to be empty of any content.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-empty-tags'
    },
    fixable: 'code',
    schema: [{
      additionalProperties: false,
      properties: {
        tags: {
          items: {
            type: 'string'
          },
          type: 'array'
        }
      },
      type: 'object'
    }],
    type: 'suggestion'
  }
});
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=emptyTags.js.map