estraverse-fb
=============
[![Build Status](https://travis-ci.org/RReverser/estraverse-fb.svg?branch=master)](https://travis-ci.org/RReverser/estraverse-fb)

Drop-in for estraverse that enables traversal over React's JSX and Flow nodes using monkey-patching technique.

You can use estraverse-fb in two possible ways:

* by default, you just require it and it injects needed keys into your installed version of estraverse (it's installed automatically if you don't have it yet):
    ```javascript
    var estraverse = require('estraverse-fb');
    /* same as:
        require('estraverse-fb');
        var estraverse = require('estraverse');
    */

    estraverse.traverse(ast, {
        enter: ...,
        leave: ...
    });
    ```

* alternatively, you can use it manually for selected traversals:
    ```javascript
    var jsxKeys = require('estraverse-fb/keys');

    estraverse.traverse(ast, {
        enter: ...,
        leave: ...,
        keys: jsxKeys
    })
```

Check out [estraverse page](https://github.com/Constellation/estraverse) for detailed usage.
