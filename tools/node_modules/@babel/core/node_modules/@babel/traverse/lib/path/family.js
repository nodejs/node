"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getOpposite = getOpposite;
exports.getCompletionRecords = getCompletionRecords;
exports.getSibling = getSibling;
exports.getPrevSibling = getPrevSibling;
exports.getNextSibling = getNextSibling;
exports.getAllNextSiblings = getAllNextSiblings;
exports.getAllPrevSiblings = getAllPrevSiblings;
exports.get = get;
exports._getKey = _getKey;
exports._getPattern = _getPattern;
exports.getBindingIdentifiers = getBindingIdentifiers;
exports.getOuterBindingIdentifiers = getOuterBindingIdentifiers;
exports.getBindingIdentifierPaths = getBindingIdentifierPaths;
exports.getOuterBindingIdentifierPaths = getOuterBindingIdentifierPaths;

var _index = require("./index");

var t = require("@babel/types");

const NORMAL_COMPLETION = 0;
const BREAK_COMPLETION = 1;

function NormalCompletion(path) {
  return {
    type: NORMAL_COMPLETION,
    path
  };
}

function BreakCompletion(path) {
  return {
    type: BREAK_COMPLETION,
    path
  };
}

function getOpposite() {
  if (this.key === "left") {
    return this.getSibling("right");
  } else if (this.key === "right") {
    return this.getSibling("left");
  }

  return null;
}

function addCompletionRecords(path, records, context) {
  if (path) return records.concat(_getCompletionRecords(path, context));
  return records;
}

function completionRecordForSwitch(cases, records, context) {
  let lastNormalCompletions = [];

  for (let i = 0; i < cases.length; i++) {
    const casePath = cases[i];

    const caseCompletions = _getCompletionRecords(casePath, context);

    const normalCompletions = [];
    const breakCompletions = [];

    for (const c of caseCompletions) {
      if (c.type === NORMAL_COMPLETION) {
        normalCompletions.push(c);
      }

      if (c.type === BREAK_COMPLETION) {
        breakCompletions.push(c);
      }
    }

    if (normalCompletions.length) {
      lastNormalCompletions = normalCompletions;
    }

    records = records.concat(breakCompletions);
  }

  records = records.concat(lastNormalCompletions);
  return records;
}

function normalCompletionToBreak(completions) {
  completions.forEach(c => {
    c.type = BREAK_COMPLETION;
  });
}

function replaceBreakStatementInBreakCompletion(completions, reachable) {
  completions.forEach(c => {
    if (c.path.isBreakStatement({
      label: null
    })) {
      if (reachable) {
        c.path.replaceWith(t.unaryExpression("void", t.numericLiteral(0)));
      } else {
        c.path.remove();
      }
    }
  });
}

function getStatementListCompletion(paths, context) {
  let completions = [];

  if (context.canHaveBreak) {
    let lastNormalCompletions = [];

    for (let i = 0; i < paths.length; i++) {
      const path = paths[i];
      const newContext = Object.assign({}, context, {
        inCaseClause: false
      });

      if (path.isBlockStatement() && (context.inCaseClause || context.shouldPopulateBreak)) {
          newContext.shouldPopulateBreak = true;
        } else {
        newContext.shouldPopulateBreak = false;
      }

      const statementCompletions = _getCompletionRecords(path, newContext);

      if (statementCompletions.length > 0 && statementCompletions.every(c => c.type === BREAK_COMPLETION)) {
        if (lastNormalCompletions.length > 0 && statementCompletions.every(c => c.path.isBreakStatement({
          label: null
        }))) {
          normalCompletionToBreak(lastNormalCompletions);
          completions = completions.concat(lastNormalCompletions);

          if (lastNormalCompletions.some(c => c.path.isDeclaration())) {
            completions = completions.concat(statementCompletions);
            replaceBreakStatementInBreakCompletion(statementCompletions, true);
          }

          replaceBreakStatementInBreakCompletion(statementCompletions, false);
        } else {
          completions = completions.concat(statementCompletions);

          if (!context.shouldPopulateBreak) {
            replaceBreakStatementInBreakCompletion(statementCompletions, true);
          }
        }

        break;
      }

      if (i === paths.length - 1) {
        completions = completions.concat(statementCompletions);
      } else {
        completions = completions.concat(statementCompletions.filter(c => c.type === BREAK_COMPLETION));
        lastNormalCompletions = statementCompletions.filter(c => c.type === NORMAL_COMPLETION);
      }
    }
  } else if (paths.length) {
    for (let i = paths.length - 1; i >= 0; i--) {
      const pathCompletions = _getCompletionRecords(paths[i], context);

      if (pathCompletions.length > 1 || pathCompletions.length === 1 && !pathCompletions[0].path.isVariableDeclaration()) {
        completions = completions.concat(pathCompletions);
        break;
      }
    }
  }

  return completions;
}

function _getCompletionRecords(path, context) {
  let records = [];

  if (path.isIfStatement()) {
    records = addCompletionRecords(path.get("consequent"), records, context);
    records = addCompletionRecords(path.get("alternate"), records, context);
  } else if (path.isDoExpression() || path.isFor() || path.isWhile() || path.isLabeledStatement()) {
    records = addCompletionRecords(path.get("body"), records, context);
  } else if (path.isProgram() || path.isBlockStatement()) {
    records = records.concat(getStatementListCompletion(path.get("body"), context));
  } else if (path.isFunction()) {
    return _getCompletionRecords(path.get("body"), context);
  } else if (path.isTryStatement()) {
    records = addCompletionRecords(path.get("block"), records, context);
    records = addCompletionRecords(path.get("handler"), records, context);
  } else if (path.isCatchClause()) {
    records = addCompletionRecords(path.get("body"), records, context);
  } else if (path.isSwitchStatement()) {
    records = completionRecordForSwitch(path.get("cases"), records, context);
  } else if (path.isSwitchCase()) {
    records = records.concat(getStatementListCompletion(path.get("consequent"), {
      canHaveBreak: true,
      shouldPopulateBreak: false,
      inCaseClause: true
    }));
  } else if (path.isBreakStatement()) {
    records.push(BreakCompletion(path));
  } else {
    records.push(NormalCompletion(path));
  }

  return records;
}

function getCompletionRecords() {
  const records = _getCompletionRecords(this, {
    canHaveBreak: false,
    shouldPopulateBreak: false,
    inCaseClause: false
  });

  return records.map(r => r.path);
}

function getSibling(key) {
  return _index.default.get({
    parentPath: this.parentPath,
    parent: this.parent,
    container: this.container,
    listKey: this.listKey,
    key: key
  }).setContext(this.context);
}

function getPrevSibling() {
  return this.getSibling(this.key - 1);
}

function getNextSibling() {
  return this.getSibling(this.key + 1);
}

function getAllNextSiblings() {
  let _key = this.key;
  let sibling = this.getSibling(++_key);
  const siblings = [];

  while (sibling.node) {
    siblings.push(sibling);
    sibling = this.getSibling(++_key);
  }

  return siblings;
}

function getAllPrevSiblings() {
  let _key = this.key;
  let sibling = this.getSibling(--_key);
  const siblings = [];

  while (sibling.node) {
    siblings.push(sibling);
    sibling = this.getSibling(--_key);
  }

  return siblings;
}

function get(key, context = true) {
  if (context === true) context = this.context;
  const parts = key.split(".");

  if (parts.length === 1) {
    return this._getKey(key, context);
  } else {
    return this._getPattern(parts, context);
  }
}

function _getKey(key, context) {
  const node = this.node;
  const container = node[key];

  if (Array.isArray(container)) {
    return container.map((_, i) => {
      return _index.default.get({
        listKey: key,
        parentPath: this,
        parent: node,
        container: container,
        key: i
      }).setContext(context);
    });
  } else {
    return _index.default.get({
      parentPath: this,
      parent: node,
      container: node,
      key: key
    }).setContext(context);
  }
}

function _getPattern(parts, context) {
  let path = this;

  for (const part of parts) {
    if (part === ".") {
      path = path.parentPath;
    } else {
      if (Array.isArray(path)) {
        path = path[part];
      } else {
        path = path.get(part, context);
      }
    }
  }

  return path;
}

function getBindingIdentifiers(duplicates) {
  return t.getBindingIdentifiers(this.node, duplicates);
}

function getOuterBindingIdentifiers(duplicates) {
  return t.getOuterBindingIdentifiers(this.node, duplicates);
}

function getBindingIdentifierPaths(duplicates = false, outerOnly = false) {
  const path = this;
  let search = [].concat(path);
  const ids = Object.create(null);

  while (search.length) {
    const id = search.shift();
    if (!id) continue;
    if (!id.node) continue;
    const keys = t.getBindingIdentifiers.keys[id.node.type];

    if (id.isIdentifier()) {
      if (duplicates) {
        const _ids = ids[id.node.name] = ids[id.node.name] || [];

        _ids.push(id);
      } else {
        ids[id.node.name] = id;
      }

      continue;
    }

    if (id.isExportDeclaration()) {
      const declaration = id.get("declaration");

      if (declaration.isDeclaration()) {
        search.push(declaration);
      }

      continue;
    }

    if (outerOnly) {
      if (id.isFunctionDeclaration()) {
        search.push(id.get("id"));
        continue;
      }

      if (id.isFunctionExpression()) {
        continue;
      }
    }

    if (keys) {
      for (let i = 0; i < keys.length; i++) {
        const key = keys[i];
        const child = id.get(key);

        if (Array.isArray(child) || child.node) {
          search = search.concat(child);
        }
      }
    }
  }

  return ids;
}

function getOuterBindingIdentifierPaths(duplicates) {
  return this.getBindingIdentifierPaths(duplicates, true);
}