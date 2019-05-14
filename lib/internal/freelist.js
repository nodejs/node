'use strict';

const { Reflect } = primordials;

class FreeList {
  constructor(name, max, ctor) {
    this.name = name;
    this.ctor = ctor;
    this.max = max;
    this.list = [];
  }

  hasItems() {
    return this.list.length > 0;
  }

  alloc() {
    return this.list.length > 0 ?
      this.list.pop() :
      Reflect.apply(this.ctor, this, arguments);
  }

  free(obj) {
    if (this.list.length < this.max) {
      this.list.push(obj);
      return true;
    }
    return false;
  }
}

module.exports = FreeList;
