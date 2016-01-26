Jodid25519 [![Build Status](https://secure.travis-ci.org/meganz/jodid25519.png)](https://travis-ci.org/meganz/jodid25519)
===================================================================================================================================

Javascript implementation of the Curve25519 and Ed25519 elliptic cryptography functions by Daniel J. Bernstein.

For the API, please consult the generated documentation under doc/ (you can run `make` to generate it).

To run the tests do the following on the console from the project's root directory:

    $ npm install
    $ make test


Contributors
------------

If you are one of the contributors and want to add yourself or change the information here, please do submit a pull request.   Contributors appear in no particular order.

### For the Curve25519 submodule

* [Graydon Hoare](https://github.com/graydon): suggested clamping the private key by default for increased safety and uniformity with other implementations.
* [liliakai](https://github.com/liliakai): spotted an unused argument in some of the functions
* [RyanC](https://github.com/ryancdotorg): removed dependency of a function to the Javascript Math library
* [Guy Kloss](https://github.com/pohutukawa): performance improvements through bit-shift operations, performance and conformance testing, documentation, compatibility with the npm package ecosystem, and more
* [Michele Bini](https://github.com/rev22): originally wrote the Javascript implementation


Copyright and MIT licensing
---------------------------

* Copyright (c) 2012 Ron Garret
* Copyright (c) 2007, 2013, 2014 Michele Bini <michele.bini@gmail.com>
* Copyright (c) 2014 Mega Limited

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
