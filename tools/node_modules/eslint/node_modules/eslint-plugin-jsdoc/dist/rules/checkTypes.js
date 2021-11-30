"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _jsdocTypePrattParser = require("jsdoc-type-pratt-parser");

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const strictNativeTypes = ['undefined', 'null', 'boolean', 'number', 'bigint', 'string', 'symbol', 'object', 'Array', 'Function', 'Date', 'RegExp'];

const adjustNames = (type, preferred, isGenericMatch, nodeName, node, parentNode) => {
  let ret = preferred;

  if (isGenericMatch) {
    if (preferred === '[]') {
      parentNode.meta.brackets = 'square';
      parentNode.meta.dot = false;
      ret = 'Array';
    } else {
      const dotBracketEnd = preferred.match(/\.(?:<>)?$/u);

      if (dotBracketEnd) {
        parentNode.meta.brackets = 'angle';
        parentNode.meta.dot = true;
        ret = preferred.slice(0, -dotBracketEnd[0].length);
      } else {
        const bracketEnd = preferred.endsWith('<>');

        if (bracketEnd) {
          parentNode.meta.brackets = 'angle';
          parentNode.meta.dot = false;
          ret = preferred.slice(0, -2);
        } else if (parentNode.meta.brackets === 'square' && (nodeName === '[]' || nodeName === 'Array')) {
          parentNode.meta.brackets = 'angle';
          parentNode.meta.dot = false;
        }
      }
    }
  } else if (type === 'JsdocTypeAny') {
    node.type = 'JsdocTypeName';
  }

  node.value = ret.replace(/(?:\.|<>|\.<>|\[\])$/u, ''); // For bare pseudo-types like `<>`

  if (!ret) {
    node.value = nodeName;
  }
};

var _default = (0, _iterateJsdoc.default)(({
  jsdocNode,
  sourceCode,
  report,
  utils,
  settings,
  context
}) => {
  const jsdocTagsWithPossibleType = utils.filterTags(tag => {
    return utils.tagMightHaveTypePosition(tag.tag);
  });
  const {
    preferredTypes,
    structuredTags,
    mode
  } = settings;
  const {
    noDefaults,
    unifyParentAndChildTypeChecks,
    exemptTagContexts = []
  } = context.options[0] || {};

  const getPreferredTypeInfo = (_type, nodeName, parentNode, property) => {
    let hasMatchingPreferredType;
    let isGenericMatch;
    let typeName = nodeName;

    if (Object.keys(preferredTypes).length) {
      const isNameOfGeneric = parentNode !== undefined && parentNode.type === 'JsdocTypeGeneric' && property === 'left';

      if (unifyParentAndChildTypeChecks || isNameOfGeneric) {
        var _parentNode$meta, _parentNode$meta2;

        const brackets = parentNode === null || parentNode === void 0 ? void 0 : (_parentNode$meta = parentNode.meta) === null || _parentNode$meta === void 0 ? void 0 : _parentNode$meta.brackets;
        const dot = parentNode === null || parentNode === void 0 ? void 0 : (_parentNode$meta2 = parentNode.meta) === null || _parentNode$meta2 === void 0 ? void 0 : _parentNode$meta2.dot;

        if (brackets === 'angle') {
          const checkPostFixes = dot ? ['.', '.<>'] : ['<>'];
          isGenericMatch = checkPostFixes.some(checkPostFix => {
            if ((preferredTypes === null || preferredTypes === void 0 ? void 0 : preferredTypes[nodeName + checkPostFix]) !== undefined) {
              typeName += checkPostFix;
              return true;
            }

            return false;
          });
        }

        if (!isGenericMatch && property) {
          const checkPostFixes = dot ? ['.', '.<>'] : [brackets === 'angle' ? '<>' : '[]'];
          isGenericMatch = checkPostFixes.some(checkPostFix => {
            if ((preferredTypes === null || preferredTypes === void 0 ? void 0 : preferredTypes[checkPostFix]) !== undefined) {
              typeName = checkPostFix;
              return true;
            }

            return false;
          });
        }
      }

      const directNameMatch = (preferredTypes === null || preferredTypes === void 0 ? void 0 : preferredTypes[nodeName]) !== undefined && !Object.values(preferredTypes).includes(nodeName);
      const unifiedSyntaxParentMatch = property && directNameMatch && unifyParentAndChildTypeChecks;
      isGenericMatch = isGenericMatch || unifiedSyntaxParentMatch;
      hasMatchingPreferredType = isGenericMatch || directNameMatch && !property;
    }

    return [hasMatchingPreferredType, typeName, isGenericMatch];
  };

  for (const jsdocTag of jsdocTagsWithPossibleType) {
    const invalidTypes = [];
    let typeAst;

    try {
      typeAst = mode === 'permissive' ? (0, _jsdocTypePrattParser.tryParse)(jsdocTag.type) : (0, _jsdocTypePrattParser.parse)(jsdocTag.type, mode);
    } catch {
      continue;
    }

    const tagName = jsdocTag.tag;
    (0, _jsdocTypePrattParser.traverse)(typeAst, (node, parentNode, property) => {
      const {
        type,
        value
      } = node;

      if (!['JsdocTypeName', 'JsdocTypeAny'].includes(type)) {
        return;
      }

      let nodeName = type === 'JsdocTypeAny' ? '*' : value;
      const [hasMatchingPreferredType, typeName, isGenericMatch] = getPreferredTypeInfo(type, nodeName, parentNode, property);
      let preferred;
      let types;

      if (hasMatchingPreferredType) {
        const preferredSetting = preferredTypes[typeName];
        nodeName = typeName === '[]' ? typeName : nodeName;

        if (!preferredSetting) {
          invalidTypes.push([nodeName]);
        } else if (typeof preferredSetting === 'string') {
          preferred = preferredSetting;
          invalidTypes.push([nodeName, preferred]);
        } else if (typeof preferredSetting === 'object') {
          preferred = preferredSetting === null || preferredSetting === void 0 ? void 0 : preferredSetting.replacement;
          invalidTypes.push([nodeName, preferred, preferredSetting === null || preferredSetting === void 0 ? void 0 : preferredSetting.message]);
        } else {
          utils.reportSettings('Invalid `settings.jsdoc.preferredTypes`. Values must be falsy, a string, or an object.');
          return;
        }
      } else if (Object.entries(structuredTags).some(([tag, {
        type: typs
      }]) => {
        types = typs;
        return tag === tagName && Array.isArray(types) && !types.includes(nodeName);
      })) {
        invalidTypes.push([nodeName, types]);
      } else if (!noDefaults && type === 'JsdocTypeName') {
        for (const strictNativeType of strictNativeTypes) {
          if (strictNativeType === 'object' && mode === 'typescript') {
            continue;
          }

          if (strictNativeType.toLowerCase() === nodeName.toLowerCase() && strictNativeType !== nodeName && ( // Don't report if user has own map for a strict native type
          !preferredTypes || (preferredTypes === null || preferredTypes === void 0 ? void 0 : preferredTypes[strictNativeType]) === undefined)) {
            preferred = strictNativeType;
            invalidTypes.push([nodeName, preferred]);
            break;
          }
        }
      } // For fixer


      if (preferred) {
        adjustNames(type, preferred, isGenericMatch, nodeName, node, parentNode);
      }
    });

    if (invalidTypes.length) {
      const fixedType = (0, _jsdocTypePrattParser.stringify)(typeAst);

      const fix = fixer => {
        return fixer.replaceText(jsdocNode, sourceCode.getText(jsdocNode).replace(`{${jsdocTag.type}}`, `{${fixedType}}`));
      };

      for (const [badType, preferredType = '', message] of invalidTypes) {
        const tagValue = jsdocTag.name ? ` "${jsdocTag.name}"` : '';

        if (exemptTagContexts.some(({
          tag,
          types
        }) => {
          return tag === tagName && (types === true || types.includes(jsdocTag.type));
        })) {
          continue;
        }

        report(message || `Invalid JSDoc @${tagName}${tagValue} type "${badType}"` + (preferredType ? '; ' : '.') + (preferredType ? `prefer: ${JSON.stringify(preferredType)}.` : ''), preferredType ? fix : null, jsdocTag, message ? {
          tagName,
          tagValue
        } : null);
      }
    }
  }
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: 'Reports invalid types.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-check-types'
    },
    fixable: 'code',
    schema: [{
      additionalProperties: false,
      properties: {
        exemptTagContexts: {
          items: {
            additionalProperties: false,
            properties: {
              tag: {
                type: 'string'
              },
              types: {
                oneOf: [{
                  type: 'boolean'
                }, {
                  items: {
                    type: 'string'
                  },
                  type: 'array'
                }]
              }
            },
            type: 'object'
          },
          type: 'array'
        },
        noDefaults: {
          type: 'boolean'
        },
        unifyParentAndChildTypeChecks: {
          type: 'boolean'
        }
      },
      type: 'object'
    }],
    type: 'suggestion'
  }
});

exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=checkTypes.js.map