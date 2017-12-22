"use strict";

exports.__esModule = true;
var _exportNames = {
  assertNode: true,
  createTypeAnnotationBasedOnTypeof: true,
  createUnionTypeAnnotation: true,
  clone: true,
  cloneDeep: true,
  cloneWithoutLoc: true,
  addComment: true,
  addComments: true,
  inheritInnerComments: true,
  inheritLeadingComments: true,
  inheritsComments: true,
  inheritTrailingComments: true,
  removeComments: true,
  ensureBlock: true,
  toBindingIdentifierName: true,
  toBlock: true,
  toComputedKey: true,
  toExpression: true,
  toIdentifier: true,
  toKeyAlias: true,
  toSequenceExpression: true,
  toStatement: true,
  valueToNode: true,
  appendToMemberExpression: true,
  inherits: true,
  prependToMemberExpression: true,
  removeProperties: true,
  removePropertiesDeep: true,
  removeTypeDuplicates: true,
  getBindingIdentifiers: true,
  getOuterBindingIdentifiers: true,
  traverse: true,
  traverseFast: true,
  shallowEqual: true,
  is: true,
  isBinding: true,
  isBlockScoped: true,
  isImmutable: true,
  isLet: true,
  isNode: true,
  isNodesEquivalent: true,
  isReferenced: true,
  isScope: true,
  isSpecifierDefault: true,
  isType: true,
  isValidES3Identifier: true,
  isValidIdentifier: true,
  isVar: true,
  matchesPattern: true,
  validate: true,
  buildMatchMemberExpression: true,
  react: true
};
exports.react = exports.buildMatchMemberExpression = exports.validate = exports.matchesPattern = exports.isVar = exports.isValidIdentifier = exports.isValidES3Identifier = exports.isType = exports.isSpecifierDefault = exports.isScope = exports.isReferenced = exports.isNodesEquivalent = exports.isNode = exports.isLet = exports.isImmutable = exports.isBlockScoped = exports.isBinding = exports.is = exports.shallowEqual = exports.traverseFast = exports.traverse = exports.getOuterBindingIdentifiers = exports.getBindingIdentifiers = exports.removeTypeDuplicates = exports.removePropertiesDeep = exports.removeProperties = exports.prependToMemberExpression = exports.inherits = exports.appendToMemberExpression = exports.valueToNode = exports.toStatement = exports.toSequenceExpression = exports.toKeyAlias = exports.toIdentifier = exports.toExpression = exports.toComputedKey = exports.toBlock = exports.toBindingIdentifierName = exports.ensureBlock = exports.removeComments = exports.inheritTrailingComments = exports.inheritsComments = exports.inheritLeadingComments = exports.inheritInnerComments = exports.addComments = exports.addComment = exports.cloneWithoutLoc = exports.cloneDeep = exports.clone = exports.createUnionTypeAnnotation = exports.createTypeAnnotationBasedOnTypeof = exports.assertNode = void 0;

var _isReactComponent = _interopRequireDefault(require("./validators/react/isReactComponent"));

var _isCompatTag = _interopRequireDefault(require("./validators/react/isCompatTag"));

var _buildChildren = _interopRequireDefault(require("./builders/react/buildChildren"));

var _assertNode = _interopRequireDefault(require("./asserts/assertNode"));

exports.assertNode = _assertNode.default;

var _generated = require("./asserts/generated");

Object.keys(_generated).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _generated[key];
});

var _createTypeAnnotationBasedOnTypeof = _interopRequireDefault(require("./builders/flow/createTypeAnnotationBasedOnTypeof"));

exports.createTypeAnnotationBasedOnTypeof = _createTypeAnnotationBasedOnTypeof.default;

var _createUnionTypeAnnotation = _interopRequireDefault(require("./builders/flow/createUnionTypeAnnotation"));

exports.createUnionTypeAnnotation = _createUnionTypeAnnotation.default;

var _generated2 = require("./builders/generated");

Object.keys(_generated2).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _generated2[key];
});

var _clone = _interopRequireDefault(require("./clone/clone"));

exports.clone = _clone.default;

var _cloneDeep = _interopRequireDefault(require("./clone/cloneDeep"));

exports.cloneDeep = _cloneDeep.default;

var _cloneWithoutLoc = _interopRequireDefault(require("./clone/cloneWithoutLoc"));

exports.cloneWithoutLoc = _cloneWithoutLoc.default;

var _addComment = _interopRequireDefault(require("./comments/addComment"));

exports.addComment = _addComment.default;

var _addComments = _interopRequireDefault(require("./comments/addComments"));

exports.addComments = _addComments.default;

var _inheritInnerComments = _interopRequireDefault(require("./comments/inheritInnerComments"));

exports.inheritInnerComments = _inheritInnerComments.default;

var _inheritLeadingComments = _interopRequireDefault(require("./comments/inheritLeadingComments"));

exports.inheritLeadingComments = _inheritLeadingComments.default;

var _inheritsComments = _interopRequireDefault(require("./comments/inheritsComments"));

exports.inheritsComments = _inheritsComments.default;

var _inheritTrailingComments = _interopRequireDefault(require("./comments/inheritTrailingComments"));

exports.inheritTrailingComments = _inheritTrailingComments.default;

var _removeComments = _interopRequireDefault(require("./comments/removeComments"));

exports.removeComments = _removeComments.default;

var _generated3 = require("./constants/generated");

Object.keys(_generated3).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _generated3[key];
});

var _constants = require("./constants");

Object.keys(_constants).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _constants[key];
});

var _ensureBlock = _interopRequireDefault(require("./converters/ensureBlock"));

exports.ensureBlock = _ensureBlock.default;

var _toBindingIdentifierName = _interopRequireDefault(require("./converters/toBindingIdentifierName"));

exports.toBindingIdentifierName = _toBindingIdentifierName.default;

var _toBlock = _interopRequireDefault(require("./converters/toBlock"));

exports.toBlock = _toBlock.default;

var _toComputedKey = _interopRequireDefault(require("./converters/toComputedKey"));

exports.toComputedKey = _toComputedKey.default;

var _toExpression = _interopRequireDefault(require("./converters/toExpression"));

exports.toExpression = _toExpression.default;

var _toIdentifier = _interopRequireDefault(require("./converters/toIdentifier"));

exports.toIdentifier = _toIdentifier.default;

var _toKeyAlias = _interopRequireDefault(require("./converters/toKeyAlias"));

exports.toKeyAlias = _toKeyAlias.default;

var _toSequenceExpression = _interopRequireDefault(require("./converters/toSequenceExpression"));

exports.toSequenceExpression = _toSequenceExpression.default;

var _toStatement = _interopRequireDefault(require("./converters/toStatement"));

exports.toStatement = _toStatement.default;

var _valueToNode = _interopRequireDefault(require("./converters/valueToNode"));

exports.valueToNode = _valueToNode.default;

var _definitions = require("./definitions");

Object.keys(_definitions).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _definitions[key];
});

var _appendToMemberExpression = _interopRequireDefault(require("./modifications/appendToMemberExpression"));

exports.appendToMemberExpression = _appendToMemberExpression.default;

var _inherits = _interopRequireDefault(require("./modifications/inherits"));

exports.inherits = _inherits.default;

var _prependToMemberExpression = _interopRequireDefault(require("./modifications/prependToMemberExpression"));

exports.prependToMemberExpression = _prependToMemberExpression.default;

var _removeProperties = _interopRequireDefault(require("./modifications/removeProperties"));

exports.removeProperties = _removeProperties.default;

var _removePropertiesDeep = _interopRequireDefault(require("./modifications/removePropertiesDeep"));

exports.removePropertiesDeep = _removePropertiesDeep.default;

var _removeTypeDuplicates = _interopRequireDefault(require("./modifications/flow/removeTypeDuplicates"));

exports.removeTypeDuplicates = _removeTypeDuplicates.default;

var _getBindingIdentifiers = _interopRequireDefault(require("./retrievers/getBindingIdentifiers"));

exports.getBindingIdentifiers = _getBindingIdentifiers.default;

var _getOuterBindingIdentifiers = _interopRequireDefault(require("./retrievers/getOuterBindingIdentifiers"));

exports.getOuterBindingIdentifiers = _getOuterBindingIdentifiers.default;

var _traverse = _interopRequireDefault(require("./traverse/traverse"));

exports.traverse = _traverse.default;

var _traverseFast = _interopRequireDefault(require("./traverse/traverseFast"));

exports.traverseFast = _traverseFast.default;

var _shallowEqual = _interopRequireDefault(require("./utils/shallowEqual"));

exports.shallowEqual = _shallowEqual.default;

var _is = _interopRequireDefault(require("./validators/is"));

exports.is = _is.default;

var _isBinding = _interopRequireDefault(require("./validators/isBinding"));

exports.isBinding = _isBinding.default;

var _isBlockScoped = _interopRequireDefault(require("./validators/isBlockScoped"));

exports.isBlockScoped = _isBlockScoped.default;

var _isImmutable = _interopRequireDefault(require("./validators/isImmutable"));

exports.isImmutable = _isImmutable.default;

var _isLet = _interopRequireDefault(require("./validators/isLet"));

exports.isLet = _isLet.default;

var _isNode = _interopRequireDefault(require("./validators/isNode"));

exports.isNode = _isNode.default;

var _isNodesEquivalent = _interopRequireDefault(require("./validators/isNodesEquivalent"));

exports.isNodesEquivalent = _isNodesEquivalent.default;

var _isReferenced = _interopRequireDefault(require("./validators/isReferenced"));

exports.isReferenced = _isReferenced.default;

var _isScope = _interopRequireDefault(require("./validators/isScope"));

exports.isScope = _isScope.default;

var _isSpecifierDefault = _interopRequireDefault(require("./validators/isSpecifierDefault"));

exports.isSpecifierDefault = _isSpecifierDefault.default;

var _isType = _interopRequireDefault(require("./validators/isType"));

exports.isType = _isType.default;

var _isValidES3Identifier = _interopRequireDefault(require("./validators/isValidES3Identifier"));

exports.isValidES3Identifier = _isValidES3Identifier.default;

var _isValidIdentifier = _interopRequireDefault(require("./validators/isValidIdentifier"));

exports.isValidIdentifier = _isValidIdentifier.default;

var _isVar = _interopRequireDefault(require("./validators/isVar"));

exports.isVar = _isVar.default;

var _matchesPattern = _interopRequireDefault(require("./validators/matchesPattern"));

exports.matchesPattern = _matchesPattern.default;

var _validate = _interopRequireDefault(require("./validators/validate"));

exports.validate = _validate.default;

var _buildMatchMemberExpression = _interopRequireDefault(require("./validators/buildMatchMemberExpression"));

exports.buildMatchMemberExpression = _buildMatchMemberExpression.default;

var _generated4 = require("./validators/generated");

Object.keys(_generated4).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _generated4[key];
});

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var react = {
  isReactComponent: _isReactComponent.default,
  isCompatTag: _isCompatTag.default,
  buildChildren: _buildChildren.default
};
exports.react = react;