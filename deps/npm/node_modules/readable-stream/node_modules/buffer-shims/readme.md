buffer-shims
===

functions to make sure the new buffer methods work in older browsers.

```js
var bufferShim = require('buffer-shims');
bufferShim.from('foo');
bufferShim.alloc(9, 'cafeface', 'hex');
bufferShim.allocUnsafe(15);
bufferShim.allocUnsafeSlow(21);
```

should just use the original  in newer nodes and on older nodes uses fallbacks.

Known Issues
===
- this does not patch the buffer object, only the constructor stuff
- it's actually a polyfill

![](https://i.imgur.com/zxII3jJ.gif)
