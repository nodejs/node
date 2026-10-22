import { isBoringSSL } from '../../common/crypto.js';
import { ECDH, X25519 } from './supports-level-2.mjs';

const RSA_KEY_GEN = {
  modulusLength: 2048,
  publicExponent: new Uint8Array([1, 0, 1])
};

export const vectors = {
  'sign': [
    [!isBoringSSL, { name: 'ECDSA', hash: 'SHA3-256' }],
    [!isBoringSSL, { name: 'ECDSA', hash: 'SHA3-384' }],
    [!isBoringSSL, { name: 'ECDSA', hash: 'SHA3-512' }],
  ],
  'digest': [
    [true, 'SHA3-256'],
    [true, 'SHA3-384'],
    [true, 'SHA3-512'],
  ],
  'generateKey': [
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [!isBoringSSL, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [!isBoringSSL, { name: 'RSA-PSS', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [!isBoringSSL, { name: 'RSA-OAEP', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 0 }],

    // This interaction is not defined for now.
    // https://github.com/WICG/webcrypto-modern-algos/issues/23
    [false, { name: 'HMAC', hash: 'SHA3-256' }],
  ],
  'deriveKey': [
    [!isBoringSSL,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     { name: 'AES-CBC', length: 128 }],
    [!isBoringSSL,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [false,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     'HKDF'],
    [!isBoringSSL,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     { name: 'AES-CBC', length: 128 }],
    [!isBoringSSL,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [false,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     'HKDF'],
    [!isBoringSSL && X25519 !== undefined,
     { name: 'X25519', public: X25519?.publicKey },
     { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [!isBoringSSL,
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
    [!isBoringSSL, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 8],
    [!isBoringSSL, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 0],
    [false, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, null],
    [false, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 7],

    [!isBoringSSL, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 8],
    [!isBoringSSL, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 0],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 0 }, 8],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, null],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 7],
  ],
  'importKey': [
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256' }],
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [!isBoringSSL, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [!isBoringSSL, { name: 'RSA-PSS', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [!isBoringSSL, { name: 'RSA-OAEP', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256' }],
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [!isBoringSSL, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 0 }],
  ],
  'get key length': [
    [false, { name: 'HMAC', hash: 'SHA3-256' }],
  ],
};
