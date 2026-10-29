var enforceRangeTestCases = [
    // These cases should throw an error in all algorithms due to [EnforceRange]
    {length: NaN, expected: "TypeError"},
    {length: Infinity, expected: "TypeError"},
    {length: -8, expected: "TypeError"},
    {length: 2**32 + 8, expected: "TypeError"},
];
var testCases = {
    "HKDF": [
        {length: 256, expected: algorithms["HKDF"].derivation},
        {length: 384, expected: algorithms["HKDF"].derivation384},
        {length: 230, expected: "OperationError"}, // should throw an exception, not multiple of 8
        {length: 0, expected: emptyArray},
        {length: null, expected: "OperationError"}, // should throw an exception
        {length: undefined, expected: "OperationError"}, // should throw an exception
        {length: "omitted", expected: "OperationError"}, // default value is null, so should throw
        ...enforceRangeTestCases,
    ],
    "PBKDF2": [
        {length: 256, expected: algorithms["PBKDF2"].derivation},
        {length: 384, expected: algorithms["PBKDF2"].derivation384},
        {length: 230, expected: "OperationError"}, // should throw an exception, not multiple of 8
        {length: 0, expected: emptyArray},
        {length: null, expected: "OperationError"}, // should throw an exception
        {length: undefined, expected: "OperationError"}, // should throw an exception
        {length: "omitted", expected: "OperationError"}, // default value is null, so should throw
        ...enforceRangeTestCases,
    ],
    "ECDH": [
        {length: 256, expected: algorithms["ECDH"].derivation},
        {length: 384, expected: "OperationError"}, // should throw an exception, bigger than the output size
        {length: 230, expected: algorithms["ECDH"].derivation230},
        {length: 0, expected: emptyArray},
        {length: null, expected: algorithms["ECDH"].derivation},
        {length: undefined, expected: algorithms["ECDH"].derivation},
        {length: "omitted", expected: algorithms["ECDH"].derivation}, // default value is null
        ...enforceRangeTestCases,
    ],
    "X25519": [
        {length: 256, expected: algorithms["X25519"].derivation},
        {length: 384, expected: "OperationError"}, // should throw an exception, bigger than the output size
        {length: 230, expected: algorithms["X25519"].derivation230},
        {length: 0, expected: emptyArray},
        {length: null, expected: algorithms["X25519"].derivation},
        {length: undefined, expected: algorithms["X25519"].derivation},
        {length: "omitted", expected: algorithms["X25519"].derivation}, // default value is null
        ...enforceRangeTestCases,
    ],
}
