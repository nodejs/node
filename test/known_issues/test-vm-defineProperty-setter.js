// Refs:https://github.com/nodejs/node/pull/38557

const assert = require('assert');
const vm = require('vm')
const context = {
    console,
    setter: 0,
    assert,
}
vm.createContext(context)
vm.runInContext(`
  var global = (function() {return this})()
  Object.defineProperty(this, "test", {
    get: function get() {
        
    },
    set: function set(newValue) {
      global.setter++
      assert.strictEqual(global === this, false);
    }
  })
  this.test = 1
`, context)

assert.strictEqual(context.setter, 1);