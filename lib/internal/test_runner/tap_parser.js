/** TAP14 specifications

TAPDocument := Version Plan Body | Version Body Plan
Version     := "TAP version 14\n"
Plan        := "1.." (Number) (" # " Reason)? "\n"
Body        := (TestPoint | BailOut | Pragma | Comment | Anything | Empty | Subtest)*
TestPoint   := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?
Directive   := " # " ("todo" | "skip") (" " Reason)?
YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
YAMLLine    := "  " (YAML)* "\n"
BailOut     := "Bail out!" (" " Reason)? "\n"
Reason      := [^\n]+
Pragma      := "pragma " [+-] PragmaKey "\n"
PragmaKey   := ([a-zA-Z0-9_-])+
Subtest     := ("# Subtest" (": " SubtestName)?)? "\n" SubtestDocument TestPoint
Comment     := ^ (" ")* "#" [^\n]* "\n"
Empty       := [\s\t]* "\n"
Anything    := [^\n]+ "\n"

*/

const console = require('console');

class Token {
  constructor(type, value, input) {
    this.type = type;
    this.value = value;
    this.line = input.line;
    this.column = input.column - ('' + value).length;
    this.position = input.pos - ('' + value).length;
  }
}

class InputStream {
  constructor(input) {
    this.input = input;
    this.pos = 0;
    this.column = 0;
    this.line = 1;
  }

  eof() {
    return this.peek() === undefined;
  }

  peek(offset = 0) {
    return this.input[this.pos + offset];
  }

  next() {
    const c = this.peek();
    if (c === undefined) {
      return undefined;
    }

    this.pos++;
    this.column++;
    if (c === '\n') {
      this.line++;
      this.column = 0;
    }
    return c;
  }

  error(message) {
    this.dump();
    throw new Error(
      `TAP: ${message} at line ${this.line}, column ${this.column} (position ${this.pos})`
    );
  }

  dump() {
    console.log('-----');
    console.log(this.input.split('\n')[this.line]);
    if (this.column === 1) {
      console.log('^');
    } else {
      console.log(' '.repeat(Math.max(0, this.column - 1)), '^');
    }
    console.log('pos:', this.pos, 'line:', this.line, 'column:', this.column);
    console.log('-----');
  }
}

class TapTokenizer {
  #keywords = new Set([
    'ok',
    'not ok',
    '#',
    'Bail out!',
    'TODO',
    'SKIP',
    '1..',
    'TAP',
    'version',
    '---',
    '...',

    // TAP14
    'pragma',
    '+',
    '-',
  ]);

  #specs = {
    '\n': { type: 'newline' },
    ' ': { type: 'whitespace' },
    '\t': { type: 'whitespace' },
    '\r': { type: 'whitespace' },
    '\f': { type: 'whitespace' },
    '#': { type: 'comment' },
    'Bail out!': { type: 'bailout' },
    TODO: { type: 'todo' },
    SKIP: { type: 'skip' },
    '1..': { type: 'plan' },
    TAP: { type: 'tap' },
    version: { type: 'version' },
    '---': { type: 'yaml-start' },
    '...': { type: 'yaml-end' },
    pragma: { type: 'pragma' },
    '+': { type: 'plus' },
    '-': { type: 'minus' },
    ok: { type: 'ok' },
    'not ok': { type: 'not-ok' },
    '#': { type: 'directive' },
    Subtest: { type: 'subtest' },
    'Subtest:': { type: 'subtest-name' },
  };

  lastKeyword = null;

  constructor(input) {
    this.input = new InputStream(input);
  }

  scan() {
    this.input.dump();

    if (this.input.eof()) {
      return undefined;
    }

    const char = this.input.peek();

    // Skip whitespace
    this.skipWhitespace();

    // Skip newlines
    if (char === '\n') {
      this.input.next();
      this.input.column = 0;
      this.input.line++;
      return this.scan();
    }

    // Read numbers
    if (char >= '0' && char <= '9') {
      if (
        char === '1' &&
        this.input.peek(1) === '.' &&
        this.input.peek(2) === '.'
      ) {
        // Read plan x..y
        return this.readPlan();
      }

      return this.readNumber();
    }

    // Read literals
    if ((char >= 'a' && char <= 'z') || (char >= 'A' && char <= 'Z')) {
      return this.readLiteral(char);
    }

    // Read YAML
    if (
      char === '-' &&
      this.input.peek(1) === '-' &&
      this.input.peek(2) === '-'
    ) {
      return this.readYaml();
    }

    this.input.error(`Unexpected character ${char}`);
  }

  eof() {
    return this.input.eof();
  }

  skipComment() {
    while (!this.input.eof() && this.input.peek() !== '\n') {
      this.input.next();
    }
  }

  isKeyword(word) {
    return this.#keywords.has(word);
  }

  isYaml(word) {
    return word === '---';
  }

  isPlan(word) {
    return word === '1..';
  }

  isTestPoint(word) {
    return word === 'ok' || word === 'not ok';
  }

  isVersion(word) {
    return word === 'TAP' || word === 'version';
  }

  isNumber(char) {
    return char.match(/^\d+$/);
  }

  isWhitespace(char) {
    return ' \t\n'.indexOf(char) >= 0;
  }

  skipWhitespace() {
    while (!this.input.eof() && this.isWhitespace(this.input.peek())) {
      this.input.next();
    }
  }

  skip(word) {
    this.skipWhitespace();

    if (word === undefined) {
      return;
    }

    if (typeof word === 'string') {
      if (word.length === 0) {
        return;
      }

      let str = '';
      while (!this.input.eof()) {
        const c = this.input.peek();
        if (this.isWhitespace(c)) {
          break;
        }

        str += this.input.next();
        if (str === word) {
          console.log('skipped', `"${str}"`);
          return;
        }
      }
      this.input.error(`Expected token "${word}" after "${this.lastKeyword}"`);
    }
  }

  consume(regexOrWord, isOptional = false) {
    let str = '';

    this.skipWhitespace();

    if (typeof regexOrWord === 'string') {
      while (!this.input.eof()) {
        const c = this.input.peek();
        if (this.isWhitespace(c)) {
          break;
        }

        str += this.input.next();
        if (str === regexOrWord) {
          console.log(`P:${this.input.pos}, L:${this.input.line}, C:${this.input.column}:`, 'consumed', `"${str}"`);
          return str;
        }
      }

      if (isOptional) {
        this.input.pos -= str.length;
        console.log('skipped', `"${str}"`);
        return;
      }
      this.input.error(`Expected token "${regexOrWord}"`);
    }

    while (!this.input.eof() && regexOrWord.test(this.input.peek())) {
      str += this.input.next();
    }

    console.log(`P:${this.input.pos}, L:${this.input.line}, C:${this.input.column}:`, 'consumed', `"${str}"`);
    return str;
  }

  readLiteral(word) {
    word = this.consume(word)?.trim();

    if (this.isKeyword(word)) {
      return new Token('keyword', word, this.input);
    }
    if (this.isVersion(word)) {
      return new Token('version', this.readVersion(), this.input);
    }
    if (this.isTestPoint(word)) {
      return new Token('TestPoint', this.readTestPoint(), this.input);
    }
  }

  readNumber() {
    const number = this.consume(/^\d+/);
    return new Token('Number', parseInt(number, 10), this.input);
  }
}

// TAP protocol version 13 parser
class TapParser {
  #document = null;

  constructor(input) {
    this.lexer = new TapTokenizer(input);
  }

  parse() {
    // TAPDocument := Version Plan Body | Version Body Plan
    return {
      type: 'TAPDocument',
      body: {
        version: this.Version(),
        plan: this.Plan(),
        body: this.Body(),
      },
    };
  }

  // Version := "TAP version 14\n"
  Version() {
    const token = this.lexer.readLiteral('TAP');
    const version = this.lexer.readLiteral('version');
    const number = this.lexer.readNumber();
    return {
      type: 'Version',
      body: [token, version, number],
    };
  }

  // Plan := "1.." (Number) (" # " Reason)? "\n"
  Plan() {
    this.lexer.readLiteral('1..');
    const number = this.lexer.readNumber();

    // Read optional reason
    const reason = this.parseReason();
    const body = [number];
    if (reason) {
      body.push(reason);
    }

    return {
      type: 'Plan',
      body,
    };
  }

  // Body := (TestPoint | BailOut | Pragma | Comment | Anything | Empty | Subtest)*
  Body() {
    const body = [];

    while (!this.lexer.eof()) {
      const token = this.lexer.scan();
      console.log(this.lexer.eof())
      if (!token) {
        continue;
      }

      switch (token.type) {
        case 'TestPoint':
          body.push(this.TestPoint());
          break;
        case 'BailOut':
          body.push(this.BailOut());
          break;
        case 'Pragma':
          body.push(this.Pragma());
          break;
        case 'Comment':
          body.push(this.Comment());
          break;
        case 'Anything':
          body.push(this.Anything());
          break;
        case 'Empty':
          body.push(this.Empty());
          break;
        case 'Subtest':
          body.push(this.Subtest());
          break;
        default:
          return;
      }
    }

    return body;
  }

  // TestPoint := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?
  TestPoint() {
    const not = this.lexer.consume('not', true);
    const ok = this.lexer.consume('ok');
    const testNumber = this.lexer.readNumber();

    // Read optional reason
    const description = this.parseDecription();

    // Read optional directive
    const directive = this.parseDirective();

    const body = {
      testNumber,
      status: not ? 'fail' : 'pass',
    };

    if (description) {
      body.description = description;
    }

    if (directive) {
      body.directive = directive;
    }

    return {
      type: 'TestPoint',
      body
    };
  }

  parseDecription() {
    this.lexer.consume('-', true);
    // read everything after the - and before the newline or a #
    const description = this.lexer.consume(/[^\n#]+/);
    return new Token('Description', description, this.lexer.input);
  }

  parseReason() {
    this.lexer.consume('#', true);
    // read everything after the # and before the newline
    const reason = this.lexer.consume(/[^\n]+/);
    return new Token('Reason', reason, this.lexer.input);
  }

  parseDirective() {
    this.lexer.consume('#', true);
    // read everything after the # and before the newline
    const directive = this.lexer.consume(/[^\n]+/);
    return new Token('Directive', directive, this.lexer.input);
  }
}

const util = require('util');

const input = `TAP version 14
1..3 # plan description
ok 2 - Summarized NOOK # TODO Not written yet`;
const parser = new TapParser(input);
const result = parser.parse();
console.log('****');
console.log(util.inspect(result, { depth: 6, colors: true }));

module.exports = { TapParser };
