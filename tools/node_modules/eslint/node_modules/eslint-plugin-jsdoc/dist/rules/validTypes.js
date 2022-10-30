"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _jsdoccomment = require("@es-joy/jsdoccomment");
var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
const asExpression = /as\s+/u;
const suppressTypes = new Set([
// https://github.com/google/closure-compiler/wiki/@suppress-annotations
// https://github.com/google/closure-compiler/blob/master/src/com/google/javascript/jscomp/parsing/ParserConfig.properties#L154
'accessControls', 'checkDebuggerStatement', 'checkPrototypalTypes', 'checkRegExp', 'checkTypes', 'checkVars', 'closureDepMethodUsageChecks', 'const', 'constantProperty', 'deprecated', 'duplicate', 'es5Strict', 'externsValidation', 'extraProvide', 'extraRequire', 'globalThis', 'invalidCasts', 'lateProvide', 'legacyGoogScopeRequire', 'lintChecks', 'messageConventions', 'misplacedTypeAnnotation', 'missingOverride', 'missingPolyfill', 'missingProperties', 'missingProvide', 'missingRequire', 'missingSourcesWarnings', 'moduleLoad', 'nonStandardJsDocs', 'partialAlias', 'polymer', 'reportUnknownTypes', 'strictMissingProperties', 'strictModuleDepCheck', 'strictPrimitiveOperators', 'suspiciousCode',
// Not documented in enum
'switch', 'transitionalSuspiciousCodeWarnings', 'undefinedNames', 'undefinedVars', 'underscore', 'unknownDefines', 'untranspilableFeatures', 'unusedLocalVariables', 'unusedPrivateMembers', 'useOfGoogProvide', 'uselessCode', 'visibility', 'with']);
const tryParsePathIgnoreError = path => {
  try {
    (0, _jsdoccomment.tryParse)(path);
    return true;
  } catch {
    // Keep the original error for including the whole type
  }
  return false;
};

// eslint-disable-next-line complexity
var _default = (0, _iterateJsdoc.default)(({
  jsdoc,
  report,
  utils,
  context,
  settings
}) => {
  const {
    allowEmptyNamepaths = false
  } = context.options[0] || {};
  const {
    mode
  } = settings;
  for (const tag of jsdoc.tags) {
    const validNamepathParsing = function (namepath, tagName) {
      if (tryParsePathIgnoreError(namepath)) {
        return true;
      }
      let handled = false;
      if (tagName) {
        // eslint-disable-next-line default-case
        switch (tagName) {
          case 'requires':
          case 'module':
            {
              if (!namepath.startsWith('module:')) {
                handled = tryParsePathIgnoreError(`module:${namepath}`);
              }
              break;
            }
          case 'memberof':
          case 'memberof!':
            {
              const endChar = namepath.slice(-1);
              if (['#', '.', '~'].includes(endChar)) {
                handled = tryParsePathIgnoreError(namepath.slice(0, -1));
              }
              break;
            }
          case 'borrows':
            {
              const startChar = namepath.charAt();
              if (['#', '.', '~'].includes(startChar)) {
                handled = tryParsePathIgnoreError(namepath.slice(1));
              }
            }
        }
      }
      if (!handled) {
        report(`Syntax error in namepath: ${namepath}`, null, tag);
        return false;
      }
      return true;
    };
    const validTypeParsing = function (type) {
      try {
        if (mode === 'permissive') {
          (0, _jsdoccomment.tryParse)(type);
        } else {
          (0, _jsdoccomment.parse)(type, mode);
        }
      } catch {
        report(`Syntax error in type: ${type}`, null, tag);
        return false;
      }
      return true;
    };
    if (tag.problems.length) {
      const msg = tag.problems.reduce((str, {
        message
      }) => {
        return str + '; ' + message;
      }, '').slice(2);
      report(`Invalid name: ${msg}`, null, tag);
      continue;
    }
    if (tag.tag === 'borrows') {
      const thisNamepath = utils.getTagDescription(tag).replace(asExpression, '').trim();
      if (!asExpression.test(utils.getTagDescription(tag)) || !thisNamepath) {
        report(`@borrows must have an "as" expression. Found "${utils.getTagDescription(tag)}"`, null, tag);
        continue;
      }
      if (validNamepathParsing(thisNamepath, 'borrows')) {
        const thatNamepath = tag.name;
        validNamepathParsing(thatNamepath);
      }
      continue;
    }
    if (tag.tag === 'suppress' && mode === 'closure') {
      let parsedTypes;
      try {
        parsedTypes = (0, _jsdoccomment.tryParse)(tag.type);
      } catch {
        // Ignore
      }
      if (parsedTypes) {
        (0, _jsdoccomment.traverse)(parsedTypes, node => {
          const {
            value: type
          } = node;
          if (type !== undefined && !suppressTypes.has(type)) {
            report(`Syntax error in supresss type: ${type}`, null, tag);
          }
        });
      }
    }
    const otherModeMaps = ['jsdoc', 'typescript', 'closure', 'permissive'].filter(mde => {
      return mde !== mode;
    }).map(mde => {
      return utils.getTagStructureForMode(mde);
    });
    const tagMightHaveNamePosition = utils.tagMightHaveNamePosition(tag.tag, otherModeMaps);
    if (tagMightHaveNamePosition !== true && tag.name) {
      const modeInfo = tagMightHaveNamePosition === false ? '' : ` in "${mode}" mode`;
      report(`@${tag.tag} should not have a name${modeInfo}.`, null, tag);
      continue;
    }
    const mightHaveTypePosition = utils.tagMightHaveTypePosition(tag.tag, otherModeMaps);
    if (mightHaveTypePosition !== true && tag.type) {
      const modeInfo = mightHaveTypePosition === false ? '' : ` in "${mode}" mode`;
      report(`@${tag.tag} should not have a bracketed type${modeInfo}.`, null, tag);
      continue;
    }

    // REQUIRED NAME
    const tagMustHaveNamePosition = utils.tagMustHaveNamePosition(tag.tag, otherModeMaps);

    // Don't handle `@param` here though it does require name as handled by
    //  `require-param-name` (`@property` would similarly seem to require one,
    //  but is handled by `require-property-name`)
    if (tagMustHaveNamePosition !== false && !tag.name && !allowEmptyNamepaths && !['param', 'arg', 'argument', 'property', 'prop'].includes(tag.tag) && (tag.tag !== 'see' || !utils.getTagDescription(tag).includes('{@link'))) {
      const modeInfo = tagMustHaveNamePosition === true ? '' : ` in "${mode}" mode`;
      report(`Tag @${tag.tag} must have a name/namepath${modeInfo}.`, null, tag);
      continue;
    }

    // REQUIRED TYPE
    const mustHaveTypePosition = utils.tagMustHaveTypePosition(tag.tag, otherModeMaps);
    if (mustHaveTypePosition !== false && !tag.type) {
      const modeInfo = mustHaveTypePosition === true ? '' : ` in "${mode}" mode`;
      report(`Tag @${tag.tag} must have a type${modeInfo}.`, null, tag);
      continue;
    }

    // REQUIRED TYPE OR NAME/NAMEPATH
    const tagMissingRequiredTypeOrNamepath = utils.tagMissingRequiredTypeOrNamepath(tag, otherModeMaps);
    if (tagMissingRequiredTypeOrNamepath !== false && !allowEmptyNamepaths) {
      const modeInfo = tagMissingRequiredTypeOrNamepath === true ? '' : ` in "${mode}" mode`;
      report(`Tag @${tag.tag} must have either a type or namepath${modeInfo}.`, null, tag);
      continue;
    }

    // VALID TYPE
    const hasTypePosition = mightHaveTypePosition === true && Boolean(tag.type);
    if (hasTypePosition) {
      validTypeParsing(tag.type);
    }

    // VALID NAME/NAMEPATH
    const hasNameOrNamepathPosition = (tagMustHaveNamePosition !== false || utils.tagMightHaveNamepath(tag.tag)) && Boolean(tag.name);
    if (hasNameOrNamepathPosition) {
      if (mode !== 'jsdoc' && tag.tag === 'template') {
        for (const namepath of utils.parseClosureTemplateTag(tag)) {
          validNamepathParsing(namepath);
        }
      } else {
        validNamepathParsing(tag.name, tag.tag);
      }
    }
  }
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: 'Requires all types to be valid JSDoc or Closure compiler types without syntax errors.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-valid-types'
    },
    schema: [{
      additionalProperies: false,
      properties: {
        allowEmptyNamepaths: {
          default: false,
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
//# sourceMappingURL=validTypes.js.map