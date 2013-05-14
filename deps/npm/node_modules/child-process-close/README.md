
# child-process-close

This module makes child process objects, (created with `spawn`, `fork`, `exec`
or `execFile`) emit the `close` event in node v0.6 like they do in node v0.8.
This makes it easier to write code that works correctly on both version of
node.


## Usage

Just make sure to `require('child-process-close')` anywhere. It will patch the `child_process` module.

```js
require('child-process-close');
var spawn = require('child_process').spawn;

var cp = spawn('foo');
cp.on('close', function(exitCode, signal) {
  // This now works on all node versions.
});
```


## License

Copyright (C) 2012 Bert Belder

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
