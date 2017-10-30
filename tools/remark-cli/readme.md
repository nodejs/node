# remark-cli [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

Command-line interface for [**remark**][remark].

*   Loads [`remark-` plugins][plugins]
*   Searches for [markdown extensions][markdown-extensions]
*   Ignores paths found in [`.remarkignore` files][ignore-file]
*   Loads configuration from [`.remarkrc`, `.remarkrc.js` files][config-file]
*   Uses configuration from [`remarkConfig` fields in `package.json`
    files][config-file]

## Installation

[npm][]:

```sh
npm install remark-cli
```

## Usage

```sh
# Add a table of contents to `readme.md`
$ remark readme.md --use toc --output

# Lint markdown files in the current directory
# according to the markdown style guide.
$ remark . --use preset-lint-markdown-style-guide
```

## CLI

See [**unified-args**][unified-args], which provides the interface,
for more information on all available options.

```txt
Usage: remark [options] [path | glob ...]

  CLI to process markdown with remark using plugins

Options:

  -h  --help                output usage information
  -v  --version             output version number
  -o  --output [path]       specify output location
  -r  --rc-path <path>      specify configuration file
  -i  --ignore-path <path>  specify ignore file
  -s  --setting <settings>  specify settings
  -e  --ext <extensions>    specify extensions
  -u  --use <plugins>       use plugins
  -p  --preset <presets>    use presets
  -w  --watch               watch for changes and reprocess
  -q  --quiet               output only warnings and errors
  -S  --silent              output only errors
  -f  --frail               exit with 1 on warnings
  -t  --tree                specify input and output as syntax tree
      --file-path <path>    specify path to process as
      --tree-in             specify input as syntax tree
      --tree-out            output syntax tree
      --[no-]stdout         specify writing to stdout (on by default)
      --[no-]color          specify color in report (on by default)
      --[no-]config         search for configuration files (on by default)
      --[no-]ignore         search for ignore files (on by default)

Examples:

  # Process `input.md`
  $ remark input.md -o output.md

  # Pipe
  $ remark < input.md > output.md

  # Rewrite all applicable files
  $ remark . -o
```

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/remark.svg

[build-status]: https://travis-ci.org/wooorm/remark

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/remark.svg

[coverage-status]: https://codecov.io/github/wooorm/remark

[chat-badge]: https://img.shields.io/gitter/room/wooorm/remark.svg

[chat]: https://gitter.im/wooorm/remark

[license]: https://github.com/wooorm/remark/blob/master/LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[remark]: https://github.com/wooorm/remark

[plugins]: https://github.com/wooorm/remark/blob/master/doc/plugins.md

[markdown-extensions]: https://github.com/sindresorhus/markdown-extensions

[config-file]: https://github.com/wooorm/unified-engine/blob/master/doc/configure.md

[ignore-file]: https://github.com/wooorm/unified-engine/blob/master/doc/ignore.md

[unified-args]: https://github.com/wooorm/unified-args#cli
