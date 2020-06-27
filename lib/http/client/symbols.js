'use strict';

const {
  Symbol,
} = primordials;

module.exports = {
  kUrl: Symbol('url'),
  kWriting: Symbol('writing'),
  kQueue: Symbol('queue'),
  kSocketTimeout: Symbol('socket timeout'),
  kRequestTimeout: Symbol('request timeout'),
  kServerName: Symbol('server name'),
  kTLSOpts: Symbol('TLS Options'),
  kClosed: Symbol('closed'),
  kDestroyed: Symbol('destroyed'),
  kMaxHeadersSize: Symbol('maxHeaderSize'),
  kRunningIdx: Symbol('running index'),
  kPendingIdx: Symbol('pending index'),
  kResume: Symbol('resume'),
  kError: Symbol('error'),
  kOnDestroyed: Symbol('destroy callbacks'),
  kPipelining: Symbol('pipelinig'),
  kRetryDelay: Symbol('retry delay'),
  kSocket: Symbol('socket'),
  kParser: Symbol('parser'),
  kClients: Symbol('clients'),
  kRetryTimeout: Symbol('retry timeout'),
  kClient: Symbol('client'),
  kEnqueue: Symbol('enqueue'),
  kMaxAbortedPayload: Symbol('max aborted payload')
};
