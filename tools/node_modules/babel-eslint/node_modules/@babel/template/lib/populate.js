"use strict";

exports.__esModule = true;
exports.default = populatePlaceholders;

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function populatePlaceholders(metadata, replacements) {
  var ast = t.cloneDeep(metadata.ast);

  if (replacements) {
    metadata.placeholders.forEach(function (placeholder) {
      if (!Object.prototype.hasOwnProperty.call(replacements, placeholder.name)) {
        throw new Error("No substitution given for \"" + placeholder.name + "\"");
      }
    });
    Object.keys(replacements).forEach(function (key) {
      if (!metadata.placeholderNames.has(key)) {
        throw new Error("Unknown substitution \"" + key + "\" given");
      }
    });
  }

  metadata.placeholders.slice().reverse().forEach(function (placeholder) {
    try {
      applyReplacement(placeholder, ast, replacements && replacements[placeholder.name] || null);
    } catch (e) {
      e.message = "babel-template placeholder \"" + placeholder.name + "\": " + e.message;
      throw e;
    }
  });
  return ast;
}

function applyReplacement(placeholder, ast, replacement) {
  if (placeholder.isDuplicate) {
    if (Array.isArray(replacement)) {
      replacement = replacement.map(function (node) {
        return t.cloneDeep(node);
      });
    } else if (typeof replacement === "object") {
      replacement = t.cloneDeep(replacement);
    }
  }

  var _placeholder$resolve = placeholder.resolve(ast),
      parent = _placeholder$resolve.parent,
      key = _placeholder$resolve.key,
      index = _placeholder$resolve.index;

  if (placeholder.type === "string") {
    if (typeof replacement === "string") {
      replacement = t.stringLiteral(replacement);
    }

    if (!replacement || !t.isStringLiteral(replacement)) {
      throw new Error("Expected string substitution");
    }
  } else if (placeholder.type === "statement") {
    if (index === undefined) {
      if (!replacement) {
        replacement = t.emptyStatement();
      } else if (Array.isArray(replacement)) {
        replacement = t.blockStatement(replacement);
      } else if (typeof replacement === "string") {
        replacement = t.expressionStatement(t.identifier(replacement));
      } else if (!t.isStatement(replacement)) {
        replacement = t.expressionStatement(replacement);
      }
    } else {
      if (replacement && !Array.isArray(replacement)) {
        if (typeof replacement === "string") {
          replacement = t.identifier(replacement);
        }

        if (!t.isStatement(replacement)) {
          replacement = t.expressionStatement(replacement);
        }
      }
    }
  } else if (placeholder.type === "param") {
    if (typeof replacement === "string") {
      replacement = t.identifier(replacement);
    }

    if (index === undefined) throw new Error("Assertion failure.");
  } else {
    if (typeof replacement === "string") {
      replacement = t.identifier(replacement);
    }

    if (Array.isArray(replacement)) {
      throw new Error("Cannot replace single expression with an array.");
    }
  }

  if (index === undefined) {
    t.validate(parent, key, replacement);
    parent[key] = replacement;
  } else {
    var items = parent[key].slice();

    if (placeholder.type === "statement" || placeholder.type === "param") {
      if (replacement == null) {
        items.splice(index, 1);
      } else if (Array.isArray(replacement)) {
        items.splice.apply(items, [index, 1].concat(replacement));
      } else {
        items[index] = replacement;
      }
    } else {
      items[index] = replacement;
    }

    t.validate(parent, key, items);
    parent[key] = items;
  }
}