import * as crypto from 'node:crypto'

import { hasOpenSSL } from '../../common/crypto.js'

const pqc = hasOpenSSL(3, 5);
const shake128 = crypto.getHashes().includes('shake128');
const shake256 = crypto.getHashes().includes('shake256');
const chacha = crypto.getCiphers().includes('chacha20-poly1305');

export const vectors = {
  'digest': [
    [false, 'cSHAKE128'],
    [shake128, { name: 'cSHAKE128', length: 128 }],
    [shake128, { name: 'cSHAKE128', length: 128, functionName: Buffer.alloc(0), customization: Buffer.alloc(0) }],
    [false, { name: 'cSHAKE128', length: 128, functionName: Buffer.alloc(1) }],
    [false, { name: 'cSHAKE128', length: 128, customization: Buffer.alloc(1) }],
    [false, { name: 'cSHAKE128', length: 127 }],
    [false, 'cSHAKE256'],
    [shake256, { name: 'cSHAKE256', length: 256 }],
    [shake256, { name: 'cSHAKE256', length: 256, functionName: Buffer.alloc(0), customization: Buffer.alloc(0) }],
    [false, { name: 'cSHAKE256', length: 256, functionName: Buffer.alloc(1) }],
    [false, { name: 'cSHAKE256', length: 256, customization: Buffer.alloc(1) }],
    [false, { name: 'cSHAKE256', length: 255 }],
  ],
  'sign': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
  ],
  'generateKey': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
    [chacha, 'ChaCha20-Poly1305'],
  ],
  'importKey': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
    [chacha, 'ChaCha20-Poly1305'],
  ],
  'exportKey': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
    [chacha, 'ChaCha20-Poly1305'],
  ],
  'getPublicKey': [
    [true, 'RSA-OAEP'],
    [true, 'RSA-PSS'],
    [true, 'RSASSA-PKCS1-v1_5'],
    [true, 'X25519'],
    [true, 'X448'],
    [true, 'Ed25519'],
    [true, 'Ed448'],
    [true, 'ECDH'],
    [true, 'ECDSA'],
    [pqc, 'ML-DSA-44'],
    [false, 'AES-CTR'],
    [false, 'AES-CBC'],
    [false, 'AES-GCM'],
    [false, 'AES-KW'],
    [false, 'ChaCha20-Poly1305'],
  ],
  'encrypt': [
    [chacha, { name: 'ChaCha20-Poly1305', iv: Buffer.alloc(12) }],
    [false, { name: 'ChaCha20-Poly1305', iv: Buffer.alloc(16) }],
    [chacha, { name: 'ChaCha20-Poly1305', iv: Buffer.alloc(12), tagLength: 128 }],
    [false, { name: 'ChaCha20-Poly1305', iv: Buffer.alloc(12), tagLength: 64 }],
    [false, 'ChaCha20-Poly1305'],
  ]
};
