"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const setDefaults = state => {
  if (!state.selectorMap) {
    state.selectorMap = {};
  }
};

const incrementSelector = (state, selector, comment) => {
  if (!state.selectorMap[selector]) {
    state.selectorMap[selector] = {};
  }

  if (!state.selectorMap[selector][comment]) {
    state.selectorMap[selector][comment] = 0;
  }

  state.selectorMap[selector][comment]++;
};

var _default = (0, _iterateJsdoc.default)(({
  info: {
    selector,
    comment
  },
  state
}) => {
  setDefaults(state);
  incrementSelector(state, selector, comment);
}, {
  contextSelected: true,

  exit({
    context,
    state
  }) {
    if (!context.options.length) {
      context.report({
        loc: {
          start: {
            column: 1,
            line: 1
          }
        },
        message: 'Rule `no-missing-syntax` is missing a `context` option.'
      });
      return;
    }

    setDefaults(state);
    const {
      contexts
    } = context.options[0]; // Report when MISSING

    contexts.some(cntxt => {
      var _cntxt$context, _cntxt$comment, _cntxt$minimum;

      const contextStr = typeof cntxt === 'object' ? (_cntxt$context = cntxt.context) !== null && _cntxt$context !== void 0 ? _cntxt$context : 'any' : cntxt;
      const comment = (_cntxt$comment = cntxt === null || cntxt === void 0 ? void 0 : cntxt.comment) !== null && _cntxt$comment !== void 0 ? _cntxt$comment : '';
      const contextKey = contextStr === 'any' ? undefined : contextStr;

      if ((!state.selectorMap[contextKey] || !state.selectorMap[contextKey][comment] || state.selectorMap[contextKey][comment] < ((_cntxt$minimum = cntxt === null || cntxt === void 0 ? void 0 : cntxt.minimum) !== null && _cntxt$minimum !== void 0 ? _cntxt$minimum : 1)) && (contextStr !== 'any' || Object.values(state.selectorMap).every(cmmnt => {
        var _cntxt$minimum2;

        return !cmmnt[comment] || cmmnt[comment] < ((_cntxt$minimum2 = cntxt === null || cntxt === void 0 ? void 0 : cntxt.minimum) !== null && _cntxt$minimum2 !== void 0 ? _cntxt$minimum2 : 1);
      }))) {
        var _cntxt$message;

        const message = (_cntxt$message = cntxt === null || cntxt === void 0 ? void 0 : cntxt.message) !== null && _cntxt$message !== void 0 ? _cntxt$message : 'Syntax is required: {{context}}' + (comment ? ' with {{comment}}' : '');
        context.report({
          data: {
            comment,
            context: contextStr
          },
          loc: {
            end: {
              line: 1
            },
            start: {
              line: 1
            }
          },
          message
        });
        return true;
      }

      return false;
    });
  },

  meta: {
    docs: {
      description: 'Reports when certain comment structures are always expected.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-no-missing-syntax'
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
                },
                minimum: {
                  type: 'integer'
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
//# sourceMappingURL=noMissingSyntax.js.map