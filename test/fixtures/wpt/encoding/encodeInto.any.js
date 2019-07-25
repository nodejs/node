[
  {
    "input": "Hi",
    "read": 0,
    "destinationLength": 0,
    "written": []
  },
  {
    "input": "A",
    "read": 1,
    "destinationLength": 10,
    "written": [0x41]
  },
  {
    "input": "\u{1D306}", // "\uD834\uDF06"
    "read": 2,
    "destinationLength": 4,
    "written": [0xF0, 0x9D, 0x8C, 0x86]
  },
  {
    "input": "\u{1D306}A",
    "read": 0,
    "destinationLength": 3,
    "written": []
  },
  {
    "input": "\uD834A\uDF06A¥Hi",
    "read": 5,
    "destinationLength": 10,
    "written": [0xEF, 0xBF, 0xBD, 0x41, 0xEF, 0xBF, 0xBD, 0x41, 0xC2, 0xA5]
  },
  {
    "input": "A\uDF06",
    "read": 2,
    "destinationLength": 4,
    "written": [0x41, 0xEF, 0xBF, 0xBD]
  },
  {
    "input": "¥¥",
    "read": 2,
    "destinationLength": 4,
    "written": [0xC2, 0xA5, 0xC2, 0xA5]
  }
].forEach(testData => {
  [
    {
      "bufferIncrease": 0,
      "destinationOffset": 0,
      "filler": 0
    },
    {
      "bufferIncrease": 10,
      "destinationOffset": 4,
      "filler": 0
    },
    {
      "bufferIncrease": 0,
      "destinationOffset": 0,
      "filler": 0x80
    },
    {
      "bufferIncrease": 10,
      "destinationOffset": 4,
      "filler": 0x80
    },
    {
      "bufferIncrease": 0,
      "destinationOffset": 0,
      "filler": "random"
    },
    {
      "bufferIncrease": 10,
      "destinationOffset": 4,
      "filler": "random"
    }
  ].forEach(destinationData => {
    test(() => {
      // Setup
      const bufferLength = testData.destinationLength + destinationData.bufferIncrease,
            destinationOffset = destinationData.destinationOffset,
            destinationLength = testData.destinationLength,
            destinationFiller = destinationData.filler,
            encoder = new TextEncoder(),
            buffer = new ArrayBuffer(bufferLength),
            view = new Uint8Array(buffer, destinationOffset, destinationLength),
            fullView = new Uint8Array(buffer),
            control = new Array(bufferLength);
      let byte = destinationFiller;
      for (let i = 0; i < bufferLength; i++) {
        if (destinationFiller === "random") {
          byte = Math.floor(Math.random() * 256);
        }
        control[i] = byte;
        fullView[i] = byte;
      }

      // It's happening
      const result = encoder.encodeInto(testData.input, view);

      // Basics
      assert_equals(view.byteLength, destinationLength);
      assert_equals(view.length, destinationLength);

      // Remainder
      assert_equals(result.read, testData.read);
      assert_equals(result.written, testData.written.length);
      for (let i = 0; i < bufferLength; i++) {
        if (i < destinationOffset || i >= (destinationOffset + testData.written.length)) {
          assert_equals(fullView[i], control[i]);
        } else {
          assert_equals(fullView[i], testData.written[i - destinationOffset]);
        }
      }
    }, "encodeInto() with " + testData.input + " and destination length " + testData.destinationLength + ", offset " + destinationData.destinationOffset + ", filler " + destinationData.filler);
  });
});

[DataView,
 Int8Array,
 Int16Array,
 Int32Array,
 Uint16Array,
 Uint32Array,
 Uint8ClampedArray,
 Float32Array,
 Float64Array].forEach(view => {
  test(() => {
    assert_throws(new TypeError(), () => new TextEncoder().encodeInto("", new view(new ArrayBuffer(0))));
  }, "Invalid encodeInto() destination: " + view.name);
 });

test(() => {
  assert_throws(new TypeError(), () => new TextEncoder().encodeInto("", new ArrayBuffer(10)));
}, "Invalid encodeInto() destination: ArrayBuffer");

test(() => {
  const buffer = new ArrayBuffer(10),
        view = new Uint8Array(buffer);
  let { read, written } = new TextEncoder().encodeInto("", view);
  assert_equals(read, 0);
  assert_equals(written, 0);
  new MessageChannel().port1.postMessage(buffer, [buffer]);
  ({ read, written } = new TextEncoder().encodeInto("", view));
  assert_equals(read, 0);
  assert_equals(written, 0);
  ({ read, written } = new TextEncoder().encodeInto("test", view));
  assert_equals(read, 0);
  assert_equals(written, 0);
}, "encodeInto() and a detached output buffer");
