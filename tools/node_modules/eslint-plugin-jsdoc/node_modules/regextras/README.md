# regextras

Array extras for regular expressions.

Also provides optional `String` and `RegExp` prototype extensions.

No more writing the implementation-detail-leaking, non-semantic, and
otherwise ugly:

```js
let matches;
while ((matches = regex.exec(str)) !== null) {
  // Do something
  if (condition) {
    break;
  }
}
```

While all of the array extras could be useful, `some`, might be the most
general purpose as it (as with `every`) allows short-circuiting (breaking).

The following is equivalent to that above (though with `matches` as local):

```js
RegExtras(regex).some(str, (matches) => {
  // Do something
  if (condition) {
    return true;
  }
  return false;
});
```

And if the condition is at the end of the loop, just this:

```js
RegExtras(regex).some(str, (matches) => {
  // Do something
  return condition;
});
```

## Installation

Node:

```js
const RegExtras = require('regextras');
```

Modern browsers:

```js
import {RegExtras} from './node_modules/regextras/dist/index-es.js';
```

Older browsers:

```html
<script src="regextras/dist/index-umd.js"></script>
```

The prototype versions must be required or included separately.

If you need the generator methods, you should also add the following:

```html
<script src="regextras/dist/index-generators-umd.js"></script>
```

## API

### Constructor

`new RegExtras(regex, flags, newLastIndex)`

Example:

```js
const piglatinArray = RegExtras(/\w*w?ay/).reduce('ouyay areway illysay', function (arr, i, word) {
  if (word.endsWith('way')) { arr.push(word.replace(/way$/, '')); } else { arr.push(word.slice(-3, -2) + word.slice(0, -3)); }
  return arr;
}, []);
```

All arguments but the first are optional, and the first argument can be
expressed as a string.

The `new` keywords is not required.

### Instance methods

These methods (and their callbacks) behave like the [array extra](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array#Iteration_methods)
to which they correspond with exceptions detailed below.

-   ***forEach(str, callback, thisObject)*** - Unlike the other extras, this
    method returns the RegExtras object (to enable chaining).

-   ***some(str, callback, thisObject)***

-   ***every(str, callback, thisObject)***

-   ***map(str, callback, thisObject)***

-   ***filter(str, callback, thisObject)***

-   ***reduce(str, cb, prev, thisObj)*** - Unlike the array extras, allows a
    fourth argument to set an alternative value for `this` within the callback.

-   ***reduceRight(str, cb, prev, thisObj)*** - Unlike the array extras,
    allows a fourth argument to set an alternative value for `this` within
    the callback.

-   ***find(str, cb, thisObj)***

-   ***findIndex(str, cb, thisObj)***

Also adds the following methods:

-   ***findExec(str, cb, thisObj)*** - Operates like `find()` except that it
    returns the `exec` result array (with `index` and `input` as well as
    numeric properties as returned by [RegExp.prototype.exec](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/exec)).

-   ***filterExec(str, cb, thisObj)*** - Operates like `filter()` except that
    the resulting array will contain the full `exec` results.

If you are using the Node version (or if, for the browser, you add the
`index-generators.js` file and you are only supporting modern browsers), one
can use the following generator methods:

-   ***values(str)*** - Returns an iterator with the array of matches (for each
    `RegExp.prototype.exec` result)

-   ***keys(str)*** - Returns an iterator with 0-based indexes (from
    `RegExp.prototype.exec` result)

-   ***entries(str)*** - Returns an iterator with an array containing the
    key and the array of matches (for each `RegExp.prototype.exec` result)

### Class methods

-   ***mixinRegex(regex, newFlags='', newLastIndex=regex.lastIndex)*** -
    Makes a copy of a regular expression.

### Callbacks

All callbacks follow the signature:

`cb(n1, n2..., i, n0);`

...except for the `reduce` and `reduceRight` callbacks which follow:

`cb(prev, n1, n2..., i, n0);`

### Prototype versions

`String` and `RegExp` versions of the above methods are also available.

The `RegExp` prototype version acts in the same way as `RegExtra` just
without the need for a separate constructor call.

The `String` prototype version differs in that instead of the first argument
being a string, it is the regular expression.

## Todos

1.  Could add [Array accessor methods](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array#Accessor_methods)
    like `slice()`, with an additional supplied regular expression to gather
    the `exec` results into an array.

2.  Utilize `nodeunit` browser testing (and add `mixinRegex` tests)

    1. Convert nodeunit tests to ES6 modules running through babel-register?;
        streamline as sole set of tests, reconciling `test` with `tests` folder

3.  Add generators for prototype versions
