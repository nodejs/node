'use strict';

var errcode = require('../index');
var expect = require('expect.js');

describe('errcode', function () {
    describe('string as first argument', function () {
        it('should create an error object without code', function () {
            var err = errcode('my message');

            expect(err).to.be.an(Error);
            expect(err.hasOwnProperty(err.code)).to.be(false);
        });

        it('should create an error object with code', function () {
            var err = errcode('my message', 'ESOME');

            expect(err).to.be.an(Error);
            expect(err.code).to.be('ESOME');
        });

        it('should create an error object with code and properties', function () {
            var err = errcode('my message', 'ESOME', { foo: 'bar', bar: 'foo' });

            expect(err).to.be.an(Error);
            expect(err.code).to.be('ESOME');
            expect(err.foo).to.be('bar');
            expect(err.bar).to.be('foo');
        });

        it('should create an error object without code but with properties', function () {
            var err = errcode('my message', { foo: 'bar', bar: 'foo' });

            expect(err).to.be.an(Error);
            expect(err.code).to.be(undefined);
            expect(err.foo).to.be('bar');
            expect(err.bar).to.be('foo');
        });
    });

    describe('error as first argument', function () {
        it('should accept an error and do nothing', function () {
            var myErr = new Error('my message');
            var err = errcode(myErr);

            expect(err).to.be(myErr);
            expect(err.hasOwnProperty(err.code)).to.be(false);
        });

        it('should accept an error and add a code', function () {
            var myErr = new Error('my message');
            var err = errcode(myErr, 'ESOME');

            expect(err).to.be(myErr);
            expect(err.code).to.be('ESOME');
        });

        it('should accept an error object and add code & properties', function () {
            var myErr = new Error('my message');
            var err = errcode(myErr, 'ESOME', { foo: 'bar', bar: 'foo' });

            expect(err).to.be.an(Error);
            expect(err.code).to.be('ESOME');
            expect(err.foo).to.be('bar');
            expect(err.bar).to.be('foo');
        });

        it('should create an error object without code but with properties', function () {
            var myErr = new Error('my message');
            var err = errcode(myErr, { foo: 'bar', bar: 'foo' });

            expect(err).to.be.an(Error);
            expect(err.code).to.be(undefined);
            expect(err.foo).to.be('bar');
            expect(err.bar).to.be('foo');
        });
    });

    it('should allow passing null & undefined in the first argument', function () {
        var err;

        err = errcode(null, 'ESOME');
        expect(err).to.be.an(Error);
        expect(err.message).to.be('null');
        expect(err.code).to.be('ESOME');

        err = errcode(undefined, 'ESOME');
        expect(err).to.be.an(Error);
        expect(err.message).to.be('');
        expect(err.code).to.be('ESOME');
    });
});
