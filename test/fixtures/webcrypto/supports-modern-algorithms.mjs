import { hasOpenSSL } from '../../common/crypto.js'

const pqc = hasOpenSSL(3, 5);

export const vectors = {
  'sign': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
  ],
  'generateKey': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
    [true, 'ChaCha20-Poly1305'],
  ],
  'importKey': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
    [true, 'ChaCha20-Poly1305'],
  ],
  'exportKey': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
    [true, 'ChaCha20-Poly1305'],
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
    [true, { name: 'ChaCha20-Poly1305', iv: Buffer.alloc(12) }],
    [false, { name: 'ChaCha20-Poly1305', iv: Buffer.alloc(16) }],
    [true, { name: 'ChaCha20-Poly1305', iv: Buffer.alloc(12), tagLength: 128 }],
    [false, { name: 'ChaCha20-Poly1305', iv: Buffer.alloc(12), tagLength: 64 }],
    [false, 'ChaCha20-Poly1305'],
  ]
};
