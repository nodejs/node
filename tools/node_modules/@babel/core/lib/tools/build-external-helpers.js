"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _default;

function helpers() {
  const data = require("@babel/helpers");

  helpers = function () {
    return data;
  };

  return data;
}

function _generator() {
  const data = require("@babel/generator");

  _generator = function () {
    return data;
  };

  return data;
}

function _template() {
  const data = require("@babel/template");

  _template = function () {
    return data;
  };

  return data;
}

function t() {
  const data = require("@babel/types");

  t = function () {
    return data;
  };

  return data;
}

var _file = require("../transformation/file/file");

const buildUmdWrapper = replacements => (0, _template().default)`
    (function (root, factory) {
      if (typeof define === "function" && define.amd) {
        define(AMD_ARGUMENTS, factory);
      } else if (typeof exports === "object") {
        factory(COMMON_ARGUMENTS);
      } else {
        factory(BROWSER_ARGUMENTS);
      }
    })(UMD_ROOT, function (FACTORY_PARAMETERS) {
      FACTORY_BODY
    });
  `(replacements);

function buildGlobal(allowlist) {
  const namespace = t().identifier("babelHelpers");
  const body = [];
  const container = t().functionExpression(null, [t().identifier("global")], t().blockStatement(body));
  const tree = t().program([t().expressionStatement(t().callExpression(container, [t().conditionalExpression(t().binaryExpression("===", t().unaryExpression("typeof", t().identifier("global")), t().stringLiteral("undefined")), t().identifier("self"), t().identifier("global"))]))]);
  body.push(t().variableDeclaration("var", [t().variableDeclarator(namespace, t().assignmentExpression("=", t().memberExpression(t().identifier("global"), namespace), t().objectExpression([])))]));
  buildHelpers(body, namespace, allowlist);
  return tree;
}

function buildModule(allowlist) {
  const body = [];
  const refs = buildHelpers(body, null, allowlist);
  body.unshift(t().exportNamedDeclaration(null, Object.keys(refs).map(name => {
    return t().exportSpecifier(t().cloneNode(refs[name]), t().identifier(name));
  })));
  return t().program(body, [], "module");
}

function buildUmd(allowlist) {
  const namespace = t().identifier("babelHelpers");
  const body = [];
  body.push(t().variableDeclaration("var", [t().variableDeclarator(namespace, t().identifier("global"))]));
  buildHelpers(body, namespace, allowlist);
  return t().program([buildUmdWrapper({
    FACTORY_PARAMETERS: t().identifier("global"),
    BROWSER_ARGUMENTS: t().assignmentExpression("=", t().memberExpression(t().identifier("root"), namespace), t().objectExpression([])),
    COMMON_ARGUMENTS: t().identifier("exports"),
    AMD_ARGUMENTS: t().arrayExpression([t().stringLiteral("exports")]),
    FACTORY_BODY: body,
    UMD_ROOT: t().identifier("this")
  })]);
}

function buildVar(allowlist) {
  const namespace = t().identifier("babelHelpers");
  const body = [];
  body.push(t().variableDeclaration("var", [t().variableDeclarator(namespace, t().objectExpression([]))]));
  const tree = t().program(body);
  buildHelpers(body, namespace, allowlist);
  body.push(t().expressionStatement(namespace));
  return tree;
}

function buildHelpers(body, namespace, allowlist) {
  const getHelperReference = name => {
    return namespace ? t().memberExpression(namespace, t().identifier(name)) : t().identifier(`_${name}`);
  };

  const refs = {};
  helpers().list.forEach(function (name) {
    if (allowlist && allowlist.indexOf(name) < 0) return;
    const ref = refs[name] = getHelperReference(name);
    helpers().ensure(name, _file.default);
    const {
      nodes
    } = helpers().get(name, getHelperReference, ref);
    body.push(...nodes);
  });
  return refs;
}

function _default(allowlist, outputType = "global") {
  let tree;
  const build = {
    global: buildGlobal,
    module: buildModule,
    umd: buildUmd,
    var: buildVar
  }[outputType];

  if (build) {
    tree = build(allowlist);
  } else {
    throw new Error(`Unsupported output type ${outputType}`);
  }

  return (0, _generator().default)(tree).code;
}