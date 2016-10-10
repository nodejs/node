/*istanbul ignore next*/"use strict";

exports.__esModule = true;

var _symbol = require("babel-runtime/core-js/symbol");

var _symbol2 = _interopRequireDefault(_symbol);

exports.default = function (code, opts) {
  // since we lazy parse the template, we get the current stack so we have the
  // original stack to append if it errors when parsing
  var stack = /*istanbul ignore next*/void 0;
  try {
    // error stack gets populated in IE only on throw (https://msdn.microsoft.com/en-us/library/hh699850(v=vs.94).aspx)
    throw new Error();
  } catch (error) {
    if (error.stack) {
      // error.stack does not exists in IE <= 9
      stack = error.stack.split("\n").slice(1).join("\n");
    }
  }

  var _getAst = function /*istanbul ignore next*/getAst() {
    var ast = /*istanbul ignore next*/void 0;

    try {
      ast = babylon.parse(code, /*istanbul ignore next*/(0, _assign2.default)({
        allowReturnOutsideFunction: true,
        allowSuperOutsideMethod: true
      }, opts));

      ast = /*istanbul ignore next*/_babelTraverse2.default.removeProperties(ast);

      /*istanbul ignore next*/_babelTraverse2.default.cheap(ast, function (node) {
        node[FROM_TEMPLATE] = true;
      });
    } catch (err) {
      err.stack = /*istanbul ignore next*/err.stack + "from\n" + stack;
      throw err;
    }

    _getAst = function /*istanbul ignore next*/getAst() {
      return ast;
    };

    return ast;
  };

  return function () {
    /*istanbul ignore next*/
    for (var _len = arguments.length, args = Array(_len), _key = 0; _key < _len; _key++) {
      args[_key] = arguments[_key];
    }

    return useTemplate(_getAst(), args);
  };
};

var /*istanbul ignore next*/_cloneDeep = require("lodash/cloneDeep");

/*istanbul ignore next*/
var _cloneDeep2 = _interopRequireDefault(_cloneDeep);

var /*istanbul ignore next*/_assign = require("lodash/assign");

/*istanbul ignore next*/
var _assign2 = _interopRequireDefault(_assign);

var /*istanbul ignore next*/_has = require("lodash/has");

/*istanbul ignore next*/
var _has2 = _interopRequireDefault(_has);

var /*istanbul ignore next*/_babelTraverse = require("babel-traverse");

/*istanbul ignore next*/
var _babelTraverse2 = _interopRequireDefault(_babelTraverse);

var /*istanbul ignore next*/_babylon = require("babylon");

/*istanbul ignore next*/
var babylon = _interopRequireWildcard(_babylon);

var /*istanbul ignore next*/_babelTypes = require("babel-types");

/*istanbul ignore next*/
var t = _interopRequireWildcard(_babelTypes);

/*istanbul ignore next*/
function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* eslint max-len: 0 */

var FROM_TEMPLATE = "_fromTemplate"; //Symbol(); // todo: probably wont get copied over
var TEMPLATE_SKIP = /*istanbul ignore next*/(0, _symbol2.default)();

function useTemplate(ast, nodes) {
  ast = /*istanbul ignore next*/(0, _cloneDeep2.default)(ast);
  /*istanbul ignore next*/var _ast = ast;
  /*istanbul ignore next*/var program = _ast.program;


  if (nodes.length) {
    /*istanbul ignore next*/(0, _babelTraverse2.default)(ast, templateVisitor, null, nodes);
  }

  if (program.body.length > 1) {
    return program.body;
  } else {
    return program.body[0];
  }
}

var templateVisitor = {
  // 360
  noScope: true,

  /*istanbul ignore next*/enter: function enter(path, args) {
    /*istanbul ignore next*/var node = path.node;

    if (node[TEMPLATE_SKIP]) return path.skip();

    if (t.isExpressionStatement(node)) {
      node = node.expression;
    }

    var replacement = /*istanbul ignore next*/void 0;

    if (t.isIdentifier(node) && node[FROM_TEMPLATE]) {
      if ( /*istanbul ignore next*/(0, _has2.default)(args[0], node.name)) {
        replacement = args[0][node.name];
      } else if (node.name[0] === "$") {
        var i = +node.name.slice(1);
        if (args[i]) replacement = args[i];
      }
    }

    if (replacement === null) {
      path.remove();
    }

    if (replacement) {
      replacement[TEMPLATE_SKIP] = true;
      path.replaceInline(replacement);
    }
  },
  /*istanbul ignore next*/exit: function exit(_ref) {
    /*istanbul ignore next*/var node = _ref.node;

    if (!node.loc) /*istanbul ignore next*/_babelTraverse2.default.clearNode(node);
  }
};
/*istanbul ignore next*/module.exports = exports["default"];