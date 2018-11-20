/*
  Copyright (C) 2012 Yusuke Suzuki <utatane.tea@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*jslint node:true */
'use strict';

var fs = require('fs'),
    path = require('path'),
    root = path.join(path.dirname(fs.realpathSync(__filename)), '..'),
    doctrine = require(root);
require('should');

describe('unwrapComment', function () {
  it('normal', function () {
    doctrine.unwrapComment('/**\n * @const\n * @const\n */').should.equal('\n@const\n@const\n');
  });

  it('single', function () {
    doctrine.unwrapComment('/**x*/').should.equal('x');
  });

  it('more stars', function () {
    doctrine.unwrapComment('/***x*/').should.equal('x');
    doctrine.unwrapComment('/****x*/').should.equal('*x');
  });

  it('2 lines', function () {
    doctrine.unwrapComment('/**x\n * y\n*/').should.equal('x\ny\n');
  });

  it('2 lines with space', function () {
    doctrine.unwrapComment('/**x\n *    y\n*/').should.equal('x\n   y\n');
  });

  it('3 lines with blank line', function () {
    doctrine.unwrapComment('/**x\n *\n \* y\n*/').should.equal('x\n\ny\n');
  });
});
/* vim: set sw=4 ts=4 et tw=80 : */
