var testCases = {
    "HKDF": [
        {length: 256, expected: algorithms["HKDF"].derivation},
        {length: 0, expected: undefined}, // explicitly disallowed, so should throw
        {length: null, expected: undefined }, // should throw an exception
        {length: undefined, expected: undefined }, // should throw an exception
        {length: "omitted", expected: undefined }, // default value is null, so should throw
    ],
    "PBKDF2": [
        {length: 256, expected: algorithms["PBKDF2"].derivation},
        {length: 0, expected: undefined}, // explicitly disallowed, so should throw
        {length: null, expected: undefined }, // should throw an exception
        {length: undefined, expected: undefined }, // should throw an exception
        {length: "omitted", expected: undefined }, // default value is null, so should throw
    ],
    "ECDH": [
        {length: 256, expected: algorithms["ECDH"].derivation},
        {length: 0, expected: emptyArray},
        {length: null, expected: algorithms["ECDH"].derivation},
        {length: undefined, expected: algorithms["ECDH"].derivation},
        {length: "omitted", expected: algorithms["ECDH"].derivation }, // default value is null
    ],
    "X25519": [
        {length: 256, expected: algorithms["X25519"].derivation},
        {length: 0, expected: emptyArray},
        {length: null, expected: algorithms["X25519"].derivation},
        {length: undefined, expected: algorithms["X25519"].derivation},
        {length: "omitted", expected: algorithms["X25519"].derivation }, // default value is null
    ],
}
