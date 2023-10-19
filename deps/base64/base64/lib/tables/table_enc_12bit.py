#!/usr/bin/python3

def tr(x):
    """Translate a 6-bit value to the Base64 alphabet."""
    s = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' \
      + 'abcdefghijklmnopqrstuvwxyz' \
      + '0123456789' \
      + '+/'
    return ord(s[x])

def table(fn):
    """Generate a 12-bit lookup table."""
    ret = []
    for n in range(0, 2**12):
        pre = "\n\t" if n % 8 == 0 else " "
        pre = "\t" if n == 0 else pre
        ret.append("{}0x{:04X}U,".format(pre, fn(n)))
    return "".join(ret)

def table_be():
    """Generate a 12-bit big-endian lookup table."""
    return table(lambda n: (tr(n & 0x3F) << 0) | (tr(n >> 6) << 8))

def table_le():
    """Generate a 12-bit little-endian lookup table."""
    return table(lambda n: (tr(n >> 6) << 0) | (tr(n & 0x3F) << 8))

def main():
    """Entry point."""
    lines = [
        "#include <stdint.h>",
        "",
        "const uint16_t base64_table_enc_12bit[] = {",
        "#if BASE64_LITTLE_ENDIAN",
        table_le(),
        "#else",
        table_be(),
        "#endif",
        "};"
    ]
    for line in lines:
        print(line)

if __name__ == "__main__":
    main()
