const { subtle } = globalThis.crypto;

const RSA_KEY_GEN = {
  modulusLength: 2048,
  publicExponent: new Uint8Array([1, 0, 1])
};

const [ECDH, X448, X25519] = await Promise.all([
  subtle.generateKey({ name: 'ECDH', namedCurve: 'P-256' }, false, ['deriveBits', 'deriveKey']),
  subtle.generateKey('X448', false, ['deriveBits', 'deriveKey']),
  subtle.generateKey('X25519', false, ['deriveBits', 'deriveKey']),
]);

export const vectors = {
  'digest': [
    [true, 'SHA3-256'],
    [true, 'SHA3-384'],
    [true, 'SHA3-512'],
  ],
  'generateKey': [
    [true, { name: 'HMAC', hash: 'SHA3-256' }],
    [true, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [true, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [true, { name: 'RSA-PSS', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [true, { name: 'RSA-OAEP', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [true, { name: 'HMAC', hash: 'SHA3-256' }],
    [true, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 0 }],
  ],
  'deriveKey': [
    [true,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     { name: 'AES-CBC', length: 128 }],
    [true,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     { name: 'HMAC', hash: 'SHA3-256' }],
    [false,
     { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) },
     'HKDF'],
    [true,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     { name: 'AES-CBC', length: 128 }],
    [true,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     { name: 'HMAC', hash: 'SHA3-256' }],
    [false,
     { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 },
     'HKDF'],
    [true,
     { name: 'X25519', public: X25519.publicKey },
     { name: 'HMAC', hash: 'SHA3-256' }],
    [true,
     { name: 'X448', public: X448.publicKey },
     { name: 'HMAC', hash: 'SHA3-256' }],
    [true,
     { name: 'ECDH', public: ECDH.publicKey },
     { name: 'HMAC', hash: 'SHA3-256' }],
  ],
  'deriveBits': [
    [true, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 8],
    [true, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 0],
    [false, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, null],
    [false, { name: 'HKDF', hash: 'SHA3-256', salt: Buffer.alloc(0), info: Buffer.alloc(0) }, 7],

    [true, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 8],
    [true, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 0],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 0 }, 8],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, null],
    [false, { name: 'PBKDF2', hash: 'SHA3-256', salt: Buffer.alloc(0), iterations: 1 }, 7],
  ],
  'importKey': [
    [true, { name: 'HMAC', hash: 'SHA3-256' }],
    [true, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [true, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [true, { name: 'RSA-PSS', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [true, { name: 'RSA-OAEP', hash: 'SHA3-256', ...RSA_KEY_GEN }],
    [true, { name: 'HMAC', hash: 'SHA3-256' }],
    [true, { name: 'HMAC', hash: 'SHA3-256', length: 256 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 25 }],
    [false, { name: 'HMAC', hash: 'SHA3-256', length: 0 }],
  ],
  'get key length': [
    [false, { name: 'HMAC', hash: 'SHA3-256' }],
  ],
};
