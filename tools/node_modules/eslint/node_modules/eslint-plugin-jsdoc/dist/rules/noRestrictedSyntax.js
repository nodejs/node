"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _esquery = _interopRequireDefault(require("esquery"));

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var _default = (0, _iterateJsdoc.default)(({
  node,
  context,
  info: {
    comment
  },
  report
}) => {
  var _foundContext$context, _foundContext$message;

  if (!context.options.length) {
    report('Rule `no-restricted-syntax` is missing a `context` option.');
    return;
  }

  const {
    contexts
  } = context.options[0];
  const foundContext = contexts.find(cntxt => {
    return typeof cntxt === 'string' ? _esquery.default.matches(node, _esquery.default.parse(cntxt)) : (!cntxt.context || cntxt.context === 'any' || _esquery.default.matches(node, _esquery.default.parse(cntxt.context))) && comment === cntxt.comment;
  }); // We are not on the *particular* matching context/comment, so don't assume
  //   we need reporting

  if (!foundContext) {
    return;
  }

  const contextStr = typeof foundContext === 'object' ? (_foundContext$context = foundContext.context) !== null && _foundContext$context !== void 0 ? _foundContext$context : 'any' : foundContext;
  const message = (_foundContext$message = foundContext === null || foundContext === void 0 ? void 0 : foundContext.message) !== null && _foundContext$message !== void 0 ? _foundContext$message : 'Syntax is restricted: {{context}}.';
  report(message, null, null, {
    context: contextStr
  });
}, {
  contextSelected: true,
  meta: {
    docs: {
      description: 'Reports when certain comment structures are present.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-no-restricted-syntax'
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
                },
                message: {
                  type: 'string'
                }
              },
              type: 'object'
            }]
          },
          type: 'array'
        }
      },
      required: ['contexts'],
      type: 'object'
    }],
    type: 'suggestion'
  }
});

exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=noRestrictedSyntax.js.map