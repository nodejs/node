"use strict";

exports.__esModule = true;
exports.default = literalTemplate;

var _options = require("./options");

var _parse = _interopRequireDefault(require("./parse"));

var _populate = _interopRequireDefault(require("./populate"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function literalTemplate(formatter, tpl, opts) {
  var _buildLiteralData = buildLiteralData(formatter, tpl, opts),
      metadata = _buildLiteralData.metadata,
      names = _buildLiteralData.names;

  return function (arg) {
    var defaultReplacements = arg.reduce(function (acc, replacement, i) {
      acc[names[i]] = replacement;
      return acc;
    }, {});
    return function (arg) {
      var replacements = (0, _options.normalizeReplacements)(arg);

      if (replacements) {
        Object.keys(replacements).forEach(function (key) {
          if (Object.prototype.hasOwnProperty.call(defaultReplacements, key)) {
            throw new Error("Unexpected replacement overlap.");
          }
        });
      }

      return formatter.unwrap((0, _populate.default)(metadata, replacements ? Object.assign(replacements, defaultReplacements) : defaultReplacements));
    };
  };
}

function buildLiteralData(formatter, tpl, opts) {
  var names;
  var nameSet;
  var metadata;
  var prefix = "";

  do {
    prefix += "$";
    var result = buildTemplateCode(tpl, prefix);
    names = result.names;
    nameSet = new Set(names);
    metadata = (0, _parse.default)(formatter, formatter.code(result.code), {
      parser: opts.parser,
      placeholderWhitelist: new Set(result.names.concat(opts.placeholderWhitelist ? Array.from(opts.placeholderWhitelist) : [])),
      placeholderPattern: opts.placeholderPattern,
      preserveComments: opts.preserveComments
    });
  } while (metadata.placeholders.some(function (placeholder) {
    return placeholder.isDuplicate && nameSet.has(placeholder.name);
  }));

  return {
    metadata: metadata,
    names: names
  };
}

function buildTemplateCode(tpl, prefix) {
  var names = [];
  var code = tpl[0];

  for (var i = 1; i < tpl.length; i++) {
    var value = "" + prefix + (i - 1);
    names.push(value);
    code += value + tpl[i];
  }

  return {
    names: names,
    code: code
  };
}