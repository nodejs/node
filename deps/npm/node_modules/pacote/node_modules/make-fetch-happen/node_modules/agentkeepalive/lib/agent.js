/**
 * refer:
 *   * @atimb "Real keep-alive HTTP agent": https://gist.github.com/2963672
 *   * https://github.com/joyent/node/blob/master/lib/http.js
 *   * https://github.com/joyent/node/blob/master/lib/https.js
 *   * https://github.com/joyent/node/blob/master/lib/_http_agent.js
 */

'use strict';

const OriginalAgent = require('./_http_agent').Agent;
const ms = require('humanize-ms');

class Agent extends OriginalAgent {
  constructor(options) {
    options = options || {};
    options.keepAlive = options.keepAlive !== false;
    // default is keep-alive and 15s free socket timeout
    if (options.freeSocketKeepAliveTimeout === undefined) {
      options.freeSocketKeepAliveTimeout = 15000;
    }
    // Legacy API: keepAliveTimeout should be rename to `freeSocketKeepAliveTimeout`
    if (options.keepAliveTimeout) {
      options.freeSocketKeepAliveTimeout = options.keepAliveTimeout;
    }
    options.freeSocketKeepAliveTimeout = ms(options.freeSocketKeepAliveTimeout);

    // Sets the socket to timeout after timeout milliseconds of inactivity on the socket.
    // By default is double free socket keepalive timeout.
    if (options.timeout === undefined) {
      options.timeout = options.freeSocketKeepAliveTimeout * 2;
      // make sure socket default inactivity timeout >= 30s
      if (options.timeout < 30000) {
        options.timeout = 30000;
      }
    }
    options.timeout = ms(options.timeout);

    super(options);

    this.createSocketCount = 0;
    this.createSocketErrorCount = 0;
    this.closeSocketCount = 0;
    // socket error event count
    this.errorSocketCount = 0;
    this.requestCount = 0;
    this.timeoutSocketCount = 0;
    this.on('free', s => {
      this.requestCount++;
      // last enter free queue timestamp
      s.lastFreeTime = Date.now();
    });
    this.on('timeout', () => {
      this.timeoutSocketCount++;
    });
    this.on('close', () => {
      this.closeSocketCount++;
    });
    this.on('error', () => {
      this.errorSocketCount++;
    });
  }

  createSocket(req, options, cb) {
    super.createSocket(req, options, (err, socket) => {
      if (err) {
        this.createSocketErrorCount++;
        return cb(err);
      }
      if (this.keepAlive) {
        // Disable Nagle's algorithm: http://blog.caustik.com/2012/04/08/scaling-node-js-to-100k-concurrent-connections/
        // https://fengmk2.com/benchmark/nagle-algorithm-delayed-ack-mock.html
        socket.setNoDelay(true);
      }
      this.createSocketCount++;
      cb(null, socket);
    });
  }

  getCurrentStatus() {
    return {
      createSocketCount: this.createSocketCount,
      createSocketErrorCount: this.createSocketErrorCount,
      closeSocketCount: this.closeSocketCount,
      errorSocketCount: this.errorSocketCount,
      timeoutSocketCount: this.timeoutSocketCount,
      requestCount: this.requestCount,
      freeSockets: inspect(this.freeSockets),
      sockets: inspect(this.sockets),
      requests: inspect(this.requests),
    };
  }
}

module.exports = Agent;

function inspect(obj) {
  const res = {};
  for (const key in obj) {
    res[key] = obj[key].length;
  }
  return res;
}
