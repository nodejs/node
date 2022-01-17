"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _jsdoccomment = require("@es-joy/jsdoccomment");

var _exportParser = _interopRequireDefault(require("../exportParser"));

var _iterateJsdoc = require("../iterateJsdoc");

var _jsdocUtils = _interopRequireDefault(require("../jsdocUtils"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const OPTIONS_SCHEMA = {
  additionalProperties: false,
  properties: {
    checkConstructors: {
      default: true,
      type: 'boolean'
    },
    checkGetters: {
      anyOf: [{
        type: 'boolean'
      }, {
        enum: ['no-setter'],
        type: 'string'
      }],
      default: true
    },
    checkSetters: {
      anyOf: [{
        type: 'boolean'
      }, {
        enum: ['no-getter'],
        type: 'string'
      }],
      default: true
    },
    contexts: {
      items: {
        anyOf: [{
          type: 'string'
        }, {
          additionalProperties: false,
          properties: {
            context: {
              type: 'string'
            },
            inlineCommentBlock: {
              type: 'boolean'
            }
          },
          type: 'object'
        }]
      },
      type: 'array'
    },
    enableFixer: {
      default: true,
      type: 'boolean'
    },
    exemptEmptyConstructors: {
      default: false,
      type: 'boolean'
    },
    exemptEmptyFunctions: {
      default: false,
      type: 'boolean'
    },
    fixerMessage: {
      default: '',
      type: 'string'
    },
    publicOnly: {
      oneOf: [{
        default: false,
        type: 'boolean'
      }, {
        additionalProperties: false,
        default: {},
        properties: {
          ancestorsOnly: {
            type: 'boolean'
          },
          cjs: {
            type: 'boolean'
          },
          esm: {
            type: 'boolean'
          },
          window: {
            type: 'boolean'
          }
        },
        type: 'object'
      }]
    },
    require: {
      additionalProperties: false,
      default: {},
      properties: {
        ArrowFunctionExpression: {
          default: false,
          type: 'boolean'
        },
        ClassDeclaration: {
          default: false,
          type: 'boolean'
        },
        ClassExpression: {
          default: false,
          type: 'boolean'
        },
        FunctionDeclaration: {
          default: true,
          type: 'boolean'
        },
        FunctionExpression: {
          default: false,
          type: 'boolean'
        },
        MethodDefinition: {
          default: false,
          type: 'boolean'
        }
      },
      type: 'object'
    }
  },
  type: 'object'
};

const getOption = (context, baseObject, option, key) => {
  if (context.options[0] && option in context.options[0] && ( // Todo: boolean shouldn't be returning property, but tests currently require
  typeof context.options[0][option] === 'boolean' || key in context.options[0][option])) {
    return context.options[0][option][key];
  }

  return baseObject.properties[key].default;
};

const getOptions = context => {
  const {
    publicOnly,
    contexts = [],
    exemptEmptyConstructors = true,
    exemptEmptyFunctions = false,
    enableFixer = true,
    fixerMessage = ''
  } = context.options[0] || {};
  return {
    contexts,
    enableFixer,
    exemptEmptyConstructors,
    exemptEmptyFunctions,
    fixerMessage,
    publicOnly: (baseObj => {
      if (!publicOnly) {
        return false;
      }

      const properties = {};

      for (const prop of Object.keys(baseObj.properties)) {
        const opt = getOption(context, baseObj, 'publicOnly', prop);
        properties[prop] = opt;
      }

      return properties;
    })(OPTIONS_SCHEMA.properties.publicOnly.oneOf[1]),
    require: (baseObj => {
      const properties = {};

      for (const prop of Object.keys(baseObj.properties)) {
        const opt = getOption(context, baseObj, 'require', prop);
        properties[prop] = opt;
      }

      return properties;
    })(OPTIONS_SCHEMA.properties.require)
  };
};

var _default = {
  create(context) {
    const sourceCode = context.getSourceCode();
    const settings = (0, _iterateJsdoc.getSettings)(context);

    if (!settings) {
      return {};
    }

    const {
      require: requireOption,
      contexts,
      publicOnly,
      exemptEmptyFunctions,
      exemptEmptyConstructors,
      enableFixer,
      fixerMessage
    } = getOptions(context);

    const checkJsDoc = (info, handler, node) => {
      const jsDocNode = (0, _jsdoccomment.getJSDocComment)(sourceCode, node, settings);

      if (jsDocNode) {
        return;
      } // For those who have options configured against ANY constructors (or
      //  setters or getters) being reported


      if (_jsdocUtils.default.exemptSpeciaMethods({
        tags: []
      }, node, context, [OPTIONS_SCHEMA])) {
        return;
      }

      if ( // Avoid reporting param-less, return-less functions (when
      //  `exemptEmptyFunctions` option is set)
      exemptEmptyFunctions && info.isFunctionContext || // Avoid reporting  param-less, return-less constructor methods (when
      //  `exemptEmptyConstructors` option is set)
      exemptEmptyConstructors && _jsdocUtils.default.isConstructor(node)) {
        const functionParameterNames = _jsdocUtils.default.getFunctionParameterNames(node);

        if (!functionParameterNames.length && !_jsdocUtils.default.hasReturnValue(node)) {
          return;
        }
      }

      const fix = fixer => {
        // Default to one line break if the `minLines`/`maxLines` settings allow
        const lines = settings.minLines === 0 && settings.maxLines >= 1 ? 1 : settings.minLines;
        let baseNode = (0, _jsdoccomment.getReducedASTNode)(node, sourceCode);
        const decorator = (0, _jsdoccomment.getDecorator)(baseNode);

        if (decorator) {
          baseNode = decorator;
        }

        const indent = _jsdocUtils.default.getIndent({
          text: sourceCode.getText(baseNode, baseNode.loc.start.column)
        });

        const {
          inlineCommentBlock
        } = contexts.find(({
          context: ctxt
        }) => {
          return ctxt === node.type;
        }) || {};
        const insertion = (inlineCommentBlock ? `/** ${fixerMessage}` : `/**\n${indent}*${fixerMessage}\n${indent}`) + `*/${'\n'.repeat(lines)}${indent.slice(0, -1)}`;
        return fixer.insertTextBefore(baseNode, insertion);
      };

      const report = () => {
        const loc = {
          end: node.loc.start + 1,
          start: node.loc.start
        };
        context.report({
          fix: enableFixer ? fix : null,
          loc,
          messageId: 'missingJsDoc',
          node
        });
      };

      if (publicOnly) {
        var _publicOnly$ancestors, _publicOnly$esm, _publicOnly$cjs, _publicOnly$window;

        const opt = {
          ancestorsOnly: Boolean((_publicOnly$ancestors = publicOnly === null || publicOnly === void 0 ? void 0 : publicOnly.ancestorsOnly) !== null && _publicOnly$ancestors !== void 0 ? _publicOnly$ancestors : false),
          esm: Boolean((_publicOnly$esm = publicOnly === null || publicOnly === void 0 ? void 0 : publicOnly.esm) !== null && _publicOnly$esm !== void 0 ? _publicOnly$esm : true),
          initModuleExports: Boolean((_publicOnly$cjs = publicOnly === null || publicOnly === void 0 ? void 0 : publicOnly.cjs) !== null && _publicOnly$cjs !== void 0 ? _publicOnly$cjs : true),
          initWindow: Boolean((_publicOnly$window = publicOnly === null || publicOnly === void 0 ? void 0 : publicOnly.window) !== null && _publicOnly$window !== void 0 ? _publicOnly$window : false)
        };

        const exported = _exportParser.default.isUncommentedExport(node, sourceCode, opt, settings);

        if (exported) {
          report();
        }
      } else {
        report();
      }
    };

    const hasOption = prop => {
      return requireOption[prop] || contexts.some(ctxt => {
        return typeof ctxt === 'object' ? ctxt.context === prop : ctxt === prop;
      });
    };

    return { ..._jsdocUtils.default.getContextObject(_jsdocUtils.default.enforcedContexts(context, []), checkJsDoc),

      ArrowFunctionExpression(node) {
        if (!hasOption('ArrowFunctionExpression')) {
          return;
        }

        if (['VariableDeclarator', 'AssignmentExpression', 'ExportDefaultDeclaration'].includes(node.parent.type) || ['Property', 'ObjectProperty', 'ClassProperty', 'PropertyDefinition'].includes(node.parent.type) && node === node.parent.value) {
          checkJsDoc({
            isFunctionContext: true
          }, null, node);
        }
      },

      ClassDeclaration(node) {
        if (!hasOption('ClassDeclaration')) {
          return;
        }

        checkJsDoc({
          isFunctionContext: false
        }, null, node);
      },

      ClassExpression(node) {
        if (!hasOption('ClassExpression')) {
          return;
        }

        checkJsDoc({
          isFunctionContext: false
        }, null, node);
      },

      FunctionDeclaration(node) {
        if (!hasOption('FunctionDeclaration')) {
          return;
        }

        checkJsDoc({
          isFunctionContext: true
        }, null, node);
      },

      FunctionExpression(node) {
        if (hasOption('MethodDefinition') && node.parent.type === 'MethodDefinition') {
          checkJsDoc({
            isFunctionContext: true
          }, null, node);
          return;
        }

        if (!hasOption('FunctionExpression')) {
          return;
        }

        if (['VariableDeclarator', 'AssignmentExpression', 'ExportDefaultDeclaration'].includes(node.parent.type) || ['Property', 'ObjectProperty', 'ClassProperty', 'PropertyDefinition'].includes(node.parent.type) && node === node.parent.value) {
          checkJsDoc({
            isFunctionContext: true
          }, null, node);
        }
      }

    };
  },

  meta: {
    docs: {
      category: 'Stylistic Issues',
      description: 'Require JSDoc comments',
      recommended: 'true',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-require-jsdoc'
    },
    fixable: 'code',
    messages: {
      missingJsDoc: 'Missing JSDoc comment.'
    },
    schema: [OPTIONS_SCHEMA],
    type: 'suggestion'
  }
};
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=requireJsdoc.js.map