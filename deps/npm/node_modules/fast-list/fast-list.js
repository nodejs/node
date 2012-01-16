;(function() { // closure for web browsers

function Item (data, prev, next) {
  this.next = next
  if (next) next.prev = this
  this.prev = prev
  if (prev) prev.next = this
  this.data = data
}

function FastList () {
  if (!(this instanceof FastList)) return new FastList
  this._head = null
  this._tail = null
  this.length = 0
}

FastList.prototype =
{ push: function (data) {
    this._tail = new Item(data, this._tail, null)
    if (!this._head) this._head = this._tail
    this.length ++
  }

, pop: function () {
    if (this.length === 0) return undefined
    var t = this._tail
    this._tail = t.prev
    if (t.prev) {
      t.prev = this._tail.next = null
    }
    this.length --
    if (this.length === 1) this._head = this._tail
    else if (this.length === 0) this._head = this._tail = null
    return t.data
  }

, unshift: function (data) {
    this._head = new Item(data, null, this._head)
    if (!this._tail) this._tail = this._head
    this.length ++
  }

, shift: function () {
    if (this.length === 0) return undefined
    var h = this._head
    this._head = h.next
    if (h.next) {
      h.next = this._head.prev = null
    }
    this.length --
    if (this.length === 1) this._tail = this._head
    else if (this.length === 0) this._head = this._tail = null
    return h.data
  }

, item: function (n) {
    if (n < 0) n = this.length + n
    var h = this._head
    while (n-- > 0 && h) h = h.next
    return h ? h.data : undefined
  }

, slice: function (n, m) {
    if (!n) n = 0
    if (!m) m = this.length
    if (m < 0) m = this.length + m
    if (n < 0) n = this.length + n

    if (m === n) {
      return []
    }

    if (m < n) {
      throw new Error("invalid offset: "+n+","+m+" (length="+this.length+")")
    }

    var len = m - n
      , ret = new Array(len)
      , i = 0
      , h = this._head
    while (n-- > 0 && h) h = h.next
    while (i < len && h) {
      ret[i++] = h.data
      h = h.next
    }
    return ret
  }

, drop: function () {
    FastList.call(this)
  }

, forEach: function (fn, thisp) {
    var p = this._head
      , i = 0
      , len = this.length
    while (i < len && p) {
      fn.call(thisp || this, p.data, i, this)
      p = p.next
      i ++
    }
  }

, map: function (fn, thisp) {
    var n = new FastList()
    this.forEach(function (v, i, me) {
      n.push(fn.call(thisp || me, v, i, me))
    })
    return n
  }

, filter: function (fn, thisp) {
    var n = new FastList()
    this.forEach(function (v, i, me) {
      if (fn.call(thisp || me, v, i, me)) n.push(v)
    })
    return n
  }

, reduce: function (fn, val, thisp) {
    var i = 0
      , p = this._head
      , len = this.length
    if (!val) {
      i = 1
      val = p && p.data
      p = p && p.next
    }
    while (i < len && p) {
      val = fn.call(thisp || this, val, p.data, this)
      i ++
      p = p.next
    }
    return val
  }
}

if ("undefined" !== typeof(exports)) module.exports = FastList
else if ("function" === typeof(define) && define.amd) {
  define("FastList", function() { return FastList })
} else (function () { return this })().FastList = FastList

})()
