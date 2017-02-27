'use strict';
const asn1 = require('asn1.js');
const crypto = require('crypto');
const fs = require('fs');
const rfc5280 = require('asn1.js-rfc5280');
const BN = asn1.bignum;

const id_at_commonName = [ 2, 5, 4, 3 ];
const rsaEncryption = [1, 2, 840, 113549, 1, 1, 1];
const sha256WithRSAEncryption = [1, 2, 840, 113549, 1, 1, 11];
const sigalg = 'RSA-SHA256';

const private_key = fs.readFileSync('./0-dns-key.pem');
// public key file can be generated from the private key with
// openssl rsa -in 0-dns-key.pem -RSAPublicKey_out -outform der
// -out 0-dns-rsapub.der
const public_key = fs.readFileSync('./0-dns-rsapub.der');

const now = Date.now();
const days = 3650;

const Null_ = asn1.define('Null_', function() {
  this.null_();
});
const null_ = Null_.encode('der');

const PrintStr = asn1.define('PrintStr', function() {
  this.printstr();
});
const issuer = PrintStr.encode('ca.example.com', 'der');
const subject = PrintStr.encode('evil.example.com', 'der');

const tbs = {
  version: 'v3',
  serialNumber: new BN('01', 16),
  signature: { algorithm: sha256WithRSAEncryption, parameters: null_},
  issuer: { type: 'rdnSequence',
            value: [ [{type: id_at_commonName, value: issuer}] ] },
  validity:
  { notBefore: { type: 'utcTime', value: now },
    notAfter: { type: 'utcTime', value: now + days * 86400000} },
  subject: { type: 'rdnSequence',
             value: [ [{type: id_at_commonName, value: subject}] ] },
  subjectPublicKeyInfo:
  { algorithm: { algorithm: rsaEncryption, parameters: null_},
    subjectPublicKey: { unused: 0, data: public_key} },
  extensions:
  [ { extnID: 'subjectAlternativeName',
      critical: false,
      // subjectAltName which contains '\0' character to check CVE-2009-2408
      extnValue: [
        { type: 'dNSName', value: 'good.example.org\u0000.evil.example.com' },
        { type: 'dNSName', value: 'just-another.example.com' },
        { type: 'iPAddress', value: Buffer.from('08080808', 'hex') },
        { type: 'iPAddress', value: Buffer.from('08080404', 'hex') },
        { type: 'dNSName', value: 'last.example.com' } ] }
  ]
};

const tbs_der = rfc5280.TBSCertificate.encode(tbs, 'der');

const sign = crypto.createSign(sigalg);
sign.update(tbs_der);
const signature = sign.sign(private_key);

const cert = {
  tbsCertificate: tbs,
  signatureAlgorithm: { algorithm: sha256WithRSAEncryption, parameters: null_ },
  signature:
  { unused: 0,
    data: signature }
};
const pem = rfc5280.Certificate.encode(cert, 'pem', {label: 'CERTIFICATE'});

fs.writeFileSync('./0-dns-cert.pem', pem + '\n');
