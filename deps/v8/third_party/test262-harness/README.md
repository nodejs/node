# Test262 Python Harness

### Usage

Loaded as a module, this project defines a single function, `parseTestRecord`.
This function creates an object representation of the metadata encoded in the
"frontmatter" of the provided Test262 test source code.

`test262.py` is an executable designed to execute Test262 tests. It is exposed
for public use. For usage instructions, invoke this executable with the
`--help` flag, as in:

    $ test262.py --help

### Tests

Run the following command from the root of this projcet:

    $ python -m unittest discover test
