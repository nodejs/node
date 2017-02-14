var jsonpointer = require('./')

var i
var obj = {
  a: 1,
  b: {
    c: 2
  },
  d: {
    e: [{ a: 3 }, { b: 4 }, { c: 5 }]
  }
}

// Get
console.time('get first level property')
for (i = 0; i < 1e6; i++) {
  jsonpointer.get(obj, '/a')
}
console.timeEnd('get first level property')

console.time('get second level property')
for (i = 0; i < 1e6; i++) {
  jsonpointer.get(obj, '/d/e')
}
console.timeEnd('get second level property')

console.time('get third level property')
for (i = 0; i < 1e6; i++) {
  jsonpointer.get(obj, '/d/e/0')
}
console.timeEnd('get third level property')

// Set
console.time('set first level property')
for (i = 0; i < 1e6; i++) {
  jsonpointer.set(obj, '/a', 'bla')
}
console.timeEnd('set first level property')

console.time('set second level property')
for (i = 0; i < 1e6; i++) {
  jsonpointer.set(obj, '/d/e', 'bla')
}
console.timeEnd('set second level property')

console.time('set third level property')
for (i = 0; i < 1e6; i++) {
  jsonpointer.set(obj, '/d/e/0', 'bla')
}
console.timeEnd('set third level property')

console.time('push property into array')
for (i = 0; i < 1e6; i++) {
  jsonpointer.set(obj, '/d/e/-', 'bla')
}
console.timeEnd('push property into array')
