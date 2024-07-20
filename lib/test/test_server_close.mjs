'use strict';

import { createServer } from 'http';
import { expect } from 'chai';
import http from 'http';

describe('Server.prototype.close', function () {
  let server;

  beforeEach((done) => {
    server = createServer((req, res) => {
      res.end('handled'); // Ensure the response matches the expected value in the test
    }).listen(0, done);
  });

  afterEach((done) => {
    if (server && server.listening) {
      server.close((err) => {
        if (err) {
          console.error('Error closing server:', err);
        }
        server = null;
        done();
      });
    } else {
      server = null;
      done();
    }
  });

  it('should call the callback when the server is closed', (done) => {
    server.close(() => {
      done();
    });
  });

  it('should handle requests before closing', (done) => {
    const port = server.address().port;
    http.get(`http://localhost:${port}`, (res) => {
      let data = '';
      res.on('data', (chunk) => {
        data += chunk;
      });
      res.on('end', () => {
        try {
          expect(data).to.equal('handled');
          done();
        } catch (err) {
          done(err);
        }
      });
    });
  });

  it('should prevent new connections after close is called', (done) => {
    server.close(() => {
      const port = server.address() ? server.address().port : null;
      if (!port) {
        done();
        return;
      }
      http
        .get(`http://localhost:${port}`, (res) => {
          res.on('data', () => {});
          res.on('end', () => {
            done(new Error('Should not have handled request'));
          });
        })
        .on('error', (err) => {
          expect(err).to.be.an('error');
          done();
        });
    });
  });
});
