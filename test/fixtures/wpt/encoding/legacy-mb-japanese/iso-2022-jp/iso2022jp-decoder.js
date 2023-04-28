function dec2char(n) {
    // converts a decimal number to a Unicode character
    // n: the dec codepoint value to be converted
    if (n <= 0xffff) {
        out = String.fromCharCode(n);
    } else if (n <= 0x10ffff) {
        n -= 0x10000;
        out =
            String.fromCharCode(0xd800 | (n >> 10)) +
            String.fromCharCode(0xdc00 | (n & 0x3ff));
    } else out = "dec2char error: Code point out of range: " + n;
    return out;
}

function getIndexPtr(cp, index) {
    for (p = 0; p < index.length; p++) {
        if (index[p] == cp) {
            return p;
        }
    }
    return null;
}

function iso2022jpDecoder(stream) {
    stream = stream.replace(/%/g, " ");
    stream = stream.replace(/[\s]+/g, " ").trim();
    var bytes = stream.split(" ");
    for (var i = 0; i < bytes.length; i++) bytes[i] = parseInt(bytes[i], 16);
    var endofstream = 2000000;
    //bytes.push(endofstream)
    var out = "";
    var decState = "ascii";
    var outState = "ascii";
    var isoLead = 0x00;
    var outFlag = false;
    var cp, ptr, lead;

    var finished = false;
    while (!finished) {
        if (bytes.length == 0) byte = endofstream;
        else var byte = bytes.shift();
        //byte = bytes.shift()

        switch (decState) {
            case "ascii":
                if (byte == 0x1b) {
                    decState = "escStart";
                    continue;
                } else if (
                    byte >= 0x00 &&
                    byte <= 0x7f &&
                    byte !== 0x0e &&
                    byte !== 0x0f &&
                    byte !== 0x1b
                ) {
                    outFlag = false;
                    out += dec2char(byte);
                    continue;
                } else if (byte == endofstream) {
                    finished = true;
                    continue;
                } else {
                    outFlag = false;
                    out += "�";
                    continue;
                }
                break;
            case "roman":
                if (byte == 0x1b) {
                    decState = "escStart";
                    continue;
                } else if (byte == 0x5c) {
                    outFlag = false;
                    out += dec2char(0xa5);
                    continue;
                } else if (byte == 0x7e) {
                    outFlag = false;
                    out += dec2char(0x203e);
                    continue;
                } else if (
                    byte >= 0x00 &&
                    byte <= 0x7f &&
                    byte !== 0x0e &&
                    byte !== 0x0f &&
                    byte !== 0x1b &&
                    byte !== 0x5c &&
                    byte !== 0x7e
                ) {
                    outFlag = false;
                    out += dec2char(byte);
                    continue;
                } else if (byte == endofstream) {
                    finished = true;
                    continue;
                } else {
                    outFlag = false;
                    out += "�";
                    continue;
                }
                break;
            case "katakana":
                if (byte == 0x1b) {
                    decState = "escStart";
                    continue;
                } else if (byte >= 0x21 && byte <= 0x5f) {
                    outFlag = false;
                    out += dec2char(0xff61 + byte - 0x21);
                    continue;
                } else if (byte == endofstream) {
                    finished = true;
                    continue;
                } else {
                    outFlag = false;
                    out += "�";
                    continue;
                }
                break;
            case "leadbyte":
                if (byte == 0x1b) {
                    decState = "escStart";
                    continue;
                } else if (byte >= 0x21 && byte <= 0x7e) {
                    outFlag = false;
                    isoLead = byte;
                    decState = "trailbyte";
                    continue;
                } else if (byte == endofstream) {
                    finished = true;
                    continue;
                } else {
                    outFlag = false;
                    out += "�";
                    continue;
                }
                break;
            case "trailbyte":
                if (byte == 0x1b) {
                    decState = "escStart";
                    out += "�";
                    continue;
                } else if (byte >= 0x21 && byte <= 0x7e) {
                    decState = "leadbyte";
                    ptr = (isoLead - 0x21) * 94 + byte - 0x21;
                    cp = jis0208[ptr];
                    if (cp == null) {
                        out += "�";
                        continue;
                    }
                    out += dec2char(cp);
                    continue;
                } else if (byte == endofstream) {
                    decState = "leadbyte";
                    bytes.unshift(byte);
                    out += "�";
                    continue;
                } else {
                    decState = "leadbyte";
                    out += "�";
                    continue;
                }
                break;
            case "escStart":
                if (byte == 0x24 || byte == 0x28) {
                    isoLead = byte;
                    decState = "escape";
                    continue;
                } else {
                    bytes.unshift(byte);
                    outFlag = false;
                    decState = outState;
                    out += "�";
                    continue;
                }
                break;
            case "escape":
                lead = isoLead;
                isoLead = 0x00;
                var state = null;
                if (lead == 0x28 && byte == 0x42) state = "ascii";
                if (lead == 0x28 && byte == 0x4a) state = "roman";
                if (lead == 0x28 && byte == 0x49) state = "katakana";
                if (lead == 0x24 && (byte == 0x40 || byte == 0x42))
                    state = "leadbyte";
                if (state != null) {
                    decState = state;
                    outState = state;
                    var outputflag = false;
                    outputflag = outFlag;
                    outFlag = true;
                    if (outputflag == false) continue;
                    else {
                        out += "�";
                        continue;
                    }
                }
                // Prepend the sequence (lead, byte) to the stream
                bytes.unshift(lead, byte);
                outFlag = false;
                decState = outState;
                out += "�";
                continue;
                break;
        }
    }
    return out;
}
