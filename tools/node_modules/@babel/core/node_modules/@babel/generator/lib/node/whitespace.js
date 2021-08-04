"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.list = exports.nodes = void 0;

var t = require("@babel/types");

function crawl(node, state = {}) {
  if (t.isMemberExpression(node) || t.isOptionalMemberExpression(node)) {
    crawl(node.object, state);
    if (node.computed) crawl(node.property, state);
  } else if (t.isBinary(node) || t.isAssignmentExpression(node)) {
    crawl(node.left, state);
    crawl(node.right, state);
  } else if (t.isCallExpression(node) || t.isOptionalCallExpression(node)) {
    state.hasCall = true;
    crawl(node.callee, state);
  } else if (t.isFunction(node)) {
    state.hasFunction = true;
  } else if (t.isIdentifier(node)) {
    state.hasHelper = state.hasHelper || isHelper(node.callee);
  }

  return state;
}

function isHelper(node) {
  if (t.isMemberExpression(node)) {
    return isHelper(node.object) || isHelper(node.property);
  } else if (t.isIdentifier(node)) {
    return node.name === "require" || node.name[0] === "_";
  } else if (t.isCallExpression(node)) {
    return isHelper(node.callee);
  } else if (t.isBinary(node) || t.isAssignmentExpression(node)) {
    return t.isIdentifier(node.left) && isHelper(node.left) || isHelper(node.right);
  } else {
    return false;
  }
}

function isType(node) {
  return t.isLiteral(node) || t.isObjectExpression(node) || t.isArrayExpression(node) || t.isIdentifier(node) || t.isMemberExpression(node);
}

const nodes = {
  AssignmentExpression(node) {
    const state = crawl(node.right);

    if (state.hasCall && state.hasHelper || state.hasFunction) {
      return {
        before: state.hasFunction,
        after: true
      };
    }
  },

  SwitchCase(node, parent) {
    return {
      before: !!node.consequent.length || parent.cases[0] === node,
      after: !node.consequent.length && parent.cases[parent.cases.length - 1] === node
    };
  },

  LogicalExpression(node) {
    if (t.isFunction(node.left) || t.isFunction(node.right)) {
      return {
        after: true
      };
    }
  },

  Literal(node) {
    if (t.isStringLiteral(node) && node.value === "use strict") {
      return {
        after: true
      };
    }
  },

  CallExpression(node) {
    if (t.isFunction(node.callee) || isHelper(node)) {
      return {
        before: true,
        after: true
      };
    }
  },

  OptionalCallExpression(node) {
    if (t.isFunction(node.callee)) {
      return {
        before: true,
        after: true
      };
    }
  },

  VariableDeclaration(node) {
    for (let i = 0; i < node.declarations.length; i++) {
      const declar = node.declarations[i];
      let enabled = isHelper(declar.id) && !isType(declar.init);

      if (!enabled) {
        const state = crawl(declar.init);
        enabled = isHelper(declar.init) && state.hasCall || state.hasFunction;
      }

      if (enabled) {
        return {
          before: true,
          after: true
        };
      }
    }
  },

  IfStatement(node) {
    if (t.isBlockStatement(node.consequent)) {
      return {
        before: true,
        after: true
      };
    }
  }

};
exports.nodes = nodes;

nodes.ObjectProperty = nodes.ObjectTypeProperty = nodes.ObjectMethod = function (node, parent) {
  if (parent.properties[0] === node) {
    return {
      before: true
    };
  }
};

nodes.ObjectTypeCallProperty = function (node, parent) {
  var _parent$properties;

  if (parent.callProperties[0] === node && !((_parent$properties = parent.properties) != null && _parent$properties.length)) {
    return {
      before: true
    };
  }
};

nodes.ObjectTypeIndexer = function (node, parent) {
  var _parent$properties2, _parent$callPropertie;

  if (parent.indexers[0] === node && !((_parent$properties2 = parent.properties) != null && _parent$properties2.length) && !((_parent$callPropertie = parent.callProperties) != null && _parent$callPropertie.length)) {
    return {
      before: true
    };
  }
};

nodes.ObjectTypeInternalSlot = function (node, parent) {
  var _parent$properties3, _parent$callPropertie2, _parent$indexers;

  if (parent.internalSlots[0] === node && !((_parent$properties3 = parent.properties) != null && _parent$properties3.length) && !((_parent$callPropertie2 = parent.callProperties) != null && _parent$callPropertie2.length) && !((_parent$indexers = parent.indexers) != null && _parent$indexers.length)) {
    return {
      before: true
    };
  }
};

const list = {
  VariableDeclaration(node) {
    return node.declarations.map(decl => decl.init);
  },

  ArrayExpression(node) {
    return node.elements;
  },

  ObjectExpression(node) {
    return node.properties;
  }

};
exports.list = list;
[["Function", true], ["Class", true], ["Loop", true], ["LabeledStatement", true], ["SwitchStatement", true], ["TryStatement", true]].forEach(function ([type, amounts]) {
  if (typeof amounts === "boolean") {
    amounts = {
      after: amounts,
      before: amounts
    };
  }

  [type].concat(t.FLIPPED_ALIAS_KEYS[type] || []).forEach(function (type) {
    nodes[type] = function () {
      return amounts;
    };
  });
});