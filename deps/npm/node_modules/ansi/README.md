ansi.js
=========
### Advanced ANSI formatting tool for Node.js

`ansi.js` is a module for Node.js that provides an easy-to-use API for
writing ANSI escape codes to `Stream` instances. ANSI escape codes are used to do
fancy things in a terminal window, like render text in colors, delete characters,
lines, the entire window, or hide and show the cursor, and lots more!

The code for the example in the screenshot above can be found in the
`examples/imgcat` directory.

#### Features:

 * 256 color support for the terminal!
 * Make a beep sound from your terminal!
 * Works with *any* writable `Stream` instance.
 * Allows you to move the cursor anywhere on the terminal window.
 * Allows you to delete existing contents from the terminal window.
 * Allows you to hide and show the cursor.
 * Converts CSS color codes and RGB values into ANSI escape codes.
 * Low-level; you are in control of when escape codes are used, it's not abstracted.


Installation
------------

Install with `npm`:

``` bash
$ npm install ansi
```


Example
-------

``` js
var ansi = require('ansi')
  , cursor = ansi(process.stdout)

// You can chain your calls forever:
cursor
  .red()                 // Set font color to red
  .bg.grey()             // Set background color to grey
  .write('Hello World!') // Write 'Hello World!' to stdout
  .bg.reset()            // Reset the bgcolor before writing the trailing \n,
                         //      to avoid Terminal glitches
  .write('\n')           // And a final \n to wrap things up

// Rendering modes are persistent:
cursor.hex('#660000').bold().underline()

// You can use the regular logging functions, text will be green
console.log('This is blood red, bold text')

// To reset just the foreground color:
cursor.fg.reset()

console.log('This will still be bold')

// Clean up after yourself!
cursor.reset()
```


License
-------

(The MIT License)

Copyright (c) 2012 Nathan Rajlich &lt;nathan@tootallnate.net&gt;

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
