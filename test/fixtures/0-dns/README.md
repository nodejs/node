# 0-dns

## Purpose
The test cert file for use `test/parallel/test-tls-0-dns-altname.js`
can be created by using `asn1.js` and `asn1.js-rfc5280`,

## How to create a test cert.

```sh
$ openssl genrsa -out 0-dns-key.pem 2048
Generating RSA private key, 2048 bit long modulus
...................+++
..............................................................................................+++
e is 65537 (0x10001)
$ openssl rsa -in 0-dns-key.pem -RSAPublicKey_out -outform der -out 0-dns-rsapub.der
writing RSA key
$ npm install
0-dns@1.0.0 /home/github/node/test/fixtures/0-dns
+-- asn1.js@4.9.1
| +-- bn.js@4.11.6
| +-- inherits@2.0.3
| `-- minimalistic-assert@1.0.0
`-- asn1.js-rfc5280@1.2.2

$ node ./createCert.js
$ openssl x509 -text -in 0-dns-cert.pem
(You can not see evil.example.com in subjectAltName field)
```
