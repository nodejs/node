process-nexttick-args
=====

[![Build Status](https://travis-ci.org/calvinmetcalf/process-nextick-args.svg?branch=master)](https://travis-ci.org/calvinmetcalf/process-nextick-args)

```bash
npm install --save process-nexttick-args
```

Always be able to pass arguments to process.nextTick, no matter the platform

```js
var nextTick = require('process-nexttick-args');

nextTick(function (a, b, c) {
  console.log(a, b, c);
}, 'step', 3,  'profit');
```
