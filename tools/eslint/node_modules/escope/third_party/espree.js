/*
  Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>

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

var espree = require('espree');

module.exports = function (code) {
    return espree.parse(code, {

        // attach range information to each node
        range: true,

        // attach line/column location information to each node
        loc: true,

        // create a top-level comments array containing all comments
        comments: true,

        // attach comments to the closest relevant node as leadingComments and
        // trailingComments
        attachComment: true,

        // create a top-level tokens array containing all tokens
        tokens: true,

        // try to continue parsing if an error is encountered, store errors in a
        // top-level errors array
        tolerant: true,

        // specify parsing features (default only has blockBindings: true)
        ecmaFeatures: {

            // enable parsing of arrow functions
            arrowFunctions: true,

            // enable parsing of let/const
            blockBindings: true,

            // enable parsing of destructured arrays and objects
            destructuring: true,

            // enable parsing of regular expression y flag
            regexYFlag: true,

            // enable parsing of regular expression u flag
            regexUFlag: true,

            // enable parsing of template strings
            templateStrings: true,

            // enable parsing of binary literals
            binaryLiterals: true,

            // enable parsing of ES6 octal literals
            octalLiterals: true,

            // enable parsing unicode code point escape sequences
            unicodeCodePointEscapes: true,

            // enable parsing of default parameters
            defaultParams: true,

            // enable parsing of rest parameters
            restParams: true,

            // enable parsing of for-of statement
            forOf: true,

            // enable parsing computed object literal properties
            objectLiteralComputedProperties: true,

            // enable parsing of shorthand object literal methods
            objectLiteralShorthandMethods: true,

            // enable parsing of shorthand object literal properties
            objectLiteralShorthandProperties: true,

            // Allow duplicate object literal properties (except '__proto__')
            objectLiteralDuplicateProperties: true,

            // enable parsing of generators/yield
            generators: true,

            // enable parsing spread operator
            spread: true,

            // enable parsing classes
            classes: true,

            // enable parsing of modules
            modules: true,

            // enable React JSX parsing
            jsx: true,

            // enable return in global scope
            globalReturn: true
        }
    });
};

/* vim: set sw=4 ts=4 et tw=80 : */
