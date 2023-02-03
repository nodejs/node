"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _jsdoccomment = require("@es-joy/jsdoccomment");
var _iterateJsdoc = _interopRequireWildcard(require("../iterateJsdoc"));
var _jsdocUtils = _interopRequireDefault(require("../jsdocUtils"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
function _getRequireWildcardCache(nodeInterop) { if (typeof WeakMap !== "function") return null; var cacheBabelInterop = new WeakMap(); var cacheNodeInterop = new WeakMap(); return (_getRequireWildcardCache = function (nodeInterop) { return nodeInterop ? cacheNodeInterop : cacheBabelInterop; })(nodeInterop); }
function _interopRequireWildcard(obj, nodeInterop) { if (!nodeInterop && obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(nodeInterop); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (key !== "default" && Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }
const extraTypes = ['null', 'undefined', 'void', 'string', 'boolean', 'object', 'function', 'symbol', 'number', 'bigint', 'NaN', 'Infinity', 'any', '*', 'never', 'unknown', 'const', 'this', 'true', 'false', 'Array', 'Object', 'RegExp', 'Date', 'Function'];
const typescriptGlobals = [
// https://www.typescriptlang.org/docs/handbook/utility-types.html
'Partial', 'Required', 'Readonly', 'Record', 'Pick', 'Omit', 'Exclude', 'Extract', 'NonNullable', 'Parameters', 'ConstructorParameters', 'ReturnType', 'InstanceType', 'ThisParameterType', 'OmitThisParameter', 'ThisType', 'Uppercase', 'Lowercase', 'Capitalize', 'Uncapitalize'];
const stripPseudoTypes = str => {
  return str && str.replace(/(?:\.|<>|\.<>|\[\])$/u, '');
};
var _default = (0, _iterateJsdoc.default)(({
  context,
  node,
  report,
  settings,
  sourceCode,
  utils
}) => {
  var _globalScope$childSco, _globalScope$childSco2;
  const {
    scopeManager
  } = sourceCode;
  const {
    globalScope
  } = scopeManager;
  const {
    definedTypes = []
  } = context.options[0] || {};
  let definedPreferredTypes = [];
  const {
    preferredTypes,
    structuredTags,
    mode
  } = settings;
  if (Object.keys(preferredTypes).length) {
    definedPreferredTypes = Object.values(preferredTypes).map(preferredType => {
      if (typeof preferredType === 'string') {
        // May become an empty string but will be filtered out below
        return stripPseudoTypes(preferredType);
      }
      if (!preferredType) {
        return undefined;
      }
      if (typeof preferredType !== 'object') {
        utils.reportSettings('Invalid `settings.jsdoc.preferredTypes`. Values must be falsy, a string, or an object.');
      }
      return stripPseudoTypes(preferredType.replacement);
    }).filter(preferredType => {
      return preferredType;
    });
  }
  const typedefDeclarations = context.getAllComments().filter(comment => {
    return /^\*\s/u.test(comment.value);
  }).map(commentNode => {
    return (0, _iterateJsdoc.parseComment)(commentNode, '');
  }).flatMap(doc => {
    return doc.tags.filter(({
      tag
    }) => {
      return utils.isNamepathDefiningTag(tag);
    });
  }).map(tag => {
    return tag.name;
  });
  const ancestorNodes = [];
  let currentNode = node;
  // No need for Program node?
  while ((_currentNode = currentNode) !== null && _currentNode !== void 0 && _currentNode.parent) {
    var _currentNode;
    ancestorNodes.push(currentNode);
    currentNode = currentNode.parent;
  }
  const getTemplateTags = function (ancestorNode) {
    const commentNode = (0, _jsdoccomment.getJSDocComment)(sourceCode, ancestorNode, settings);
    if (!commentNode) {
      return [];
    }
    const jsdoc = (0, _iterateJsdoc.parseComment)(commentNode, '');
    return _jsdocUtils.default.filterTags(jsdoc.tags, tag => {
      return tag.tag === 'template';
    });
  };

  // `currentScope` may be `null` or `Program`, so in such a case,
  //  we look to present tags instead
  const templateTags = ancestorNodes.length ? ancestorNodes.flatMap(ancestorNode => {
    return getTemplateTags(ancestorNode);
  }) : utils.getPresentTags('template');
  const closureGenericTypes = templateTags.flatMap(tag => {
    return utils.parseClosureTemplateTag(tag);
  });

  // In modules, including Node, there is a global scope at top with the
  //  Program scope inside
  const cjsOrESMScope = ((_globalScope$childSco = globalScope.childScopes[0]) === null || _globalScope$childSco === void 0 ? void 0 : (_globalScope$childSco2 = _globalScope$childSco.block) === null || _globalScope$childSco2 === void 0 ? void 0 : _globalScope$childSco2.type) === 'Program';
  const allDefinedTypes = new Set(globalScope.variables.map(({
    name
  }) => {
    return name;
  })

  // If the file is a module, concat the variables from the module scope.
  .concat(cjsOrESMScope ? globalScope.childScopes.flatMap(({
    variables
  }) => {
    return variables;
  }).map(({
    name
  }) => {
    return name;
  }) : []).concat(extraTypes).concat(typedefDeclarations).concat(definedTypes).concat(definedPreferredTypes).concat(settings.mode === 'jsdoc' ? [] : [...(settings.mode === 'typescript' ? typescriptGlobals : []), ...closureGenericTypes]));
  const jsdocTagsWithPossibleType = utils.filterTags(({
    tag
  }) => {
    return utils.tagMightHaveTypePosition(tag) && (tag !== 'suppress' || settings.mode !== 'closure');
  });
  for (const tag of jsdocTagsWithPossibleType) {
    let parsedType;
    try {
      parsedType = mode === 'permissive' ? (0, _jsdoccomment.tryParse)(tag.type) : (0, _jsdoccomment.parse)(tag.type, mode);
    } catch {
      // On syntax error, will be handled by valid-types.
      continue;
    }
    (0, _jsdoccomment.traverse)(parsedType, ({
      type,
      value
    }) => {
      if (type === 'JsdocTypeName') {
        var _structuredTags$tag$t;
        const structuredTypes = (_structuredTags$tag$t = structuredTags[tag.tag]) === null || _structuredTags$tag$t === void 0 ? void 0 : _structuredTags$tag$t.type;
        if (!allDefinedTypes.has(value) && (!Array.isArray(structuredTypes) || !structuredTypes.includes(value))) {
          report(`The type '${value}' is undefined.`, null, tag);
        } else if (!extraTypes.includes(value)) {
          context.markVariableAsUsed(value);
        }
      }
    });
  }
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: 'Checks that types in jsdoc comments are defined.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-no-undefined-types'
    },
    schema: [{
      additionalProperties: false,
      properties: {
        definedTypes: {
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
//# sourceMappingURL=noUndefinedTypes.js.map