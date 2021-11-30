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
  utils.forEachPreferredTag('param', (jsdocParameter, targetTagName) => {
    if (!jsdocParameter.description.trim()) {
      report(`Missing JSDoc @${targetTagName} "${jsdocParameter.name}" description.`, null, jsdocParameter);
    }
  });
}, {
  contextDefaults: true,
  meta: {
    docs: {
      description: 'Requires that each `@param` tag has a `description` value.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-require-param-description'
    },
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
//# sourceMappingURL=requireParamDescription.js.map