'use strict';

/** TAP14 specifications

See https://testanything.org/tap-version-14-specification.html

Grammar:

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
const { TapLexer, TokenKind } = require('./tap_lexer');

class TapParser {
  constructor(input) {
    this.lexer = new TapLexer(input);
    this.tokens = [...this.lexer.scanAll()];
    this.currentTokenIndex = 0;
    this.currentToken = null;

    this.testId = 0;
  }

  debug() {
    console.log(this.tokens);
  }

  peek(shouldSkipEOL = true) {
    if (shouldSkipEOL) {
      this.skipEOL();
    }

    return this.tokens[this.currentTokenIndex];
  }

  next(shouldSkipEOL = true) {
    if (shouldSkipEOL) {
      this.skipEOL();
    }

    this.currentToken = this.tokens[this.currentTokenIndex++];
    return this.currentToken;
  }

  // insead of dealing with EOL, we allow next/peek to skip EOL
  skipEOL() {
    let token = this.tokens[this.currentTokenIndex];
    while (token && token.type === TokenKind.EOL) {
      // pre-increment to skip current EOL token and don't advance token index
      token = this.tokens[++this.currentTokenIndex];
    }
  }

  readNextLiterals() {
    const literals = [];

    // read all tokens until EOL
    let nextToken = this.peek();
    while (nextToken && (nextToken.type === TokenKind.LITERAL || nextToken.type === TokenKind.NUMERIC)) {
      literals.push(nextToken.value);
      this.next(); // skip literal we just consumed
      nextToken = this.peek(false); // scan for EOL as well
    }
    return literals.join(' ');
  }

  //--------------------------------------------------------------------------//
  //------------------------------ Parser rules ------------------------------//
  //--------------------------------------------------------------------------//

  // TAPDocument := Version Plan Body | Version Body Plan
  parse() {
    return {
      type: 'TAPDocument',
      document: {
        version: this.Version(),
        plan: this.Plan(),
        body: this.Body(),
      },
    };
  }

  // ----------------Version----------------
  // Version := "TAP version 14\n"
  Version() {
    const tapToken = this.next();
    console.log({ tapToken });

    if (tapToken.type !== TokenKind.TAP) {
      this.lexer.error(`Expected "TAP"`, tapToken);
    }

    const versionToken = this.next();
    if (versionToken.type !== TokenKind.TAP_VERSION) {
      this.lexer.error(`Expected "version"`, versionToken);
    }

    const numberToken = this.next();
    if (numberToken.type !== TokenKind.NUMERIC) {
      this.lexer.error(`Expected Numeric`, numberToken);
    }

    return numberToken.value;
  }

  // ----------------Plan----------------
  // Plan := "1.." (Number) (" # " Reason)? "\n"
  Plan() {
    const planStart = this.next();

    if (planStart.type !== TokenKind.NUMERIC) {
      this.lexer.error(`Expected a Numeric`, planStart);
    }

    const planToken = this.next();
    if (planToken.type !== TokenKind.TAP_PLAN) {
      this.lexer.error(`Expected ".."`, planToken);
    }

    const planEnd = this.next();
    if (planEnd.type !== TokenKind.NUMERIC) {
      this.lexer.error(`Expected a Numeric`, planEnd);
    }

    const body = {
      start: planStart.value,
      end: planEnd.value,
    };

    // Read optional reason
    const hashToken = this.peek();
    if (hashToken.type === TokenKind.HASH) {
      this.next(); // skip hash
      body.reason = this.readNextLiterals();
    }
    else if (hashToken.type === TokenKind.LITERAL) {
      this.lexer.error(`Expected "#"`, hashToken);
    }

    return body;
  }

  // ----------------Body----------------
  // Body := (TestPoint | BailOut | Pragma | Comment | Anything | Empty | Subtest)*
  Body() {
    const body = {};

    let nextToken = this.peek();
    while (nextToken && nextToken.type !== TokenKind.EOF) {

      const testPoint = this.TestPoint();
      body.testPoints = body.testPoints || [];
      body.testPoints.push(testPoint);

      nextToken = this.peek();
    }

    return body;
  }

  // ----------------TestPoint----------------
  // TestPoint := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?

  // Test Status: ok/not ok (required)
  // Test number (recommended)
  // Description (recommended, prefixed by " - ")
  // Directive (only when necessary)
  TestPoint() {
    const notToken = this.peek();
    let isTestFailed = false;

    if (notToken.type === TokenKind.TAP_TEST_NOTOK) {
      this.next(); // skip "not" token
      isTestFailed = true;
    }

    const okToken = this.next();
    if (okToken.type !== TokenKind.TAP_TEST_OK) {
      this.lexer.error(`Expected "ok" or "not ok"`, okToken);
    }

    // TODO(@manekinekko): handle case when test ID is not provided
    const numberToken = this.Number();

    const body = {
      test: {
        status: isTestFailed ? 'failed' : 'passed',
        id: numberToken.value,
      },
    };

    // Read optional description
    const description = this.Description();
    if (description) {
      body.description = description;
    }

    // Read optional directive and reason
    const directive = this.Directive();
    if (directive) {

      if (directive.reason) {
        body.reason = directive.reason;
      }

      if (directive.directive === 'todo') {
        body.test.status = 'todo';
      }
      if (directive.directive === 'skip') {
        body.test.status = 'skipped';
      }
    }

    // Read optional YAML block
    const yamlBlock = this.YAMLBlock();
    if (yamlBlock) {
      body.yaml = yamlBlock;
    }

    return body;
  }

  // ----------------Description----------------
  // Description := (" -")? (" " [^\n]+)
  Description() {
    const descriptionToken = this.peek();
    if (descriptionToken.type === TokenKind.DASH) {
      this.next(); // skip dash
    }
    return this.readNextLiterals();
  }

  // ----------------Reason----------------
  // Reason := [^\n]+
  Reason() {
    const hashToken = this.peek();
    if (hashToken.type === TokenKind.HASH) {
      this.next(); // skip hash
      return this.readNextLiterals();
    }
    return '';
  }

  // ----------------Directive----------------
  // Directive   := " # " ("todo" | "skip") (" " Reason)?
  Directive() {
    const hashToken = this.peek();
    if (hashToken.type === TokenKind.HASH) {
      this.next(); // skip hash
    }

    let todoOrSkipToken = this.peek();
    if (todoOrSkipToken.type === TokenKind.LITERAL) {
      if (/todo/i.test(todoOrSkipToken.value)) {
        todoOrSkipToken = 'todo'; // force set directive to "todo"
        this.next(); // skip token
      }
      if (/skip/i.test(todoOrSkipToken.value)) {
        todoOrSkipToken = 'skip'; // force set directive to "skip"
        this.next(); // skip token
      }
    }

    const reason = this.readNextLiterals();
    return {
      reason,
      directive: todoOrSkipToken,
    };
  }

  // ----------------Number----------------
  // Number := ([0-9]+)
  Number() {
    const numberToken = this.peek();
    if (numberToken.type === TokenKind.NUMERIC) {
      return this.next();
    }

    // TODO(@manekinekko): handle case when test ID is not provided
    return {
      value: ++this.testId + '', // set as string
    };
  }

  // ----------------YAMLBlock----------------
  // YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
  // YAMLLine    := "  " (YAML)* "\n"
  // TODO(@manekinekko): YAML content is consumed as a string. Add support for YAML parsing in the future (if needed)
  YAMLBlock() {
    const yaml = this.peek();
    if (yaml && yaml.type === TokenKind.TAP_YAML) {
      return this.next(); // consume YAML
    }
    return null;
  }
}

module.exports = { TapParser };
