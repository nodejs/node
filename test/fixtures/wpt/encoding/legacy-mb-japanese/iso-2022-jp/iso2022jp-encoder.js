// set up a sparse array of all unicode codepoints listed in the index
// this will be used for lookup in iso2022jpEncoded
var jis0208CPs = []; // index is unicode cp, value is pointer
for (var p = 0; p < jis0208.length; p++) {
    if (jis0208[p] != null && jis0208CPs[jis0208[p]] == null) {
        jis0208CPs[jis0208[p]] = p;
    }
}

// set up mappings for half/full width katakana
// index is a katakana index pointer, value is Unicode codepoint (dec)
// this is copy-pasted from the json version of the index belonging to the Encoding spec
var iso2022jpkatakana = [
    12290,
    12300,
    12301,
    12289,
    12539,
    12530,
    12449,
    12451,
    12453,
    12455,
    12457,
    12515,
    12517,
    12519,
    12483,
    12540,
    12450,
    12452,
    12454,
    12456,
    12458,
    12459,
    12461,
    12463,
    12465,
    12467,
    12469,
    12471,
    12473,
    12475,
    12477,
    12479,
    12481,
    12484,
    12486,
    12488,
    12490,
    12491,
    12492,
    12493,
    12494,
    12495,
    12498,
    12501,
    12504,
    12507,
    12510,
    12511,
    12512,
    12513,
    12514,
    12516,
    12518,
    12520,
    12521,
    12522,
    12523,
    12524,
    12525,
    12527,
    12531,
    12443,
    12444
];

function chars2cps(chars) {
    // this is needed because of javascript's handling of supplementary characters
    // char: a string of unicode characters
    // returns an array of decimal code point values
    var haut = 0;
    var out = [];
    for (var i = 0; i < chars.length; i++) {
        var b = chars.charCodeAt(i);
        if (b < 0 || b > 0xffff) {
            alert(
                "Error in chars2cps: byte out of range " + b.toString(16) + "!"
            );
        }
        if (haut != 0) {
            if (0xdc00 <= b && b <= 0xdfff) {
                out.push(0x10000 + ((haut - 0xd800) << 10) + (b - 0xdc00));
                haut = 0;
                continue;
            } else {
                alert(
                    "Error in chars2cps: surrogate out of range " +
                        haut.toString(16) +
                        "!"
                );
                haut = 0;
            }
        }
        if (0xd800 <= b && b <= 0xdbff) {
            haut = b;
        } else {
            out.push(b);
        }
    }
    return out;
}

function iso2022jpEncoder(stream) {
    var cps = chars2cps(stream);
    var endofstream = 2000000;
    var out = "";
    var encState = "ascii";
    var finished = false;
    var cp, ptr;

    while (!finished) {
        if (cps.length == 0) cp = endofstream;
        else cp = cps.shift();
        if (cp == endofstream && encState != "ascii") {
            cps.unshift(cp);
            encState = "ascii";
            out += " 1B 28 42";
            continue;
        }
        if (cp == endofstream && encState == "ascii") {
            finished = true;
            continue;
        }
        if (
            (encState === "ascii" || encState === "roman") &&
            (cp === 0x0e || cp === 0x0f || cp === 0x1b)
        ) {
            //out += ' &#'+cp+';'
            // continue
            return null;
        }
        if (encState == "ascii" && cp >= 0x00 && cp <= 0x7f) {
            out += " " + cp.toString(16).toUpperCase();
            continue;
        }
        if (
            encState == "roman" &&
            ((cp >= 0x00 && cp <= 0x7f && cp !== 0x5c && cp !== 0x7e) ||
                cp == 0xa5 ||
                cp == 0x203e)
        ) {
            if (cp >= 0x00 && cp <= 0x7f) {
                // ASCII
                out += " " + cp.toString(16).toUpperCase();
                continue;
            }
            if (cp == 0xa5) {
                out += " 5C";
                continue;
            }
            if (cp == 0x203e) {
                out += " 7E";
                continue;
            }
        }
        if (encState != "ascii" && cp >= 0x00 && cp <= 0x7f) {
            cps.unshift(cp);
            encState = "ascii";
            out += " 1B 28 42";
            continue;
        }
        if ((cp == 0xa5 || cp == 0x203e) && encState != "roman") {
            cps.unshift(cp);
            encState = "roman";
            out += " 1B 28 4A";
            continue;
        }
        if (cp == 0x2212) cp = 0xff0d;
        if (cp >= 0xff61 && cp <= 0xff9f) {
            cp = iso2022jpkatakana[cp - 0xff61];
        }
        ptr = jis0208CPs[cp];
        if (ptr == null) {
            //out += ' &#'+cp+';'
            //continue
            return null;
        }
        if (encState != "jis0208") {
            cps.unshift(cp);
            encState = "jis0208";
            out += " 1B 24 42";
            continue;
        }
        var lead = Math.floor(ptr / 94) + 0x21;
        var trail = ptr % 94 + 0x21;
        out +=
            " " +
            lead.toString(16).toUpperCase() +
            " " +
            trail.toString(16).toUpperCase();
    }
    return out.trim();
}

function convertToHex(str) {
    // converts a string of ASCII characters to hex byte codes
    var out = "";
    var result;
    for (var c = 0; c < str.length; c++) {
        result =
            str
                .charCodeAt(c)
                .toString(16)
                .toUpperCase() + " ";
        out += result;
    }
    return out;
}

function normalizeStr(str) {
    var out = "";
    for (var c = 0; c < str.length; c++) {
        if (
            str.charAt(c) == "%" &&
            str.charAt(c + 1) != "%" &&
            str.charAt(c + 2) != "%"
        ) {
            out += String.fromCodePoint(
                parseInt(str.charAt(c + 1) + str.charAt(c + 2), 16)
            );
            c += 2;
        } else out += str.charAt(c);
    }
    var result = "";
    for (var o = 0; o < out.length; o++) {
        result +=
            "%" +
            out
                .charCodeAt(o)
                .toString(16)
                .toUpperCase();
    }
    return result.replace(/%1B%28%42$/, "");
}
