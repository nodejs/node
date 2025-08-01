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
  ],
  'importKey': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
  ],
  'exportKey': [
    [pqc, 'ML-DSA-44'],
    [pqc, 'ML-DSA-65'],
    [pqc, 'ML-DSA-87'],
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
  ],
};
