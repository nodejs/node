;(function () { // closure for web browsers

if (module) {
  module.exports = LRUCache
} else {
  // just set the global for non-node platforms.
  ;(function () { return this })().LRUCache = LRUCache
}

function hOP (obj, key) {
  return Object.prototype.hasOwnProperty.call(obj, key)
}

function naiveLength () { return 1 }

function LRUCache (maxLength, lengthCalculator) {
  if (!(this instanceof LRUCache)) {
    return new LRUCache(maxLength, lengthCalculator)
  }

  if (typeof lengthCalculator !== "function") {
    lengthCalculator = naiveLength
  }
  if (!maxLength || !(typeof maxLength === "number") || maxLength <= 0 ) {
    maxLength = Infinity
  }


  var cache = {} // hash of items by key
    , lruList = {} // list of items in order of use recency
    , lru = 0 // least recently used
    , mru = 0 // most recently used
    , length = 0 // number of items in the list
    , itemCount = 0


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

  // resize the cache when the lengthCalculator changes.
  Object.defineProperty(this, "lengthCalculator",
    { set : function (lC) {
        if (typeof lC !== "function") {
          lengthCalculator = naiveLength
          length = itemCount
          Object.keys(cache).forEach(function (key) {
            cache[key].length = 1
          })
        } else {
          lengthCalculator = lC
          length = 0
          Object.keys(cache).forEach(function (key) {
            cache[key].length = lengthCalculator(cache[key].value)
            length += cache[key].length
          })
        }

        if (length > maxLength) trim()
      }
    , get : function () { return lengthCalculator }
    , enumerable : true
    })

  Object.defineProperty(this, "length",
    { get : function () { return length }
    , enumerable : true
    })


  Object.defineProperty(this, "itemCount",
    { get : function () { return itemCount }
    , enumerable : true
    })

  this.reset = function () {
    cache = {}
    lruList = {}
    lru = 0
    mru = 0
    length = 0
    itemCount = 0
  }

  // Provided for debugging/dev purposes only. No promises whatsoever that
  // this API stays stable.
  this.dump = function () {
    return cache
  }

  this.set = function (key, value) {
    if (hOP(cache, key)) {
      this.get(key)
      cache[key].value = value
      return
    }

    var hit = {key:key, value:value, lu:mru++, length:lengthCalculator(value)}

    // oversized objects fall out of cache automatically.
    if (hit.length > maxLength) return

    length += hit.length
    lruList[hit.lu] = cache[key] = hit
    itemCount ++

    if (length > maxLength) trim()
  }

  this.get = function (key) {
    if (!hOP(cache, key)) return
    var hit = cache[key]
    delete lruList[hit.lu]
    if (hit.lu === lru) lruWalk()
    hit.lu = mru ++
    lruList[hit.lu] = hit
    return hit.value
  }

  this.del = function (key) {
    if (!hOP(cache, key)) return
    var hit = cache[key]
    delete cache[key]
    delete lruList[hit.lu]
    if (hit.lu === lru) lruWalk()
    length -= hit.length
    itemCount --
  }

  function lruWalk () {
    // lru has been deleted, hop up to the next hit.
    lru = Object.keys(lruList)[0]
  }

  function trim () {
    if (length <= maxLength) return
    var prune = Object.keys(lruList)
    for (var i = 0; i < prune.length && length > maxLength; i ++) {
      length -= lruList[prune[i]].length
      delete cache[ lruList[prune[i]].key ]
      delete lruList[prune[i]]
    }
    lruWalk()
  }
}

})()
