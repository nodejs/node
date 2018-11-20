# some

Short-circuited async Array.prototype.some implementation.

Serially evaluates a list of values from a JS array or arraylike
against an asynchronous predicate, terminating on the first truthy
value. If the predicate encounters an error, pass it to the completion
callback. Otherwise, pass the truthy value passed by the predicate, or
`false` if no truthy value was passed.

Is
[Zalgo](http://blog.izs.me/post/59142742143/designing-apis-for-asynchrony)-proof,
browser-safe, and pretty efficient.

## Usage

```javascript
var some = require("async-some");
var resolve = require("path").resolve;
var stat = require("fs").stat;
var readFileSync = require("fs").readFileSync;

some(["apple", "seaweed", "ham", "quince"], porkDetector, function (error, match) {
  if (error) return console.error(error);

  if (match) return console.dir(JSON.parse(readFileSync(match)));

  console.error("time to buy more Sporkle™!");
});

var PREFIX = resolve(__dirname, "../pork_store");
function porkDetector(value, cb) {
  var path = resolve(PREFIX, value + ".json");
  stat(path, function (er, stat) {
    if (er) {
      if (er.code === "ENOENT") return cb(null, false);

      return cb(er);
    }

    cb(er, path);
  });
}
```

### some(list, test, callback)

* `list` {Object} An arraylike (either an Array or the arguments arraylike) to
  be checked.
* `test` {Function} The predicate against which the elements of `list` will be
  tested. Takes two parameters:
  * `element` {any} The element of the list to be tested.
  * `callback` {Function} The continuation to be called once the test is
    complete. Takes (again) two values:
    * `error` {Error} Any errors that the predicate encountered.
    * `value` {any} A truthy value. A non-falsy result terminates checking the
      entire list.
* `callback` {Function} The callback to invoke when either a value has been
  found or the entire input list has been processed with no result. Is invoked
  with the traditional two parameters:
  * `error` {Error} Errors that were encountered during the evaluation of some().
  * `match` {any} Value successfully matched by `test`, if any.
