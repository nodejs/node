// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter

// Segmenter constructor can't be called as function.
assertThrows(() => Intl.Segmenter(["sr"]), TypeError);

// Invalid locale string.
assertThrows(() => new Intl.Segmenter(["abcdefghi"]), RangeError);

assertDoesNotThrow(() => new Intl.Segmenter(["sr"], {}), TypeError);

assertDoesNotThrow(() => new Intl.Segmenter([], {}));

assertDoesNotThrow(() => new Intl.Segmenter(["fr", "ar"], {}));

assertDoesNotThrow(() => new Intl.Segmenter({ 0: "ja", 1: "fr" }, {}));

assertDoesNotThrow(() => new Intl.Segmenter({ 1: "ja", 2: "fr" }, {}));

assertDoesNotThrow(() => new Intl.Segmenter(["sr"]));

assertDoesNotThrow(() => new Intl.Segmenter());

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            lineBreakStyle: "strict",
            granularity: "grapheme"
        })
);

assertDoesNotThrow(
    () => new Intl.Segmenter(["sr"], { granularity: "sentence" })
);

assertDoesNotThrow(() => new Intl.Segmenter(["sr"], { granularity: "word" }));

assertDoesNotThrow(
    () => new Intl.Segmenter(["sr"], { granularity: "grapheme" })
);

assertDoesNotThrow(() => new Intl.Segmenter(["sr"], { granularity: "line" }));

assertThrows(
    () => new Intl.Segmenter(["sr"], { granularity: "standard" }),
    RangeError
);

assertDoesNotThrow(
    () => new Intl.Segmenter(["sr"], { lineBreakStyle: "normal" })
);

assertDoesNotThrow(
    () => new Intl.Segmenter(["sr"], { lineBreakStyle: "strict" })
);

assertDoesNotThrow(
    () => new Intl.Segmenter(["sr"], { lineBreakStyle: "loose" })
);

assertThrows(
    () => new Intl.Segmenter(["sr"], { lineBreakStyle: "giant" }),
    RangeError
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "sentence",
            lineBreakStyle: "normal"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "sentence",
            lineBreakStyle: "strict"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "sentence",
            lineBreakStyle: "loose"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "word",
            lineBreakStyle: "normal"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "word",
            lineBreakStyle: "strict"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "word",
            lineBreakStyle: "loose"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "grapheme",
            lineBreakStyle: "normal"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "grapheme",
            lineBreakStyle: "strict"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "grapheme",
            lineBreakStyle: "loose"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "line",
            lineBreakStyle: "loose"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "line",
            lineBreakStyle: "normal"
        })
);

assertDoesNotThrow(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "line",
            lineBreakStyle: "strict"
        })
);

// propagate exception from getter
assertThrows(
    () =>
        new Intl.Segmenter(undefined, {
            get localeMatcher() {
                throw new TypeError("");
            }
        }),
    TypeError
);
assertThrows(
    () =>
        new Intl.Segmenter(undefined, {
            get lineBreakStyle() {
                throw new TypeError("");
            }
        }),
    TypeError
);
assertThrows(
    () =>
        new Intl.Segmenter(undefined, {
            get granularity() {
                throw new TypeError("");
            }
        }),
    TypeError
);

// Throws only once during construction.
// Check for all getters to prevent regression.
// Preserve the order of getter initialization.
let getCount = 0;
let localeMatcher = -1;
let lineBreakStyle = -1;
let granularity = -1;

new Intl.Segmenter(["en-US"], {
    get localeMatcher() {
        localeMatcher = ++getCount;
    },
    get lineBreakStyle() {
        lineBreakStyle = ++getCount;
    },
    get granularity() {
        granularity = ++getCount;
    }
});

assertEquals(1, localeMatcher);
assertEquals(2, lineBreakStyle);
assertEquals(3, granularity);
