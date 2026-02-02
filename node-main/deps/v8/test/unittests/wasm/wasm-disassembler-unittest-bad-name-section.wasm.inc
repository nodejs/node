// This Wasm module has a name section which is invalid in that it
// contains each sub-section twice.

  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  // The only purpose of this table section is to trigger lazy decoding
  // of the name section.
  0x04,                    // section kind: Table
  0x04,                    // section length 4
  0x01, 0x70, 0x00,        // table count 1:  funcref no maximum
  0x00,                    // initial size 0

  0x00,                    // section kind: Unknown
  0xb3, 0x01,              // section length 179
  0x04,                    // section name length: 4
  0x6e, 0x61, 0x6d, 0x65,  // section name: name

  0x01,              // name type: function
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x02,              // name type: local
  0x0b,              // payload length: 11
  0x02,              // outer count 2
  0x00, 0x01,        // outer index 0 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"
  0x01, 0x01,        // outer index 1 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"

  0x03,              // name type: label
  0x0b,              // payload length: 11
  0x02,              // outer count 2
  0x00, 0x01,        // outer index 0 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"
  0x01, 0x01,        // outer index 1 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"

  0x04,              // name type: type
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x05,              // name type: table
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x06,              // name type: memory
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x07,              // name type: global
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x08,              // name type: element segment
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x09,              // name type: data segment
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x0a,              // name type: field
  0x0b,              // payload length: 11
  0x02,              // outer count 2
  0x00, 0x01,        // outer index 0 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"
  0x01, 0x01,        // outer index 1 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"

  0x0b,              // name type: tag
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x01,              // name type: function
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x02,              // name type: local
  0x0b,              // payload length: 11
  0x02,              // outer count 2
  0x00, 0x01,        // outer index 0 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"
  0x01, 0x01,        // outer index 1 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"

  0x03,              // name type: label
  0x0b,              // payload length: 11
  0x02,              // outer count 2
  0x00, 0x01,        // outer index 0 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"
  0x01, 0x01,        // outer index 1 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"

  0x04,              // name type: type
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x05,              // name type: table
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x06,              // name type: memory
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x07,              // name type: global
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x08,              // name type: element segment
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x09,              // name type: data segment
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"

  0x0a,              // name type: field
  0x0b,              // payload length: 11
  0x02,              // outer count 2
  0x00, 0x01,        // outer index 0 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"
  0x01, 0x01,        // outer index 1 inner count 1
  0x00, 0x01, 0x78,  // inner index 0 name length 1 "x"

  0x0b,              // name type: tag
  0x04,              // payload length: 4
  0x01,              // names count 1
  0x00, 0x01, 0x78,  // index 0 name length 1 "x"
