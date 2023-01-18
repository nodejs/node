"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _jsdoccomment = require("@es-joy/jsdoccomment");
var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
const strictNativeTypes = ['undefined', 'null', 'boolean', 'number', 'bigint', 'string', 'symbol', 'object', 'Array', 'Function', 'Date', 'RegExp'];

/**
 * Adjusts the parent type node `meta` for generic matches (or type node
 * `type` for `JsdocTypeAny`) and sets the type node `value`.
 *
 * @param {string} type The actual type
 * @param {string} preferred The preferred type
 * @param {boolean} isGenericMatch
 * @param {string} typeNodeName
 * @param {import('jsdoc-type-pratt-parser/dist/src/index.d.ts').NonTerminalResult} node
 * @param {import('jsdoc-type-pratt-parser/dist/src/index.d.ts').NonTerminalResult} parentNode
 * @returns {void}
 */
const adjustNames = (type, preferred, isGenericMatch, typeNodeName, node, parentNode) => {
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
        var _parentNode$meta;
        const bracketEnd = preferred.endsWith('<>');
        if (bracketEnd) {
          parentNode.meta.brackets = 'angle';
          parentNode.meta.dot = false;
          ret = preferred.slice(0, -2);
        } else if (((_parentNode$meta = parentNode.meta) === null || _parentNode$meta === void 0 ? void 0 : _parentNode$meta.brackets) === 'square' && (typeNodeName === '[]' || typeNodeName === 'Array')) {
          parentNode.meta.brackets = 'angle';
          parentNode.meta.dot = false;
        }
      }
    }
  } else if (type === 'JsdocTypeAny') {
    node.type = 'JsdocTypeName';
  }
  node.value = ret.replace(/(?:\.|<>|\.<>|\[\])$/u, '');

  // For bare pseudo-types like `<>`
  if (!ret) {
    node.value = typeNodeName;
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
    preferredTypes: preferredTypesOriginal,
    structuredTags,
    mode
  } = settings;
  const injectObjectPreferredTypes = !('Object' in preferredTypesOriginal || 'object' in preferredTypesOriginal || 'object.<>' in preferredTypesOriginal || 'Object.<>' in preferredTypesOriginal || 'object<>' in preferredTypesOriginal);
  const preferredTypes = {
    ...(injectObjectPreferredTypes ? {
      Object: 'object',
      'object.<>': 'Object<>',
      'Object.<>': 'Object<>',
      'object<>': 'Object<>'
    } : {}),
    ...preferredTypesOriginal
  };
  const {
    noDefaults,
    unifyParentAndChildTypeChecks,
    exemptTagContexts = []
  } = context.options[0] || {};

  /**
   * Gets information about the preferred type: whether there is a matching
   * preferred type, what the type is, and whether it is a match to a generic.
   *
   * @param {string} _type Not currently in use
   * @param {string} typeNodeName
   * @param {import('jsdoc-type-pratt-parser/dist/src/index.d.ts').NonTerminalResult} parentNode
   * @param {string} property
   * @returns {[hasMatchingPreferredType: boolean, typeName: string, isGenericMatch: boolean]}
   */
  const getPreferredTypeInfo = (_type, typeNodeName, parentNode, property) => {
    let hasMatchingPreferredType = false;
    let isGenericMatch = false;
    let typeName = typeNodeName;
    const isNameOfGeneric = parentNode !== undefined && parentNode.type === 'JsdocTypeGeneric' && property === 'left';
    if (unifyParentAndChildTypeChecks || isNameOfGeneric) {
      var _parentNode$meta2, _parentNode$meta3;
      const brackets = parentNode === null || parentNode === void 0 ? void 0 : (_parentNode$meta2 = parentNode.meta) === null || _parentNode$meta2 === void 0 ? void 0 : _parentNode$meta2.brackets;
      const dot = parentNode === null || parentNode === void 0 ? void 0 : (_parentNode$meta3 = parentNode.meta) === null || _parentNode$meta3 === void 0 ? void 0 : _parentNode$meta3.dot;
      if (brackets === 'angle') {
        const checkPostFixes = dot ? ['.', '.<>'] : ['<>'];
        isGenericMatch = checkPostFixes.some(checkPostFix => {
          if ((preferredTypes === null || preferredTypes === void 0 ? void 0 : preferredTypes[typeNodeName + checkPostFix]) !== undefined) {
            typeName += checkPostFix;
            return true;
          }
          return false;
        });
      }
      if (!isGenericMatch && property && parentNode.type === 'JsdocTypeGeneric') {
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
    const directNameMatch = (preferredTypes === null || preferredTypes === void 0 ? void 0 : preferredTypes[typeNodeName]) !== undefined && !Object.values(preferredTypes).includes(typeNodeName);
    const unifiedSyntaxParentMatch = property && directNameMatch && unifyParentAndChildTypeChecks;
    isGenericMatch = isGenericMatch || unifiedSyntaxParentMatch;
    hasMatchingPreferredType = isGenericMatch || directNameMatch && !property;
    return [hasMatchingPreferredType, typeName, isGenericMatch];
  };

  /**
   * Iterates strict types to see if any should be added to `invalidTypes` (and
   * the the relevant strict type returned as the new preferred type).
   *
   * @param {string} typeNodeName
   * @param {string} preferred
   * @param {import('jsdoc-type-pratt-parser/dist/src/index.d.ts').NonTerminalResult} parentNode
   * @param {string[]} invalidTypes
   * @returns {string} The `preferred` type string, optionally changed
   */
  const checkNativeTypes = (typeNodeName, preferred, parentNode, invalidTypes) => {
    let changedPreferred = preferred;
    for (const strictNativeType of strictNativeTypes) {
      var _parentNode$elements, _parentNode$left, _parentNode$left2;
      if (strictNativeType === 'object' && (
      // This is not set to remap with exact type match (e.g.,
      //   `object: 'Object'`), so can ignore (including if circular)
      !(preferredTypes !== null && preferredTypes !== void 0 && preferredTypes[typeNodeName]) ||
      // Although present on `preferredTypes` for remapping, this is a
      //   parent object without a parent match (and not
      //   `unifyParentAndChildTypeChecks`) and we don't want
      //   `object<>` given TypeScript issue https://github.com/microsoft/TypeScript/issues/20555
      parentNode !== null && parentNode !== void 0 && (_parentNode$elements = parentNode.elements) !== null && _parentNode$elements !== void 0 && _parentNode$elements.length && (parentNode === null || parentNode === void 0 ? void 0 : (_parentNode$left = parentNode.left) === null || _parentNode$left === void 0 ? void 0 : _parentNode$left.type) === 'JsdocTypeName' && (parentNode === null || parentNode === void 0 ? void 0 : (_parentNode$left2 = parentNode.left) === null || _parentNode$left2 === void 0 ? void 0 : _parentNode$left2.value) === 'Object')) {
        continue;
      }
      if (strictNativeType !== typeNodeName && strictNativeType.toLowerCase() === typeNodeName.toLowerCase() && (
      // Don't report if user has own map for a strict native type
      !preferredTypes || (preferredTypes === null || preferredTypes === void 0 ? void 0 : preferredTypes[strictNativeType]) === undefined)) {
        changedPreferred = strictNativeType;
        invalidTypes.push([typeNodeName, changedPreferred]);
        break;
      }
    }
    return changedPreferred;
  };

  /**
   * Collect invalid type info.
   *
   * @param {string} type
   * @param {string} value
   * @param {string} tagName
   * @param {string} nameInTag
   * @param {number} idx
   * @param {string} property
   * @param {import('jsdoc-type-pratt-parser/dist/src/index.d.ts').NonTerminalResult} node
   * @param {import('jsdoc-type-pratt-parser/dist/src/index.d.ts').NonTerminalResult} parentNode
   * @param {string[]} invalidTypes
   * @returns {void}
   */
  const getInvalidTypes = (type, value, tagName, nameInTag, idx, property, node, parentNode, invalidTypes) => {
    let typeNodeName = type === 'JsdocTypeAny' ? '*' : value;
    const [hasMatchingPreferredType, typeName, isGenericMatch] = getPreferredTypeInfo(type, typeNodeName, parentNode, property);
    let preferred;
    let types;
    if (hasMatchingPreferredType) {
      const preferredSetting = preferredTypes[typeName];
      typeNodeName = typeName === '[]' ? typeName : typeNodeName;
      if (!preferredSetting) {
        invalidTypes.push([typeNodeName]);
      } else if (typeof preferredSetting === 'string') {
        preferred = preferredSetting;
        invalidTypes.push([typeNodeName, preferred]);
      } else if (preferredSetting && typeof preferredSetting === 'object') {
        const nextItem = preferredSetting.skipRootChecking && jsdocTagsWithPossibleType[idx + 1];
        if (!nextItem || !nextItem.name.startsWith(`${nameInTag}.`)) {
          preferred = preferredSetting.replacement;
          invalidTypes.push([typeNodeName, preferred, preferredSetting.message]);
        }
      } else {
        utils.reportSettings('Invalid `settings.jsdoc.preferredTypes`. Values must be falsy, a string, or an object.');
        return;
      }
    } else if (Object.entries(structuredTags).some(([tag, {
      type: typs
    }]) => {
      types = typs;
      return tag === tagName && Array.isArray(types) && !types.includes(typeNodeName);
    })) {
      invalidTypes.push([typeNodeName, types]);
    } else if (!noDefaults && type === 'JsdocTypeName') {
      preferred = checkNativeTypes(typeNodeName, preferred, parentNode, invalidTypes);
    }

    // For fixer
    if (preferred) {
      adjustNames(type, preferred, isGenericMatch, typeNodeName, node, parentNode);
    }
  };
  for (const [idx, jsdocTag] of jsdocTagsWithPossibleType.entries()) {
    const invalidTypes = [];
    let typeAst;
    try {
      typeAst = mode === 'permissive' ? (0, _jsdoccomment.tryParse)(jsdocTag.type) : (0, _jsdoccomment.parse)(jsdocTag.type, mode);
    } catch {
      continue;
    }
    const {
      tag: tagName,
      name: nameInTag
    } = jsdocTag;
    (0, _jsdoccomment.traverse)(typeAst, (node, parentNode, property) => {
      const {
        type,
        value
      } = node;
      if (!['JsdocTypeName', 'JsdocTypeAny'].includes(type)) {
        return;
      }
      getInvalidTypes(type, value, tagName, nameInTag, idx, property, node, parentNode, invalidTypes);
    });
    if (invalidTypes.length) {
      const fixedType = (0, _jsdoccomment.stringify)(typeAst);

      /**
       * @param {any} fixer The ESLint fixer
       * @returns {string}
       */
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