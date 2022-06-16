'use strict';

const util = require('util');
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
    this.tokens = this.chunk(this.lexer.scanAll());
    this.currentTokenIndex = 0;
    this.currentTokenChunk = 0;
    this.currentToken = null;
    this.testId = 0;
    this.output = {};

    this.debug();
  }

  debug() {
    console.log(util.inspect(this.tokens, { colors: true, depth: 6 }));
  }

  peek(shouldSkipBlankTokens = true) {
    if (shouldSkipBlankTokens) {
      this.skip(TokenKind.WHITESPACE);
    }

    // if we're at the end of the current chunk, move to the next chunk
    // this is a peek, so we don't advance the real indices
    let tmpCurrentTokenIndex = this.currentTokenIndex;
    let tmpCurrentTokenChunk = this.currentTokenChunk;
    if (tmpCurrentTokenIndex + 1 > this.tokens[tmpCurrentTokenChunk].length) {
      // read the next chunk
      tmpCurrentTokenIndex = 0;
      tmpCurrentTokenChunk++;
    }

    if (this.tokens[tmpCurrentTokenChunk]) {
      return this.tokens[tmpCurrentTokenChunk][tmpCurrentTokenIndex];
    }

    return null;
  }

  next(shouldSkipBlankTokens = true) {
    if (shouldSkipBlankTokens) {
      this.skip(TokenKind.WHITESPACE);
    }

    // if we're at the end of the current chunk, move to the next chunk
    if (
      this.currentTokenIndex + 1 >
      this.tokens[this.currentTokenChunk].length
    ) {
      this.currentTokenIndex = 0;
      this.currentTokenChunk++;
    }

    if (this.tokens[this.currentTokenChunk]) {
      this.currentToken =
        this.tokens[this.currentTokenChunk][this.currentTokenIndex++];
    } else {
      this.currentToken = null;
    }

    return this.currentToken;
  }

  skip(...tokensToSkip) {
    let token = this.tokens[this.currentTokenChunk][this.currentTokenIndex];
    while (token && tokensToSkip.includes(token.type)) {
      // pre-increment to skip current tokens but make sure we don't advance index on the last iteration
      token = this.tokens[this.currentTokenChunk][++this.currentTokenIndex];
      // this.updateChunkPointers();
    }
  }

  updateChunkPointers() {
    if (
      this.currentTokenIndex + 1 >=
      this.tokens[this.currentTokenChunk].length
    ) {
      this.currentTokenChunk++;
      this.currentTokenIndex = 0;
    }
  }

  readNextLiterals() {
    const literals = [];
    let nextToken = this.peek();

    // read all literals (and numerics) until we hit a non-literal or end of current chunk
    while (
      nextToken &&
      [TokenKind.LITERAL, TokenKind.NUMERIC, TokenKind.WHITESPACE].includes(
        nextToken.type
      )
    ) {
      literals.push(this.next().value);
      nextToken = this.peek();
    }

    return literals.join('').trim();
  }

  // split all tokens by EOL: [[token1, token2, ...], [token1, token2, ...]]
  chunk(tokens) {
    return [...tokens]
      .reduce(
        (acc, token) => {
          if (token.type === TokenKind.EOL) {
            acc.push([]);
          } else {
            acc[acc.length - 1].push(token);
          }
          return acc;
        },
        [[]]
      )
      .filter((chunk) => chunk.length > 0 && chunk[0].type !== TokenKind.EOF);
  }

  //--------------------------------------------------------------------------//
  //------------------------------ Parser rules ------------------------------//
  //--------------------------------------------------------------------------//

  // TAPDocument := Version Plan Body | Version Body Plan
  parse() {
    this.output = {};

    this.tokens.forEach((chunk) => {
      if (chunk[0].type === TokenKind.TAP) {
        this.output.version = this.Version();
      } else if (chunk[0].type === TokenKind.NUMERIC) {
        this.output.plan = this.Plan();
      } else if (
        chunk[0].type === TokenKind.TAP_TEST_OK ||
        chunk[0].type === TokenKind.TAP_TEST_NOTOK
      ) {
        this.output.tests = this.output.tests || [];
        this.output.tests.push(this.TestPoint());
      } else if (chunk[0].type === TokenKind.COMMENT) {
        // TODO(@manekinekko): should we emit comments?
        this.Comment();
      }
    });

    return this.output;
  }

  // ----------------Version----------------
  // Version := "TAP version 14\n"
  Version() {
    const tapToken = this.next();

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
    if (hashToken && hashToken.type === TokenKind.HASH) {
      this.next(); // skip hash
      body.reason = this.readNextLiterals();
    } else if (hashToken.type === TokenKind.LITERAL) {
      this.lexer.error(`Expected "#"`, hashToken);
    }

    return body;
  }

  // ----------------Body----------------
  // Body := (TestPoint | BailOut | Pragma | Comment | Anything | Empty | Subtest)*
  Body() {
    const body = {};

    let nextToken = this.peek();
    while (nextToken) {
      const testPoint = this.TestPoint();
      body.testPoints = body.testPoints || [];
      body.testPoints.push(testPoint);

      nextToken = this.peek();
    }

    return body;
  }

  // ----------------TestPoint----------------
  // TestPoint := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?
  // Directive   := " # " ("todo" | "skip") (" " Reason)?

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

    // Read optional test number
    let numberToken = this.peek();
    if (numberToken && numberToken.type === TokenKind.NUMERIC) {
      numberToken = this.next().value;
    }
    else {
      // TODO(@manekinekko): handle case when test ID is not provided
      numberToken = ++this.testId + '' // set as string
    }

    const body = {
      test: {
        status: isTestFailed ? 'failed' : 'passed',
        id: numberToken,
      },
    };

    // Read optional description prefix " - "
    const descriptionDashToken = this.peek();
    if (descriptionDashToken && descriptionDashToken.type === TokenKind.DASH) {
      this.next(); // skip dash
    }

    // Read optional description
    if (this.peek()) {
      body.description = this.readNextLiterals();;
    }

    // Read optional directive and reason
    const hashToken = this.peek();
    if (hashToken && hashToken.type === TokenKind.HASH) {
      this.next(); // skip hash
    }

    let todoOrSkipToken = this.peek();
    if (todoOrSkipToken && todoOrSkipToken.type === TokenKind.LITERAL) {
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
    const directive =  {
      reason,
      directive: todoOrSkipToken,
    };

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

  // ----------------Comment----------------
  // Comment := ^ (" ")* "#" [^\n]* "\n"
  Comment() {
    const commentToken = this.next();
    if (commentToken.type !== TokenKind.COMMENT) {
      this.lexer.error(`Expected " # "`, commentToken);
    }

    const comment = this.readNextLiterals();
    return comment;
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
