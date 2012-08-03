var test = require("tap").test
  , LRU = require("../")

test("basic", function (t) {
  var cache = new LRU(10)
  cache.set("key", "value")
  t.equal(cache.get("key"), "value")
  t.equal(cache.get("nada"), undefined)
  t.equal(cache.length, 1)
  t.equal(cache.maxLength, 10)
  t.end()
})

test("least recently set", function (t) {
  var cache = new LRU(2)
  cache.set("a", "A")
  cache.set("b", "B")
  cache.set("c", "C")
  t.equal(cache.get("c"), "C")
  t.equal(cache.get("b"), "B")
  t.equal(cache.get("a"), undefined)
  t.end()
})

test("lru recently gotten", function (t) {
  var cache = new LRU(2)
  cache.set("a", "A")
  cache.set("b", "B")
  cache.get("a")
  cache.set("c", "C")
  t.equal(cache.get("c"), "C")
  t.equal(cache.get("b"), undefined)
  t.equal(cache.get("a"), "A")
  t.end()
})

test("del", function (t) {
  var cache = new LRU(2)
  cache.set("a", "A")
  cache.del("a")
  t.equal(cache.get("a"), undefined)
  t.end()
})

test("maxLength", function (t) {
  var cache = new LRU(3)

  // test changing the maxLength, verify that the LRU items get dropped.
  cache.maxLength = 100
  for (var i = 0; i < 100; i ++) cache.set(i, i)
  t.equal(cache.length, 100)
  for (var i = 0; i < 100; i ++) {
    t.equal(cache.get(i), i)
  }
  cache.maxLength = 3
  t.equal(cache.length, 3)
  for (var i = 0; i < 97; i ++) {
    t.equal(cache.get(i), undefined)
  }
  for (var i = 98; i < 100; i ++) {
    t.equal(cache.get(i), i)
  }

  // now remove the maxLength restriction, and try again.
  cache.maxLength = "hello"
  for (var i = 0; i < 100; i ++) cache.set(i, i)
  t.equal(cache.length, 100)
  for (var i = 0; i < 100; i ++) {
    t.equal(cache.get(i), i)
  }
  // should trigger an immediate resize
  cache.maxLength = 3
  t.equal(cache.length, 3)
  for (var i = 0; i < 97; i ++) {
    t.equal(cache.get(i), undefined)
  }
  for (var i = 98; i < 100; i ++) {
    t.equal(cache.get(i), i)
  }
  t.end()
})

test("reset", function (t) {
  var cache = new LRU(10)
  cache.set("a", "A")
  cache.set("b", "B")
  cache.reset()
  t.equal(cache.length, 0)
  t.equal(cache.maxLength, 10)
  t.equal(cache.get("a"), undefined)
  t.equal(cache.get("b"), undefined)
  t.end()
})


// Note: `<cache>.dump()` is a debugging tool only. No guarantees are made
// about the format/layout of the response.
test("dump", function (t) {
  var cache = new LRU(10)
  var d = cache.dump();
  t.equal(Object.keys(d).length, 0, "nothing in dump for empty cache")
  cache.set("a", "A")
  var d = cache.dump()  // { a: { key: "a", value: "A", lu: 0 } }
  t.ok(d.a)
  t.equal(d.a.key, "a")
  t.equal(d.a.value, "A")
  t.equal(d.a.lu, 0)

  cache.set("b", "B")
  cache.get("b")
  d = cache.dump()
  t.ok(d.b)
  t.equal(d.b.key, "b")
  t.equal(d.b.value, "B")
  t.equal(d.b.lu, 2)

  t.end()
})


test("basic with weighed length", function (t) {
  var cache = new LRU(100, function (item) { return item.size } )
  cache.set("key", {val: "value", size: 50})
  t.equal(cache.get("key").val, "value")
  t.equal(cache.get("nada"), undefined)
  t.equal(cache.lengthCalculator(cache.get("key")), 50)
  t.equal(cache.length, 50)
  t.equal(cache.maxLength, 100)
  t.end()
})


test("weighed length item too large", function (t) {
  var cache = new LRU(10, function (item) { return item.size } )
  t.equal(cache.maxLength, 10)

  // should fall out immediately
  cache.set("key", {val: "value", size: 50})

  t.equal(cache.length, 0)
  t.equal(cache.get("key"), undefined)
  t.end()
})

test("least recently set with weighed length", function (t) {
  var cache = new LRU(8, function (item) { return item.length })
  cache.set("a", "A")
  cache.set("b", "BB")
  cache.set("c", "CCC")
  cache.set("d", "DDDD")
  t.equal(cache.get("d"), "DDDD")
  t.equal(cache.get("c"), "CCC")
  t.equal(cache.get("b"), undefined)
  t.equal(cache.get("a"), undefined)
  t.end()
})

test("lru recently gotten with weighed length", function (t) {
  var cache = new LRU(8, function (item) { return item.length })
  cache.set("a", "A")
  cache.set("b", "BB")
  cache.set("c", "CCC")
  cache.get("a")
  cache.get("b")
  cache.set("d", "DDDD")
  t.equal(cache.get("c"), undefined)
  t.equal(cache.get("d"), "DDDD")
  t.equal(cache.get("b"), "BB")
  t.equal(cache.get("a"), "A")
  t.end()
})

test("set returns proper booleans", function(t) {
  var cache = new LRU(5, function (item) { return item.length })
  t.equal(cache.set("a", "A"), true)

  // should return false for maxLength exceeded
  t.equal(cache.set("b", "donuts"), false)

  t.equal(cache.set("b", "B"), true)
  t.equal(cache.set("c", "CCCC"), true)
  t.end()
})

test("drop the old items", function(t) {
  var cache = new LRU(5, null, 50)

  cache.set("a", "A")

  setTimeout(function () {
    cache.set("b", "b")
    t.equal(cache.get("a"), "A")
  }, 25)

  setTimeout(function () {
    cache.set("c", "C")
    // timed out
    t.notOk(cache.get("a"))
  }, 51)

  setTimeout(function () {
    t.notOk(cache.get("b"))
    t.equal(cache.get("c"), "C")
  }, 90)

  setTimeout(function () {
    t.notOk(cache.get("c"))
    t.end()
  }, 155)
})
