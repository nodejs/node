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
    doctrine = require(root);
require('should');

describe('parse', function () {
    it('alias', function () {
        var res = doctrine.parse('/** @alias */', { unwrap: true });
        res.tags.should.have.length(0);
    });

    it('alias with name', function () {
        var res = doctrine.parse('/** @alias aliasName */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'alias');
        res.tags[0].should.have.property('name', 'aliasName');
    });

    it('alias with namepath', function () {
        var res = doctrine.parse('/** @alias aliasName.OK */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'alias');
        res.tags[0].should.have.property('name', 'aliasName.OK');
    });

    it('const', function () {
        var res = doctrine.parse('/** @const */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'const');
    });

    it('const with name', function () {
        var res = doctrine.parse('/** @const constname */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'const');
        res.tags[0].should.have.property('name', 'constname');
    });

    it('constant with name', function () {
        var res = doctrine.parse('/** @constant constname */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'constant');
        res.tags[0].should.have.property('name', 'constname');
    });

    it('const with type and name', function () {
        var res = doctrine.parse('/** @const {String} constname */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'const');
        res.tags[0].should.have.property('name', 'constname');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'String'
        });
    });

    it('constant with type and name', function () {
        var res = doctrine.parse('/** @constant {String} constname */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'constant');
        res.tags[0].should.have.property('name', 'constname');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'String'
        });
    });

    it('const multiple', function () {
        var res = doctrine.parse("/**@const\n @const*/", { unwrap: true });
        res.tags.should.have.length(2);
        res.tags[0].should.have.property('title', 'const');
        res.tags[1].should.have.property('title', 'const');
    });

    it('const double', function () {
        var res = doctrine.parse("/**@const\n @const*/", { unwrap: true });
        res.tags.should.have.length(2);
        res.tags[0].should.have.property('title', 'const');
        res.tags[1].should.have.property('title', 'const');
    });

    it('const triple', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @const @const",
                " * @const @const",
                " * @const @const",
                " */"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(3);
        res.tags[0].should.have.property('title', 'const');
        res.tags[1].should.have.property('title', 'const');
        res.tags[2].should.have.property('title', 'const');
    });

    it('constructor', function () {
        var res = doctrine.parse('/** @constructor */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'constructor');
    });

    it('constructor with type', function () {
        var res = doctrine.parse('/** @constructor {Object} */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'constructor');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'Object'
        });
    });

    it('constructor with type and name', function () {
        var res = doctrine.parse('/** @constructor {Object} objName */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'constructor');
        res.tags[0].should.have.property('name', 'objName');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'Object'
        });
    });

    it('class', function () {
        var res = doctrine.parse('/** @class */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'class');
    });

    it('class with type', function () {
        var res = doctrine.parse('/** @class {Object} */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'class');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'Object'
        });
    });

    it('class with type and name', function () {
        var res = doctrine.parse('/** @class {Object} objName */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'class');
        res.tags[0].should.have.property('name', 'objName');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'Object'
        });
    });

    it('deprecated', function () {
        var res = doctrine.parse('/** @deprecated */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'deprecated');
    });

    it('deprecated', function () {
        var res = doctrine.parse('/** @deprecated some text here describing why it is deprecated */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'deprecated');
        res.tags[0].should.have.property('description', 'some text here describing why it is deprecated');
    });

    it('func', function () {
        var res = doctrine.parse('/** @func */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'func');
    });

    it('func with name', function () {
        var res = doctrine.parse('/** @func thingName.func */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'func');
        res.tags[0].should.have.property('name', 'thingName.func');
    });

    it('func with type', function () {
        var res = doctrine.parse('/** @func {Object} thingName.func */', { unwrap: true });
        res.tags.should.have.length(0);
        // func does not accept type
    });

    it('function', function () {
        var res = doctrine.parse('/** @function */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'function');
    });

    it('function with name', function () {
        var res = doctrine.parse('/** @function thingName.function */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'function');
        res.tags[0].should.have.property('name', 'thingName.function');
    });

    it('function with type', function () {
        var res = doctrine.parse('/** @function {Object} thingName.function */', { unwrap: true });
        res.tags.should.have.length(0);
        // function does not accept type
    });

    it('member', function () {
        var res = doctrine.parse('/** @member */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'member');
    });

    it('member with name', function () {
        var res = doctrine.parse('/** @member thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'member');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('member with type', function () {
        var res = doctrine.parse('/** @member {Object} thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'member');
        res.tags[0].should.have.property('name', 'thingName.name');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'Object'
        });
    });

    it('method', function () {
        var res = doctrine.parse('/** @method */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'method');
    });

    it('method with name', function () {
        var res = doctrine.parse('/** @method thingName.function */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'method');
        res.tags[0].should.have.property('name', 'thingName.function');
    });

    it('method with type', function () {
        var res = doctrine.parse('/** @method {Object} thingName.function */', { unwrap: true });
        res.tags.should.have.length(0);
        // method does not accept type
    });

    it('mixes', function () {
        var res = doctrine.parse('/** @mixes */', { unwrap: true });
        res.tags.should.have.length(0);
    });

    it('mixes with name', function () {
        var res = doctrine.parse('/** @mixes thingName */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'mixes');
        res.tags[0].should.have.property('name', 'thingName');
    });

    it('mixes with namepath', function () {
        var res = doctrine.parse('/** @mixes thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'mixes');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('mixin', function () {
        var res = doctrine.parse('/** @mixin */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'mixin');
    });

    it('mixin with name', function () {
        var res = doctrine.parse('/** @mixin thingName */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'mixin');
        res.tags[0].should.have.property('name', 'thingName');
    });

    it('mixin with namepath', function () {
        var res = doctrine.parse('/** @mixin thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'mixin');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('module', function () {
        var res = doctrine.parse('/** @module */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'module');
    });

    it('module with name', function () {
        var res = doctrine.parse('/** @module thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'module');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('module with type', function () {
        var res = doctrine.parse('/** @module {Object} thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'module');
        res.tags[0].should.have.property('name', 'thingName.name');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'Object'
        });
    });

    it('name', function () {
        var res = doctrine.parse('/** @name thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'name');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('name', function () {
        var res = doctrine.parse('/** @name thingName#name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'name');
        res.tags[0].should.have.property('name', 'thingName#name');
    });

    it('name', function () {
        var res = doctrine.parse('/** @name thingName~name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'name');
        res.tags[0].should.have.property('name', 'thingName~name');
    });

    it('name', function () {
        var res = doctrine.parse('/** @name {thing} thingName.name */', { unwrap: true });
        // name does not accept type
        res.tags.should.have.length(0);
    });

    it('namespace', function () {
        var res = doctrine.parse('/** @namespace */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'namespace');
    });

    it('namespace with name', function () {
        var res = doctrine.parse('/** @namespace thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'namespace');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('namespace with type', function () {
        var res = doctrine.parse('/** @namespace {Object} thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'namespace');
        res.tags[0].should.have.property('name', 'thingName.name');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'Object'
        });
    });

    it('param', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {String} userName",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'userName');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'String'
        });
    });

    it('param with properties', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {String} user.name",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'user.name');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'String'
        });
    });

    it('arg with properties', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @arg {String} user.name",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'arg');
        res.tags[0].should.have.property('name', 'user.name');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'String'
        });
    });

    it('argument with properties', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @argument {String} user.name",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'argument');
        res.tags[0].should.have.property('name', 'user.name');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'String'
        });
    });

    it('param typeless', function () {
        var res = doctrine.parse(
        [
            "/**",
            " * @param userName",
            "*/"
        ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.eql({
            title: 'param',
            type: null,
            name: 'userName',
            description: null
        });

        var res = doctrine.parse(
        [
            "/**",
            " * @param userName Something descriptive",
            "*/"
        ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.eql({
            title: 'param',
            type: null,
            name: 'userName',
            description: 'Something descriptive'
        });

        var res = doctrine.parse(
        [
            "/**",
            " * @param user.name Something descriptive",
            "*/"
        ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.eql({
            title: 'param',
            type: null,
            name: 'user.name',
            description: 'Something descriptive'
        });
    });

    it('param broken', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {String} userName",
                " * @param {String userName",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'userName');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'String'
        });
    });

    it('param record', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {{ok:String}} userName",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'userName');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'RecordType',
            fields: [{
                type: 'FieldType',
                key: 'ok',
                value: {
                    type: 'NameExpression',
                    name: 'String'
                }
            }]
        });
    });

    it('param record broken', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {{ok:String} userName",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.be.empty;
    });

    it('param multiple lines', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {string|",
                " *     number} userName",
                " * }}",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'userName');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'UnionType',
            elements: [{
                type: 'NameExpression',
                name: 'string'
            }, {
                type: 'NameExpression',
                name: 'number'
            }]
        });
    });

    it('param without braces', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param string name description",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'string');
        res.tags[0].should.have.property('type', null);
        res.tags[0].should.have.property('description', 'name description');
    });

    it('param w/ hyphen before description', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {string} name - description",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.eql({
            title: 'param',
            type: {
                type: 'NameExpression',
                name: 'string'
            },
            name: 'name',
            description: 'description'
        });
    });

    it('param w/ hyphen + leading space before description', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {string} name -   description",
                "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.eql({
            title: 'param',
            type: {
                type: 'NameExpression',
                name: 'string'
            },
            name: 'name',
            description: '  description'
        });
    });

    it('description and param separated by blank line', function () {
        var res = doctrine.parse(
            [
                "/**",
                " * Description",
                " * blah blah blah",
                " *",
                " * @param {string} name description",
                "*/"
            ].join('\n'), { unwrap: true });
        res.description.should.eql('Description\nblah blah blah');
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'name');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'string'
        });
        res.tags[0].should.have.property('description', 'description');
    });

    it('regular block comment instead of jsdoc-style block comment', function () {
        var res = doctrine.parse(
            [
                "/*",
                " * Description",
                " * blah blah blah",
                "*/"
            ].join('\n'), { unwrap: true });
        res.description.should.eql("Description\nblah blah blah");
    });

    it('augments', function () {
        var res = doctrine.parse('/** @augments */', { unwrap: true });
        res.tags.should.have.length(1);
    });

    it('augments with name', function () {
        var res = doctrine.parse('/** @augments ClassName */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'augments');
        res.tags[0].should.have.property('name', 'ClassName');
    });

    it('augments with type', function () {
        var res = doctrine.parse('/** @augments {ClassName} */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'augments');
        res.tags[0].should.have.property('type', {
          type: 'NameExpression',
          name: 'ClassName'
        });
    });

    it('augments with name', function () {
        var res = doctrine.parse('/** @augments ClassName.OK */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'augments');
        res.tags[0].should.have.property('name', 'ClassName.OK');
    });

    it('extends', function () {
        var res = doctrine.parse('/** @extends */', { unwrap: true });
        res.tags.should.have.length(1);
    });

    it('extends with name', function () {
        var res = doctrine.parse('/** @extends ClassName */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'extends');
        res.tags[0].should.have.property('name', 'ClassName');
    });

    it('extends with type', function () {
        var res = doctrine.parse('/** @extends {ClassName} */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'extends');
        res.tags[0].should.have.property('type', {
          type: 'NameExpression',
          name: 'ClassName'
        });
    });

    it('extends with namepath', function () {
        var res = doctrine.parse('/** @extends ClassName.OK */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'extends');
        res.tags[0].should.have.property('name', 'ClassName.OK');
    });

    it('prop', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @prop {string} thingName - does some stuff",
              "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'prop');
        res.tags[0].should.have.property('description', 'does some stuff');
        res.tags[0].type.should.have.property('name', 'string');
        res.tags[0].should.have.property('name', 'thingName');
    });

    it('prop without type', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @prop thingName - does some stuff",
              "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(0);
    });


    it('property', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @property {string} thingName - does some stuff",
              "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'property');
        res.tags[0].should.have.property('description', 'does some stuff');
        res.tags[0].type.should.have.property('name', 'string');
        res.tags[0].should.have.property('name', 'thingName');
    });

    it('property without type', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @property thingName - does some stuff",
              "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(0);
    });

    it('property with nested name', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @property {string} thingName.name - does some stuff",
              "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'property');
        res.tags[0].should.have.property('description', 'does some stuff');
        res.tags[0].type.should.have.property('name', 'string');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('throws', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @throws {Error} if something goes wrong",
              " */"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'throws');
        res.tags[0].should.have.property('description', 'if something goes wrong');
        res.tags[0].type.should.have.property('name', 'Error');
    });

    it('throws without type', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @throws if something goes wrong",
              " */"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'throws');
        res.tags[0].should.have.property('description', 'if something goes wrong');
    });

    it('kind', function () {
        var res = doctrine.parse('/** @kind class */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'kind');
        res.tags[0].should.have.property('kind', 'class');
    });

    it('kind error', function () {
        var res = doctrine.parse('/** @kind ng */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Invalid kind name \'ng\'');
    });

    it('todo', function () {
        var res = doctrine.parse('/** @todo Write the documentation */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'todo');
        res.tags[0].should.have.property('description', 'Write the documentation');
    });

    it('summary', function () {
        // japanese lang
        var res = doctrine.parse('/** @summary ゆるゆり3期おめでとー */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'summary');
        res.tags[0].should.have.property('description', 'ゆるゆり3期おめでとー');
    });

    it('variation', function () {
        // japanese lang
        var res = doctrine.parse('/** @variation 42 */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'variation');
        res.tags[0].should.have.property('variation', 42);
    });

    it('variation error', function () {
        // japanese lang
        var res = doctrine.parse('/** @variation Animation */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Invalid variation \'Animation\'');
    });

    it('access', function () {
        var res = doctrine.parse('/** @access public */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'access');
        res.tags[0].should.have.property('access', 'public');
    });

    it('access error', function () {
        var res = doctrine.parse('/** @access ng */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Invalid access name \'ng\'');
    });

    it('public', function () {
        var res = doctrine.parse('/** @public */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'public');
    });

    it('public type and description', function () {
        var res = doctrine.parse('/** @public {number} ok */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'public');
        res.tags[0].should.have.property('description', 'ok');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'number'
        });
    });

    it('protected', function () {
        var res = doctrine.parse('/** @protected */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'protected');
    });

    it('protected type and description', function () {
        var res = doctrine.parse('/** @protected {number} ok */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'protected');
        res.tags[0].should.have.property('description', 'ok');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'number'
        });
    });

    it('private', function () {
        var res = doctrine.parse('/** @private */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'private');
    });

    it('private type and description', function () {
        var res = doctrine.parse('/** @private {number} ok */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'private');
        res.tags[0].should.have.property('description', 'ok');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'number'
        });
    });

    it('readonly', function () {
        var res = doctrine.parse('/** @readonly */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'readonly');
    });

    it('readonly error', function () {
        var res = doctrine.parse('/** @readonly ng */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Unknown content \'ng\'');
    });

    it('requires', function () {
        var res = doctrine.parse('/** @requires */', { unwrap: true });
        res.tags.should.have.length(0);
    });

    it('requires with module name', function () {
        var res = doctrine.parse('/** @requires name.path */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'requires');
        res.tags[0].should.have.property('name', 'name.path');
    });

    it('global', function () {
        var res = doctrine.parse('/** @global */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'global');
    });

    it('global error', function () {
        var res = doctrine.parse('/** @global ng */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Unknown content \'ng\'');
    });

    it('inner', function () {
        var res = doctrine.parse('/** @inner */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'inner');
    });

    it('inner error', function () {
        var res = doctrine.parse('/** @inner ng */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Unknown content \'ng\'');
    });

    it('instance', function () {
        var res = doctrine.parse('/** @instance */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'instance');
    });

    it('instance error', function () {
        var res = doctrine.parse('/** @instance ng */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Unknown content \'ng\'');
    });

    it('since', function () {
        var res = doctrine.parse('/** @since 1.2.1 */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'since');
        res.tags[0].should.have.property('description', '1.2.1');
    });

    it('static', function () {
        var res = doctrine.parse('/** @static */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'static');
    });

    it('static error', function () {
        var res = doctrine.parse('/** @static ng */', { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Unknown content \'ng\'');
    });

    it('this', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @this thingName",
              "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'this');
        res.tags[0].should.have.property('name', 'thingName');
    });

    it('this with namepath', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @this thingName.name",
              "*/"
            ].join('\n'), { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'this');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('this error', function () {
        var res = doctrine.parse(
            [
              "/**",
              " * @this",
              "*/"
            ].join('\n'), { unwrap: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'this');
        res.tags[0].should.have.property('errors');
        res.tags[0].errors.should.have.length(1);
        res.tags[0].errors[0].should.equal('Missing or invalid tag name');
    });

    it('var', function () {
        var res = doctrine.parse('/** @var */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'var');
    });

    it('var with name', function () {
        var res = doctrine.parse('/** @var thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'var');
        res.tags[0].should.have.property('name', 'thingName.name');
    });

    it('var with type', function () {
        var res = doctrine.parse('/** @var {Object} thingName.name */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'var');
        res.tags[0].should.have.property('name', 'thingName.name');
        res.tags[0].should.have.property('type');
        res.tags[0].type.should.eql({
            type: 'NameExpression',
            name: 'Object'
        });
    });

    it('version', function () {
        var res = doctrine.parse('/** @version 1.2.1 */', { unwrap: true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'version');
        res.tags[0].should.have.property('description', '1.2.1');
    });

    it('incorrect name', function () {
        var res = doctrine.parse('/** @name thingName#%name */', { unwrap: true });
        // name does not accept type
        res.tags.should.have.length(0);
        res.should.eql({
            "description": "",
            "tags": [
            ]
        });
    });
});

describe('parseType', function () {
    it('union type closure-compiler extended', function () {
        var type = doctrine.parseType("string|number");
        type.should.eql({
            type: 'UnionType',
            elements: [{
                type: 'NameExpression',
                name: 'string'
            }, {
                type: 'NameExpression',
                name: 'number'
            }]
        });
    });

    it('empty union type', function () {
        var type = doctrine.parseType("()");
        type.should.eql({
            type: 'UnionType',
            elements: []
        });
    });

    it('comma last array type', function () {
        var type = doctrine.parseType("[string,]");
        type.should.eql({
            type: 'ArrayType',
            elements: [{
                type: 'NameExpression',
                name: 'string'
            }]
        });
    });

    it('array type of all literal', function () {
        var type = doctrine.parseType("[*]");
        type.should.eql({
            type: 'ArrayType',
            elements: [{
                type: 'AllLiteral'
            }]
        });
    });

    it('array type of nullable literal', function () {
        var type = doctrine.parseType("[?]");
        type.should.eql({
            type: 'ArrayType',
            elements: [{
                type: 'NullableLiteral'
            }]
        });
    });

    it('comma last record type', function () {
        var type = doctrine.parseType("{,}");
        type.should.eql({
            type: 'RecordType',
            fields: []
        });
    });

    it('type application', function () {
        var type = doctrine.parseType("Array.<String>");
        type.should.eql({
            type: 'TypeApplication',
            expression: {
                type: 'NameExpression',
                name: 'Array'
            },
            applications: [{
                type: 'NameExpression',
                name: 'String'
            }]
        });
    });

    it('type application with NullableLiteral', function () {
        var type = doctrine.parseType("Array<?>");
        type.should.eql({
            type: 'TypeApplication',
            expression: {
                type: 'NameExpression',
                name: 'Array'
            },
            applications: [{
                type: 'NullableLiteral'
            }]
        });
    });

    it('type application with multiple patterns', function () {
        var type = doctrine.parseType("Array.<String, Number>");
        type.should.eql({
            type: 'TypeApplication',
            expression: {
                type: 'NameExpression',
                name: 'Array'
            },
            applications: [{
                type: 'NameExpression',
                name: 'String'
            }, {
                type: 'NameExpression',
                name: 'Number'
            }]
        });
    });

    it('type application without dot', function () {
        var type = doctrine.parseType("Array<String>");
        type.should.eql({
            type: 'TypeApplication',
            expression: {
                type: 'NameExpression',
                name: 'Array'
            },
            applications: [{
                type: 'NameExpression',
                name: 'String'
            }]
        });
    });

    it('array-style type application', function () {
        var type = doctrine.parseType("String[]");
        type.should.eql({
            type: 'TypeApplication',
            expression: {
                type: 'NameExpression',
                name: 'Array'
            },
            applications: [{
                type: 'NameExpression',
                name: 'String'
            }]
        });
    });

    it('function type simple', function () {
        var type = doctrine.parseType("function()");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [],
		 "result": null
		});
    });

    it('function type with name', function () {
        var type = doctrine.parseType("function(a)");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [
		{
		   "type": "NameExpression",
		   "name": "a"
		  }
			 ],
		 "result": null
		});
    });
    it('function type with name and type', function () {
        var type = doctrine.parseType("function(a:b)");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [
		  {
		   "type": "ParameterType",
		   "name": "a",
		   "expression": {
		    "type": "NameExpression",
		    "name": "b"
		   }
		  }
		 ],
		 "result": null
		});
    });
    it('function type with optional param', function () {
        var type = doctrine.parseType("function(a=)");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [
		  {
		   "type": "OptionalType",
		   "expression": {
		    "type": "NameExpression",
		    "name": "a"
		   }
		  }
		 ],
		 "result": null
		});
    });
    it('function type with optional param name and type', function () {
        var type = doctrine.parseType("function(a:b=)");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [
		  {
		   "type": "OptionalType",
		   "expression": {
		    "type": "ParameterType",
		    "name": "a",
		    "expression": {
		     "type": "NameExpression",
		     "name": "b"
		    }
		   }
		  }
		 ],
		 "result": null
		});
    });
    it('function type with rest param', function () {
        var type = doctrine.parseType("function(...a)");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [
		  {
		   "type": "RestType",
		   "expression": {
		    "type": "NameExpression",
		    "name": "a"
		   }
		  }
		 ],
		 "result": null
		});
    });
    it('function type with rest param name and type', function () {
        var type = doctrine.parseType("function(...a:b)");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [
		  {
		   "type": "RestType",
		   "expression": {
		    "type": "ParameterType",
		    "name": "a",
		    "expression": {
		     "type": "NameExpression",
		     "name": "b"
		    }
		   }
		  }
		 ],
		 "result": null
		});
    });

    it('function type with optional rest param', function () {
        var type = doctrine.parseType("function(...a=)");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [
		  {
			"type": "RestType",
			"expression": {
			   "type": "OptionalType",
			   "expression": {
			    "type": "NameExpression",
			    "name": "a"
			   }
			  }
			}
		 ],
		 "result": null
		});
    });
    it('function type with optional rest param name and type', function () {
        var type = doctrine.parseType("function(...a:b=)");
        type.should.eql({
		 "type": "FunctionType",
		 "params": [
		  {
			"type": "RestType",
			"expression": {
			   "type": "OptionalType",
			   "expression": {
			    "type": "ParameterType",
			    "name": "a",
			    "expression": {
			     "type": "NameExpression",
			     "name": "b"
			    }
			  }
			}
		 }],
		 "result": null
		});
    });

    it('string value in type', function () {
        var type;

        type = doctrine.parseType("{'ok':String}");
        type.should.eql({
            "fields": [
                {
                    "key": "ok",
                    "type": "FieldType",
                    "value": {
                        "name": "String",
                        "type": "NameExpression"
                    }
                }
            ],
            "type": "RecordType"
        });

        type = doctrine.parseType('{"\\r\\n\\t\\u2028\\x20\\u20\\b\\f\\v\\\r\n\\\n\\0\\07\\012\\o":String}');
        type.should.eql({
            "fields": [
                {
                    "key": "\r\n\t\u2028\x20u20\b\f\v\0\u0007\u000ao",
                    "type": "FieldType",
                    "value": {
                        "name": "String",
                        "type": "NameExpression"
                    }
                }
            ],
            "type": "RecordType"
        });

        doctrine.parseType.bind(doctrine, "{'ok\":String}").should.throw('unexpected quote');
        doctrine.parseType.bind(doctrine, "{'o\n':String}").should.throw('unexpected quote');
    });

    it('number value in type', function () {
        var type;

        type = doctrine.parseType("{20:String}");
        type.should.eql({
            "fields": [
                {
                    "key": "20",
                    "type": "FieldType",
                    "value": {
                        "name": "String",
                        "type": "NameExpression"
                    }
                }
            ],
            "type": "RecordType"
        });

        type = doctrine.parseType("{.2:String, 30:Number, 0x20:String}");
        type.should.eql({
            "fields": [
                {
                    "key": "0.2",
                    "type": "FieldType",
                    "value": {
                        "name": "String",
                        "type": "NameExpression"
                    }
                },
                {
                    "key": "30",
                    "type": "FieldType",
                    "value": {
                        "name": "Number",
                        "type": "NameExpression"
                    }
                },
                {
                    "key": "32",
                    "type": "FieldType",
                    "value": {
                        "name": "String",
                        "type": "NameExpression"
                    }
                }
            ],
            "type": "RecordType"
        });


        type = doctrine.parseType("{0X2:String, 0:Number, 100e200:String, 10e-20:Number}");
        type.should.eql({
            "fields": [
                {
                    "key": "2",
                    "type": "FieldType",
                    "value": {
                        "name": "String",
                        "type": "NameExpression"
                    }
                },
                {
                    "key": "0",
                    "type": "FieldType",
                    "value": {
                        "name": "Number",
                        "type": "NameExpression"
                    }
                },
                {
                    "key": "1e+202",
                    "type": "FieldType",
                    "value": {
                        "name": "String",
                        "type": "NameExpression"
                    }
                },
                {
                    "key": "1e-19",
                    "type": "FieldType",
                    "value": {
                        "name": "Number",
                        "type": "NameExpression"
                    }
                }
            ],
            "type": "RecordType"
        });


        doctrine.parseType.bind(doctrine, "{0x:String}").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{0x").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{0xd").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{0x2_:").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{021:").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{021_:").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{021").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{08").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{0y").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{0").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{100e2").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{100e-2").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{100e-200:").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "{100e:").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "function(number=, string)").should.throw('not reach to EOF');
    });

    it('dotted type', function () {
        var type;
        type = doctrine.parseType("Cocoa.Cappuccino");
        type.should.eql({
            "name": "Cocoa.Cappuccino",
            "type": "NameExpression"
        });
    });

    it('rest array type', function () {
        var type;
        type = doctrine.parseType("[string,...string]");
        type.should.eql({
            "elements": [
                {
                    "name": "string",
                    "type": "NameExpression"
                },
                {
                    "expression": {
                        "name": "string",
                        "type": "NameExpression"
                    },
                "type": "RestType"
                }
            ],
            "type": "ArrayType"
        });
    });

    it ('nullable type', function () {
        var type;
        type = doctrine.parseType("string?");
        type.should.eql({
            "expression": {
                "name": "string",
                "type": "NameExpression"
            },
            "prefix": false,
            "type": "NullableType"
        });
    });

    it ('non-nullable type', function () {
        var type;
        type = doctrine.parseType("string!");
        type.should.eql({
            "expression": {
                "name": "string",
                "type": "NameExpression"
            },
            "prefix": false,
            "type": "NonNullableType"
        });
    });

    it ('toplevel multiple pipe type', function () {
        var type;
        type = doctrine.parseType("string|number|Test");
        type.should.eql({
            "elements": [
                {
                    "name": "string",
                    "type": "NameExpression"
                },
                {
                    "name": "number",
                    "type": "NameExpression"
                },
                {
                    "name": "Test",
                    "type": "NameExpression"
                }
            ],
            "type": "UnionType"
        });
    });

    it('illegal tokens', function () {
        doctrine.parseType.bind(doctrine, ".").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, ".d").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "(").should.throw('unexpected token');
        doctrine.parseType.bind(doctrine, "Test.").should.throw('unexpected token');
    });
});

describe('parseParamType', function () {
    it('question', function () {
        var type = doctrine.parseParamType("?");
        type.should.eql({
            type: 'NullableLiteral'
        });
    });

    it('question option', function () {
        var type = doctrine.parseParamType("?=");
        type.should.eql({
            type: 'OptionalType',
            expression: {
                type: 'NullableLiteral'
            }
        });
    });

    it('function option parameters former', function () {
        var type = doctrine.parseParamType("function(?, number)");
        type.should.eql({
            type: 'FunctionType',
            params: [{
                type: 'NullableLiteral'
            }, {
                type: 'NameExpression',
                name: 'number'
            }],
            result: null
        });
    });

    it('function option parameters latter', function () {
        var type = doctrine.parseParamType("function(number, ?)");
        type.should.eql({
            type: 'FunctionType',
            params: [{
                type: 'NameExpression',
                name: 'number'
            }, {
                type: 'NullableLiteral'
            }],
            result: null
        });
    });

    it('function type union', function () {
        var type = doctrine.parseParamType("function(): ?|number");
        type.should.eql({
            type: 'UnionType',
            elements: [{
                type: 'FunctionType',
                params: [],
                result: {
                    type: 'NullableLiteral'
                }
            }, {
                type: 'NameExpression',
                name: 'number'
            }]
        });
    });
});

describe('invalid', function () {
    it('empty union pipe', function () {
        doctrine.parseType.bind(doctrine, "(|)").should.throw();
        doctrine.parseType.bind(doctrine, "(string|)").should.throw();
        doctrine.parseType.bind(doctrine, "(string||)").should.throw();
    });

    it('comma only array type', function () {
        doctrine.parseType.bind(doctrine, "[,]").should.throw();
    });

    it('comma only record type', function () {
        doctrine.parseType.bind(doctrine, "{,,}").should.throw();
    });

    it('incorrect bracket', function () {
        doctrine.parseParamType.bind(doctrine, "int[").should.throw();
    });
});

describe('tags option', function() {
	it ('only param', function() {
        var res = doctrine.parse(
            [
                "/**",
                " * @const @const",
                " * @param {String} y",
                " */"
            ].join('\n'), { tags: ['param'], unwrap:true });
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'y');
     });

	it ('param and type', function() {
        var res = doctrine.parse(
            [
                "/**",
                " * @const x",
                " * @param {String} y",
                " * @type {String} ",
                " */"
            ].join('\n'), { tags: ['param', 'type'], unwrap:true });
        res.tags.should.have.length(2);
        res.tags[0].should.have.property('title', 'param');
        res.tags[0].should.have.property('name', 'y');
        res.tags[1].should.have.property('title', 'type');
        res.tags[1].should.have.property('type');
        res.tags[1].type.should.have.property('name', 'String');
     });

});

describe('invalid tags', function() {
	it ('bad tag 1', function() {
        doctrine.parse.bind(doctrine,
            [
                "/**",
                " * @param {String} hucairz",
                " */"
            ].join('\n'), { tags: 1, unwrap:true }).should.throw();
     });

	it ('bad tag 2', function() {
        doctrine.parse.bind(doctrine,
            [
                "/**",
                " * @param {String} hucairz",
                " */"
            ].join('\n'), { tags: ['a', 1], unwrap:true }).should.throw();
     });
});

describe('optional params', function() {

    // should fail since sloppy option not set
    it('failure 0', function() {
        doctrine.parse(
        ["/**", " * @param {String} [val]", " */"].join('\n'), {
            unwrap: true
        }).should.eql({
            "description": "",
            "tags": []
        });
    });

    it('failure 1', function() {
        doctrine.parse(
        ["/**", " * @param [val", " */"].join('\n'), {
            unwrap: true, sloppy: true
        }).should.eql({
            "description": "",
            "tags": []
        });
    });

    it('success 1', function() {
        doctrine.parse(
        ["/**", " * @param {String} [val]", " */"].join('\n'), {
            unwrap: true, sloppy: true
        }).should.eql({
            "description": "",
            "tags": [{
                "title": "param",
                "description": null,
                "type": {
                    "type": "OptionalType",
                    "expression": {
                        "type": "NameExpression",
                        "name": "String"
                    }
                },
                "name": "val"
            }]
        });
    });
    it('success 2', function() {
        doctrine.parse(
        ["/**", " * @param {String=} val", " */"].join('\n'), {
            unwrap: true, sloppy: true
        }).should.eql({
            "description": "",
            "tags": [{
                "title": "param",
                "description": null,
                "type": {
                    "type": "OptionalType",
                    "expression": {
                        "type": "NameExpression",
                        "name": "String"
                    }
                },
                "name": "val"
            }]
        });
    });

    it('success 3', function() {
        doctrine.parse(
            ["/**", " * @param {String=} [val=abc] some description", " */"].join('\n'),
            { unwrap: true, sloppy: true}
        ).should.eql({
            "description": "",
            "tags": [{
                "title": "param",
                "description": "some description",
                "type": {
                    "type": "OptionalType",
                    "expression": {
                        "type": "NameExpression",
                        "name": "String"
                    }
                },
                "name": "val",
                "default": "abc"
            }]
        });
    });

    it('line numbers', function() {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {string} foo",
                " * @returns {string}",
                " *",
                " * @example",
                " * f('blah'); // => undefined",
                " */"
            ].join('\n'),
            { unwrap: true, lineNumbers: true }
        );

        res.tags[0].should.have.property('lineNumber', 1);
        res.tags[1].should.have.property('lineNumber', 2);
        res.tags[2].should.have.property('lineNumber', 4);
    });

    it('should handle \\r\\n line endings correctly', function() {
        var res = doctrine.parse(
            [
                "/**",
                " * @param {string} foo",
                " * @returns {string}",
                " *",
                " * @example",
                " * f('blah'); // => undefined",
                " */"
            ].join('\r\n'),
            { unwrap: true, lineNumbers: true }
        );

        res.tags[0].should.have.property('lineNumber', 1);
        res.tags[1].should.have.property('lineNumber', 2);
        res.tags[2].should.have.property('lineNumber', 4);
    });
});

describe('recovery tests', function() {
	it ('params 2', function () {
		var res = doctrine.parse(
            [
                "@param f",
                "@param {string} f2"
            ].join('\n'), { recoverable: true });

         // ensure both parameters are OK
         res.tags.should.have.length(2);
         res.tags[0].should.have.property('title', 'param');
         res.tags[0].should.have.property('type', null);
         res.tags[0].should.have.property('name', 'f');

         res.tags[1].should.have.property('title', 'param');
         res.tags[1].should.have.property('type');
         res.tags[1].type.should.have.property('name', 'string');
         res.tags[1].type.should.have.property('type', 'NameExpression');
         res.tags[1].should.have.property('name', 'f2');
	});

	it ('params 2', function () {
		var res = doctrine.parse(
            [
                "@param string f",
                "@param {string} f2"
            ].join('\n'), { recoverable: true });

         // ensure first parameter is OK even with invalid type name
         res.tags.should.have.length(2);
         res.tags[0].should.have.property('title', 'param');
         res.tags[0].should.have.property('type', null);
         res.tags[0].should.have.property('name', 'string');
         res.tags[0].should.have.property('description', 'f');

         res.tags[1].should.have.property('title', 'param');
         res.tags[1].should.have.property('type');
         res.tags[1].type.should.have.property('name', 'string');
         res.tags[1].type.should.have.property('type', 'NameExpression');
         res.tags[1].should.have.property('name', 'f2');
	});

	it ('return 1', function() {
		var res = doctrine.parse(
            [
                "@returns"
            ].join('\n'), { recoverable: true });

         // return tag should exist
         res.tags.should.have.length(1);
         res.tags[0].should.have.property('title', 'returns');
         res.tags[0].should.have.property('type', null);
	});
	it ('return 2', function() {
		var res = doctrine.parse(
            [
                "@returns",
				"@param {string} f2"
            ].join('\n'), { recoverable: true });

         // return tag should exist as well as next tag
         res.tags.should.have.length(2);
         res.tags[0].should.have.property('title', 'returns');
         res.tags[0].should.have.property('type', null);

         res.tags[1].should.have.property('title', 'param');
         res.tags[1].should.have.property('type');
         res.tags[1].type.should.have.property('name', 'string');
         res.tags[1].type.should.have.property('type', 'NameExpression');
         res.tags[1].should.have.property('name', 'f2');
	});

	it ('extra @ 1', function() {
		var res = doctrine.parse(
            [
                "@",
                "@returns",
				"@param {string} f2"
            ].join('\n'), { recoverable: true });

         // empty tag name shouldn't affect subsequent tags
         res.tags.should.have.length(3);
         res.tags[0].should.have.property('title', '');
         res.tags[0].should.not.have.property('type');

         res.tags[1].should.have.property('title', 'returns');
         res.tags[1].should.have.property('type', null);

         res.tags[2].should.have.property('title', 'param');
         res.tags[2].should.have.property('type');
         res.tags[2].type.should.have.property('name', 'string');
         res.tags[2].type.should.have.property('type', 'NameExpression');
         res.tags[2].should.have.property('name', 'f2');
	});

	it ('extra @ 2', function() {
		var res = doctrine.parse(
            [
                "@ invalid name",
				"@param {string} f2"
            ].join('\n'), { recoverable: true });

         // empty tag name shouldn't affect subsequent tags
         res.tags.should.have.length(2);
         res.tags[0].should.have.property('title', '');
         res.tags[0].should.not.have.property('type');
         res.tags[0].should.not.have.property('name');
         res.tags[0].should.have.property('description', 'invalid name');

         res.tags[1].should.have.property('title', 'param');
         res.tags[1].should.have.property('type');
         res.tags[1].type.should.have.property('name', 'string');
         res.tags[1].type.should.have.property('type', 'NameExpression');
         res.tags[1].should.have.property('name', 'f2');
	});

	it ('invalid tag 1', function() {
		var res = doctrine.parse(
            [
                "@111 invalid name",
				"@param {string} f2"
            ].join('\n'), { recoverable: true });

         // invalid tag name shouldn't affect subsequent tags
         res.tags.should.have.length(2);
         res.tags[0].should.have.property('title', '111');
         res.tags[0].should.not.have.property('type');
         res.tags[0].should.not.have.property('name');
         res.tags[0].should.have.property('description', 'invalid name');

         res.tags[1].should.have.property('title', 'param');
         res.tags[1].should.have.property('type');
         res.tags[1].type.should.have.property('name', 'string');
         res.tags[1].type.should.have.property('type', 'NameExpression');
         res.tags[1].should.have.property('name', 'f2');
	});

	it ('invalid tag 1', function() {
		var res = doctrine.parse(
            [
                "@111",
				"@param {string} f2"
            ].join('\n'), { recoverable: true });

         // invalid tag name shouldn't affect subsequent tags
         res.tags.should.have.length(2);
         res.tags[0].should.have.property('title', '111');
         res.tags[0].should.not.have.property('type');
         res.tags[0].should.not.have.property('name');
         res.tags[0].should.have.property('description', null);

         res.tags[1].should.have.property('title', 'param');
         res.tags[1].should.have.property('type');
         res.tags[1].type.should.have.property('name', 'string');
         res.tags[1].type.should.have.property('type', 'NameExpression');
         res.tags[1].should.have.property('name', 'f2');
	});

	it ('should not crash on bad type in @param without name', function() {
		var res = doctrine.parse("@param {Function(DOMNode)}", { recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.eql({
            "description": null,
            "errors": [
                "not reach to EOF",
                "Missing or invalid tag name"
            ],
            "name": null,
            "title": "param",
            "type": null
        });
    });

	it ('should not crash on bad type in @param in sloppy mode', function() {
		var res = doctrine.parse("@param {int[} [x]", { sloppy: true, recoverable: true });
        res.tags.should.have.length(1);
        res.tags[0].should.eql({
            "description": null,
            "errors": [
                "expected an array-style type declaration (int[])"
            ],
            "name": "x",
            "title": "param",
            "type": null
        });
    });
});

describe('exported Syntax', function() {
	it ('members', function () {
        doctrine.Syntax.should.eql({
            NullableLiteral: 'NullableLiteral',
            AllLiteral: 'AllLiteral',
            NullLiteral: 'NullLiteral',
            UndefinedLiteral: 'UndefinedLiteral',
            VoidLiteral: 'VoidLiteral',
            UnionType: 'UnionType',
            ArrayType: 'ArrayType',
            RecordType: 'RecordType',
            FieldType: 'FieldType',
            FunctionType: 'FunctionType',
            ParameterType: 'ParameterType',
            RestType: 'RestType',
            NonNullableType: 'NonNullableType',
            OptionalType: 'OptionalType',
            NullableType: 'NullableType',
            NameExpression: 'NameExpression',
            TypeApplication: 'TypeApplication'
        });
    });
});

describe('@ mark contained descriptions', function () {
    it ('comment description #10', function () {
        doctrine.parse(
            [
                '/**',
                ' * Prevents the default action. It is equivalent to',
                ' * {@code e.preventDefault()}, but can be used as the callback argument of',
                ' * {@link goog.events.listen} without declaring another function.',
                ' * @param {!goog.events.Event} e An event.',
                ' */'
            ].join('\n'),
            { unwrap: true, sloppy: true }).should.eql({
            'description': 'Prevents the default action. It is equivalent to\n{@code e.preventDefault()}, but can be used as the callback argument of\n{@link goog.events.listen} without declaring another function.',
            'tags': [{
                'title': 'param',
                'description': 'An event.',
                'type': {
                    'type': 'NonNullableType',
                    'expression': {
                        'type': 'NameExpression',
                        'name': 'goog.events.Event'
                    },
                    'prefix': true
                },
                'name': 'e'
            }]
        });
    });

    it ('tag description', function () {
        doctrine.parse(
            [
                '/**',
                ' * Prevents the default action. It is equivalent to',
                ' * @param {!goog.events.Event} e An event.',
                ' * {@code e.preventDefault()}, but can be used as the callback argument of',
                ' * {@link goog.events.listen} without declaring another function.',
                ' */'
            ].join('\n'),
            { unwrap: true, sloppy: true }).should.eql({
            'description': 'Prevents the default action. It is equivalent to',
            'tags': [{
                'title': 'param',
                'description': 'An event.\n{@code e.preventDefault()}, but can be used as the callback argument of\n{@link goog.events.listen} without declaring another function.',
                'type': {
                    'type': 'NonNullableType',
                    'expression': {
                        'type': 'NameExpression',
                        'name': 'goog.events.Event'
                    },
                    'prefix': true
                },
                'name': 'e'
            }]
        });
    });
});

describe('function', function () {
    it ('recognize "function" type', function () {
		var res = doctrine.parse(
            [
                "@param {function} foo description",
            ].join('\n'), {});
         res.tags.should.have.length(1);
         res.tags[0].should.have.property('title', 'param');
         res.tags[0].should.have.property('type');
         res.tags[0].type.should.eql({
             "name": "function",
             "type": "NameExpression"
         });
         res.tags[0].should.have.property('name', 'foo');
         res.tags[0].should.have.property('description', 'description');
    });
});

describe('tagged namepaths', function () {
    it ('recognize module:', function () {
        var res = doctrine.parse(
            [
                "@alias module:Foo.bar"
            ].join('\n'), {});
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'alias');
        res.tags[0].should.have.property('name', 'module:Foo.bar');
        res.tags[0].should.have.property('description', null);
    });

    it ('recognize external:', function () {
        var res = doctrine.parse(
            [
                "@param {external:Foo.bar} baz description"
            ].join('\n'), {});
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'param');
         res.tags[0].type.should.eql({
             "name": "external:Foo.bar",
             "type": "NameExpression"
         });
        res.tags[0].should.have.property('name', 'baz');
        res.tags[0].should.have.property('description', 'description');
    });

    it ('recognize event:', function () {
        var res = doctrine.parse(
            [
                "@function event:Foo.bar"
            ].join('\n'), {});
        res.tags.should.have.length(1);
        res.tags[0].should.have.property('title', 'function');
        res.tags[0].should.have.property('name', 'event:Foo.bar');
        res.tags[0].should.have.property('description', null);
    });

    it ('invalid bogus:', function () {
        var res = doctrine.parse(
            [
                "@method bogus:Foo.bar"
            ].join('\n'), {});
        res.tags.should.have.length(0);
    });
});

/* vim: set sw=4 ts=4 et tw=80 : */
