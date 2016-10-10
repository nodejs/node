# tmatch

This module exists to facilitate the `t.match()` method in
[`tap`](http://npm.im/tap).

It checks whether a value matches a given "pattern".  A pattern is an
object with a set of fields that must be in the test object, or a
regular expression that a test string must match, or any combination
thereof.

The algorithm is borrowed heavily from
[`only-shallow`](http://npm.im/only-shallow), with some notable
differences with respect to the handling of missing properties and the
way that regular expressions are compared to strings.

## usage

```javascript
var matches = require('tmatch')

if (!matches(testObject, pattern)) console.log("yay! diversity!");

// somewhat more realistic example..
http.get(someUrl).on('response', function (res) {
  var expect = {
    statusCode: 200,
    headers: {
      server: /express/
    }
  }

  if (!tmatch(res, expect)) {
    throw new Error('Expect 200 status code from express server')
  }
})
```

## details

Copied from the source, here are the details of `tmatch`'s algorithm:

1. If the object loosely equals the pattern, then return true.  Note
   that this covers object identity, some type coercion, and matching
   `null` against `undefined`.
2. If the object is a string, and the pattern is a RegExp, then return
   true if `pattern.test(object)`.
3. If the object is a string and the pattern is a non-empty string,
   then return true if the string occurs within the object.
5. If the object and the pattern are both Date objects, then return
   true if they represent the same date.
6. If the object is a Date object, and the pattern is a string, then
   return true if the pattern is parseable as a date that is the same
   date as the object.
7. If the object is an `arguments` object, or the pattern is an
   `arguments` object, then cast them to arrays and compare their
   contents.
8. If the pattern is the `Buffer` constructor, then return true if the
   object is a Buffer.
9. If the pattern is the `Function` constructor, then return true if
   the object is a function.
10. If the pattern is the String constructor, then return true if the
    pattern is a string.
11. If the pattern is the Boolean constructor, then return true if the
    pattern is a boolean.
12. If the pattern is the Array constructor, then return true if the
    pattern is an array.
13. If the pattern is any function, and then object is an object, then
    return true if the object is an `instanceof` the pattern.
14. At this point, if the object or the pattern are not objects, then
    return false (because they would have matched earlier).
15. If the object is a RegExp and the pattern is also a RegExp, return
    true if their source, global, multiline, lastIndex, and ignoreCase
    fields all match.
16. If the object is a buffer, and the pattern is also a buffer, then
    return true if they contain the same bytes.
17. At this point, both object and pattern are object type values, so
    compare their keys:
    1. Get list of all iterable keys in pattern and object.  If both
       are zero (two empty objects), return true.
    2. Check to see if this pattern and this object have been tested
       already (to handle cycles).  If so, return true, since the
       check higher up in the stack will catch any mismatch.
    3. For each key in the pattern, match it against the corresponding
       key in object.  Missing keys in object will be resolved to
       `undefined`, so it's possible to use `{foo:null}` as a pattern
       to ensure that the object *doesn't* have a `foo` property.

## license

ISC. Go nuts.
