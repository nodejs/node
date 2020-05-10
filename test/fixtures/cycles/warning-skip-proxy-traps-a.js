module.exports = new Proxy({}, {
  get(_target, prop) { throw new Error(`get: ${String(prop)}`); },
  getPrototypeOf() { throw new Error('getPrototypeOf'); },
  setPrototypeOf() { throw new Error('setPrototypeOf'); },
  isExtensible() { throw new Error('isExtensible'); },
  preventExtensions() { throw new Error('preventExtensions'); },
  getOwnPropertyDescriptor() { throw new Error('getOwnPropertyDescriptor'); },
  defineProperty() { throw new Error('defineProperty'); },
  has() { throw new Error('has'); },
  set() { throw new Error('set'); },
  deleteProperty() { throw new Error('deleteProperty'); },
  ownKeys() { throw new Error('ownKeys'); },
  apply() { throw new Error('apply'); },
  construct() { throw new Error('construct'); }
});

require('./warning-skip-proxy-traps-b.js');
