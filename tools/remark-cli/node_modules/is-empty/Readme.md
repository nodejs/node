
# is-empty

  Check whether a value is empty.

## Installation
  
```
$ npm install is-empty
$ npm test
```

## Example

```js
var empty = require('is-empty');

empty([]);              // true
empty({});              // true
empty('');              // true
empty(0);               // true
empty(function(){});    // true
empty(null);            // true
empty(undefined);       // true
empty(new Map());       // true
empty(new Set());       // true
empty(new Error());     // true

empty(true);            // false
empty(false);           // false
empty(['a', 'b']);      // false
empty({ a: 'b' });      // false
empty('string');        // false
empty(42);              // false
empty(function(a,b){}); // false
empty(new Map([['key', 'value']])); // false
empty(new Set([1]));    // false
empty(new Error('fail'))// false
```

## API

### isEmpty(value)

  Check whether `value` is empty.

## License

  MIT
