'use strict';

const asn1 = require('asn1.js');
const crypto = require('crypto');
const { writeFileSync } = require('fs');
const rfc5280 = require('asn1.js-rfc5280');
const BN = asn1.bignum;

const oid = {
  commonName: [2, 5, 4, 3],
  countryName: [2, 5, 4, 6],
  localityName: [2, 5, 4, 7],
  rsaEncryption: [1, 2, 840, 113549, 1, 1, 1],
  sha256WithRSAEncryption: [1, 2, 840, 113549, 1, 1, 11],
  xmppAddr: [1, 3, 6, 1, 5, 5, 7, 8, 5],
  srvName: [1, 3, 6, 1, 5, 5, 7, 8, 7],
  ocsp: [1, 3, 6, 1, 5, 5, 7, 48, 1],
  caIssuers: [1, 3, 6, 1, 5, 5, 7, 48, 2],
  privateUnrecognized: [1, 3, 9999, 12, 34]
};

const digest = 'SHA256';

const { privateKey, publicKey } = crypto.generateKeyPairSync('rsa', {
  modulusLength: 4096,
  publicKeyEncoding: {
    type: 'pkcs1',
    format: 'der'
  }
});

writeFileSync('server-key.pem', privateKey.export({
  type: 'pkcs8',
  format: 'pem'
}));

const now = Date.now();
const days = 3650;

function utilType(name, fn) {
  return asn1.define(name, function() {
    this[fn]();
  });
}

const Null_ = utilType('Null_', 'null_');
const null_ = Null_.encode('der');

const IA5String = utilType('IA5String', 'ia5str');
const PrintableString = utilType('PrintableString', 'printstr');
const UTF8String = utilType('UTF8String', 'utf8str');

const subjectCommonName = PrintableString.encode('evil.example.com', 'der');

const sans = [
  { type: 'dNSName', value: 'good.example.com, DNS:evil.example.com' },
  { type: 'uniformResourceIdentifier', value: 'http://example.com/' },
  { type: 'uniformResourceIdentifier', value: 'http://example.com/?a=b&c=d' },
  { type: 'uniformResourceIdentifier', value: 'http://example.com/a,b' },
  { type: 'uniformResourceIdentifier', value: 'http://example.com/a%2Cb' },
  {
    type: 'uniformResourceIdentifier',
    value: 'http://example.com/a, DNS:good.example.com'
  },
  { type: 'dNSName', value: Buffer.from('exämple.com', 'latin1') },
  { type: 'dNSName', value: '"evil.example.com"' },
  { type: 'iPAddress', value: Buffer.from('08080808', 'hex') },
  { type: 'iPAddress', value: Buffer.from('08080404', 'hex') },
  { type: 'iPAddress', value: Buffer.from('0008080404', 'hex') },
  { type: 'iPAddress', value: Buffer.from('000102030405', 'hex') },
  {
    type: 'iPAddress',
    value: Buffer.from('0a0b0c0d0e0f0000000000007a7b7c7d', 'hex')
  },
  { type: 'rfc822Name', value: 'foo@example.com' },
  { type: 'rfc822Name', value: 'foo@example.com, DNS:good.example.com' },
  {
    type: 'directoryName',
    value: {
      type: 'rdnSequence',
      value: [
        [
          {
            type: oid.countryName,
            value: PrintableString.encode('DE', 'der')
          }
        ],
        [
          {
            type: oid.localityName,
            value: UTF8String.encode('Hannover', 'der')
          }
        ]
      ]
    }
  },
  {
    type: 'directoryName',
    value: {
      type: 'rdnSequence',
      value: [
        [
          {
            type: oid.countryName,
            value: PrintableString.encode('DE', 'der')
          }
        ],
        [
          {
            type: oid.localityName,
            value: UTF8String.encode('München', 'der')
          }
        ]
      ]
    }
  },
  {
    type: 'directoryName',
    value: {
      type: 'rdnSequence',
      value: [
        [
          {
            type: oid.countryName,
            value: PrintableString.encode('DE', 'der')
          }
        ],
        [
          {
            type: oid.localityName,
            value: UTF8String.encode('Berlin, DNS:good.example.com', 'der')
          }
        ]
      ]
    }
  },
  {
    type: 'directoryName',
    value: {
      type: 'rdnSequence',
      value: [
        [
          {
            type: oid.countryName,
            value: PrintableString.encode('DE', 'der')
          }
        ],
        [
          {
            type: oid.localityName,
            value: UTF8String.encode('Berlin, DNS:good.example.com\0evil.example.com', 'der')
          }
        ]
      ]
    }
  },
  {
    type: 'directoryName',
    value: {
      type: 'rdnSequence',
      value: [
        [
          {
            type: oid.countryName,
            value: PrintableString.encode('DE', 'der')
          }
        ],
        [
          {
            type: oid.localityName,
            value: UTF8String.encode(
              'Berlin, DNS:good.example.com\\\0evil.example.com', 'der')
          }
        ]
      ]
    }
  },
  {
    type: 'directoryName',
    value: {
      type: 'rdnSequence',
      value: [
        [
          {
            type: oid.countryName,
            value: PrintableString.encode('DE', 'der')
          }
        ],
        [
          {
            type: oid.localityName,
            value: UTF8String.encode('Berlin\r\n', 'der')
          }
        ]
      ]
    }
  },
  {
    type: 'directoryName',
    value: {
      type: 'rdnSequence',
      value: [
        [
          {
            type: oid.countryName,
            value: PrintableString.encode('DE', 'der')
          }
        ],
        [
          {
            type: oid.localityName,
            value: UTF8String.encode('Berlin/CN=good.example.com', 'der')
          }
        ]
      ]
    }
  },
  {
    type: 'registeredID',
    value: oid.sha256WithRSAEncryption
  },
  {
    type: 'registeredID',
    value: oid.privateUnrecognized
  },
  {
    type: 'otherName',
    value: {
      'type-id': oid.xmppAddr,
      value: UTF8String.encode('abc123', 'der')
    }
  },
  {
    type: 'otherName',
    value: {
      'type-id': oid.xmppAddr,
      value: UTF8String.encode('abc123, DNS:good.example.com', 'der')
    }
  },
  {
    type: 'otherName',
    value: {
      'type-id': oid.xmppAddr,
      value: UTF8String.encode('good.example.com\0abc123', 'der')
    }
  },
  {
    type: 'otherName',
    value: {
      'type-id': oid.privateUnrecognized,
      value: UTF8String.encode('abc123', 'der')
    }
  },
  {
    type: 'otherName',
    value: {
      'type-id': oid.srvName,
      value: IA5String.encode('abc123', 'der')
    }
  },
  {
    type: 'otherName',
    value: {
      'type-id': oid.srvName,
      value: UTF8String.encode('abc123', 'der')
    }
  },
  {
    type: 'otherName',
    value: {
      'type-id': oid.srvName,
      value: IA5String.encode('abc\0def', 'der')
    }
  }
];

for (let i = 0; i < sans.length; i++) {
  const san = sans[i];

  const tbs = {
    version: 'v3',
    serialNumber: new BN('01', 16),
    signature: {
      algorithm: oid.sha256WithRSAEncryption,
      parameters: null_
    },
    issuer: {
      type: 'rdnSequence',
      value: [
        [
          { type: oid.commonName, value: subjectCommonName }
        ]
      ]
    },
    validity: {
      notBefore: { type: 'utcTime', value: now },
      notAfter: { type: 'utcTime', value: now + days * 86400000 }
    },
    subject: {
      type: 'rdnSequence',
      value: [
        [
          { type: oid.commonName, value: subjectCommonName }
        ]
      ]
    },
    subjectPublicKeyInfo: {
      algorithm: {
        algorithm: oid.rsaEncryption,
        parameters: null_
      },
      subjectPublicKey: {
        unused: 0,
        data: publicKey
      }
    },
    extensions: [
      {
        extnID: 'subjectAlternativeName',
        critical: false,
        extnValue: [san]
      }
    ]
  };

  // Self-sign the certificate.
  const tbsDer = rfc5280.TBSCertificate.encode(tbs, 'der');
  const signature = crypto.createSign(digest).update(tbsDer).sign(privateKey);

  // Construct the signed certificate.
  const cert = {
    tbsCertificate: tbs,
    signatureAlgorithm: {
      algorithm: oid.sha256WithRSAEncryption,
      parameters: null_
    },
    signature: {
      unused: 0,
      data: signature
    }
  };

  // Store the signed certificate.
  const pem = rfc5280.Certificate.encode(cert, 'pem', {
    label: 'CERTIFICATE'
  });
  writeFileSync(`./alt-${i}-cert.pem`, `${pem}\n`);
}

const infoAccessExtensions = [
  [
    {
      accessMethod: oid.ocsp,
      accessLocation: {
        type: 'uniformResourceIdentifier',
        value: 'http://good.example.com/\nOCSP - URI:http://evil.example.com/',
      },
    },
  ],
  [
    {
      accessMethod: oid.caIssuers,
      accessLocation: {
        type: 'uniformResourceIdentifier',
        value: 'http://ca.example.com/\nOCSP - URI:http://evil.example.com',
      },
    },
    {
      accessMethod: oid.ocsp,
      accessLocation: {
        type: 'dNSName',
        value: 'good.example.com\nOCSP - URI:http://ca.nodejs.org/ca.cert',
      },
    },
  ],
  [
    {
      accessMethod: oid.privateUnrecognized,
      accessLocation: {
        type: 'uniformResourceIdentifier',
        value: 'http://ca.example.com/',
      },
    },
  ],
  [
    {
      accessMethod: oid.ocsp,
      accessLocation: {
        type: 'otherName',
        value: {
          'type-id': oid.xmppAddr,
          value: UTF8String.encode('good.example.com', 'der'),
        },
      },
    },
    {
      accessMethod: oid.ocsp,
      accessLocation: {
        type: 'otherName',
        value: {
          'type-id': oid.privateUnrecognized,
          value: UTF8String.encode('abc123', 'der')
        },
      },
    },
    {
      accessMethod: oid.ocsp,
      accessLocation: {
        type: 'otherName',
        value: {
          'type-id': oid.srvName,
          value: IA5String.encode('abc123', 'der')
        }
      }
    },
  ],
  [
    {
      accessMethod: oid.ocsp,
      accessLocation: {
        type: 'otherName',
        value: {
          'type-id': oid.xmppAddr,
          value: UTF8String.encode('good.example.com\0abc123', 'der'),
        },
      },
    },
  ],
];

for (let i = 0; i < infoAccessExtensions.length; i++) {
  const infoAccess = infoAccessExtensions[i];

  const tbs = {
    version: 'v3',
    serialNumber: new BN('01', 16),
    signature: {
      algorithm: oid.sha256WithRSAEncryption,
      parameters: null_
    },
    issuer: {
      type: 'rdnSequence',
      value: [
        [
          { type: oid.commonName, value: subjectCommonName }
        ]
      ]
    },
    validity: {
      notBefore: { type: 'utcTime', value: now },
      notAfter: { type: 'utcTime', value: now + days * 86400000 }
    },
    subject: {
      type: 'rdnSequence',
      value: [
        [
          { type: oid.commonName, value: subjectCommonName }
        ]
      ]
    },
    subjectPublicKeyInfo: {
      algorithm: {
        algorithm: oid.rsaEncryption,
        parameters: null_
      },
      subjectPublicKey: {
        unused: 0,
        data: publicKey
      }
    },
    extensions: [
      {
        extnID: 'authorityInformationAccess',
        critical: false,
        extnValue: infoAccess
      }
    ]
  };

  // Self-sign the certificate.
  const tbsDer = rfc5280.TBSCertificate.encode(tbs, 'der');
  const signature = crypto.createSign(digest).update(tbsDer).sign(privateKey);

  // Construct the signed certificate.
  const cert = {
    tbsCertificate: tbs,
    signatureAlgorithm: {
      algorithm: oid.sha256WithRSAEncryption,
      parameters: null_
    },
    signature: {
      unused: 0,
      data: signature
    }
  };

  // Store the signed certificate.
  const pem = rfc5280.Certificate.encode(cert, 'pem', {
    label: 'CERTIFICATE'
  });
  writeFileSync(`./info-${i}-cert.pem`, `${pem}\n`);
}

const subjects = [
  [
    [
      { type: oid.localityName, value: UTF8String.encode('Somewhere') }
    ],
    [
      { type: oid.commonName, value: UTF8String.encode('evil.example.com') }
    ]
  ],
  [
    [
      {
        type: oid.localityName,
        value: UTF8String.encode('Somewhere\0evil.example.com'),
      }
    ]
  ],
  [
    [
      {
        type: oid.localityName,
        value: UTF8String.encode('Somewhere\nCN=evil.example.com')
      }
    ]
  ],
  [
    [
      {
        type: oid.localityName,
        value: UTF8String.encode('Somewhere, CN = evil.example.com')
      }
    ]
  ],
  [
    [
      {
        type: oid.localityName,
        value: UTF8String.encode('Somewhere/CN=evil.example.com')
      }
    ]
  ],
  [
    [
      {
        type: oid.localityName,
        value: UTF8String.encode('M\u00fcnchen\\\nCN=evil.example.com')
      }
    ]
  ],
  [
    [
      { type: oid.localityName, value: UTF8String.encode('Somewhere') },
      { type: oid.commonName, value: UTF8String.encode('evil.example.com') },
    ]
  ],
  [
    [
      {
        type: oid.localityName,
        value: UTF8String.encode('Somewhere + CN=evil.example.com'),
      }
    ]
  ],
  [
    [
      { type: oid.localityName, value: UTF8String.encode('L1') },
      { type: oid.localityName, value: UTF8String.encode('L2') },
    ],
    [
      { type: oid.localityName, value: UTF8String.encode('L3') },
    ]
  ],
  [
    [
      { type: oid.localityName, value: UTF8String.encode('L1') },
    ],
    [
      { type: oid.localityName, value: UTF8String.encode('L2') },
    ],
    [
      { type: oid.localityName, value: UTF8String.encode('L3') },
    ],
  ],
];

for (let i = 0; i < subjects.length; i++) {
  const tbs = {
    version: 'v3',
    serialNumber: new BN('01', 16),
    signature: {
      algorithm: oid.sha256WithRSAEncryption,
      parameters: null_
    },
    issuer: {
      type: 'rdnSequence',
      value: subjects[i]
    },
    validity: {
      notBefore: { type: 'utcTime', value: now },
      notAfter: { type: 'utcTime', value: now + days * 86400000 }
    },
    subject: {
      type: 'rdnSequence',
      value: subjects[i]
    },
    subjectPublicKeyInfo: {
      algorithm: {
        algorithm: oid.rsaEncryption,
        parameters: null_
      },
      subjectPublicKey: {
        unused: 0,
        data: publicKey
      }
    }
  };

  // Self-sign the certificate.
  const tbsDer = rfc5280.TBSCertificate.encode(tbs, 'der');
  const signature = crypto.createSign(digest).update(tbsDer).sign(privateKey);

  // Construct the signed certificate.
  const cert = {
    tbsCertificate: tbs,
    signatureAlgorithm: {
      algorithm: oid.sha256WithRSAEncryption,
      parameters: null_
    },
    signature: {
      unused: 0,
      data: signature
    }
  };

  // Store the signed certificate.
  const pem = rfc5280.Certificate.encode(cert, 'pem', {
    label: 'CERTIFICATE'
  });
  writeFileSync(`./subj-${i}-cert.pem`, `${pem}\n`);
}
