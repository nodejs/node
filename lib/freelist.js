// This is a free list to avoid creating so many of the same object.
exports.FreeList = function(name, max, constructor) {
  this.name = name;
  this.constructor = constructor;
  this.max = max;
  this.list = [];
};


exports.FreeList.prototype.alloc = function() {
  //debug("alloc " + this.name + " " + this.list.length);
  return this.list.length ? this.list.shift() :
                            this.constructor.apply(this, arguments);
};


exports.FreeList.prototype.free = function(obj) {
  //debug("free " + this.name + " " + this.list.length);
  if (this.list.length < this.max) {
    this.list.push(obj);
  }
};
