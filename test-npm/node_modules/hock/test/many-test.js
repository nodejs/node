var should = require('should'),
  request = require('request'),
  hock = require('../');

var PORT = 5678;

describe('Hock Multiple Request Tests', function () {

  var server;

  describe("With minimum requests", function () {
    beforeEach(function (done) {
      hock.createHock(function (err, hockServer) {
        should.not.exist(err);
        should.exist(hockServer);

        server = hockServer;
        done();
      });
    });

    it('should succeed with once', function (done) {
      server
        .get('/url')
        .once()
        .reply(200, { 'hock': 'ok' });

      request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
        should.not.exist(err);
        should.exist(res);
        res.statusCode.should.equal(200);
        JSON.parse(body).should.eql({ 'hock': 'ok' });
        server.done();
        done();
      });
    });

    it('should fail with min: 2 and a single request', function (done) {
      server
        .get('/url')
        .min(2)
        .reply(200, { 'hock': 'ok' });

      request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
        should.not.exist(err);
        should.exist(res);
        res.statusCode.should.equal(200);
        JSON.parse(body).should.eql({ 'hock': 'ok' });
        (function() {
          server.done();
        }).should.throw();
        done();
      });
    });

    it('should succeed with min:2 and 2 requests', function (done) {
      server
        .get('/url')
        .min(2)
        .reply(200, { 'hock': 'ok' });

      request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
        should.not.exist(err);
        should.exist(res);
        res.statusCode.should.equal(200);
        JSON.parse(body).should.eql({ 'hock': 'ok' });

        request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          JSON.parse(body).should.eql({ 'hock': 'ok' });

          server.done();
          done();
        });
      });
    });

    it('should succeed with max:2 and 1 request', function (done) {
      server
        .get('/url')
        .max(2)
        .reply(200, { 'hock': 'ok' });

      request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
        should.not.exist(err);
        should.exist(res);
        res.statusCode.should.equal(200);
        JSON.parse(body).should.eql({ 'hock': 'ok' });

        server.done();
        done();
      });
    });

    it('should succeed with max:2 and 2 requests', function (done) {
      server
        .get('/url')
        .max(2)
        .reply(200, { 'hock': 'ok' });

      request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
        should.not.exist(err);
        should.exist(res);
        res.statusCode.should.equal(200);
        JSON.parse(body).should.eql({ 'hock': 'ok' });

        request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          JSON.parse(body).should.eql({ 'hock': 'ok' });

          server.done();
          done();
        });
      });
    });

    it('should succeed with min:2, max:3 and 2 requests', function (done) {
      server
        .get('/url')
        .min(2)
        .max(3)
        .reply(200, { 'hock': 'ok' });

      request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
        should.not.exist(err);
        should.exist(res);
        res.statusCode.should.equal(200);
        JSON.parse(body).should.eql({ 'hock': 'ok' });

        request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          JSON.parse(body).should.eql({ 'hock': 'ok' });

          server.done();
          done();
        });
      });
    });

    it('should succeed with min:2, max:Infinity and 2 requests', function (done) {
      server
        .get('/url')
        .min(2)
        .max(Infinity)
        .reply(200, { 'hock': 'ok' });

      request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
        should.not.exist(err);
        should.exist(res);
        res.statusCode.should.equal(200);
        JSON.parse(body).should.eql({ 'hock': 'ok' });

        request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          JSON.parse(body).should.eql({ 'hock': 'ok' });

          server.done();
          done();
        });
      });
    });

    it('should succeed with 2 different routes with different min, max values', function (done) {
      server
        .get('/url')
        .min(2)
        .max(3)
        .reply(200, { 'hock': 'ok' })
        .get('/asdf')
        .once()
        .reply(200, { 'hock': 'ok' });

      request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
        should.not.exist(err);
        should.exist(res);
        res.statusCode.should.equal(200);
        JSON.parse(body).should.eql({ 'hock': 'ok' });

        request('http://localhost:' + server.address().port + '/asdf', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          JSON.parse(body).should.eql({ 'hock': 'ok' });

          request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
            should.not.exist(err);
            should.exist(res);
            res.statusCode.should.equal(200);
            JSON.parse(body).should.eql({ 'hock': 'ok' });

            server.done();
            done();
          });
        });
      });
    });

    describe('min() and max() with replyWithFile', function () {
      it('should succeed with a single call', function (done) {
        server
          .get('/url')
          .replyWithFile(200, process.cwd() + '/test/data/hello.txt');

        request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          body.should.equal('this\nis\nmy\nsample\n');
          server.done(function (err) {
            should.not.exist(err);
            done();
          });
        });
      });

      it('should succeed with a multiple calls', function (done) {
        server
          .get('/url')
          .twice()
          .replyWithFile(200, process.cwd() + '/test/data/hello.txt');

        request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          body.should.equal('this\nis\nmy\nsample\n');

          request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
            should.not.exist(err);
            should.exist(res);
            res.statusCode.should.equal(200);
            body.should.equal('this\nis\nmy\nsample\n');
            server.done(function (err) {
              should.not.exist(err);
              done();
            });
          });
        });
      });
    });

    describe('many()', function() {

      it('should fail with no requests', function (done) {
        server
          .get('/url')
          .many()
          .reply(200, { 'hock': 'ok' });

          (function() {
            server.done();
          }).should.throw();
          done();
      })

      it('should succeed with many requests', function (done) {
        server
          .get('/url')
          .many()
          .reply(200, { 'hock': 'ok' })

        request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          JSON.parse(body).should.eql({ 'hock': 'ok' });

          request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
            should.not.exist(err);
            should.exist(res);
            res.statusCode.should.equal(200);
            JSON.parse(body).should.eql({ 'hock': 'ok' });

            request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
              should.not.exist(err);
              should.exist(res);
              res.statusCode.should.equal(200);
              JSON.parse(body).should.eql({ 'hock': 'ok' });

              server.done();
              done();
            });
          });
        });
      });
    });


    describe('any', function() {
      it('should succeed with no requests', function (done) {
        server
          .get('/url')
          .any()
          .reply(200, { 'hock': 'ok' })
          .done();
          done();
      })

      it('should succeed with many requests', function (done) {
        server
          .get('/url')
          .any()
          .reply(200, { 'hock': 'ok' });

        request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
          should.not.exist(err);
          should.exist(res);
          res.statusCode.should.equal(200);
          JSON.parse(body).should.eql({ 'hock': 'ok' });

          request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
            should.not.exist(err);
            should.exist(res);
            res.statusCode.should.equal(200);
            JSON.parse(body).should.eql({ 'hock': 'ok' });

            request('http://localhost:' + server.address().port + '/url', function (err, res, body) {
              should.not.exist(err);
              should.exist(res);
              res.statusCode.should.equal(200);
              JSON.parse(body).should.eql({ 'hock': 'ok' });

              server.done();
              done();
            });
          });
        });
      });
    });

    afterEach(function (done) {
      server.close(done);
    });
  });
});
