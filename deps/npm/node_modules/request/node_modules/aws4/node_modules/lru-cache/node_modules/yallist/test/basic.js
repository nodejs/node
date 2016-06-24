var t  = require('tap')
var Yallist = require('../yallist.js')

var y = new Yallist(1,2,3,4,5)
var z = new Yallist([1,2,3,4,5])
t.similar(y, z, 'build from single list or args')

function add10 (i) {
  return i + 10
}
t.similar(y.map(add10).toArray(), [11, 12, 13, 14, 15])
t.similar(y.mapReverse(add10).toArray(), [15, 14, 13, 12, 11])

t.similar(y.map(add10).toArrayReverse(), [15, 14, 13, 12, 11])
t.isa(Yallist(1,2,3), 'Yallist')
t.equal(y.push(6, 7, 8), 8)
t.similar(y.toArray(), [1, 2, 3, 4, 5, 6, 7, 8])
y.pop()
y.shift()
y.unshift(100)

var expect = [100, 2, 3, 4, 5, 6, 7]
var expectReverse = [ 7, 6, 5, 4, 3, 2, 100 ]

t.similar(y.toArray(), expect)
t.equal(y.length, y.toArray().length)

t.test(function forEach (t) {
  t.plan(y.length * 2)
  y.forEach(function (item, i, list) {
    t.equal(item, expect[i])
    t.equal(list, y)
  })
})

t.test(function forEach (t) {
  t.plan(y.length * 5)
  var n = 0
  y.forEachReverse(function (item, i, list) {
    t.equal(item, expectReverse[n])
    t.equal(item, expect[i])
    t.equal(item, y.get(i))
    t.equal(item, y.getReverse(n))
    n += 1
    t.equal(list, y)
  })
})

t.equal(y.getReverse(100), undefined)

t.equal(y.get(9999), undefined)


function sum (a, b) { return a + b }
t.equal(y.reduce(sum), 127)
t.equal(y.reduce(sum, 100), 227)
t.equal(y.reduceReverse(sum), 127)
t.equal(y.reduceReverse(sum, 100), 227)

t.equal(Yallist().pop(), undefined)
t.equal(Yallist().shift(), undefined)

var x = Yallist()
x.unshift(1)
t.equal(x.length, 1)
t.similar(x.toArray(), [1])

// verify that y.toArray() returns an array and if we create a
// new Yallist from that array, we get a list matching 
t.similar(Yallist(y.toArray()), y)
t.similar(Yallist.apply(null, y.toArray()), y)

t.throws(function () {
  new Yallist().reduce(function () {})
}, {}, new TypeError('Reduce of empty list with no initial value'))
t.throws(function () {
  new Yallist().reduceReverse(function () {})
}, {}, new TypeError('Reduce of empty list with no initial value'))

var z = y.reverse()
t.equal(z, y)
t.similar(y.toArray(), expectReverse)
y.reverse()
t.similar(y.toArray(), expect)

var a = Yallist(1,2,3,4,5,6)
var cases = [
  [ [2, 4], [3, 4] ],
  [ [2, -4], [] ],
  [ [2, -2], [3, 4] ],
  [ [1, -2], [2, 3, 4] ],
  [ [-1, -2], [] ],
  [ [-5, -2], [2, 3, 4] ],
  [ [-99, 2], [1, 2] ],
  [ [5, 99], [6] ],
  [ [], [1,2,3,4,5,6] ]
]
t.test('slice', function (t) {
  t.plan(cases.length)
  cases.forEach(function (c) {
    t.test(JSON.stringify(c), function (t) {
      t.similar(a.slice.apply(a, c[0]), Yallist(c[1]))
      t.similar([].slice.apply(a.toArray(), c[0]), c[1])
      t.end()
    })
  })
})

t.test('sliceReverse', function (t) {
  t.plan(cases.length)
  cases.forEach(function (c) {
    var rev = c[1].slice().reverse()
    t.test(JSON.stringify([c[0], rev]), function (t) {
      t.similar(a.sliceReverse.apply(a, c[0]), Yallist(rev))
      t.similar([].slice.apply(a.toArray(), c[0]).reverse(), rev)
      t.end()
    })
  })
})

var inserter = Yallist(1,2,3,4,5)
inserter.unshiftNode(inserter.head.next)
t.similar(inserter.toArray(), [2,1,3,4,5])
inserter.unshiftNode(inserter.tail)
t.similar(inserter.toArray(), [5,2,1,3,4])
inserter.unshiftNode(inserter.head)
t.similar(inserter.toArray(), [5,2,1,3,4])

var single = Yallist(1)
single.unshiftNode(single.head)
t.similar(single.toArray(), [1])

inserter = Yallist(1,2,3,4,5)
inserter.pushNode(inserter.tail.prev)
t.similar(inserter.toArray(), [1,2,3,5,4])
inserter.pushNode(inserter.head)
t.similar(inserter.toArray(), [2,3,5,4,1])
inserter.unshiftNode(inserter.head)
t.similar(inserter.toArray(), [2,3,5,4,1])

single = Yallist(1)
single.pushNode(single.tail)
t.similar(single.toArray(), [1])

var swiped = Yallist(9,8,7)
inserter.unshiftNode(swiped.head.next)
t.similar(inserter.toArray(), [8,2,3,5,4,1])
t.similar(swiped.toArray(), [9,7])

swiped = Yallist(9,8,7)
inserter.pushNode(swiped.head.next)
t.similar(inserter.toArray(), [8,2,3,5,4,1,8])
t.similar(swiped.toArray(), [9,7])

swiped.unshiftNode(Yallist.Node(99))
t.similar(swiped.toArray(), [99,9,7])
swiped.pushNode(Yallist.Node(66))
t.similar(swiped.toArray(), [99,9,7,66])

var e = Yallist()
e.unshiftNode(Yallist.Node(1))
t.same(e.toArray(), [1])
e = Yallist()
e.pushNode(Yallist.Node(1))
t.same(e.toArray(), [1])

// steal them back, don't break the lists
swiped.unshiftNode(inserter.head)
t.same(swiped, Yallist(8,99,9,7,66))
t.same(inserter, Yallist(2,3,5,4,1,8))
swiped.unshiftNode(inserter.tail)
t.same(inserter, Yallist(2,3,5,4,1))
t.same(swiped, Yallist(8,8,99,9,7,66))


t.throws(function remove_foreign_node () {
  e.removeNode(swiped.head)
}, {}, new Error('removing node which does not belong to this list'))
t.throws(function remove_unlisted_node () {
  e.removeNode(Yallist.Node('nope'))
}, {}, new Error('removing node which does not belong to this list'))

e = Yallist(1,2)
e.removeNode(e.head)
t.same(e, Yallist(2))
e = Yallist(1,2)
e.removeNode(e.tail)
t.same(e, Yallist(1))
