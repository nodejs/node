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

assertThrows(() => new Intl.Segmenter(["sr"], { granularity: "line" }), RangeError);

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

assertDoesNotThrow(
    () => new Intl.Segmenter(["sr"], { lineBreakStyle: "giant" })
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

assertThrows(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "line",
            lineBreakStyle: "loose"
        }), RangeError
);

assertThrows(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "line",
            lineBreakStyle: "normal"
        }), RangeError
);

assertThrows(
    () =>
        new Intl.Segmenter(["sr"], {
            granularity: "line",
            lineBreakStyle: "strict"
        }), RangeError
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
assertDoesNotThrow(
    () =>
        new Intl.Segmenter(undefined, {
            get lineBreakStyle() {
                throw new TypeError("");
            }
        })
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
