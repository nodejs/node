// Copyright (C) 2011 by Ben Noordhuis <info@bnoordhuis.nl>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

punycode = require('punycode');
assert = require('assert');

assert.equal(punycode.encode('ü'), 'tda');
assert.equal(punycode.encode('Goethe'), 'Goethe-');
assert.equal(punycode.encode('Bücher'), 'Bcher-kva');
assert.equal(punycode.encode(
    'Willst du die Blüthe des frühen, die Früchte des späteren Jahres'),
    'Willst du die Blthe des frhen, die Frchte des spteren Jahres-x9e96lkal');
assert.equal(punycode.encode('日本語'), 'wgv71a119e');

assert.equal(punycode.decode('tda'), 'ü');
assert.equal(punycode.decode('Goethe-'), 'Goethe');
assert.equal(punycode.decode('Bcher-kva'), 'Bücher');
assert.equal(punycode.decode(
    'Willst du die Blthe des frhen, die Frchte des spteren Jahres-x9e96lkal'),
    'Willst du die Blüthe des frühen, die Früchte des späteren Jahres');
assert.equal(punycode.decode('wgv71a119e'), '日本語');
