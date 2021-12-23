'use strict';

const {
  Map,
  SymbolIterator,
} = primordials;

function push(root, value) {
  const node = { value };
  if (root.last) {
    node.previous = root.last;
    root.last.next = node;
  } else {
    root.first = node;
  }
  root.last = node;
  root.length++;

  return node;
}

function unshift(root, value) {
  const node = { value };
  if (root.first) {
    node.next = root.first;
    root.first.previous = node;
  } else {
    root.last = node;
  }
  root.first = node;
  root.length++;

  return node;
}

function shift(root) {
  if (root.first) {
    const { value } = root.first;
    root.first = root.first.next;
    root.length--;
    return value;
  }
}

function getIterator(root, initialProperty, nextProperty) {
  let node = root[initialProperty];

  return {
    next() {
      if (!node) {
        return { done: true };
      }
      const result = {
        done: false,
        value: node.value,
      };
      node = node[nextProperty];
      return result;
    },
    [SymbolIterator]() {
      return this;
    }
  };
}


function getMappedLinkedList(getKey = (value) => value) {
  const map = new Map();
  function addToMap(value, node, operation) {
    const key = getKey(value);
    let refs = map.get(key);
    if (!refs) {
      map.set(key, refs = { length: 0 });
    }
    operation(refs, node);
  }
  const root = { length: 0 };

  return {
    root,
    get length() {
      return root.length;
    },
    get first() {
      return root.first?.value;
    },
    push(value) {
      const node = push(root, value);
      addToMap(value, node, push);

      return this;
    },
    unshift(value) {
      const node = unshift(root, value);
      addToMap(value, node, unshift);

      return this;
    },
    remove(value) {
      const key = getKey(value);
      const refs = map.get(key);
      if (refs) {
        const result = shift(refs);
        if (result.previous) {
          result.previous.next = result.next;
        }
        if (result === root.last) {
          root.last = result.previous || result.next;
        }
        if (result.next) {
          result.next.previous = result.previous;
        }
        if (result === root.first) {
          root.first = result.next || result.previous;
        }
        if (refs.length === 0) {
          map.delete(key);
        }
        root.length--;
        return 1;
      }
      return 0;
    },
    [SymbolIterator]() {
      return getIterator(root, 'first', 'next');
    },
    reverseIterator() {
      return getIterator(root, 'last', 'previous');
    },
  };
}

module.exports = getMappedLinkedList;
