/*istanbul ignore next*/"use strict";

exports.__esModule = true;
exports.MESSAGES = undefined;

var _stringify = require("babel-runtime/core-js/json/stringify");

var _stringify2 = _interopRequireDefault(_stringify);

exports.get = get;
/*istanbul ignore next*/exports.parseArgs = parseArgs;

var /*istanbul ignore next*/_util = require("util");

/*istanbul ignore next*/
var util = _interopRequireWildcard(_util);

/*istanbul ignore next*/
function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Mapping of messages to be used in Babel.
 * Messages can include $0-style placeholders.
 */

var MESSAGES = /*istanbul ignore next*/exports.MESSAGES = {
  tailCallReassignmentDeopt: "Function reference has been reassigned, so it will probably be dereferenced, therefore we can't optimise this with confidence",
  classesIllegalBareSuper: "Illegal use of bare super",
  classesIllegalSuperCall: "Direct super call is illegal in non-constructor, use super.$1() instead",
  scopeDuplicateDeclaration: "Duplicate declaration $1",
  settersNoRest: "Setters aren't allowed to have a rest",
  noAssignmentsInForHead: "No assignments allowed in for-in/of head",
  expectedMemberExpressionOrIdentifier: "Expected type MemberExpression or Identifier",
  invalidParentForThisNode: "We don't know how to handle this node within the current parent - please open an issue",
  readOnly: "$1 is read-only",
  unknownForHead: "Unknown node type $1 in ForStatement",
  didYouMean: "Did you mean $1?",
  codeGeneratorDeopt: "Note: The code generator has deoptimised the styling of $1 as it exceeds the max of $2.",
  missingTemplatesDirectory: "no templates directory - this is most likely the result of a broken `npm publish`. Please report to https://github.com/babel/babel/issues",
  unsupportedOutputType: "Unsupported output type $1",
  illegalMethodName: "Illegal method name $1",
  lostTrackNodePath: "We lost track of this node's position, likely because the AST was directly manipulated",

  modulesIllegalExportName: "Illegal export $1",
  modulesDuplicateDeclarations: "Duplicate module declarations with the same source but in different scopes",

  undeclaredVariable: "Reference to undeclared variable $1",
  undeclaredVariableType: "Referencing a type alias outside of a type annotation",
  undeclaredVariableSuggestion: "Reference to undeclared variable $1 - did you mean $2?",

  traverseNeedsParent: "You must pass a scope and parentPath unless traversing a Program/File. Instead of that you tried to traverse a $1 node without passing scope and parentPath.",
  traverseVerifyRootFunction: "You passed `traverse()` a function when it expected a visitor object, are you sure you didn't mean `{ enter: Function }`?",
  traverseVerifyVisitorProperty: "You passed `traverse()` a visitor object with the property $1 that has the invalid property $2",
  traverseVerifyNodeType: "You gave us a visitor for the node type $1 but it's not a valid type",

  pluginNotObject: "Plugin $2 specified in $1 was expected to return an object when invoked but returned $3",
  pluginNotFunction: "Plugin $2 specified in $1 was expected to return a function but returned $3",
  pluginUnknown: "Unknown plugin $1 specified in $2 at $3, attempted to resolve relative to $4",
  pluginInvalidProperty: "Plugin $2 specified in $1 provided an invalid property of $3"
};

/**
 * Get a message with $0 placeholders replaced by arguments.
 */

/* eslint max-len: 0 */

function get(key) {
  /*istanbul ignore next*/
  for (var _len = arguments.length, args = Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
    args[_key - 1] = arguments[_key];
  }

  var msg = MESSAGES[key];
  if (!msg) throw new ReferenceError( /*istanbul ignore next*/"Unknown message " + /*istanbul ignore next*/(0, _stringify2.default)(key));

  // stringify args
  args = parseArgs(args);

  // replace $0 placeholders with args
  return msg.replace(/\$(\d+)/g, function (str, i) {
    return args[i - 1];
  });
}

/**
 * Stingify arguments to be used inside messages.
 */

function parseArgs(args) {
  return args.map(function (val) {
    if (val != null && val.inspect) {
      return val.inspect();
    } else {
      try {
        return (/*istanbul ignore next*/(0, _stringify2.default)(val) || val + ""
        );
      } catch (e) {
        return util.inspect(val);
      }
    }
  });
}