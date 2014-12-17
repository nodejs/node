// Copyright (c) 2014, StrongLoop Inc.
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

var common = require('../common');
var assert = require('assert');
var v8 = require('v8');
var vm = require('vm');

v8.setFlagsFromString('--allow_natives_syntax');
assert(eval('%_IsSmi(42)'));
assert(vm.runInThisContext('%_IsSmi(42)'));

v8.setFlagsFromString('--noallow_natives_syntax');
assert.throws(function() { eval('%_IsSmi(42)') }, SyntaxError);
assert.throws(function() { vm.runInThisContext('%_IsSmi(42)') }, SyntaxError);
