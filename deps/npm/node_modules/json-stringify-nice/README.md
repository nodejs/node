# json-stringify-nice

Stringify an object sorting scalars before objects, and defaulting to
2-space indent.

Sometimes you want to stringify an object in a consistent way, and for
human legibility reasons, you may want to put any non-object properties
ahead of any object properties, so that it's easier to track the nesting
level as you read through the object, but you don't want to have to be
meticulous about maintaining object property order as you're building up
the object, since it doesn't matter in code, it only matters in the output
file.  Also, it'd be nice to have it default to reasonable spacing without
having to remember to add `, null, 2)` to all your `JSON.stringify()`
calls.

If that is what you want, then this module is for you, because it does
all of that.

## USAGE

```js
const stringify = require('json-stringify-nice')
const obj = {
  z: 1,
  y: 'z',
  obj: { a: {}, b: 'x' },
  a: { b: 1, a: { nested: true} },
  yy: 'a',
}

console.log(stringify(obj))
/* output:
{
  "y": "z", <-- alphabetical sorting like whoa!
  "yy": "a",
  "z": 1,
  "a": { <-- a sorted before obj, because alphabetical, and both objects
    "b": 1,
    "a": {  <-- note that a comes after b, because it's an object
      "nested": true
    }
  },
  "obj": {
    "b": "x",
    "a": {}
  }
}
*/

// specify an array of keys if you have some that you prefer
// to be sorted in a specific order.  preferred keys come before
// any other keys, and in the order specified, but objects are
// still sorted AFTER scalars, so the preferences only apply
// when both values are objects or both are non-objects.
console.log(stringify(obj, ['z', 'yy', 'obj']))
/* output
{
  "z": 1, <-- z comes before other scalars
  "yy": "a", <-- yy comes after z, but before other scalars
  "y": "z", <-- then all the other scalar values
  "obj": { <-- obj comes before other objects, but after scalars
    "b": "x",
    "a": {}
  },
  "a": {
    "b": 1,
    "a": {
      "nested": true
    }
  }
}
*/

// can also specify a replacer or indent value like with JSON.stringify
// this turns all values with an 'a' key into a doggo meme from 2011
const replacer = (key, val) =>
  key === 'a' ? { hello: '📞 yes', 'this is': '🐕', ...val } : val

console.log(stringify(obj, replacer, '📞🐶'))

/* output:
{
📞🐶"y": "z",
📞🐶"yy": "a",
📞🐶"z": 1,
📞🐶"a": {
📞🐶📞🐶"b": 1,
📞🐶📞🐶"hello": "📞 yes",
📞🐶📞🐶"this is": "🐕",
📞🐶📞🐶"a": {
📞🐶📞🐶📞🐶"hello": "📞 yes",
📞🐶📞🐶📞🐶"nested": true,
📞🐶📞🐶📞🐶"this is": "🐕"
📞🐶📞🐶}
📞🐶},
📞🐶"obj": {
📞🐶📞🐶"b": "x",
📞🐶📞🐶"a": {
📞🐶📞🐶📞🐶"hello": "📞 yes",
📞🐶📞🐶📞🐶"this is": "🐕"
📞🐶📞🐶}
📞🐶}
}
*/
```
