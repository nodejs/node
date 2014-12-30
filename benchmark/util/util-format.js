// Copyright (c) 2014, Ben Noordhuis <info@bnoordhuis.nl>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

'use strict';

var common = require('../common.js');
var format = require('util').format;

var bench = common.createBenchmark(main, {});

function main(conf) {
  var fmts = [];

  for (var i = 0; i < 26; i += 1) {
    var k = i, fmt = '';
    do {
      fmt += '%' + String.fromCharCode(97 + k);
      k = (k + 1) % 26;
    } while (k != i);
    fmts.push(fmt);
  }

  bench.start();

  for (var i = 0; i < 1e5; i += 1)
    for (var k = 0; k < fmts.length; k += 1)
      format(fmts[k]);

  bench.end(1e5);
}
