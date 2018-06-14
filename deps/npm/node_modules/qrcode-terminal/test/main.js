var expect = require('expect.js'),
    qrcode = require('./../lib/main'),
    sinon = require('sinon');

describe('in the main module', function() {
    describe('the generate method', function () {
        describe('when not providing a callback', function () {
            beforeEach(function () {
                sinon.stub(console, 'log');
            });

            afterEach(function () {
                sinon.sandbox.restore();
                console.log.reset();
            });

            it('logs to the console', function () {
                qrcode.generate('test');
                expect(console.log.called).to.be(true);
            });
        });

        describe('when providing a callback', function () {
            it('will call the callback', function () {
                var cb = sinon.spy();
                qrcode.generate('test', cb);
                expect(cb.called).to.be(true);
            });

            it('will not call the console.log method', function () {
                qrcode.generate('test', sinon.spy());
                expect(console.log.called).to.be(false);
            });
        });

        describe('the QR Code', function () {
            it('should be a string', function (done) {
                qrcode.generate('test', function(result) {
                    expect(result).to.be.a('string');
                    done();
                });
            });

            it('should not end with a newline', function (done) {
                qrcode.generate('test', function(result) {
                    expect(result).not.to.match(/\n$/);
                    done();
                });
            });
        });

        describe('the error level', function () {
            it('should default to 1', function() {
                expect(qrcode.error).to.be(1);
            });

            it('should not allow other levels', function() {
                qrcode.setErrorLevel = 'something';
                expect(qrcode.error).to.be(1);
            });
        });
    });
});
