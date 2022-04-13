"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const removeType = ({
  tokens
}) => {
  tokens.postTag = '';
  tokens.type = '';
};

var _default = (0, _iterateJsdoc.default)(({
  utils
}) => {
  if (!utils.isIteratingFunction() && !utils.isVirtualFunction()) {
    return;
  }

  const tags = utils.getPresentTags(['param', 'arg', 'argument', 'returns', 'return']);

  for (const tag of tags) {
    if (tag.type) {
      utils.reportJSDoc(`Types are not permitted on @${tag.tag}.`, tag, () => {
        for (const source of tag.source) {
          removeType(source);
        }
      });
    }
  }
}, {
  contextDefaults: true,
  meta: {
    docs: {
      description: 'This rule reports types being used on `@param` or `@returns`.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-no-types'
    },
    fixable: 'code',
    schema: [{
      additionalProperties: false,
      properties: {
        contexts: {
          items: {
            anyOf: [{
              type: 'string'
            }, {
              additionalProperties: false,
              properties: {
                comment: {
                  type: 'string'
                },
                context: {
                  type: 'string'
                }
              },
              type: 'object'
            }]
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
//# sourceMappingURL=noTypes.js.map