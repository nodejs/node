"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.rewriteModuleStatementsAndPrepareHeader = rewriteModuleStatementsAndPrepareHeader;
exports.ensureStatementsHoisted = ensureStatementsHoisted;
exports.wrapInterop = wrapInterop;
exports.buildNamespaceInitStatements = buildNamespaceInitStatements;
Object.defineProperty(exports, "isModule", {
  enumerable: true,
  get: function () {
    return _helperModuleImports.isModule;
  }
});
Object.defineProperty(exports, "rewriteThis", {
  enumerable: true,
  get: function () {
    return _rewriteThis.default;
  }
});
Object.defineProperty(exports, "hasExports", {
  enumerable: true,
  get: function () {
    return _normalizeAndLoadMetadata.hasExports;
  }
});
Object.defineProperty(exports, "isSideEffectImport", {
  enumerable: true,
  get: function () {
    return _normalizeAndLoadMetadata.isSideEffectImport;
  }
});
Object.defineProperty(exports, "getModuleName", {
  enumerable: true,
  get: function () {
    return _getModuleName.default;
  }
});

var _assert = require("assert");

var t = require("@babel/types");

var _template = require("@babel/template");

var _helperModuleImports = require("@babel/helper-module-imports");

var _rewriteThis = require("./rewrite-this");

var _rewriteLiveReferences = require("./rewrite-live-references");

var _normalizeAndLoadMetadata = require("./normalize-and-load-metadata");

var _getModuleName = require("./get-module-name");

function rewriteModuleStatementsAndPrepareHeader(path, {
  loose,
  exportName,
  strict,
  allowTopLevelThis,
  strictMode,
  noInterop,
  importInterop = noInterop ? "none" : "babel",
  lazy,
  esNamespaceOnly,
  constantReexports = loose,
  enumerableModuleMeta = loose,
  noIncompleteNsImportDetection
}) {
  (0, _normalizeAndLoadMetadata.validateImportInteropOption)(importInterop);

  _assert((0, _helperModuleImports.isModule)(path), "Cannot process module statements in a script");

  path.node.sourceType = "script";
  const meta = (0, _normalizeAndLoadMetadata.default)(path, exportName, {
    importInterop,
    initializeReexports: constantReexports,
    lazy,
    esNamespaceOnly
  });

  if (!allowTopLevelThis) {
    (0, _rewriteThis.default)(path);
  }

  (0, _rewriteLiveReferences.default)(path, meta);

  if (strictMode !== false) {
    const hasStrict = path.node.directives.some(directive => {
      return directive.value.value === "use strict";
    });

    if (!hasStrict) {
      path.unshiftContainer("directives", t.directive(t.directiveLiteral("use strict")));
    }
  }

  const headers = [];

  if ((0, _normalizeAndLoadMetadata.hasExports)(meta) && !strict) {
    headers.push(buildESModuleHeader(meta, enumerableModuleMeta));
  }

  const nameList = buildExportNameListDeclaration(path, meta);

  if (nameList) {
    meta.exportNameListName = nameList.name;
    headers.push(nameList.statement);
  }

  headers.push(...buildExportInitializationStatements(path, meta, constantReexports, noIncompleteNsImportDetection));
  return {
    meta,
    headers
  };
}

function ensureStatementsHoisted(statements) {
  statements.forEach(header => {
    header._blockHoist = 3;
  });
}

function wrapInterop(programPath, expr, type) {
  if (type === "none") {
    return null;
  }

  if (type === "node-namespace") {
    return t.callExpression(programPath.hub.addHelper("interopRequireWildcard"), [expr, t.booleanLiteral(true)]);
  } else if (type === "node-default") {
    return null;
  }

  let helper;

  if (type === "default") {
    helper = "interopRequireDefault";
  } else if (type === "namespace") {
    helper = "interopRequireWildcard";
  } else {
    throw new Error(`Unknown interop: ${type}`);
  }

  return t.callExpression(programPath.hub.addHelper(helper), [expr]);
}

function buildNamespaceInitStatements(metadata, sourceMetadata, constantReexports = false) {
  const statements = [];
  let srcNamespace = t.identifier(sourceMetadata.name);
  if (sourceMetadata.lazy) srcNamespace = t.callExpression(srcNamespace, []);

  for (const localName of sourceMetadata.importsNamespace) {
    if (localName === sourceMetadata.name) continue;
    statements.push(_template.default.statement`var NAME = SOURCE;`({
      NAME: localName,
      SOURCE: t.cloneNode(srcNamespace)
    }));
  }

  if (constantReexports) {
    statements.push(...buildReexportsFromMeta(metadata, sourceMetadata, true));
  }

  for (const exportName of sourceMetadata.reexportNamespace) {
    statements.push((sourceMetadata.lazy ? _template.default.statement`
            Object.defineProperty(EXPORTS, "NAME", {
              enumerable: true,
              get: function() {
                return NAMESPACE;
              }
            });
          ` : _template.default.statement`EXPORTS.NAME = NAMESPACE;`)({
      EXPORTS: metadata.exportName,
      NAME: exportName,
      NAMESPACE: t.cloneNode(srcNamespace)
    }));
  }

  if (sourceMetadata.reexportAll) {
    const statement = buildNamespaceReexport(metadata, t.cloneNode(srcNamespace), constantReexports);
    statement.loc = sourceMetadata.reexportAll.loc;
    statements.push(statement);
  }

  return statements;
}

const ReexportTemplate = {
  constant: _template.default.statement`EXPORTS.EXPORT_NAME = NAMESPACE_IMPORT;`,
  constantComputed: _template.default.statement`EXPORTS["EXPORT_NAME"] = NAMESPACE_IMPORT;`,
  spec: (0, _template.default)`
    Object.defineProperty(EXPORTS, "EXPORT_NAME", {
      enumerable: true,
      get: function() {
        return NAMESPACE_IMPORT;
      },
    });
    `
};

const buildReexportsFromMeta = (meta, metadata, constantReexports) => {
  const namespace = metadata.lazy ? t.callExpression(t.identifier(metadata.name), []) : t.identifier(metadata.name);
  const {
    stringSpecifiers
  } = meta;
  return Array.from(metadata.reexports, ([exportName, importName]) => {
    let NAMESPACE_IMPORT = t.cloneNode(namespace);

    if (importName === "default" && metadata.interop === "node-default") {} else if (stringSpecifiers.has(importName)) {
      NAMESPACE_IMPORT = t.memberExpression(NAMESPACE_IMPORT, t.stringLiteral(importName), true);
    } else {
      NAMESPACE_IMPORT = t.memberExpression(NAMESPACE_IMPORT, t.identifier(importName));
    }

    const astNodes = {
      EXPORTS: meta.exportName,
      EXPORT_NAME: exportName,
      NAMESPACE_IMPORT
    };

    if (constantReexports || t.isIdentifier(NAMESPACE_IMPORT)) {
      if (stringSpecifiers.has(exportName)) {
        return ReexportTemplate.constantComputed(astNodes);
      } else {
        return ReexportTemplate.constant(astNodes);
      }
    } else {
      return ReexportTemplate.spec(astNodes);
    }
  });
};

function buildESModuleHeader(metadata, enumerableModuleMeta = false) {
  return (enumerableModuleMeta ? _template.default.statement`
        EXPORTS.__esModule = true;
      ` : _template.default.statement`
        Object.defineProperty(EXPORTS, "__esModule", {
          value: true,
        });
      `)({
    EXPORTS: metadata.exportName
  });
}

function buildNamespaceReexport(metadata, namespace, constantReexports) {
  return (constantReexports ? _template.default.statement`
        Object.keys(NAMESPACE).forEach(function(key) {
          if (key === "default" || key === "__esModule") return;
          VERIFY_NAME_LIST;
          if (key in EXPORTS && EXPORTS[key] === NAMESPACE[key]) return;

          EXPORTS[key] = NAMESPACE[key];
        });
      ` : _template.default.statement`
        Object.keys(NAMESPACE).forEach(function(key) {
          if (key === "default" || key === "__esModule") return;
          VERIFY_NAME_LIST;
          if (key in EXPORTS && EXPORTS[key] === NAMESPACE[key]) return;

          Object.defineProperty(EXPORTS, key, {
            enumerable: true,
            get: function() {
              return NAMESPACE[key];
            },
          });
        });
    `)({
    NAMESPACE: namespace,
    EXPORTS: metadata.exportName,
    VERIFY_NAME_LIST: metadata.exportNameListName ? (0, _template.default)`
            if (Object.prototype.hasOwnProperty.call(EXPORTS_LIST, key)) return;
          `({
      EXPORTS_LIST: metadata.exportNameListName
    }) : null
  });
}

function buildExportNameListDeclaration(programPath, metadata) {
  const exportedVars = Object.create(null);

  for (const data of metadata.local.values()) {
    for (const name of data.names) {
      exportedVars[name] = true;
    }
  }

  let hasReexport = false;

  for (const data of metadata.source.values()) {
    for (const exportName of data.reexports.keys()) {
      exportedVars[exportName] = true;
    }

    for (const exportName of data.reexportNamespace) {
      exportedVars[exportName] = true;
    }

    hasReexport = hasReexport || !!data.reexportAll;
  }

  if (!hasReexport || Object.keys(exportedVars).length === 0) return null;
  const name = programPath.scope.generateUidIdentifier("exportNames");
  delete exportedVars.default;
  return {
    name: name.name,
    statement: t.variableDeclaration("var", [t.variableDeclarator(name, t.valueToNode(exportedVars))])
  };
}

function buildExportInitializationStatements(programPath, metadata, constantReexports = false, noIncompleteNsImportDetection = false) {
  const initStatements = [];
  const exportNames = [];

  for (const [localName, data] of metadata.local) {
    if (data.kind === "import") {} else if (data.kind === "hoisted") {
      initStatements.push(buildInitStatement(metadata, data.names, t.identifier(localName)));
    } else {
      exportNames.push(...data.names);
    }
  }

  for (const data of metadata.source.values()) {
    if (!constantReexports) {
      initStatements.push(...buildReexportsFromMeta(metadata, data, false));
    }

    for (const exportName of data.reexportNamespace) {
      exportNames.push(exportName);
    }
  }

  if (!noIncompleteNsImportDetection) {
    initStatements.push(...chunk(exportNames, 100).map(members => {
      return buildInitStatement(metadata, members, programPath.scope.buildUndefinedNode());
    }));
  }

  return initStatements;
}

const InitTemplate = {
  computed: _template.default.expression`EXPORTS["NAME"] = VALUE`,
  default: _template.default.expression`EXPORTS.NAME = VALUE`
};

function buildInitStatement(metadata, exportNames, initExpr) {
  const {
    stringSpecifiers,
    exportName: EXPORTS
  } = metadata;
  return t.expressionStatement(exportNames.reduce((acc, exportName) => {
    const params = {
      EXPORTS,
      NAME: exportName,
      VALUE: acc
    };

    if (stringSpecifiers.has(exportName)) {
      return InitTemplate.computed(params);
    } else {
      return InitTemplate.default(params);
    }
  }, initExpr));
}

function chunk(array, size) {
  const chunks = [];

  for (let i = 0; i < array.length; i += size) {
    chunks.push(array.slice(i, i + size));
  }

  return chunks;
}