
module.exports = ProtoList

function ProtoList () { this.list = [] }
ProtoList.prototype =
  { get length () { return this.list.length }
  , get keys () {
      var k = []
      for (var i in this.list[0]) k.push(i)
      return k
    }
  , get snapshot () {
      var o = {}
      this.keys.forEach(function (k) { o[k] = this.get(k) }, this)
      return o
    }
  , push : function (obj) {
      if (typeof obj !== "object") obj = {valueOf:obj}
      if (this.list.length >= 1) {
        this.list[this.list.length - 1].__proto__ = obj
      }
      obj.__proto__ = Object.prototype
      return this.list.push(obj)
    }
  , pop : function () {
      if (this.list.length >= 2) {
        this.list[this.list.length - 2].__proto__ = Object.prototype
      }
      return this.list.pop()
    }
  , unshift : function (obj) {
      obj.__proto__ = this.list[0] || Object.prototype
      return this.list.unshift(obj)
    }
  , shift : function () {
      if (this.list.length >= 1) {
        this.list[0].__proto__ = Object.prototype
      }
      return this.list.shift()
    }
  , get : function (key) {
      return this.list[0][key]
    }
  , set : function (key, val, save) {
      if (!this.length) this.push({})
      if (save && this.list[0].hasOwnProperty(key)) this.push({})
      return this.list[0][key] = val
    }
  , forEach : function (fn, thisp) {
      for (var key in this.list[0]) fn.call(thisp, key, this.list[0][key])
    }
  , slice : function () {
      return this.list.slice.apply(this.list, arguments)
    }
  , splice : function () {
      return this.list.splice.apply(this.list, arguments)
    }
  }

if (module === require.main) {

var tap = require("tap")
  , test = tap.test

tap.plan(1)

tap.test("protoList tests", function (t) {
  var p = new ProtoList
  p.push({foo:"bar"})
  p.push({})
  p.set("foo", "baz")
  t.equal(p.get("foo"), "baz")

  var p = new ProtoList
  p.push({foo:"bar"})
  p.set("foo", "baz")
  t.equal(p.get("foo"), "baz")
  t.equal(p.length, 1)
  p.pop()
  t.equal(p.length, 0)
  p.set("foo", "asdf")
  t.equal(p.length, 1)
  t.equal(p.get("foo"), "asdf")
  p.push({bar:"baz"})
  t.equal(p.length, 2)
  t.equal(p.get("foo"), "asdf")
  p.shift()
  t.equal(p.length, 1)
  t.equal(p.get("foo"), undefined)
  t.end()
})


}
