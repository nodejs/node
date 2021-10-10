"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.get = get;
exports.minVersion = minVersion;
exports.getDependencies = getDependencies;
exports.ensure = ensure;
exports.default = exports.list = void 0;

var _traverse = require("@babel/traverse");

var _t = require("@babel/types");

var _helpers = require("./helpers");

const {
  assignmentExpression,
  cloneNode,
  expressionStatement,
  file: t_file,
  identifier,
  variableDeclaration,
  variableDeclarator
} = _t;

function makePath(path) {
  const parts = [];

  for (; path.parentPath; path = path.parentPath) {
    parts.push(path.key);
    if (path.inList) parts.push(path.listKey);
  }

  return parts.reverse().join(".");
}

let fileClass = undefined;

function getHelperMetadata(file) {
  const globals = new Set();
  const localBindingNames = new Set();
  const dependencies = new Map();
  let exportName;
  let exportPath;
  const exportBindingAssignments = [];
  const importPaths = [];
  const importBindingsReferences = [];
  const dependencyVisitor = {
    ImportDeclaration(child) {
      const name = child.node.source.value;

      if (!_helpers.default[name]) {
        throw child.buildCodeFrameError(`Unknown helper ${name}`);
      }

      if (child.get("specifiers").length !== 1 || !child.get("specifiers.0").isImportDefaultSpecifier()) {
        throw child.buildCodeFrameError("Helpers can only import a default value");
      }

      const bindingIdentifier = child.node.specifiers[0].local;
      dependencies.set(bindingIdentifier, name);
      importPaths.push(makePath(child));
    },

    ExportDefaultDeclaration(child) {
      const decl = child.get("declaration");

      if (decl.isFunctionDeclaration()) {
        if (!decl.node.id) {
          throw decl.buildCodeFrameError("Helpers should give names to their exported func declaration");
        }

        exportName = decl.node.id.name;
      }

      exportPath = makePath(child);
    },

    ExportAllDeclaration(child) {
      throw child.buildCodeFrameError("Helpers can only export default");
    },

    ExportNamedDeclaration(child) {
      throw child.buildCodeFrameError("Helpers can only export default");
    },

    Statement(child) {
      if (child.isModuleDeclaration()) return;
      child.skip();
    }

  };
  const referenceVisitor = {
    Program(path) {
      const bindings = path.scope.getAllBindings();
      Object.keys(bindings).forEach(name => {
        if (name === exportName) return;
        if (dependencies.has(bindings[name].identifier)) return;
        localBindingNames.add(name);
      });
    },

    ReferencedIdentifier(child) {
      const name = child.node.name;
      const binding = child.scope.getBinding(name);

      if (!binding) {
        globals.add(name);
      } else if (dependencies.has(binding.identifier)) {
        importBindingsReferences.push(makePath(child));
      }
    },

    AssignmentExpression(child) {
      const left = child.get("left");
      if (!(exportName in left.getBindingIdentifiers())) return;

      if (!left.isIdentifier()) {
        throw left.buildCodeFrameError("Only simple assignments to exports are allowed in helpers");
      }

      const binding = child.scope.getBinding(exportName);

      if (binding != null && binding.scope.path.isProgram()) {
        exportBindingAssignments.push(makePath(child));
      }
    }

  };
  (0, _traverse.default)(file.ast, dependencyVisitor, file.scope);
  (0, _traverse.default)(file.ast, referenceVisitor, file.scope);
  if (!exportPath) throw new Error("Helpers must default-export something.");
  exportBindingAssignments.reverse();
  return {
    globals: Array.from(globals),
    localBindingNames: Array.from(localBindingNames),
    dependencies,
    exportBindingAssignments,
    exportPath,
    exportName,
    importBindingsReferences,
    importPaths
  };
}

function permuteHelperAST(file, metadata, id, localBindings, getDependency) {
  if (localBindings && !id) {
    throw new Error("Unexpected local bindings for module-based helpers.");
  }

  if (!id) return;
  const {
    localBindingNames,
    dependencies,
    exportBindingAssignments,
    exportPath,
    exportName,
    importBindingsReferences,
    importPaths
  } = metadata;
  const dependenciesRefs = {};
  dependencies.forEach((name, id) => {
    dependenciesRefs[id.name] = typeof getDependency === "function" && getDependency(name) || id;
  });
  const toRename = {};
  const bindings = new Set(localBindings || []);
  localBindingNames.forEach(name => {
    let newName = name;

    while (bindings.has(newName)) newName = "_" + newName;

    if (newName !== name) toRename[name] = newName;
  });

  if (id.type === "Identifier" && exportName !== id.name) {
    toRename[exportName] = id.name;
  }

  const visitor = {
    Program(path) {
      const exp = path.get(exportPath);
      const imps = importPaths.map(p => path.get(p));
      const impsBindingRefs = importBindingsReferences.map(p => path.get(p));
      const decl = exp.get("declaration");

      if (id.type === "Identifier") {
        if (decl.isFunctionDeclaration()) {
          exp.replaceWith(decl);
        } else {
          exp.replaceWith(variableDeclaration("var", [variableDeclarator(id, decl.node)]));
        }
      } else if (id.type === "MemberExpression") {
        if (decl.isFunctionDeclaration()) {
          exportBindingAssignments.forEach(assignPath => {
            const assign = path.get(assignPath);
            assign.replaceWith(assignmentExpression("=", id, assign.node));
          });
          exp.replaceWith(decl);
          path.pushContainer("body", expressionStatement(assignmentExpression("=", id, identifier(exportName))));
        } else {
          exp.replaceWith(expressionStatement(assignmentExpression("=", id, decl.node)));
        }
      } else {
        throw new Error("Unexpected helper format.");
      }

      Object.keys(toRename).forEach(name => {
        path.scope.rename(name, toRename[name]);
      });

      for (const path of imps) path.remove();

      for (const path of impsBindingRefs) {
        const node = cloneNode(dependenciesRefs[path.node.name]);
        path.replaceWith(node);
      }

      path.stop();
    }

  };
  (0, _traverse.default)(file.ast, visitor, file.scope);
}

const helperData = Object.create(null);

function loadHelper(name) {
  if (!helperData[name]) {
    const helper = _helpers.default[name];

    if (!helper) {
      throw Object.assign(new ReferenceError(`Unknown helper ${name}`), {
        code: "BABEL_HELPER_UNKNOWN",
        helper: name
      });
    }

    const fn = () => {
      const file = {
        ast: t_file(helper.ast())
      };

      if (fileClass) {
        return new fileClass({
          filename: `babel-helper://${name}`
        }, file);
      }

      return file;
    };

    const metadata = getHelperMetadata(fn());
    helperData[name] = {
      build(getDependency, id, localBindings) {
        const file = fn();
        permuteHelperAST(file, metadata, id, localBindings, getDependency);
        return {
          nodes: file.ast.program.body,
          globals: metadata.globals
        };
      },

      minVersion() {
        return helper.minVersion;
      },

      dependencies: metadata.dependencies
    };
  }

  return helperData[name];
}

function get(name, getDependency, id, localBindings) {
  return loadHelper(name).build(getDependency, id, localBindings);
}

function minVersion(name) {
  return loadHelper(name).minVersion();
}

function getDependencies(name) {
  return Array.from(loadHelper(name).dependencies.values());
}

function ensure(name, newFileClass) {
  if (!fileClass) {
    fileClass = newFileClass;
  }

  loadHelper(name);
}

const list = Object.keys(_helpers.default).map(name => name.replace(/^_/, "")).filter(name => name !== "__esModule");
exports.list = list;
var _default = get;
exports.default = _default;