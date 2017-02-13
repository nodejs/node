'use strict';
const { exports: moduleScoped } = module;

function topFn(a, b = false) {
  const l1 = a;
  let t = typeof l1;
  var v = t.length;
  debugger;
  return b || t || v || moduleScoped;
}

class Ctor {
  constructor(options) {
    this.options = options;
  }

  m() {
    const mLocal = this.options;
    topFn(this);
    return mLocal;
  }
}

(function () {
  const theOptions = { x: 42 };
  const arr = [theOptions];
  arr.forEach(options => {
    const obj = new Ctor(options);
    return obj.m();
  });
}());
