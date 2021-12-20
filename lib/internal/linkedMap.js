const {
  Map,
  SymbolIterator,
} = primordials;

function push(root, value) {
  const node = { value };
  if (root.last) {
    node.previous = root.last;
    root.last.next = root.last = node;
  } else {
    root.last = root.first = node;
  }
  root.length++;

  return node;
}

function unshift(value) {
  const node = { value };
  if (root.first) {
    node.next = root.first;
    root.first.previous = root.first = node;
  } else {
    root.last = root.first = node;
  }
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


function getLinkedMap() {
  const map = new Map();
  function addToMap(key, node, operation) {
    let refs = map.get(key);
    if (!refs) {
      map.set(key, refs = { length: 0 });
    }
      operation(refs, node);
  }
  const root = { length: 0 };

  return {
    get length() {
      return root.length;
    },
    push(value) {
      const node = push(root, value);
      addToMap(value, node, push);

      return this;
    },
    unshift(value) {
      const node = unshift(root, value);
      addToMap(key, node, unshift);

      return this;
    },
    remove(value) {
      const refs = map.get(value);
      if (refs) {
        const result = shift(refs);
        if (result.previous)
          result.previous.next = result.next;
        if (result.next)
          result.next.previous = result.previous;
        if (refs.length === 0) {
          map.delete(value);
        }
        root.length--;
        return 1;
      }
      return 0;
    },
    [SymbolIterator]() {
      let node = root.first;

      const iterator = {
        next() {
          if (!node) {
            return { done: true };
          }
          const result = {
            done: false,
            value: node.value,
          };
          node = node.next;
          return result;
        },
        [SymbolIterator]() {
          return iterator;
        }
      };

      return iterator;
    }
  }
}

module.exports = getLinkedMap;
