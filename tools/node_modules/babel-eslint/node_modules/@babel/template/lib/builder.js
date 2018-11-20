"use strict";

exports.__esModule = true;
exports.default = createTemplateBuilder;

var _options = require("./options");

var _string = _interopRequireDefault(require("./string"));

var _literal = _interopRequireDefault(require("./literal"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var NO_PLACEHOLDER = (0, _options.validate)({
  placeholderPattern: false
});

function createTemplateBuilder(formatter, defaultOpts) {
  var templateFnCache = new WeakMap();
  var templateAstCache = new WeakMap();
  var cachedOpts = defaultOpts || (0, _options.validate)(null);
  return Object.assign(function (tpl) {
    for (var _len = arguments.length, args = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
      args[_key - 1] = arguments[_key];
    }

    if (typeof tpl === "string") {
      if (args.length > 1) throw new Error("Unexpected extra params.");
      return extendedTrace((0, _string.default)(formatter, tpl, (0, _options.merge)(cachedOpts, (0, _options.validate)(args[0]))));
    } else if (Array.isArray(tpl)) {
      var builder = templateFnCache.get(tpl);

      if (!builder) {
        builder = (0, _literal.default)(formatter, tpl, cachedOpts);
        templateFnCache.set(tpl, builder);
      }

      return extendedTrace(builder(args));
    } else if (typeof tpl === "object" && tpl) {
      if (args.length > 0) throw new Error("Unexpected extra params.");
      return createTemplateBuilder(formatter, (0, _options.merge)(cachedOpts, (0, _options.validate)(tpl)));
    }

    throw new Error("Unexpected template param " + typeof tpl);
  }, {
    ast: function ast(tpl) {
      for (var _len2 = arguments.length, args = new Array(_len2 > 1 ? _len2 - 1 : 0), _key2 = 1; _key2 < _len2; _key2++) {
        args[_key2 - 1] = arguments[_key2];
      }

      if (typeof tpl === "string") {
        if (args.length > 1) throw new Error("Unexpected extra params.");
        return (0, _string.default)(formatter, tpl, (0, _options.merge)((0, _options.merge)(cachedOpts, (0, _options.validate)(args[0])), NO_PLACEHOLDER))();
      } else if (Array.isArray(tpl)) {
        var builder = templateAstCache.get(tpl);

        if (!builder) {
          builder = (0, _literal.default)(formatter, tpl, (0, _options.merge)(cachedOpts, NO_PLACEHOLDER));
          templateAstCache.set(tpl, builder);
        }

        return builder(args)();
      }

      throw new Error("Unexpected template param " + typeof tpl);
    }
  });
}

function extendedTrace(fn) {
  var rootStack = "";

  try {
    throw new Error();
  } catch (error) {
    if (error.stack) {
      rootStack = error.stack.split("\n").slice(3).join("\n");
    }
  }

  return function (arg) {
    try {
      return fn(arg);
    } catch (err) {
      err.stack += "\n    =============\n" + rootStack;
      throw err;
    }
  };
}