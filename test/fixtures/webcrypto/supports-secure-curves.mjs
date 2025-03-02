const { subtle } = globalThis.crypto;

const [X448, X25519] = await Promise.all([
  subtle.generateKey('X448', false, ['deriveBits', 'deriveKey']),
  subtle.generateKey('X25519', false, ['deriveBits', 'deriveKey']),
]);

export const vectors = {
  'generateKey': [
    [true, 'X448'],
    [true, 'Ed448'],
  ],
  'deriveKey': [
    [true,
     { name: 'X448', public: X448.publicKey },
     { name: 'AES-CBC', length: 128 }],
    [true,
     { name: 'X448', public: X448.publicKey },
     { name: 'HMAC', hash: 'SHA-256' }],
    [true,
     { name: 'X448', public: X448.publicKey },
     'HKDF'],
  ],
  'deriveBits': [
    [true, { name: 'X448', public: X448.publicKey }],
    [false, { name: 'X448', public: X25519.publicKey }],
    [false, { name: 'X448', public: X448.privateKey }],
    [false, 'X448'],
  ],
  'importKey': [
    [true, 'X448'],
    [true, 'Ed448'],
  ],
  'exportKey': [
    [true, 'Ed448'],
    [true, 'X448'],
  ],
};
