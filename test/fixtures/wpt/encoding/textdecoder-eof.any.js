test(() => {
  // Truncated sequences
  assert_equals(new TextDecoder().decode(new Uint8Array([0xF0])), "\uFFFD");
  assert_equals(new TextDecoder().decode(new Uint8Array([0xF0, 0x9F])), "\uFFFD");
  assert_equals(new TextDecoder().decode(new Uint8Array([0xF0, 0x9F, 0x92])), "\uFFFD");

  // Errors near end-of-queue
  assert_equals(new TextDecoder().decode(new Uint8Array([0xF0, 0x9F, 0x41])), "\uFFFDA");
  assert_equals(new TextDecoder().decode(new Uint8Array([0xF0, 0x41, 0x42])), "\uFFFDAB");
  assert_equals(new TextDecoder().decode(new Uint8Array([0xF0, 0x41, 0xF0])), "\uFFFDA\uFFFD");
  assert_equals(new TextDecoder().decode(new Uint8Array([0xF0, 0x8F, 0x92])), "\uFFFD\uFFFD\uFFFD");
}, "TextDecoder end-of-queue handling");

test(() => {
  const decoder = new TextDecoder();
  decoder.decode(new Uint8Array([0xF0]), { stream: true });
  assert_equals(decoder.decode(), "\uFFFD");

  decoder.decode(new Uint8Array([0xF0]), { stream: true });
  decoder.decode(new Uint8Array([0x9F]), { stream: true });
  assert_equals(decoder.decode(), "\uFFFD");

  decoder.decode(new Uint8Array([0xF0, 0x9F]), { stream: true });
  assert_equals(decoder.decode(new Uint8Array([0x92])), "\uFFFD");

  assert_equals(decoder.decode(new Uint8Array([0xF0, 0x9F]), { stream: true }), "");
  assert_equals(decoder.decode(new Uint8Array([0x41]), { stream: true }), "\uFFFDA");
  assert_equals(decoder.decode(), "");

  assert_equals(decoder.decode(new Uint8Array([0xF0, 0x41, 0x42]), { stream: true }), "\uFFFDAB");
  assert_equals(decoder.decode(), "");

  assert_equals(decoder.decode(new Uint8Array([0xF0, 0x41, 0xF0]), { stream: true }), "\uFFFDA");
  assert_equals(decoder.decode(), "\uFFFD");

  assert_equals(decoder.decode(new Uint8Array([0xF0]), { stream: true }), "");
  assert_equals(decoder.decode(new Uint8Array([0x8F]), { stream: true }), "\uFFFD\uFFFD");
  assert_equals(decoder.decode(new Uint8Array([0x92]), { stream: true }), "\uFFFD");
  assert_equals(decoder.decode(), "");
}, "TextDecoder end-of-queue handling using stream: true");
