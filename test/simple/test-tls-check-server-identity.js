// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');
var util = require('util');
var tls = require('tls');

var tests = [
  // Basic CN handling
  { host: 'a.com', cert: { subject: { CN: 'a.com' } } },
  { host: 'a.com', cert: { subject: { CN: 'A.COM' } } },
  {
    host: 'a.com',
    cert: { subject: { CN: 'b.com' } },
    error: 'Host: a.com. is not cert\'s CN: b.com'
  },
  { host: 'a.com', cert: { subject: { CN: 'a.com.' } } },

  // Wildcards in CN
  { host: 'b.a.com', cert: { subject: { CN: '*.a.com' } } },
  { host: 'b.a.com', cert: {
    subjectaltname: 'DNS:omg.com',
    subject: { CN: '*.a.com' } },
    error: 'Host: b.a.com. is not in the cert\'s altnames: ' +
           'DNS:omg.com'
  },

  // Multiple CN fields
  {
    host: 'foo.com', cert: {
      subject: { CN: ['foo.com', 'bar.com'] } // CN=foo.com; CN=bar.com;
    }
  },

  // DNS names and CN
  {
    host: 'a.com', cert: {
      subjectaltname: 'DNS:*',
      subject: { CN: 'b.com' }
    },
    error: 'Host: a.com. is not in the cert\'s altnames: ' +
           'DNS:*'
  },
  {
    host: 'a.com', cert: {
      subjectaltname: 'DNS:*.com',
      subject: { CN: 'b.com' }
    },
    error: 'Host: a.com. is not in the cert\'s altnames: ' +
           'DNS:*.com'
  },
  {
    host: 'a.co.uk', cert: {
      subjectaltname: 'DNS:*.co.uk',
      subject: { CN: 'b.com' }
    }
  },
  {
    host: 'a.com', cert: {
      subjectaltname: 'DNS:*.a.com',
      subject: { CN: 'a.com' }
    },
    error: 'Host: a.com. is not in the cert\'s altnames: ' +
           'DNS:*.a.com'
  },
  {
    host: 'a.com', cert: {
      subjectaltname: 'DNS:*.a.com',
      subject: { CN: 'b.com' }
    },
    error: 'Host: a.com. is not in the cert\'s altnames: ' +
           'DNS:*.a.com'
  },
  {
    host: 'a.com', cert: {
      subjectaltname: 'DNS:a.com',
      subject: { CN: 'b.com' }
    }
  },
  {
    host: 'a.com', cert: {
      subjectaltname: 'DNS:A.COM',
      subject: { CN: 'b.com' }
    }
  },

  // DNS names
  {
    host: 'a.com', cert: {
      subjectaltname: 'DNS:*.a.com',
      subject: {}
    },
    error: 'Host: a.com. is not in the cert\'s altnames: ' +
           'DNS:*.a.com'
  },
  {
    host: 'b.a.com', cert: {
      subjectaltname: 'DNS:*.a.com',
      subject: {}
    }
  },
  {
    host: 'c.b.a.com', cert: {
      subjectaltname: 'DNS:*.a.com',
      subject: {}
    },
    error: 'Host: c.b.a.com. is not in the cert\'s altnames: ' +
           'DNS:*.a.com'
  },
  {
    host: 'b.a.com', cert: {
      subjectaltname: 'DNS:*b.a.com',
      subject: {}
    }
  },
  {
    host: 'a-cb.a.com', cert: {
      subjectaltname: 'DNS:*b.a.com',
      subject: {}
    }
  },
  {
    host: 'a.b.a.com', cert: {
      subjectaltname: 'DNS:*b.a.com',
      subject: {}
    },
    error: 'Host: a.b.a.com. is not in the cert\'s altnames: ' +
           'DNS:*b.a.com'
  },
  // Mutliple DNS names
  {
    host: 'a.b.a.com', cert: {
      subjectaltname: 'DNS:*b.a.com, DNS:a.b.a.com',
      subject: {}
    }
  },
  // URI names
  {
    host: 'a.b.a.com', cert: {
      subjectaltname: 'URI:http://a.b.a.com/',
      subject: {}
    }
  },
  {
    host: 'a.b.a.com', cert: {
      subjectaltname: 'URI:http://*.b.a.com/',
      subject: {}
    },
    error: 'Host: a.b.a.com. is not in the cert\'s altnames: ' +
           'URI:http://*.b.a.com/'
  },
  // IP addresses
  {
    host: 'a.b.a.com', cert: {
      subjectaltname: 'IP Address:127.0.0.1',
      subject: {}
    },
    error: 'Host: a.b.a.com. is not in the cert\'s altnames: ' +
           'IP Address:127.0.0.1'
  },
  {
    host: '127.0.0.1', cert: {
      subjectaltname: 'IP Address:127.0.0.1',
      subject: {}
    }
  },
  {
    host: '127.0.0.2', cert: {
      subjectaltname: 'IP Address:127.0.0.1',
      subject: {}
    },
    error: 'IP: 127.0.0.2 is not in the cert\'s list: ' +
           '127.0.0.1'
  },
  {
    host: '127.0.0.1', cert: {
      subjectaltname: 'DNS:a.com',
      subject: {}
    },
    error: 'IP: 127.0.0.1 is not in the cert\'s list: '
  },
  {
    host: 'localhost', cert: {
      subjectaltname: 'DNS:a.com',
      subject: { CN: 'localhost' }
    },
    error: 'Host: localhost. is not in the cert\'s altnames: ' +
           'DNS:a.com'
  },
];

tests.forEach(function(test, i) {
  var err = tls.checkServerIdentity(test.host, test.cert);
  assert.equal(err && err.reason,
               test.error,
               'Test#' + i + ' failed: ' + util.inspect(test) + '\n' +
               test.error + ' != ' + (err && err.reason));
});
