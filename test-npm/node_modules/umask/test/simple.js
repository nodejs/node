'use strict';

var umask = require('..');

var Code = require('code');
var Lab = require('lab');
var lab = Lab.script();
exports.lab = lab;

var describe = lab.describe;
var it = lab.it;
var expect = Code.expect;

describe('validates umask', function () {
    // signature of validator: validate(obj, key, val)
    // store valid value in obj[key]
    // return false if invalid

    it('accepts numbers', function (done) {
        var o = {},
            result = false;

        result = umask.validate(o, 'umask', 0);
        expect(result).to.equal(true);
        expect(o.umask).to.equal(0);

        result = umask.validate(o, 'umask', 511);
        expect(result).to.equal(true);
        expect(o.umask).to.equal(511);
        done();
    });

    it('accepts strings', function (done) {
        var o = {},
            result;

        result = umask.validate(o, 'umask', "0");
        expect(result).to.equal(true);
        expect(o.umask).to.equal(0);

        result = umask.validate(o, 'umask', "0777");
        expect(result).to.equal(true);
        expect(o.umask).to.equal(511);

        done();
    });

    it('rejects other types', function (done) {
        expect(umask.validate(undefined, undefined, false)).to.equal(false);
        expect(umask.validate(undefined, undefined, {})).to.equal(false);

        done();
    });

    it('rejects non-octalish strings', function (done) {
        expect(umask.validate(undefined, undefined, "1")).to.equal(false);

        done();
    });

    it('rejects NaN strings', function (done) {
        expect(umask.validate(undefined, undefined, NaN)).to.equal(false);

        done();
    });
});

describe('umask to string', function () {
    it("converts umask to string", function (done) {
        expect(umask.toString(0)).to.equal("0000");
        expect(umask.toString(1)).to.equal("0001");
        expect(umask.toString(7)).to.equal("0007");
        expect(umask.toString(8)).to.equal("0010");
        expect(umask.toString(511)).to.equal("0777");
        expect(umask.toString(18)).to.equal("0022");
        expect(umask.toString(16)).to.equal("0020");
        done();
    });
});

describe('umask from string', function () {
    it('converts valid values', function (done) {
        expect(umask.fromString("0000")).to.equal(0);
        expect(umask.fromString("0")).to.equal(0);
        expect(umask.fromString("0777")).to.equal(511);
        expect(umask.fromString("0024")).to.equal(20);

        expect(umask.fromString(0)).to.equal(0);
        expect(umask.fromString(20)).to.equal(20);
        expect(umask.fromString(21)).to.equal(21);
        expect(umask.fromString(511)).to.equal(511);

        done();
    });

    it('converts valid values', function (done) {
        expect(umask.fromString("0000")).to.equal(0);
        expect(umask.fromString("0")).to.equal(0);
        expect(umask.fromString("010")).to.equal(8);
        expect(umask.fromString("0777")).to.equal(511);
        expect(umask.fromString("0024")).to.equal(20);

        expect(umask.fromString("8")).to.equal(8);
        expect(umask.fromString("9")).to.equal(9);
        expect(umask.fromString("18")).to.equal(18);
        expect(umask.fromString("16")).to.equal(16);

        expect(umask.fromString(0)).to.equal(0);
        expect(umask.fromString(20)).to.equal(20);
        expect(umask.fromString(21)).to.equal(21);
        expect(umask.fromString(511)).to.equal(511);

        expect(umask.fromString(0.1)).to.equal(0);
        expect(umask.fromString(511.1)).to.equal(511);

        done();
    });

    it('errors on empty string', function (done) {
        umask.fromString("", function (err, val) {
            expect(err.message).to.equal('Expected octal string, got "", defaulting to "0022"');
            expect(val).to.equal(18);
            done();
        });
    });

    it('errors on invalid octal string', function (done) {
        umask.fromString("099", function (err, val) {
            expect(err.message).to.equal('Expected octal string, got "099", defaulting to "0022"');
            expect(val).to.equal(18);
            done();
        });
    });

    it('errors when non-string, non-number (boolean)', function (done) {
        umask.fromString(false, function (err, val) {
            expect(err.message).to.equal('Expected number or octal string, got false, defaulting to "0022"');
            expect(val).to.equal(18);
            done();
        });
    });

    it('errors when non-string, non-number (object)', function (done) {
        umask.fromString({}, function (err, val) {
            expect(err.message).to.equal('Expected number or octal string, got {}, defaulting to "0022"');
            expect(val).to.equal(18);
            done();
        });
    });

    it('errors when out of range (<0)', function (done) {
        umask.fromString(-1, function (err, val) {
            expect(err.message).to.equal('Must be in range 0..511 (0000..0777), got -1');
            expect(val).to.equal(18);
            done();
        });
    });

    it('errors when out of range (>511)', function (done) {
        umask.fromString(512, function (err, val) {
            expect(err.message).to.equal('Must be in range 0..511 (0000..0777), got 512');
            expect(val).to.equal(18);
            done();
        });
    });
});
