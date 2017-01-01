'use strict';

const assert = require('assert');
const Stream = require('stream');
const inherits = require('util').inherits;


/*
 * This filter consumes a stream of characters and emits one string per line.
 */
function LineSplitter() {
  const self = this;
  var buffer = '';

  Stream.call(this);
  this.writable = true;

  this.write = function(data) {
    var lines = (buffer + data).split(/\r\n|\n\r|\n|\r/);
    for (var i = 0; i < lines.length - 1; i++) {
      self.emit('data', lines[i]);
    }
    buffer = lines[lines.length - 1];
    return true;
  };

  this.end = function(data) {
    this.write(data || '');
    if (buffer) {
      self.emit('data', buffer);
    }
    self.emit('end');
  };
}
inherits(LineSplitter, Stream);


/*
 * This filter consumes lines and emits paragraph objects.
 */
function ParagraphParser() {
  const self = this;
  var block_is_license_block = false;
  var block_has_c_style_comment;
  var paragraph_line_indent;
  var paragraph;

  Stream.call(this);
  this.writable = true;

  resetBlock(false);

  this.write = function(data) {
    parseLine(data + '');
    return true;
  };

  this.end = function(data) {
    if (data) {
      parseLine(data + '');
    }
    flushParagraph();
    self.emit('end');
  };

  function resetParagraph() {
    paragraph_line_indent = -1;

    paragraph = {
      li: '',
      in_license_block: block_is_license_block,
      lines: []
    };
  }

  function resetBlock(is_license_block) {
    block_is_license_block = is_license_block;
    block_has_c_style_comment = false;
    resetParagraph();
  }

  function flushParagraph() {
    if (paragraph.lines.length || paragraph.li) {
      self.emit('data', paragraph);
    }
    resetParagraph();
  }

  function parseLine(line) {
    // Strip trailing whitespace
    line = line.replace(/\s*$/, '');

    // Detect block separator
    if (/^\s*(=|"){3,}\s*$/.test(line)) {
      flushParagraph();
      resetBlock(!block_is_license_block);
      return;
    }

    // Strip comments around block
    if (block_is_license_block) {
      if (!block_has_c_style_comment)
        block_has_c_style_comment = /^\s*(\/\*)/.test(line);
      if (block_has_c_style_comment) {
        var prev = line;
        line = line.replace(/^(\s*?)(?:\s?\*\/|\/\*\s|\s\*\s?)/, '$1');
        if (prev === line)
          line = line.replace(/^\s{2}/, '');
        if (/\*\//.test(prev))
          block_has_c_style_comment = false;
      } else {
        // Strip C++ and perl style comments.
        line = line.replace(/^(\s*)(?:\/\/\s?|#\s?)/, '$1');
      }
    }

    // Detect blank line (paragraph separator)
    if (!/\S/.test(line)) {
      flushParagraph();
      return;
    }

    // Detect separator "lines" within a block. These mark a paragraph break
    // and are stripped from the output.
    if (/^\s*[=*-]{5,}\s*$/.test(line)) {
      flushParagraph();
      return;
    }

    // Find out indentation level and the start of a lied or numbered list;
    var result = /^(\s*)(\d+\.|\*|-)?\s*/.exec(line);
    assert.ok(result);
    // The number of characters that will be stripped from the beginning of
    // the line.
    var line_strip_length = result[0].length;
    // The indentation size that will be used to detect indentation jumps.
    // Fudge by 1 space.
    var line_indent = Math.floor(result[0].length / 2) * 2;
    // The indentation level that will be exported
    var level = Math.floor(result[1].length / 2);
    // The list indicator that precedes the actual content, if any.
    var line_li = result[2];

    // Flush the paragraph when there is a li or an indentation jump
    if (line_li || (line_indent !== paragraph_line_indent &&
                    paragraph_line_indent !== -1)) {
      flushParagraph();
      paragraph.li = line_li;
    }

    // Set the paragraph indent that we use to detect indentation jumps. When
    // we just detected a list indicator, wait
    // for the next line to arrive before setting this.
    if (!line_li && paragraph_line_indent !== -1) {
      paragraph_line_indent = line_indent;
    }

    // Set the output indent level if it has not been set yet.
    if (paragraph.level === undefined)
      paragraph.level = level;

    // Strip leading whitespace and li.
    line = line.slice(line_strip_length);

    if (line)
      paragraph.lines.push(line);
  }
}
inherits(ParagraphParser, Stream);


/*
 * This filter consumes paragraph objects and emits modified paragraph objects.
 * The lines within the paragraph are unwrapped where appropriate. It also
 * replaces multiple consecutive whitespace characters by a single one.
 */
function Unwrapper() {
  var self = this;

  Stream.call(this);
  this.writable = true;

  this.write = function(paragraph) {
    var lines = paragraph.lines;
    var break_after = [];
    var i;

    for (i = 0; i < lines.length - 1; i++) {
      var line = lines[i];

      // When a line is really short, the line was probably kept separate for a
      // reason.
      if (line.length < 50) {
        // If the first word on the next line really didn't fit after the line,
        // it probably was just ordinary wrapping after all.
        var next_first_word_length = lines[i + 1].replace(/\s.*$/, '').length;
        if (line.length + next_first_word_length < 60) {
          break_after[i] = true;
        }
      }
    }

    for (i = 0; i < lines.length - 1;) {
      if (!break_after[i]) {
        lines[i] += ' ' + lines.splice(i + 1, 1)[0];
      } else {
        i++;
      }
    }

    for (i = 0; i < lines.length; i++) {
      // Replace multiple whitespace characters by a single one, and strip
      // trailing whitespace.
      lines[i] = lines[i].replace(/\s+/g, ' ').replace(/\s+$/, '');
    }

    self.emit('data', paragraph);
  };

  this.end = function(data) {
    if (data)
      self.write(data);
    self.emit('end');
  };
}
inherits(Unwrapper, Stream);


/*
 * This filter generates an rtf document from a stream of paragraph objects.
 */
function RtfGenerator() {
  const self = this;
  var did_write_anything = false;

  Stream.call(this);
  this.writable = true;

  this.write = function(paragraph) {
    if (!did_write_anything) {
      emitHeader();
      did_write_anything = true;
    }

    var li = paragraph.li;
    var level = paragraph.level + (li ? 1 : 0);
    var lic = paragraph.in_license_block;

    var rtf = '\\pard';
    rtf += '\\sa150\\sl300\\slmult1';
    if (level > 0)
      rtf += '\\li' + (level * 240);
    if (li) {
      rtf += '\\tx' + (level) * 240;
      rtf += '\\fi-240';
    }
    if (lic)
      rtf += '\\ri240';
    if (!lic)
      rtf += '\\b';
    if (li)
      rtf += ' ' + li + '\\tab';
    rtf += ' ';
    rtf += paragraph.lines.map(rtfEscape).join('\\line ');
    if (!lic)
      rtf += '\\b0';
    rtf += '\\par\n';

    self.emit('data', rtf);
  };

  this.end = function(data) {
    if (data)
      self.write(data);
    if (did_write_anything)
      emitFooter();
    self.emit('end');
  };

  function toHex(number, length) {
    var hex = (~~number).toString(16);
    while (hex.length < length)
      hex = '0' + hex;
    return hex;
  }

  function rtfEscape(string) {
    return string
      .replace(/[\\{}]/g, function(m) {
        return '\\' + m;
      })
      .replace(/\t/g, function() {
        return '\\tab ';
      })
      // eslint-disable-next-line no-control-regex
      .replace(/[\x00-\x1f\x7f-\xff]/g, function(m) {
        return '\\\'' + toHex(m.charCodeAt(0), 2);
      })
      .replace(/\ufeff/g, '')
      .replace(/[\u0100-\uffff]/g, function(m) {
        return '\\u' + toHex(m.charCodeAt(0), 4) + '?';
      });
  }

  function emitHeader() {
    self.emit('data', '{\\rtf1\\ansi\\ansicpg1252\\uc1\\deff0\\deflang1033' +
                      '{\\fonttbl{\\f0\\fswiss\\fcharset0 Tahoma;}}\\fs20\n' +
                      '{\\*\\generator txt2rtf 0.0.1;}\n');
  }

  function emitFooter() {
    self.emit('data', '}');
  }
}
inherits(RtfGenerator, Stream);


const stdin = process.stdin;
const stdout = process.stdout;
const line_splitter = new LineSplitter();
const paragraph_parser = new ParagraphParser();
const unwrapper = new Unwrapper();
const rtf_generator = new RtfGenerator();

stdin.setEncoding('utf-8');
stdin.resume();

stdin.pipe(line_splitter);
line_splitter.pipe(paragraph_parser);
paragraph_parser.pipe(unwrapper);
unwrapper.pipe(rtf_generator);
rtf_generator.pipe(stdout);
