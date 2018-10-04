'use strict';

const is_reused_symbol = Symbol('isReused');

class FreeList {
  constructor(name, max, ctor) {
    this.name = name;
    this.ctor = ctor;
    this.max = max;
    this.list = [];
  }

  alloc() {
    return this.list.length ?
      setIsReused(this.list.pop(), true) :
      setIsReused(this.ctor.apply(this, arguments), false);
  }

  free(obj) {
    if (this.list.length < this.max) {
      this.list.push(obj);
      return true;
    }
    return false;
  }
}

function setIsReused(item, reused) {
  item[is_reused_symbol] = reused;
  return item;
}

module.exports = {
  FreeList,
  symbols: {
    is_reused_symbol
  }
};
