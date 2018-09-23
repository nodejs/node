/*
  Copyright (C) 2014 Yusuke Suzuki <utatane.tea@gmail.com>

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
/*global require describe it*/
/*jslint node:true */
'use strict';

var fs = require('fs'),
    path = require('path'),
    root = path.join(path.dirname(fs.realpathSync(__filename)), '..'),
    doctrine = require(root);
require('should');

describe('strict parse', function () {
    // https://github.com/Constellation/doctrine/issues/21
    it('unbalanced braces', function () {
        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * @param {const",
                    " */"
                ].join('\n'), { unwrap: true, strict: true });
        }).should.throw('Braces are not balanced');

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * @param {const",
                    " */"
                ].join('\n'), { unwrap: true });
        }).should.not.throw();

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @param {string name Param description",
                    " * @param {int} foo Bar",
                    " */"
                ].join('\n'), { unwrap: true, strict: true });
        }).should.throw('Braces are not balanced');

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @param {string name Param description",
                    " * @param {int} foo Bar",
                    " */"
                ].join('\n'), { unwrap: true });
        }).should.not.throw();

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @returns {int",
                    " */"
                ].join('\n'), { unwrap: true, strict: true });
        }).should.throw('Braces are not balanced');

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @returns {int",
                    " */"
                ].join('\n'), { unwrap: true });
        }).should.not.throw();
    });

    // https://github.com/Constellation/doctrine/issues/21
    it('incorrect tag starting with @@', function () {
        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * @@version",
                    " */"
                ].join('\n'), { unwrap: true, strict: true });
        }).should.throw('Missing or invalid title');

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * @@version",
                    " */"
                ].join('\n'), { unwrap: true });
        }).should.not.throw();

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @@param {string} name Param description",
                    " */"
                ].join('\n'), { unwrap: true, strict: true });
        }).should.throw('Missing or invalid title');

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @@param {string} name Param description",
                    " */"
                ].join('\n'), { unwrap: true });
        }).should.not.throw();

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @kind ng",
                    " */"
                ].join('\n'), { unwrap: true, strict: true });
        }).should.throw('Invalid kind name \'ng\'');

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @variation Animation",
                    " */"
                ].join('\n'), { unwrap: true, strict: true });
        }).should.throw('Invalid variation \'Animation\'');

        (function () {
            doctrine.parse(
                [
                    "/**",
                    " * Description",
                    " * @access ng",
                    " */"
                ].join('\n'), { unwrap: true, strict: true });
        }).should.throw('Invalid access name \'ng\'');
    });
});
