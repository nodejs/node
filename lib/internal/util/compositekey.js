'use strict';
const {
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const {
  ObjectCreate,
  ObjectFreeze,
  SafeMap,
  SafeWeakMap,
} = primordials;
/**
 * @param {any} value
 * @returns {boolean}
 */
const hasLifetime = (value) => {
  return value !== null && (
    typeof value === 'object' ||
    typeof value === 'function'
  );
};
class CompositeNode {
  /**
   * @type {WeakMap<object, Map<number, CompositeNode>>}
   */
  lifetimeNodes;
  /**
   * @type {Map<any, Map<number, CompositeNode>>}
   */
  primitiveNodes;
  /**
   * @type {null | Readonly<{}>}
   */
  value;
  constructor() {
    this.value = null;
  }
  get() {
    if (this.value === null) {
      return this.value = ObjectFreeze(ObjectCreate(null));
    }
    return this.value;
  }
  /**
   * @param {any} value
   * @param {number} position
   */
  emplacePrimitive(value, position) {
    if (!this.primitiveNodes) {
      this.primitiveNodes = new SafeMap();
    }
    if (!this.primitiveNodes.has(value)) {
      this.primitiveNodes.set(value, new SafeMap());
    }
    const positions = this.primitiveNodes.get(value);
    if (!positions.has(position)) {
      positions.set(position, new CompositeNode());
    }
    return positions.get(position);
  }
  /**
   * @param {object} value
   * @param {number} position
   */
  emplaceLifetime(value, position) {
    if (!this.lifetimeNodes) {
      this.lifetimeNodes = new SafeWeakMap();
    }
    if (!this.lifetimeNodes.has(value)) {
      this.lifetimeNodes.set(value, new SafeMap());
    }
    const positions = this.lifetimeNodes.get(value);
    if (!positions.has(position)) {
      positions.set(position, new CompositeNode());
    }
    return positions.get(position);
  }
}
const compoundStore = new CompositeNode();
// Accepts objects as a key and does identity on the parts of the iterable
/**
 * @param {any[]} parts
 */
const compositeKey = (parts) => {
  /**
   * @type {CompositeNode}
   */
  let node = compoundStore;
  for (let i = 0; i < parts.length; i++) {
    const value = parts[i];
    if (hasLifetime(value)) {
      node = node.emplaceLifetime(value, i);
      parts[i] = hasLifetime;
    }
  }
  // Does not leak WeakMap paths since there are none added
  if (node === compoundStore) {
    throw new ERR_INVALID_ARG_VALUE(
      'parts',
      parts,
      'must contain a non-primitive element');
  }
  for (let i = 0; i < parts.length; i++) {
    const value = parts[i];
    if (value !== hasLifetime) {
      node = node.emplacePrimitive(value, i);
    }
  }
  return node.get();
};
module.exports = {
  compositeKey
};
