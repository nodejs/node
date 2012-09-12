vm-browserify
=============

Emulate node's vm module for the browser.

example
=======

Just write some client-side javascript:

``` js
var vm = require('vm');

$(function () {
    var res = vm.runInNewContext('a + 5', { a : 100 });
    $('#res').text(res);
});
```

compile it with [browserify](http://github.com/substack/node-browserify):

```
browserify entry.js -o bundle.js
```

then whip up some html:

``` html
<html>
  <head>
    <script src="http://ajax.googleapis.com/ajax/libs/jquery/1.7.1/jquery.min.js"></script>
    <script src="/bundle.js"></script>
  </head>
  <body>
    result = <span id="res"></span>
  </body>
</html>
```

and when you load the page you should see:

```
result = 105
```

methods
=======

vm.runInNewContext(code, context={})
------------------------------------

Evaluate some `code` in a new iframe with a `context`.

Contexts are like wrapping your code in a `with()` except slightly less terrible
because the code is sandboxed into a new iframe.

browser compatability
=====================

All modern browsers are supported.

If you have a [browserling](http://browserling.com) account,
from the testling/ directory just do:

```
$ ./test.sh substack@gmail.com
Enter host password for user 'substack@gmail.com':
chrome/17.0:
  vmRunInNewContext ................................. 5/5

iexplore/9.0:
  vmRunInNewContext ................................. 5/5

firefox/10.0:
  vmRunInNewContext ................................. 5/5

safari/5.1:
  vmRunInNewContext ................................. 5/5

opera/11.6:
  vmRunInNewContext ................................. 5/5

total ............................................... 25/25
```

In IE8 and IE7, running `vm.runInNewContext()` more than once can cause the
browsers to hang for some reason. Otherwise they work too.

install
=======

This module is depended upon by browserify, so you should just be able to
`require('vm')` and it will just work. However if you want to use this module
directly you can install it with [npm](http://npmjs.org):

```
npm install vm-browserify
```

license
=======

MIT/X11
