'use strict';

/** @typedef {import('./NodeType').Type} NodeType */
/** @typedef {{ type: NodeType }} Node */
/** @typedef {(node: Node, parentPropName: string | null, parentNode: Node | null) => void} TraversalCallback */

/**
 * Traverse the specified AST.
 * @param {Node} node AST to traverse.
 * @param {TraversalCallback} [opt_onEnter] Callback for onEnter.
 * @param {TraversalCallback} [opt_onLeave] Callback for onLeave.
 */
function traverse(node, opt_onEnter, opt_onLeave) {
  if (opt_onEnter) opt_onEnter(node, null, null);

  const childNodeInfo = _collectChildNodeInfo(node);
  childNodeInfo.forEach(function([childNode, parentPropName, parentNode]) {
    traverse(childNode, opt_onEnter ? (node, pn, pNode) => {
      opt_onEnter(node, pn || parentPropName, pNode || parentNode);
    } : null, opt_onLeave ? (node, pn, pNode) => {
      opt_onLeave(node, pn || parentPropName, pNode || parentNode);
    } : null);
  });

  if (opt_onLeave) opt_onLeave(node, null, null);
}


/**
 * @private
 */
const _PropertyAccessor = {
  NODE (fn, node, parentPropName, parentNode) {
    fn(node, parentPropName, parentNode);
  },
  NODE_LIST (fn, nodes, parentPropName, parentNode) {
    nodes.forEach(function(node) {
      fn(node, parentPropName, parentNode);
    });
  },
  NULLABLE_NODE (fn, opt_node, parentPropName, parentNode) {
    if (opt_node) fn(opt_node, parentPropName, parentNode);
  },
};


/**
 * @private
 * @type {{ [T in NodeType]: object }}
 */
const _childNodesMap = {
  NAME: {},
  NAMED_PARAMETER: {
    typeName: _PropertyAccessor.NULLABLE_NODE,
  },
  MEMBER: {
    owner: _PropertyAccessor.NODE,
  },
  UNION: {
    left: _PropertyAccessor.NODE,
    right: _PropertyAccessor.NODE,
  },
  INTERSECTION: {
    left: _PropertyAccessor.NODE,
    right: _PropertyAccessor.NODE,
  },
  VARIADIC: {
    value: _PropertyAccessor.NODE,
  },
  RECORD: {
    entries: _PropertyAccessor.NODE_LIST,
  },
  RECORD_ENTRY: {
    value: _PropertyAccessor.NULLABLE_NODE,
  },
  TUPLE: {
    entries: _PropertyAccessor.NODE_LIST,
  },
  GENERIC: {
    subject: _PropertyAccessor.NODE,
    objects: _PropertyAccessor.NODE_LIST,
  },
  MODULE: {
    value: _PropertyAccessor.NODE,
  },
  OPTIONAL: {
    value: _PropertyAccessor.NODE,
  },
  NULLABLE: {
    value: _PropertyAccessor.NODE,
  },
  NOT_NULLABLE: {
    value: _PropertyAccessor.NODE,
  },
  FUNCTION: {
    params: _PropertyAccessor.NODE_LIST,
    returns: _PropertyAccessor.NULLABLE_NODE,
    this: _PropertyAccessor.NULLABLE_NODE,
    new: _PropertyAccessor.NULLABLE_NODE,
  },
  ARROW: {
    params: _PropertyAccessor.NODE_LIST,
    returns: _PropertyAccessor.NULLABLE_NODE,
  },
  ANY: {},
  UNKNOWN: {},
  INNER_MEMBER: {
    owner: _PropertyAccessor.NODE,
  },
  INSTANCE_MEMBER: {
    owner: _PropertyAccessor.NODE,
  },
  STRING_VALUE: {},
  NUMBER_VALUE: {},
  EXTERNAL: {},
  FILE_PATH: {},
  PARENTHESIS: {
    value: _PropertyAccessor.NODE,
  },
  TYPE_QUERY: {
    name: _PropertyAccessor.NODE,
  },
  KEY_QUERY: {
    value: _PropertyAccessor.NODE,
  },
  IMPORT: {
    path: _PropertyAccessor.NODE,
  },
};


/**
 * @private
 * @param {Node} node
 */
function _collectChildNodeInfo(node) {
  const childNodeInfo = [];
  const propAccessorMap = _childNodesMap[node.type];

  Object.keys(propAccessorMap).forEach(function(propName) {
    const propAccessor = propAccessorMap[propName];
    propAccessor((node, propName, parentNode) => {
      childNodeInfo.push([node, propName, parentNode]);
    }, node[propName], propName, node);
  });

  return childNodeInfo;
}

module.exports = {
  traverse,
};
