declare function InternalBinding(binding: 'encoding_methods'): {
  validateAscii(input: Uint8Array): boolean
  validateUtf8(input: Uint8Array): boolean
  countUtf8(input: Uint8Array): boolean
};
