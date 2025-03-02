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
};
