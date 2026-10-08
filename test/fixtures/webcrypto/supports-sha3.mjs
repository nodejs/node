import { getHashes } from 'node:crypto';
import { ECDH, X25519 } from './supports-level-2.mjs';

const hashes = getHashes();
const hasSha3 = hashes.includes('sha3-256');

const RSA_KEY_GEN = {
  modulusLength: 2048,
  publicExponent: new Uint8Array([1, 0, 1])
};

export const vectors = {
  'digest': [
    [hasSha3, 'SHA3-256'],
    [hashes.includes('sha3-384'), 'SHA3-384'],
    [hashes.includes('sha3-512'), 'SHA3-512'],
  ],
  'generateKey': [
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [hasSha3, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [hasSha3, { name: 'RSA-PSS', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [hasSha3, { name: 'RSA-OAEP', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 0 }],

    // This interaction is not defined for now.
    // https://github.com/WICG/webcrypto-modern-algos/issues/23
    [false, { name: 'HMAC', hash: 'SHA3-256' }],
  ],
  'deriveKey': [
    [hasSha3,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     { name: 'AES-CBC', length: 128 }],
    [hasSha3,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [false,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     'HKDF'],
    [hasSha3,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     { name: 'AES-CBC', length: 128 }],
    [hasSha3,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [false,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     'HKDF'],
    [hasSha3 && X25519 !== undefined,
     { name: 'X25519', public: X25519?.publicKey },
     { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [hasSha3,
     { name: 'ECDH', public: ECDH.publicKey },
     { name: 'HMAC', hash: 'SHA3-256', length: 256 }],

    // This interaction is not defined for now.
    // https://github.com/WICG/webcrypto-modern-algos/issues/23
    [false,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     { name: 'HMAC', hash: 'SHA3-256' }],
    [false,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     { name: 'HMAC', hash: 'SHA3-256' }],
    [false,
     { name: 'X25519', public: X25519?.publicKey },
     { name: 'HMAC', hash: 'SHA3-256' }],
    [false,
     { name: 'ECDH', public: ECDH.publicKey },
     { name: 'HMAC', hash: 'SHA3-256' }],
  ],
  'deriveBits': [
    [hasSha3, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 8],
    [hasSha3, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 0],
    [false, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, null],
    [false, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 7],

    [hasSha3, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 8],
    [hasSha3, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 0],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 0 }, 8],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, null],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 7],
  ],
  'importKey': [
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256' }],
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [hasSha3, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [hasSha3, { name: 'RSA-PSS', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [hasSha3, { name: 'RSA-OAEP', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256' }],
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [hasSha3, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 0 }],
  ],
  'get key length': [
    [false, { name: 'HMAC', hash: 'SHA3-256' }],
  ],
};
