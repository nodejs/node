recipes
=======

Here are some recipe-style examples for getting started.

use an npm module in the browser
================================

First install a module:

```
npm install traverse
```

Then write an `entry.js`:

````javascript
var traverse = require('traverse');
var obj = traverse({ a : 3, b : [ 4, 5 ] }).map(function (x) {
    if (typeof x === 'number') this.update(x * 100)
});
console.dir(obj);
````

now bundle it!

```
$ browserify entry.js -o bundle.js
```

then put it in your html

``` html
<script src="bundle.js"></script>
```

and the entry.js will just run and `require('traverse')` will just work™.

convert a node module into a browser require-able standalone file
-----------------------------------------------------------------

Install the `traverse` package into `./node_modules`:

```
npm install traverse
```

Bundle everything up with browserify:

```
$ npm install -g browserify
$ browserify -r traverse -o bundle.js
```

Look at the files! There is a new one: `bundle.js`. Now go into HTML land:

``` html
<script src="bundle.js"></script>
<script>
   var traverse = require('traverse');
</script>
```
