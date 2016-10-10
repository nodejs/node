#### caller

Figure out your caller (thanks to @substack).

##### Initialization Time Caller
```javascript
// foo.js

var bar = require('bar');
```

```javascript
// bar.js

var caller = require('caller');
console.log(caller()); // `/path/to/foo.js`
```

##### Runtime Caller
```javascript
// foo.js

var bar = require('bar');
bar.doWork();
```

```javascript
// bar.js

var caller = require('caller');

exports.doWork = function () {
    console.log(caller());  // `/path/to/foo.js`
};
```

### Depth

Caller also accepts a `depth` argument for tracing back further (defaults to `1`).

```javascript
// foo.js

var bar = require('bar');
bar.doWork();
```

```javascript
// bar.js

var baz = require('baz');

exports.doWork = function () {
    baz.doWork();
};
```

```javascript
// baz.js

var caller = require('caller');

exports.doWork = function () {
    console.log(caller(2));  // `/path/to/foo.js`
};
```
