import { hasOpenSSL } from '../../common/crypto.js'

const supportsContext = hasOpenSSL(3, 2);

const { subtle } = globalThis.crypto;

const boringSSL = process.features.openssl_is_boringssl;

const X25519 = await subtle.generateKey('X25519', false, ['deriveBits', 'deriveKey']);
let X448;
let Ed448;
if (!boringSSL) {
  X448 = await subtle.generateKey('X448', false, ['deriveBits', 'deriveKey'])
  Ed448 = await subtle.generateKey('Ed448', false, ['sign', 'verify'])
}

export const vectors = {
  'sign': [
    [!boringSSL, 'Ed448'],
    [!boringSSL, { name: 'Ed448', context: Buffer.alloc(0) }],
    [!boringSSL && supportsContext, { name: 'Ed448', context: Buffer.alloc(32) }],
  ],
  'generateKey': [
    [!boringSSL, 'X448'],
    [!boringSSL, 'Ed448'],
  ],
  'deriveKey': [
    [!boringSSL,
     { name: 'X448', public: X448?.publicKey },
     { name: 'AES-CBC', length: 128 }],
    [!boringSSL,
     { name: 'X448', public: X448?.publicKey },
     { name: 'HMAC', hash: 'SHA-256' }],
    [!boringSSL,
     { name: 'X448', public: X448?.publicKey },
     'HKDF'],
  ],
  'deriveBits': [
    [!boringSSL, { name: 'X448', public: X448?.publicKey }],
    [false, { name: 'X448', public: X25519.publicKey }],
    [false, { name: 'X448', public: X448?.privateKey }],
    [false, 'X448'],
  ],
  'importKey': [
    [!boringSSL, 'X448'],
    [!boringSSL, 'Ed448'],
  ],
  'exportKey': [
    [!boringSSL, 'Ed448'],
    [!boringSSL, 'X448'],
  ],
};
