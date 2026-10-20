import { hasOpenSSL } from '../../common/crypto.js';
import { generateNamedKeyPair, X25519 } from './supports-level-2.mjs';

const supportsContext = hasOpenSSL(3, 2);
export const X448 = generateNamedKeyPair('X448');
export const Ed448 = generateNamedKeyPair('Ed448');
const hasX448 = X448 !== undefined;
const hasEd448 = Ed448 !== undefined;

export const vectors = {
  'sign': [
    [hasEd448, 'Ed448'],
    [hasEd448, { name: 'Ed448', context: Buffer.alloc(0) }],
    [hasEd448 && supportsContext, { name: 'Ed448', context: Buffer.alloc(32) }],
    [hasEd448 && supportsContext, { name: 'Ed448', context: Buffer.alloc(255) }],
    [false, { name: 'Ed448', context: Buffer.alloc(256) }],
  ],
  'generateKey': [
    [hasX448, 'X448'],
    [hasEd448, 'Ed448'],
  ],
  'deriveKey': [
    [hasX448,
     { name: 'X448', public: X448?.publicKey },
     { name: 'AES-CBC', length: 128 }],
    [false,
     { name: 'X448', public: X448?.publicKey },
     { name: 'HMAC', hash: 'SHA-256' }],
    [hasX448,
     { name: 'X448', public: X448?.publicKey },
     { name: 'HMAC', hash: 'SHA-256', length: 448 }],
    [false,
     { name: 'X448', public: X448?.publicKey },
     { name: 'HMAC', hash: 'SHA-256', length: 449 }],
    [hasX448,
     { name: 'X448', public: X448?.publicKey },
     'HKDF'],
  ],
  'deriveBits': [
    [hasX448, { name: 'X448', public: X448?.publicKey }],
    [hasX448, { name: 'X448', public: X448?.publicKey }, 448],
    [false, { name: 'X448', public: X448?.publicKey }, 449],
    [false, { name: 'X448', public: X25519?.publicKey }],
    [false, { name: 'X448', public: X448?.privateKey }],
    [false, 'X448'],
  ],
  'importKey': [
    [hasX448, 'X448'],
    [hasEd448, 'Ed448'],
  ],
  'exportKey': [
    [hasEd448, 'Ed448'],
    [hasX448, 'X448'],
  ],
};
