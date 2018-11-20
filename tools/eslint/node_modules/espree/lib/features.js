/**
 * @fileoverview The list of feature flags supported by the parser and their default
 *      settings.
 * @author Nicholas C. Zakas
 * @copyright 2015 Fred K. Schott. All rights reserved.
 * @copyright 2014 Nicholas C. Zakas. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

// None!

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------

module.exports = {

    // enable parsing of arrow functions
    arrowFunctions: false,

    // enable parsing of let and const
    blockBindings: true,

    // enable parsing of destructured arrays and objects
    destructuring: false,

    // enable parsing of regex u flag
    regexUFlag: false,

    // enable parsing of regex y flag
    regexYFlag: false,

    // enable parsing of template strings
    templateStrings: false,

    // enable parsing binary literals
    binaryLiterals: false,

    // enable parsing ES6 octal literals
    octalLiterals: false,

    // enable parsing unicode code point escape sequences
    unicodeCodePointEscapes: true,

    // enable parsing of default parameters
    defaultParams: false,

    // enable parsing of rest parameters
    restParams: false,

    // enable parsing of for-of statements
    forOf: false,

    // enable parsing computed object literal properties
    objectLiteralComputedProperties: false,

    // enable parsing of shorthand object literal methods
    objectLiteralShorthandMethods: false,

    // enable parsing of shorthand object literal properties
    objectLiteralShorthandProperties: false,

    // Allow duplicate object literal properties (except '__proto__')
    objectLiteralDuplicateProperties: false,

    // enable parsing of generators/yield
    generators: false,

    // support the spread operator
    spread: false,

    // enable super in functions
    superInFunctions: false,

    // enable parsing of classes
    classes: false,

    // enable parsing of modules
    modules: false,

    // React JSX parsing
    jsx: false,

    // allow return statement in global scope
    globalReturn: false
};
