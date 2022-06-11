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
    throw new Error(
      `TAP: ${message} at line ${this.line}, column ${this.column} (position ${this.pos})`
    );
  }

  dump() {
    console.log(this.input.slice(this.pos));
  }
}

class TapLexer {
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

  constructor(input) {
    this.input = new InputStream(input);
  }

  scan() {
    if (this.input.eof()) {
      return undefined;
    }

    const char = this.input.peek();
    console.log('next char is', char, `at position ${this.input.pos}`);

    // Skip whitespace
    if (this.isWhitespace(char)) {
      this.input.next();
      return this.scan();
    }

    // Skip comments
    if (char === '#') {
      this.skipComment();
      return this.scan();
    }

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

    // Read words
    if ((char >= 'a' && char <= 'z') || (char >= 'A' && char <= 'Z')) {
      return this.readWord();
    }

    // Read YAML
    if (
      char === '-' &&
      this.input.peek(1) === '-' &&
      this.input.peek(2) === '-'
    ) {
      return this.readYaml();
    }

    // Read pragma
    // if (char === '+' || char === '-') {
    //   return this.readPragma();
    // }

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
    return char.match(/^[0-9]+$/);
  }

  isWhitespace(char) {
    return " \t\n".indexOf(char) >= 0;
  }

  skip(word) {
    if (word === undefined) {
      return;
    }

    if (typeof word === 'string') {
      if (word.length === 0) {
        return;
      }

      let str = '';
      while (!this.input.eof()) {
        str += this.input.next();
        if (str.trim() === word) {
          console.log('skipped', `"${str}"`);
          return;
        }
      }
    }

    return;
  }

  consume(regex) {
    let str = '';
    while (!this.input.eof() && regex.test(this.input.peek())) {
      str += this.input.next();
      console.log(this.input.pos, regex, `"${str}"`,  this.input.peek());
    }

    console.log(this.input.pos, regex, `"${str}"`,  regex.test(this.input.peek()));
    return str;
  }

  readWord() {
    const word = this.consume(/[a-zA-Z]+/);
    if (this.isKeyword(word)) {
      return {
        type: 'keyword',
        value: word,
      };
    }

    return {
      type: 'string',
      value: word,
    };
  }

  readIdentifier() {
    const word = this.consume(/^[a-zA-Z]+/)?.trim();

    console.log('readIdentifier', word);

    if (this.isKeyword(word)) {
      console.log('found keyword', word);
      return {
        type: 'keyword',
        value: word,
      };
    }
    // if (this.isYaml(word)) {
    //   return {
    //     type: 'yaml',
    //     value: this.readYaml(),
    //   };
    // }
    // if (this.isPlan(word)) {
    //   console.log('found plan', word);
    //   return {
    //     type: 'plan',
    //     value: this.readPlan(),
    //   };
    // }
    if (this.isVersion(word)) {
      return {
        type: 'version',
        value: this.readVersion(),
      };
    }
    if (this.isTestPoint(word)) {
      return {
        type: 'testPoint',
        value: this.readTestPoint(),
      };
    }
  }

  readTestPoint() {
    const testPoint = this.consume(/^(ok|not ok)/);
    const testNumber = this.readNumber();
    const description = this.readDescription();
    return {
      type: 'testPoint',
      value: {
        testNumber,
        description,
        status: testPoint === 'ok' ? 'pass' : 'fail',
      },
    };
  }

  readDescription() {
    const description = this.consume(/^[^\n]+/);
    return {
      type: 'description',
      value: description,
    };
  }

  readNumber() {
    const number = this.consume(/^[0-9]+/);
    return {
      type: 'number',
      value: parseInt(number, 10),
    };
  }

  readYaml() {
    const yaml = this.consume(/^---/);
    return {
      type: 'yaml',
      value: yaml,
    };
  }

  readPlan() {
    const plan = this.consume(/^1..[0-9]+/);
    return {
      type: 'plan',
      value: plan,
      reason: this.readDescription(),
    };
  }

  readVersion() {
    this.skip('version');
    const version = this.consume(/^[0-9]+/);
    console.log('readVersion', version);
    this.input.dump();

    return {
      type: 'number',
      value: parseInt(version, 10),
    };
  }

  // TAP14
  readPragma() {
    const pragma = this.consume(/^(-|\+)[^\n]+/);
    return {
      type: 'pragma',
      value: pragma,
    };
  }
}

// TAP protocol version 13 parser
class TapParser {

  constructor(input) {
    this.lexer = new TapLexer(input);
  }

  parse() {
    return {
      type: 'document',
      value: this.parseDocument(),
    };
  }

  parseDocument() {
    const token = this.lexer.scan();
    console.log({ token });

    if (!token) {
      return undefined;
    }

    if (token.type === 'keyword') {
      switch (token.value) {
        case 'ok':
          return this.parseTestPoint();
        case 'not ok':
          return this.parseTestPoint();
        case 'plan':
          return this.parsePlan();
        case 'yaml':
          return this.parseYaml();
        case 'TAP':
        case 'version':
          return this.parseVersion();
        default:
          this.lexer.input.error(`Unexpected keyword ${token.value}`);
      }
    }

    if (token.type === 'number') {
      return this.parseTestPoint();
    }

    this.lexer.input.error(`Unexpected token ${token.type}`);
  }

  parsePlan() {
    const plan = this.lexer.readPlan();
    const body = this.parseBody();
    return {
      type: 'plan',
      value: {
        plan,
        body,
      },
    };
  }

  parseVersion() {
    const version = this.lexer.readVersion();
    return {
      type: 'version',
      value: version,
    };
  }

  parseYaml() {
    const yaml = this.lexer.readYaml();
    return {
      type: 'yaml',
      value: yaml,
    };
  }

  parseTestPoint() {
    const testPoint = this.lexer.readTestPoint();
    const body = this.parseBody();
    return {
      type: 'testPoint',
      value: {
        testPoint,
        body,
      },
    };
  }

  parseBody() {
    const body = [];
    while (!this.lexer.eof()) {
      const token = this.lexer.scan();
      body.push(token);
    }
    return body;
  }
}

const util = require('util');

const input = `
TAP version 14
`;
const parser = new TapParser(input);
const result = parser.parse();
console.log('****');
console.log(util.inspect(result, { depth: 6, colors: true }));

module.exports = { TapParser };
