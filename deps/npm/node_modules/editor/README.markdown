editor
======

Launch $EDITOR in your program.

example
=======

``` js
var editor = require('editor');
editor('beep.json', function (code, sig) {
    console.log('finished editing with code ' + code);
});
```

***

```
$ node edit.js
```

![editor](http://substack.net/images/screenshots/editor.png)

```
finished editing with code 0
```

methods
=======

``` js
var editor = require('editor')
```

editor(file, opts={}, cb)
-------------------------

Launch the `$EDITOR` (or `opts.editor`) for `file`.

When the editor exits, `cb(code, sig)` fires.

install
=======

With [npm](http://npmjs.org) do:

```
npm install editor
```

license
=======

MIT
