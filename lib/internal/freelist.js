'use strict';

class FreeList {
  constructor(name, max, ctor) {
    this.name = name;
    this.ctor = ctor;
    this.max = max;
    this.list = [];
  }

  alloc() {
    return this.list.length ?
      needsToCallAsyncReset(this.list.pop()) :
      mustNotCallAsyncReset(this.ctor.apply(this, arguments));
  }

  free(obj) {
    if (this.list.length < this.max) {
      this.list.push(obj);
      return true;
    }
    return false;
  }
}

function needsToCallAsyncReset(item) {
  item.needsAsyncReset = true;
  return item;
}

function mustNotCallAsyncReset(item) {
  item.needsAsyncReset = false;
  return item;
}

module.exports = FreeList;
