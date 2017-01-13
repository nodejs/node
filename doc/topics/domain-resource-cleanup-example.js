'use strict';

const domain = require('domain');
const EE = require('events');
const fs = require('fs');
const net = require('net');
const util = require('util');
const print = process._rawDebug;

const pipeList = [];
const FILENAME = '/tmp/tmp.tmp';
const PIPENAME = '/tmp/node-domain-example-';
const FILESIZE = 1024;
var uid = 0;

// Setting up temporary resources
const buf = Buffer(FILESIZE);
for (var i = 0; i < buf.length; i++)
  buf[i] = ((Math.random() * 1e3) % 78) + 48;  // Basic ASCII
fs.writeFileSync(FILENAME, buf);

function ConnectionResource(c) {
  EE.call(this);
  this._connection = c;
  this._alive = true;
  this._domain = domain.create();
  this._id = Math.random().toString(32).substr(2).substr(0, 8) + (++uid);

  this._domain.add(c);
  this._domain.on('error', () => {
    this._alive = false;
  });
}
util.inherits(ConnectionResource, EE);

ConnectionResource.prototype.end = function end(chunk) {
  this._alive = false;
  this._connection.end(chunk);
  this.emit('end');
};

ConnectionResource.prototype.isAlive = function isAlive() {
  return this._alive;
};

ConnectionResource.prototype.id = function id() {
  return this._id;
};

ConnectionResource.prototype.write = function write(chunk) {
  this.emit('data', chunk);
  return this._connection.write(chunk);
};

// Example begin
net.createServer((c) => {
  const cr = new ConnectionResource(c);

  const d1 = domain.create();
  fs.open(FILENAME, 'r', d1.intercept((fd) => {
    streamInParts(fd, cr, 0);
  }));

  pipeData(cr);

  c.on('close', () => cr.end());
}).listen(8080);

function streamInParts(fd, cr, pos) {
  const d2 = domain.create();
  var alive = true;
  d2.on('error', (er) => {
    print('d2 error:', er.message)
    cr.end();
  });
  fs.read(fd, new Buffer(10), 0, 10, pos, d2.intercept((bRead, buf) => {
    if (!cr.isAlive()) {
      return fs.close(fd);
    }
    if (cr._connection.bytesWritten < FILESIZE) {
      // Documentation says callback is optional, but doesn't mention that if
      // the write fails an exception will be thrown.
      const goodtogo = cr.write(buf);
      if (goodtogo) {
        setTimeout(() => streamInParts(fd, cr, pos + bRead), 1000);
      } else {
        cr._connection.once('drain', () => streamInParts(fd, cr, pos + bRead));
      }
      return;
    }
    cr.end(buf);
    fs.close(fd);
  }));
}

function pipeData(cr) {
  const pname = PIPENAME + cr.id();
  const ps = net.createServer();
  const d3 = domain.create();
  const connectionList = [];
  d3.on('error', (er) => {
    print('d3 error:', er.message);
    cr.end();
  });
  d3.add(ps);
  ps.on('connection', (conn) => {
    connectionList.push(conn);
    conn.on('data', () => {});  // don't care about incoming data.
    conn.on('close', () => {
      connectionList.splice(connectionList.indexOf(conn), 1);
    });
  });
  cr.on('data', (chunk) => {
    for (var i = 0; i < connectionList.length; i++) {
      connectionList[i].write(chunk);
    }
  });
  cr.on('end', () => {
    for (var i = 0; i < connectionList.length; i++) {
      connectionList[i].end();
    }
    ps.close();
  });
  pipeList.push(pname);
  ps.listen(pname);
}

process.on('SIGINT', () => process.exit());
process.on('exit', () => {
  try {
    for (var i = 0; i < pipeList.length; i++) {
      fs.unlinkSync(pipeList[i]);
    }
    fs.unlinkSync(FILENAME);
  } catch (e) { }
});
