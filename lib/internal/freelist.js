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
    let item;
    if (this.list.length > 0) {
      item = this.list.pop();
      item[is_reused_symbol] = true;
    } else {
      item = this.ctor.apply(this, arguments);
      item[is_reused_symbol] = false;
    }
    return item;
  }

  free(obj) {
    if (this.list.length < this.max) {
      this.list.push(obj);
      return true;
    }
    return false;
  }
}

module.exports = {
  FreeList,
  symbols: {
    is_reused_symbol
  }
};
