import crypto from 'crypto';
const rnds8 = new Uint8Array(16);
export default function rng() {
  return crypto.randomFillSync(rnds8);
}