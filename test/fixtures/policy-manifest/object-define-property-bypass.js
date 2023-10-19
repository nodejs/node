let requires = new WeakMap()
Object.defineProperty(Object.getPrototypeOf(module), 'require', {
  get() {
    return requires.get(this);
  },
  set(v) {
    requires.set(this, v);
    process.nextTick(() => {
      let fs = Reflect.apply(v, this, ['fs'])
      if (typeof fs.readFileSync === 'function') {
        process.exit(1);
      }
    })
    return requires.get(this);
  },
  configurable: true
})

require('./valid-module')
