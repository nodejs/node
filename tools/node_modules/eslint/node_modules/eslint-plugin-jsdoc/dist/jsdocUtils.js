"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _jsdoccomment = require("@es-joy/jsdoccomment");
var _WarnSettings = _interopRequireDefault(require("./WarnSettings"));
var _getDefaultTagStructureForMode = _interopRequireDefault(require("./getDefaultTagStructureForMode"));
var _tagNames = require("./tagNames");
var _hasReturnValue = require("./utils/hasReturnValue");
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
/* eslint-disable jsdoc/no-undefined-types */

/**
 * @typedef {"jsdoc"|"typescript"|"closure"} ParserMode
 */

let tagStructure;
const setTagStructure = mode => {
  tagStructure = (0, _getDefaultTagStructureForMode.default)(mode);
};

// Given a nested array of property names, reduce them to a single array,
// appending the name of the root element along the way if present.
const flattenRoots = (params, root = '') => {
  let hasRestElement = false;
  let hasPropertyRest = false;
  const rests = [];
  const names = params.reduce((acc, cur) => {
    if (Array.isArray(cur)) {
      let nms;
      if (Array.isArray(cur[1])) {
        nms = cur[1];
      } else {
        if (cur[1].hasRestElement) {
          hasRestElement = true;
        }
        if (cur[1].hasPropertyRest) {
          hasPropertyRest = true;
        }
        nms = cur[1].names;
      }
      const flattened = flattenRoots(nms, root ? `${root}.${cur[0]}` : cur[0]);
      if (flattened.hasRestElement) {
        hasRestElement = true;
      }
      if (flattened.hasPropertyRest) {
        hasPropertyRest = true;
      }
      const inner = [root ? `${root}.${cur[0]}` : cur[0], ...flattened.names].filter(Boolean);
      rests.push(false, ...flattened.rests);
      return acc.concat(inner);
    }
    if (typeof cur === 'object') {
      if (cur.isRestProperty) {
        hasPropertyRest = true;
        rests.push(true);
      } else {
        rests.push(false);
      }
      if (cur.restElement) {
        hasRestElement = true;
      }
      acc.push(root ? `${root}.${cur.name}` : cur.name);
    } else if (typeof cur !== 'undefined') {
      rests.push(false);
      acc.push(root ? `${root}.${cur}` : cur);
    }
    return acc;
  }, []);
  return {
    hasPropertyRest,
    hasRestElement,
    names,
    rests
  };
};

/**
 * @param {object} propSignature
 * @returns {undefined|Array|string}
 */
const getPropertiesFromPropertySignature = propSignature => {
  if (propSignature.type === 'TSIndexSignature' || propSignature.type === 'TSConstructSignatureDeclaration' || propSignature.type === 'TSCallSignatureDeclaration') {
    return undefined;
  }
  if (propSignature.typeAnnotation && propSignature.typeAnnotation.typeAnnotation.type === 'TSTypeLiteral') {
    return [propSignature.key.name, propSignature.typeAnnotation.typeAnnotation.members.map(member => {
      return getPropertiesFromPropertySignature(member);
    })];
  }
  return propSignature.key.name;
};

/**
 * @param {object} functionNode
 * @param {boolean} checkDefaultObjects
 * @returns {Array}
 */
const getFunctionParameterNames = (functionNode, checkDefaultObjects) => {
  var _functionNode$value;
  // eslint-disable-next-line complexity
  const getParamName = (param, isProperty) => {
    var _param$left, _param$left3;
    const hasLeftTypeAnnotation = 'left' in param && 'typeAnnotation' in param.left;
    if ('typeAnnotation' in param || hasLeftTypeAnnotation) {
      const typeAnnotation = hasLeftTypeAnnotation ? param.left.typeAnnotation : param.typeAnnotation;
      if (typeAnnotation.typeAnnotation.type === 'TSTypeLiteral') {
        const propertyNames = typeAnnotation.typeAnnotation.members.map(member => {
          return getPropertiesFromPropertySignature(member);
        });
        const flattened = {
          ...flattenRoots(propertyNames),
          annotationParamName: param.name
        };
        const hasLeftName = 'left' in param && 'name' in param.left;
        if ('name' in param || hasLeftName) {
          return [hasLeftName ? param.left.name : param.name, flattened];
        }
        return [undefined, flattened];
      }
    }
    if ('name' in param) {
      return param.name;
    }
    if ('left' in param && 'name' in param.left) {
      return param.left.name;
    }
    if (param.type === 'ObjectPattern' || ((_param$left = param.left) === null || _param$left === void 0 ? void 0 : _param$left.type) === 'ObjectPattern') {
      var _param$left2;
      const properties = param.properties || ((_param$left2 = param.left) === null || _param$left2 === void 0 ? void 0 : _param$left2.properties);
      const roots = properties.map(prop => {
        return getParamName(prop, true);
      });
      return [undefined, flattenRoots(roots)];
    }
    if (param.type === 'Property') {
      // eslint-disable-next-line default-case
      switch (param.value.type) {
        case 'ArrayPattern':
          return [param.key.name, param.value.elements.map((prop, idx) => {
            return {
              name: idx,
              restElement: prop.type === 'RestElement'
            };
          })];
        case 'ObjectPattern':
          return [param.key.name, param.value.properties.map(prop => {
            return getParamName(prop, isProperty);
          })];
        case 'AssignmentPattern':
          {
            // eslint-disable-next-line default-case
            switch (param.value.left.type) {
              case 'Identifier':
                // Default parameter
                if (checkDefaultObjects && param.value.right.type === 'ObjectExpression') {
                  return [param.key.name, param.value.right.properties.map(prop => {
                    return getParamName(prop, isProperty);
                  })];
                }
                break;
              case 'ObjectPattern':
                return [param.key.name, param.value.left.properties.map(prop => {
                  return getParamName(prop, isProperty);
                })];
              case 'ArrayPattern':
                return [param.key.name, param.value.left.elements.map((prop, idx) => {
                  return {
                    name: idx,
                    restElement: prop.type === 'RestElement'
                  };
                })];
            }
          }
      }
      switch (param.key.type) {
        case 'Identifier':
          return param.key.name;

        // The key of an object could also be a string or number
        case 'Literal':
          return param.key.raw ||
          // istanbul ignore next -- `raw` may not be present in all parsers
          param.key.value;

        // case 'MemberExpression':
        default:
          // Todo: We should really create a structure (and a corresponding
          //   option analogous to `checkRestProperty`) which allows for
          //   (and optionally requires) dynamic properties to have a single
          //   line of documentation
          return undefined;
      }
    }
    if (param.type === 'ArrayPattern' || ((_param$left3 = param.left) === null || _param$left3 === void 0 ? void 0 : _param$left3.type) === 'ArrayPattern') {
      var _param$left4;
      const elements = param.elements || ((_param$left4 = param.left) === null || _param$left4 === void 0 ? void 0 : _param$left4.elements);
      const roots = elements.map((prop, idx) => {
        return {
          name: `"${idx}"`,
          restElement: (prop === null || prop === void 0 ? void 0 : prop.type) === 'RestElement'
        };
      });
      return [undefined, flattenRoots(roots)];
    }
    if (['RestElement', 'ExperimentalRestProperty'].includes(param.type)) {
      return {
        isRestProperty: isProperty,
        name: param.argument.name,
        restElement: true
      };
    }
    if (param.type === 'TSParameterProperty') {
      return getParamName(param.parameter, true);
    }
    throw new Error(`Unsupported function signature format: \`${param.type}\`.`);
  };
  if (!functionNode) {
    return [];
  }
  return (functionNode.params || ((_functionNode$value = functionNode.value) === null || _functionNode$value === void 0 ? void 0 : _functionNode$value.params) || []).map(param => {
    return getParamName(param);
  });
};

/**
 * @param {Node} functionNode
 * @returns {Integer}
 */
const hasParams = functionNode => {
  // Should also check `functionNode.value.params` if supporting `MethodDefinition`
  return functionNode.params.length;
};

/**
 * Gets all names of the target type, including those that refer to a path, e.g.
 * "@param foo; @param foo.bar".
 *
 * @param {object} jsdoc
 * @param {string} targetTagName
 * @returns {Array<object>}
 */
const getJsdocTagsDeep = (jsdoc, targetTagName) => {
  const ret = [];
  for (const [idx, {
    name,
    tag,
    type
  }] of jsdoc.tags.entries()) {
    if (tag !== targetTagName) {
      continue;
    }
    ret.push({
      idx,
      name,
      type
    });
  }
  return ret;
};
const modeWarnSettings = (0, _WarnSettings.default)();

/**
 * @param {string} mode
 * @param context
 */
const getTagNamesForMode = (mode, context) => {
  switch (mode) {
    case 'jsdoc':
      return _tagNames.jsdocTags;
    case 'typescript':
      return _tagNames.typeScriptTags;
    case 'closure':
    case 'permissive':
      return _tagNames.closureTags;
    default:
      if (!modeWarnSettings.hasBeenWarned(context, 'mode')) {
        context.report({
          loc: {
            start: {
              column: 1,
              line: 1
            }
          },
          message: `Unrecognized value \`${mode}\` for \`settings.jsdoc.mode\`.`
        });
        modeWarnSettings.markSettingAsWarned(context, 'mode');
      }

      // We'll avoid breaking too many other rules
      return _tagNames.jsdocTags;
  }
};

/**
 * @param context
 * @param {ParserMode} mode
 * @param {string} name
 * @param {object} tagPreference
 * @returns {string|object}
 */
const getPreferredTagName = (context, mode, name, tagPreference = {}) => {
  var _Object$entries$find;
  const prefValues = Object.values(tagPreference);
  if (prefValues.includes(name) || prefValues.some(prefVal => {
    return prefVal && typeof prefVal === 'object' && prefVal.replacement === name;
  })) {
    return name;
  }

  // Allow keys to have a 'tag ' prefix to avoid upstream bug in ESLint
  // that disallows keys that conflict with Object.prototype,
  // e.g. 'tag constructor' for 'constructor':
  // https://github.com/eslint/eslint/issues/13289
  // https://github.com/gajus/eslint-plugin-jsdoc/issues/537
  const tagPreferenceFixed = Object.fromEntries(Object.entries(tagPreference).map(([key, value]) => {
    return [key.replace(/^tag /u, ''), value];
  }));
  if (Object.prototype.hasOwnProperty.call(tagPreferenceFixed, name)) {
    return tagPreferenceFixed[name];
  }
  const tagNames = getTagNamesForMode(mode, context);
  const preferredTagName = (_Object$entries$find = Object.entries(tagNames).find(([, aliases]) => {
    return aliases.includes(name);
  })) === null || _Object$entries$find === void 0 ? void 0 : _Object$entries$find[0];
  if (preferredTagName) {
    return preferredTagName;
  }
  return name;
};

/**
 * @param context
 * @param {ParserMode} mode
 * @param {string} name
 * @param {Array} definedTags
 * @returns {boolean}
 */
const isValidTag = (context, mode, name, definedTags) => {
  const tagNames = getTagNamesForMode(mode, context);
  const validTagNames = Object.keys(tagNames).concat(Object.values(tagNames).flat());
  const additionalTags = definedTags;
  const allTags = validTagNames.concat(additionalTags);
  return allTags.includes(name);
};

/**
 * @param {object} jsdoc
 * @param {string} targetTagName
 * @returns {boolean}
 */
const hasTag = (jsdoc, targetTagName) => {
  const targetTagLower = targetTagName.toLowerCase();
  return jsdoc.tags.some(doc => {
    return doc.tag.toLowerCase() === targetTagLower;
  });
};

/**
 * @param {object} jsdoc
 * @param {Array} targetTagNames
 * @returns {boolean}
 */
const hasATag = (jsdoc, targetTagNames) => {
  return targetTagNames.some(targetTagName => {
    return hasTag(jsdoc, targetTagName);
  });
};

/**
 * Checks if the JSDoc comment declares a defined type.
 *
 * @param {JsDocTag} tag
 *   the tag which should be checked.
 * @param {"jsdoc"|"closure"|"typescript"} mode
 * @returns {boolean}
 *   true in case a defined type is declared; otherwise false.
 */
const hasDefinedTypeTag = (tag, mode) => {
  // The function should not continue in the event the type is not defined...
  if (typeof tag === 'undefined' || tag === null) {
    return false;
  }

  // .. same applies if it declares an `{undefined}` or `{void}` type
  const tagType = tag.type.trim();

  // Exit early if matching
  if (tagType === 'undefined' || tagType === 'void') {
    return false;
  }
  let parsedTypes;
  try {
    parsedTypes = (0, _jsdoccomment.tryParse)(tagType, mode === 'permissive' ? undefined : [mode]);
  } catch {
    // Ignore
  }
  if (
  // We do not traverse deeply as it could be, e.g., `Promise<void>`
  parsedTypes && parsedTypes.type === 'JsdocTypeUnion' && parsedTypes.elements.find(elem => {
    return elem.type === 'JsdocTypeUndefined' || elem.type === 'JsdocTypeName' && elem.value === 'void';
  })) {
    return false;
  }

  // In any other case, a type is present
  return true;
};

/**
 * @param map
 * @param tag
 * @returns {Map}
 */
const ensureMap = (map, tag) => {
  if (!map.has(tag)) {
    map.set(tag, new Map());
  }
  return map.get(tag);
};

/**
 * @param structuredTags
 * @param tagMap
 */
const overrideTagStructure = (structuredTags, tagMap = tagStructure) => {
  for (const [tag, {
    name,
    type,
    required = []
  }] of Object.entries(structuredTags)) {
    const tagStruct = ensureMap(tagMap, tag);
    tagStruct.set('nameContents', name);
    tagStruct.set('typeAllowed', type);
    const requiredName = required.includes('name');
    if (requiredName && name === false) {
      throw new Error('Cannot add "name" to `require` with the tag\'s `name` set to `false`');
    }
    tagStruct.set('nameRequired', requiredName);
    const requiredType = required.includes('type');
    if (requiredType && type === false) {
      throw new Error('Cannot add "type" to `require` with the tag\'s `type` set to `false`');
    }
    tagStruct.set('typeRequired', requiredType);
    const typeOrNameRequired = required.includes('typeOrNameRequired');
    if (typeOrNameRequired && name === false) {
      throw new Error('Cannot add "typeOrNameRequired" to `require` with the tag\'s `name` set to `false`');
    }
    if (typeOrNameRequired && type === false) {
      throw new Error('Cannot add "typeOrNameRequired" to `require` with the tag\'s `type` set to `false`');
    }
    tagStruct.set('typeOrNameRequired', typeOrNameRequired);
  }
};

/**
 * @param mode
 * @param structuredTags
 * @returns {Map}
 */
const getTagStructureForMode = (mode, structuredTags) => {
  const tagStruct = (0, _getDefaultTagStructureForMode.default)(mode);
  try {
    overrideTagStructure(structuredTags, tagStruct);
  } catch {
    //
  }
  return tagStruct;
};

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const isNamepathDefiningTag = (tag, tagMap = tagStructure) => {
  const tagStruct = ensureMap(tagMap, tag);
  return tagStruct.get('nameContents') === 'namepath-defining';
};

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const tagMustHaveTypePosition = (tag, tagMap = tagStructure) => {
  const tagStruct = ensureMap(tagMap, tag);
  return tagStruct.get('typeRequired');
};

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const tagMightHaveTypePosition = (tag, tagMap = tagStructure) => {
  if (tagMustHaveTypePosition(tag, tagMap)) {
    return true;
  }
  const tagStruct = ensureMap(tagMap, tag);
  const ret = tagStruct.get('typeAllowed');
  return ret === undefined ? true : ret;
};
const namepathTypes = new Set(['namepath-defining', 'namepath-referencing']);

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const tagMightHaveNamePosition = (tag, tagMap = tagStructure) => {
  const tagStruct = ensureMap(tagMap, tag);
  const ret = tagStruct.get('nameContents');
  return ret === undefined ? true : Boolean(ret);
};

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const tagMightHaveNamepath = (tag, tagMap = tagStructure) => {
  const tagStruct = ensureMap(tagMap, tag);
  return namepathTypes.has(tagStruct.get('nameContents'));
};

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const tagMustHaveNamePosition = (tag, tagMap = tagStructure) => {
  const tagStruct = ensureMap(tagMap, tag);
  return tagStruct.get('nameRequired');
};

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const tagMightHaveEitherTypeOrNamePosition = (tag, tagMap) => {
  return tagMightHaveTypePosition(tag, tagMap) || tagMightHaveNamepath(tag, tagMap);
};

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const tagMustHaveEitherTypeOrNamePosition = (tag, tagMap) => {
  const tagStruct = ensureMap(tagMap, tag);
  return tagStruct.get('typeOrNameRequired');
};

/**
 * @param tag
 * @param {Map} tagMap
 * @returns {boolean}
 */
const tagMissingRequiredTypeOrNamepath = (tag, tagMap = tagStructure) => {
  const mustHaveTypePosition = tagMustHaveTypePosition(tag.tag, tagMap);
  const mightHaveTypePosition = tagMightHaveTypePosition(tag.tag, tagMap);
  const hasTypePosition = mightHaveTypePosition && Boolean(tag.type);
  const hasNameOrNamepathPosition = (tagMustHaveNamePosition(tag.tag, tagMap) || tagMightHaveNamepath(tag.tag, tagMap)) && Boolean(tag.name);
  const mustHaveEither = tagMustHaveEitherTypeOrNamePosition(tag.tag, tagMap);
  const hasEither = tagMightHaveEitherTypeOrNamePosition(tag.tag, tagMap) && (hasTypePosition || hasNameOrNamepathPosition);
  return mustHaveEither && !hasEither && !mustHaveTypePosition;
};

// eslint-disable-next-line complexity
const hasNonFunctionYield = (node, checkYieldReturnValue) => {
  if (!node) {
    return false;
  }
  switch (node.type) {
    case 'BlockStatement':
      {
        return node.body.some(bodyNode => {
          return !['ArrowFunctionExpression', 'FunctionDeclaration', 'FunctionExpression'].includes(bodyNode.type) && hasNonFunctionYield(bodyNode, checkYieldReturnValue);
        });
      }

    // istanbul ignore next -- In Babel?
    case 'OptionalCallExpression':
    case 'CallExpression':
      return node.arguments.some(element => {
        return hasNonFunctionYield(element, checkYieldReturnValue);
      });
    case 'ChainExpression':
    case 'ExpressionStatement':
      {
        return hasNonFunctionYield(node.expression, checkYieldReturnValue);
      }
    case 'LabeledStatement':
    case 'WhileStatement':
    case 'DoWhileStatement':
    case 'ForStatement':
    case 'ForInStatement':
    case 'ForOfStatement':
    case 'WithStatement':
      {
        return hasNonFunctionYield(node.body, checkYieldReturnValue);
      }
    case 'ConditionalExpression':
    case 'IfStatement':
      {
        return hasNonFunctionYield(node.test, checkYieldReturnValue) || hasNonFunctionYield(node.consequent, checkYieldReturnValue) || hasNonFunctionYield(node.alternate, checkYieldReturnValue);
      }
    case 'TryStatement':
      {
        return hasNonFunctionYield(node.block, checkYieldReturnValue) || hasNonFunctionYield(node.handler && node.handler.body, checkYieldReturnValue) || hasNonFunctionYield(node.finalizer, checkYieldReturnValue);
      }
    case 'SwitchStatement':
      {
        return node.cases.some(someCase => {
          return someCase.consequent.some(nde => {
            return hasNonFunctionYield(nde, checkYieldReturnValue);
          });
        });
      }
    case 'ArrayPattern':
    case 'ArrayExpression':
      return node.elements.some(element => {
        return hasNonFunctionYield(element, checkYieldReturnValue);
      });
    case 'AssignmentPattern':
      return hasNonFunctionYield(node.right, checkYieldReturnValue);
    case 'VariableDeclaration':
      {
        return node.declarations.some(nde => {
          return hasNonFunctionYield(nde, checkYieldReturnValue);
        });
      }
    case 'VariableDeclarator':
      {
        return hasNonFunctionYield(node.id, checkYieldReturnValue) || hasNonFunctionYield(node.init, checkYieldReturnValue);
      }
    case 'AssignmentExpression':
    case 'BinaryExpression':
    case 'LogicalExpression':
      {
        return hasNonFunctionYield(node.left, checkYieldReturnValue) || hasNonFunctionYield(node.right, checkYieldReturnValue);
      }

    // Comma
    case 'SequenceExpression':
    case 'TemplateLiteral':
      return node.expressions.some(subExpression => {
        return hasNonFunctionYield(subExpression, checkYieldReturnValue);
      });
    case 'ObjectPattern':
    case 'ObjectExpression':
      return node.properties.some(property => {
        return hasNonFunctionYield(property, checkYieldReturnValue);
      });

    // istanbul ignore next -- In Babel?
    case 'PropertyDefinition':
    /* eslint-disable no-fallthrough */
    // istanbul ignore next -- In Babel?
    case 'ObjectProperty':
    // istanbul ignore next -- In Babel?
    case 'ClassProperty':
    /* eslint-enable no-fallthrough */
    case 'Property':
      return node.computed && hasNonFunctionYield(node.key, checkYieldReturnValue) || hasNonFunctionYield(node.value, checkYieldReturnValue);
    // istanbul ignore next -- In Babel?
    case 'ObjectMethod':
      // istanbul ignore next -- In Babel?
      return node.computed && hasNonFunctionYield(node.key, checkYieldReturnValue) || node.arguments.some(nde => {
        return hasNonFunctionYield(nde, checkYieldReturnValue);
      });
    case 'SpreadElement':
    case 'UnaryExpression':
      return hasNonFunctionYield(node.argument, checkYieldReturnValue);
    case 'TaggedTemplateExpression':
      return hasNonFunctionYield(node.quasi, checkYieldReturnValue);

    // ?.
    // istanbul ignore next -- In Babel?
    case 'OptionalMemberExpression':
    case 'MemberExpression':
      return hasNonFunctionYield(node.object, checkYieldReturnValue) || hasNonFunctionYield(node.property, checkYieldReturnValue);

    // istanbul ignore next -- In Babel?
    case 'Import':
    case 'ImportExpression':
      return hasNonFunctionYield(node.source, checkYieldReturnValue);
    case 'ReturnStatement':
      {
        if (node.argument === null) {
          return false;
        }
        return hasNonFunctionYield(node.argument, checkYieldReturnValue);
      }
    case 'YieldExpression':
      {
        if (checkYieldReturnValue) {
          if (node.parent.type === 'VariableDeclarator') {
            return true;
          }
          return false;
        }

        // void return does not count.
        if (node.argument === null) {
          return false;
        }
        return true;
      }
    default:
      {
        return false;
      }
  }
};

/**
 * Checks if a node has a return statement. Void return does not count.
 *
 * @param {object} node
 * @returns {boolean}
 */
const hasYieldValue = (node, checkYieldReturnValue) => {
  return node.generator && (node.expression || hasNonFunctionYield(node.body, checkYieldReturnValue));
};

/**
 * Checks if a node has a throws statement.
 *
 * @param {object} node
 * @param {boolean} innerFunction
 * @returns {boolean}
 */
// eslint-disable-next-line complexity
const hasThrowValue = (node, innerFunction) => {
  if (!node) {
    return false;
  }

  // There are cases where a function may execute its inner function which
  //   throws, but we're treating functions atomically rather than trying to
  //   follow them
  switch (node.type) {
    case 'FunctionExpression':
    case 'FunctionDeclaration':
    case 'ArrowFunctionExpression':
      {
        return !innerFunction && !node.async && hasThrowValue(node.body, true);
      }
    case 'BlockStatement':
      {
        return node.body.some(bodyNode => {
          return bodyNode.type !== 'FunctionDeclaration' && hasThrowValue(bodyNode);
        });
      }
    case 'LabeledStatement':
    case 'WhileStatement':
    case 'DoWhileStatement':
    case 'ForStatement':
    case 'ForInStatement':
    case 'ForOfStatement':
    case 'WithStatement':
      {
        return hasThrowValue(node.body);
      }
    case 'IfStatement':
      {
        return hasThrowValue(node.consequent) || hasThrowValue(node.alternate);
      }

    // We only consider it to throw an error if the catch or finally blocks throw an error.
    case 'TryStatement':
      {
        return hasThrowValue(node.handler && node.handler.body) || hasThrowValue(node.finalizer);
      }
    case 'SwitchStatement':
      {
        return node.cases.some(someCase => {
          return someCase.consequent.some(nde => {
            return hasThrowValue(nde);
          });
        });
      }
    case 'ThrowStatement':
      {
        return true;
      }
    default:
      {
        return false;
      }
  }
};

/**
 * @param {string} tag
 */
/*
const isInlineTag = (tag) => {
  return /^(@link|@linkcode|@linkplain|@tutorial) /u.test(tag);
};
*/

/**
 * Parses GCC Generic/Template types
 *
 * @see {https://github.com/google/closure-compiler/wiki/Generic-Types}
 * @see {https://www.typescriptlang.org/docs/handbook/jsdoc-supported-types.html#template}
 * @param {JsDocTag} tag
 * @returns {Array<string>}
 */
const parseClosureTemplateTag = tag => {
  return tag.name.split(',').map(type => {
    return type.trim().replace(/^\[(?<name>.*?)=.*\]$/u, '$<name>');
  });
};

/**
 * @typedef {true|string[]} DefaultContexts
 */

/**
 * Checks user option for `contexts` array, defaulting to
 *   contexts designated by the rule. Returns an array of
 *   ESTree AST types, indicating allowable contexts.
 *
 * @param {*} context
 * @param {DefaultContexts} defaultContexts
 * @returns {string[]}
 */
const enforcedContexts = (context, defaultContexts) => {
  const {
    contexts = defaultContexts === true ? ['ArrowFunctionExpression', 'FunctionDeclaration', 'FunctionExpression', 'TSDeclareFunction'] : defaultContexts
  } = context.options[0] || {};
  return contexts;
};

/**
 * @param {string[]} contexts
 * @param {Function} checkJsdoc
 * @param {Function} handler
 */
const getContextObject = (contexts, checkJsdoc, handler) => {
  const properties = {};
  for (const [idx, prop] of contexts.entries()) {
    let property;
    let value;
    if (typeof prop === 'object') {
      const selInfo = {
        lastIndex: idx,
        selector: prop.context
      };
      if (prop.comment) {
        property = prop.context;
        value = checkJsdoc.bind(null, {
          ...selInfo,
          comment: prop.comment
        }, handler.bind(null, prop.comment));
      } else {
        property = prop.context;
        value = checkJsdoc.bind(null, selInfo, null);
      }
    } else {
      const selInfo = {
        lastIndex: idx,
        selector: prop
      };
      property = prop;
      value = checkJsdoc.bind(null, selInfo, null);
    }
    const old = properties[property];
    properties[property] = old ? function (...args) {
      old(...args);
      value(...args);
    } : value;
  }
  return properties;
};
const filterTags = (tags, filter) => {
  return tags.filter(tag => {
    return filter(tag);
  });
};
const tagsWithNamesAndDescriptions = new Set(['param', 'arg', 'argument', 'property', 'prop', 'template',
// These two are parsed by our custom parser as though having a `name`
'returns', 'return']);
const getTagsByType = (context, mode, tags, tagPreference) => {
  const descName = getPreferredTagName(context, mode, 'description', tagPreference);
  const tagsWithoutNames = [];
  const tagsWithNames = filterTags(tags, tag => {
    const {
      tag: tagName
    } = tag;
    const tagWithName = tagsWithNamesAndDescriptions.has(tagName);
    if (!tagWithName && tagName !== descName) {
      tagsWithoutNames.push(tag);
    }
    return tagWithName;
  });
  return {
    tagsWithNames,
    tagsWithoutNames
  };
};
const getIndent = sourceCode => {
  var _sourceCode$text$matc;
  return (((_sourceCode$text$matc = sourceCode.text.match(/^\n*([ \t]+)/u)) === null || _sourceCode$text$matc === void 0 ? void 0 : _sourceCode$text$matc[1]) ?? '') + ' ';
};
const isConstructor = node => {
  var _node$parent;
  return (node === null || node === void 0 ? void 0 : node.type) === 'MethodDefinition' && node.kind === 'constructor' || (node === null || node === void 0 ? void 0 : (_node$parent = node.parent) === null || _node$parent === void 0 ? void 0 : _node$parent.kind) === 'constructor';
};
const isGetter = node => {
  var _node$parent2;
  return node && ((_node$parent2 = node.parent) === null || _node$parent2 === void 0 ? void 0 : _node$parent2.kind) === 'get';
};
const isSetter = node => {
  var _node$parent3;
  return node && ((_node$parent3 = node.parent) === null || _node$parent3 === void 0 ? void 0 : _node$parent3.kind) === 'set';
};
const hasAccessorPair = node => {
  const {
    type,
    kind: sourceKind,
    key: {
      name: sourceName
    }
  } = node;
  const oppositeKind = sourceKind === 'get' ? 'set' : 'get';
  const children = type === 'MethodDefinition' ? 'body' : 'properties';
  return node.parent[children].some(({
    kind,
    key: {
      name
    }
  }) => {
    return kind === oppositeKind && name === sourceName;
  });
};
const exemptSpeciaMethods = (jsdoc, node, context, schema) => {
  const hasSchemaOption = prop => {
    var _context$options$;
    const schemaProperties = schema[0].properties;
    return ((_context$options$ = context.options[0]) === null || _context$options$ === void 0 ? void 0 : _context$options$[prop]) ?? (schemaProperties[prop] && schemaProperties[prop].default);
  };
  const checkGetters = hasSchemaOption('checkGetters');
  const checkSetters = hasSchemaOption('checkSetters');
  return !hasSchemaOption('checkConstructors') && (isConstructor(node) || hasATag(jsdoc, ['class', 'constructor'])) || isGetter(node) && (!checkGetters || checkGetters === 'no-setter' && hasAccessorPair(node.parent)) || isSetter(node) && (!checkSetters || checkSetters === 'no-getter' && hasAccessorPair(node.parent));
};

/**
 * Since path segments may be unquoted (if matching a reserved word,
 * identifier or numeric literal) or single or double quoted, in either
 * the `@param` or in source, we need to strip the quotes to give a fair
 * comparison.
 *
 * @param {string} str
 * @returns {string}
 */
const dropPathSegmentQuotes = str => {
  return str.replace(/\.(['"])(.*)\1/gu, '.$2');
};

/**
 * @param {string} name
 * @returns {(otherPathName: string) => void}
 */
const comparePaths = name => {
  return otherPathName => {
    return otherPathName === name || dropPathSegmentQuotes(otherPathName) === dropPathSegmentQuotes(name);
  };
};

/**
 * @param {string} name
 * @param {string} otherPathName
 * @returns {boolean}
 */
const pathDoesNotBeginWith = (name, otherPathName) => {
  return !name.startsWith(otherPathName) && !dropPathSegmentQuotes(name).startsWith(dropPathSegmentQuotes(otherPathName));
};

/**
 * @param {string} regexString
 * @param {string} requiredFlags
 * @returns {RegExp}
 */
const getRegexFromString = (regexString, requiredFlags) => {
  const match = regexString.match(/^\/(.*)\/([gimyus]*)$/us);
  let flags = 'u';
  let regex = regexString;
  if (match) {
    [, regex, flags] = match;
    if (!flags) {
      flags = 'u';
    }
  }
  const uniqueFlags = [...new Set(flags + (requiredFlags || ''))];
  flags = uniqueFlags.join('');
  return new RegExp(regex, flags);
};
var _default = {
  comparePaths,
  dropPathSegmentQuotes,
  enforcedContexts,
  exemptSpeciaMethods,
  filterTags,
  flattenRoots,
  getContextObject,
  getFunctionParameterNames,
  getIndent,
  getJsdocTagsDeep,
  getPreferredTagName,
  getRegexFromString,
  getTagsByType,
  getTagStructureForMode,
  hasATag,
  hasDefinedTypeTag,
  hasParams,
  hasReturnValue: _hasReturnValue.hasReturnValue,
  hasTag,
  hasThrowValue,
  hasValueOrExecutorHasNonEmptyResolveValue: _hasReturnValue.hasValueOrExecutorHasNonEmptyResolveValue,
  hasYieldValue,
  isConstructor,
  isGetter,
  isNamepathDefiningTag,
  isSetter,
  isValidTag,
  overrideTagStructure,
  parseClosureTemplateTag,
  pathDoesNotBeginWith,
  setTagStructure,
  tagMightHaveNamepath,
  tagMightHaveNamePosition,
  tagMightHaveTypePosition,
  tagMissingRequiredTypeOrNamepath,
  tagMustHaveNamePosition,
  tagMustHaveTypePosition
};
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=jsdocUtils.js.map