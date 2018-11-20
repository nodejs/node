/*
  Copyright (C) 2013 Yusuke Suzuki <utatane.tea@gmail.com>

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
    doctrine = require(root),
    assert = require('assert');
require('should');

// tests for the stringify function.
// ensure that we can parse and then stringify and the results are identical
describe('stringify', function () {

    function testStringify(text) {
        it (text, function() {
            var result = doctrine.parse("@param {" + text + "} name");
            //    console.log("Parse Tree: " + JSON.stringify(result, null, " "));
            var stringed = doctrine.type.stringify(result.tags[0].type, {compact:true});
            stringed.should.equal(text);
        });
    }

    // simple
    testStringify("String");
    testStringify("*");
    testStringify("null");
    testStringify("undefined");
    testStringify("void");
    //testStringify("?=");  // Failing

    // rest
    testStringify("...string");
    testStringify("...[string]");
    testStringify("...[[string]]");

    // optional, nullable, nonnullable
    testStringify("string=");
    testStringify("?string");
    testStringify("!string");
    testStringify("!string=");

    // type applications
    testStringify("Array.<String>");
    testStringify("Array.<String,Number>");

    // union types
    testStringify("()");
    testStringify("(String|Number)");

    // Arrays
    testStringify("[String]");
    testStringify("[String,Number]");
    testStringify("[(String|Number)]");

    // Record types
    testStringify("{a}");
    testStringify("{a:String}");
    testStringify("{a:String,b}");
    testStringify("{a:String,b:object}");
    testStringify("{a:String,b:foo.bar.baz}");
    testStringify("{a:(String|Number),b,c:Array.<String>}");
    testStringify("...{a:(String|Number),b,c:Array.<String>}");
    testStringify("{a:(String|Number),b,c:Array.<String>}=");

    // fn types
    testStringify("function(a)");
    testStringify("function(a):String");
    testStringify("function(a:number):String");
    testStringify("function(a:number,b:Array.<(String|Number|Object)>):String");
    testStringify("function(a:number,callback:function(a:Array.<(String|Number|Object)>):boolean):String");
    testStringify("function(a:(string|number),this:string,new:true):function():number");
    testStringify("function(a:(string|number),this:string,new:true):function(a:function(val):result):number");
});

describe('literals', function() {
    it('NullableLiteral', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.NullableLiteral
        }).should.equal('?');
    });

    it('AllLiteral', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.AllLiteral
        }).should.equal('*');
    });

    it('NullLiteral', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.NullLiteral
        }).should.equal('null');
    });

    it('UndefinedLiteral', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.UndefinedLiteral
        }).should.equal('undefined');
    });
});

describe('Expression', function () {
    it('NameExpression', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.NameExpression,
            name: 'this.is.valid'
        }).should.equal('this.is.valid');

        doctrine.type.stringify({
            type: doctrine.Syntax.NameExpression,
            name: 'String'
        }).should.equal('String');
    });

    it('ArrayType', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.ArrayType,
            elements: [{
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            }]
        }).should.equal('[String]');

        doctrine.type.stringify({
            type: doctrine.Syntax.ArrayType,
            elements: [{
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            }, {
                type: doctrine.Syntax.NameExpression,
                name: 'Number'
            }]
        }).should.equal('[String, Number]');

        doctrine.type.stringify({
            type: doctrine.Syntax.ArrayType,
            elements: []
        }).should.equal('[]');
    });

    it('RecordType', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.RecordType,
            fields: [{
                type: doctrine.Syntax.FieldType,
                key: 'name',
                value: null
            }]
        }).should.equal('{name}');

        doctrine.type.stringify({
            type: doctrine.Syntax.RecordType,
            fields: [{
                type: doctrine.Syntax.FieldType,
                key: 'name',
                value: {
                    type: doctrine.Syntax.NameExpression,
                    name: 'String'
                }
            }]
        }).should.equal('{name: String}');

        doctrine.type.stringify({
            type: doctrine.Syntax.RecordType,
            fields: [{
                type: doctrine.Syntax.FieldType,
                key: 'string',
                value: {
                    type: doctrine.Syntax.NameExpression,
                    name: 'String'
                }
            }, {
                type: doctrine.Syntax.FieldType,
                key: 'number',
                value: {
                    type: doctrine.Syntax.NameExpression,
                    name: 'Number'
                }
            }]
        }).should.equal('{string: String, number: Number}');

        doctrine.type.stringify({
            type: doctrine.Syntax.RecordType,
            fields: []
        }).should.equal('{}');
    });

    it('UnionType', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.UnionType,
            elements: [{
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            }]
        }).should.equal('(String)');

        doctrine.type.stringify({
            type: doctrine.Syntax.UnionType,
            elements: [{
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            }, {
                type: doctrine.Syntax.NameExpression,
                name: 'Number'
            }]
        }).should.equal('(String|Number)');

        doctrine.type.stringify({
            type: doctrine.Syntax.UnionType,
            elements: [{
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            }, {
                type: doctrine.Syntax.NameExpression,
                name: 'Number'
            }]
        }, { topLevel: true }).should.equal('String|Number');
    });

    it('RestType', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.RestType,
            expression: {
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            }
        }).should.equal('...String');
    });

    it('NonNullableType', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.NonNullableType,
            expression: {
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            },
            prefix: true
        }).should.equal('!String');

        doctrine.type.stringify({
            type: doctrine.Syntax.NonNullableType,
            expression: {
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            },
            prefix: false
        }).should.equal('String!');
    });

    it('OptionalType', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.OptionalType,
            expression: {
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            }
        }).should.equal('String=');
    });

    it('NullableType', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.NullableType,
            expression: {
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            },
            prefix: true
        }).should.equal('?String');

        doctrine.type.stringify({
            type: doctrine.Syntax.NullableType,
            expression: {
                type: doctrine.Syntax.NameExpression,
                name: 'String'
            },
            prefix: false
        }).should.equal('String?');
    });

    it('TypeApplication', function () {
        doctrine.type.stringify({
            type: doctrine.Syntax.TypeApplication,
            expression: {
                type: doctrine.Syntax.NameExpression,
                name: 'Array'
            },
            applications: [
                {
                    type: doctrine.Syntax.NameExpression,
                    name: 'String'
                }
            ]
        }).should.equal('Array.<String>');

        doctrine.type.stringify({
            type: doctrine.Syntax.TypeApplication,
            expression: {
                type: doctrine.Syntax.NameExpression,
                name: 'Array'
            },
            applications: [
                {
                    type: doctrine.Syntax.NameExpression,
                    name: 'String'
                },
                {
                    type: doctrine.Syntax.AllLiteral
                }
            ]
        }).should.equal('Array.<String, *>');
    });
});

describe('Complex identity', function () {
    it('Functions', function () {
        var data01 = 'function (): void';
        doctrine.type.stringify(
            doctrine.type.parseType(data01)
        ).should.equal(data01);

        var data02 = 'function (): String';
        doctrine.type.stringify(
            doctrine.type.parseType(data02)
        ).should.equal(data02);

        var data03 = 'function (test: string): String';
        doctrine.type.stringify(
            doctrine.type.parseType(data03)
        ).should.equal(data03);

        var data04 = 'function (this: Date, test: String): String';
        doctrine.type.stringify(
            doctrine.type.parseType(data04)
        ).should.equal(data04);

        var data05 = 'function (this: Date, a: String, b: Number): String';
        doctrine.type.stringify(
            doctrine.type.parseType(data05)
        ).should.equal(data05);

        var data06 = 'function (this: Date, a: Array.<String, Number>, b: Number): String';
        doctrine.type.stringify(
            doctrine.type.parseType(data06)
        ).should.equal(data06);

        var data07 = 'function (new: Date, a: Array.<String, Number>, b: Number): HashMap.<String, Number>';
        doctrine.type.stringify(
            doctrine.type.parseType(data07)
        ).should.equal(data07);

        var data08 = 'function (new: Date, a: Array.<String, Number>, b: (Number|String|Date)): HashMap.<String, Number>';
        doctrine.type.stringify(
            doctrine.type.parseType(data08)
        ).should.equal(data08);

        var data09 = 'function (new: Date)';
        doctrine.type.stringify(
            doctrine.type.parseType(data09)
        ).should.equal(data09);

        var data10 = 'function (this: Date)';
        doctrine.type.stringify(
            doctrine.type.parseType(data10)
        ).should.equal(data10);

        var data11 = 'function (this: Date, ...list)';
        doctrine.type.stringify(
            doctrine.type.parseType(data11)
        ).should.equal(data11);

        var data11a = 'function (this: Date, test: String=)';
        doctrine.type.stringify(
            doctrine.type.parseType(data11a)
        ).should.equal(data11a);

        // raw ... are not supported
//        var data12 = 'function (this: Date, ...)';
//        doctrine.type.stringify(
//            doctrine.type.parseType(data12)
//        ).should.equal(data12);

        var data12a = 'function (this: Date, ?=)';
        doctrine.type.stringify(
            doctrine.type.parseType(data12a)
        ).should.equal(data12a);
    });
});

/* vim: set sw=4 ts=4 et tw=80 : */
