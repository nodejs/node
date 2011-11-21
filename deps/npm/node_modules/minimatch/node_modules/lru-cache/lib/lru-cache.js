
module.exports = LRUCache

function hOP (obj, key) {
  return Object.prototype.hasOwnProperty.call(obj, key)
}

function LRUCache (maxLength) {
  if (!(this instanceof LRUCache)) {
    return new LRUCache(maxLength)
  }
  var cache = {} // hash of items by key
    , lruList = {} // list of items in order of use recency
    , lru = 0 // least recently used
    , mru = 0 // most recently used
    , length = 0 // number of items in the list

  // resize the cache when the maxLength changes.
  Object.defineProperty(this, "maxLength",
    { set : function (mL) {
        if (!mL || !(typeof mL === "number") || mL <= 0 ) mL = Infinity
        maxLength = mL
        // if it gets above double maxLength, trim right away.
        // otherwise, do it whenever it's convenient.
        if (length > maxLength) trim()
      }
    , get : function () { return maxLength }
    , enumerable : true
    })
  this.maxLength = maxLength
  Object.defineProperty(this, "length",
    { get : function () { return length }
    , enumerable : true
    })

  this.set = function (key, value) {
    if (hOP(cache, key)) {
      this.get(key)
      cache[key].value = value
      return undefined
    }
    var hit = {key:key, value:value, lu:mru++}
    lruList[hit.lu] = cache[key] = hit
    length ++
    if (length > maxLength) trim()
  }
  this.get = function (key) {
    if (!hOP(cache, key)) return undefined
    var hit = cache[key]
    delete lruList[hit.lu]
    if (hit.lu === lru) lruWalk()
    hit.lu = mru ++
    lruList[hit.lu] = hit
    return hit.value
  }
  this.del = function (key) {
    if (!hOP(cache, key)) return undefined
    var hit = cache[key]
    delete cache[key]
    delete lruList[hit.lu]
    if (hit.lu === lru) lruWalk()
    length --
  }
  function lruWalk () {
    // lru has been deleted, hop up to the next hit.
    lru = Object.keys(lruList).shift()
  }
  function trim () {
    if (length <= maxLength) return undefined
    var prune = Object.keys(lruList).slice(0, length - maxLength)
    for (var i = 0, l = (length - maxLength); i < l; i ++) {
      delete cache[ lruList[prune[i]].key ]
      delete lruList[prune[i]]
    }
    length = maxLength
    lruWalk()
  }
}

if (!process || !module || module !== process.mainModule) return undefined

var l = LRUCache(3)
  , assert = require("assert")

l.set(1, 1)
l.set(2, 1)
l.set(3, 1)
l.set(4, 1)
l.set(5, 1)
l.set(6, 1)

assert.equal(l.get(1), undefined)
assert.equal(l.get(2), undefined)
assert.equal(l.get(3), undefined)
assert.equal(l.get(4), 1)
assert.equal(l.get(5), 1)
assert.equal(l.get(6), 1)

// now keep re-getting the 6 so it remains the most recently used.
// in this case, we'll have 6, 7, 8, 9, 10, 11, so the ending length = 5
l.set(7, 1)
l.get(6)
l.set(8, 1)
l.get(6)
l.set(9, 1)
l.get(6)
l.set(10, 1)
l.get(6)
l.set(11, 1)
assert.equal(l.length, 3)
assert.equal(l.get(4), undefined)
assert.equal(l.get(5), undefined)
assert.equal(l.get(6), 1)
assert.equal(l.get(7), undefined)
assert.equal(l.get(8), undefined)
assert.equal(l.get(9), undefined)
assert.equal(l.get(10), 1)
assert.equal(l.get(11), 1)

// test changing the maxLength, verify that the LRU items get dropped.
l.maxLength = 100
for (var i = 0; i < 100; i ++) l.set(i, i)
assert.equal(l.length, 100)
for (var i = 0; i < 100; i ++) {
  assert.equal(l.get(i), i)
}
l.maxLength = 3
assert.equal(l.length, 3)
for (var i = 0; i < 97; i ++) {
  assert.equal(l.get(i), undefined)
}
for (var i = 98; i < 100; i ++) {
  assert.equal(l.get(i), i)
}

// now remove the maxLength restriction, and try again.
l.maxLength = "hello"
for (var i = 0; i < 100; i ++) l.set(i, i)
assert.equal(l.length, 100)
for (var i = 0; i < 100; i ++) {
  assert.equal(l.get(i), i)
}
// should trigger an immediate resize
l.maxLength = 3
assert.equal(l.length, 3)
for (var i = 0; i < 97; i ++) {
  assert.equal(l.get(i), undefined)
}
for (var i = 98; i < 100; i ++) {
  assert.equal(l.get(i), i)
}
