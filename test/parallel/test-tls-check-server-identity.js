'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const util = require('util');

const tls = require('tls');

const tests = [
  // False-y values.
  {
    host: false,
    cert: { subject: { CN: 'a.com' } },
    error: 'Host: false. is not cert\'s CN: a.com'
  },
  {
    host: null,
    cert: { subject: { CN: 'a.com' } },
    error: 'Host: null. is not cert\'s CN: a.com'
  },
  {
    host: undefined,
    cert: { subject: { CN: 'a.com' } },
    error: 'Host: undefined. is not cert\'s CN: a.com'
  },

  // Basic CN handling
  { host: 'a.com', cert: { subject: { CN: 'a.com' } } },
  { host: 'a.com', cert: { subject: { CN: 'A.COM' } } },
  {
    host: 'a.com',
    cert: { subject: { CN: 'b.com' } },
    error: 'Host: a.com. is not cert\'s CN: b.com'
  },
  { host: 'a.com', cert: { subject: { CN: 'a.com.' } } },
  {
    host: 'a.com',
    cert: { subject: { CN: '.a.com' } },
    error: 'Host: a.com. is not cert\'s CN: .a.com'
  },

  // Wildcards in CN
  { host: 'b.a.com', cert: { subject: { CN: '*.a.com' } } },
  {
    host: 'ba.com',
    cert: { subject: { CN: '*.a.com' } },
    error: 'Host: ba.com. is not cert\'s CN: *.a.com'
  },
  {
    host: '\n.b.com',
    cert: { subject: { CN: '*n.b.com' } },
    error: 'Host: \n.b.com. is not cert\'s CN: *n.b.com'
  },
  { host: 'b.a.com', cert: {
    subjectaltname: 'DNS:omg.com',
    subject: { CN: '*.a.com' } },
    error: 'Host: b.a.com. is not in the cert\'s altnames: ' +
           'DNS:omg.com'
  },
  {
    host: 'b.a.com',
    cert: { subject: { CN: 'b*b.a.com' } },
    error: 'Host: b.a.com. is not cert\'s CN: b*b.a.com'
  },

  // Empty Cert
  {
    host: 'a.com',
    cert: { },
    error: 'Cert is empty'
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
  // IDNA
  {
    host: 'xn--bcher-kva.example.com',
    cert: { subject: { CN: '*.example.com' } },
  },
  // RFC 6125, section 6.4.3: "[...] the client SHOULD NOT attempt to match
  // a presented identifier where the wildcard character is embedded within
  // an A-label [...]"
  {
    host: 'xn--bcher-kva.example.com',
    cert: { subject: { CN: 'xn--*.example.com' } },
    error: 'Host: xn--bcher-kva.example.com. is not cert\'s CN: ' +
            'xn--*.example.com',
  },
];

tests.forEach(function(test, i) {
  const err = tls.checkServerIdentity(test.host, test.cert);
  assert.strictEqual(err && err.reason,
                     test.error,
                     `Test# ${i} failed: ${util.inspect(test)} \n` +
                     `${test.error} != ${(err && err.reason)}`);
});
