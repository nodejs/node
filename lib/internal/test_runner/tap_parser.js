'use strict';

const util = require('util');

/** TAP14 specifications

See https://testanything.org/tap-version-14-specification.html

Note that the following is intended as a rough “pseudocode” guidance.
It is not strict EBNF:

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

/**
 * An LL(1) parser for TAP14.
 */
class TapParser {
  constructor(input, options = {}) {
    this.lexer = new TapLexer(input);
    this.tokens = this.chunk(this.lexer.scanAll());
    this.currentTokenIndex = 0;
    this.currentTokenChunk = 0;
    this.currentToken = null;
    this.output = {};
    this.subTestLevel = 0;

    // TODO(@manekinekko): should we use a large number to start with, so we don't collide with existing test ids?
    this.testId = 90000;

    if (options.debug) {
      this.debug();
    }
  }

  debug() {
    console.log(util.inspect(this.tokens, { colors: true, depth: 20 }));
  }

  peek(shouldSkipBlankTokens = true) {
    if (shouldSkipBlankTokens) {
      this.skip(TokenKind.WHITESPACE);
    }

    return this.tokens[this.currentTokenChunk][this.currentTokenIndex];
  }

  next(shouldSkipBlankTokens = true) {
    if (shouldSkipBlankTokens) {
      this.skip(TokenKind.WHITESPACE);
    }

    if (this.tokens[this.currentTokenChunk]) {
      this.currentToken =
        this.tokens[this.currentTokenChunk][this.currentTokenIndex++];
    } else {
      this.currentToken = null;
    }

    return this.currentToken;
  }

  // skip the provided tokens in the current chunk
  skip(...tokensToSkip) {
    let token = this.tokens[this.currentTokenChunk][this.currentTokenIndex];
    while (token && tokensToSkip.includes(token.type)) {
      // pre-increment to skip current tokens but make sure we don't advance index on the last iteration
      token = this.tokens[this.currentTokenChunk][++this.currentTokenIndex];
    }
  }

  readNextLiterals() {
    const literals = [];
    let nextToken = this.peek(false);

    // read all literal, numeric, whitespace and escape tokens until we hit a different token or reach end of current chunk
    while (
      nextToken &&
      [
        TokenKind.LITERAL,
        TokenKind.NUMERIC,
        TokenKind.WHITESPACE,
        TokenKind.ESCAPE,
      ].includes(nextToken.type)
    ) {
      const word = this.next(false).value;

      // don't output escaped characters
      if (nextToken.type !== TokenKind.ESCAPE) {
        literals.push(word);
      }

      nextToken = this.peek(false);
    }

    return literals.join('');
  }

  // split all tokens by EOL: [[token1, token2, ...], [token1, token2, ...]]
  // this would simplify the parsing logic
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

  generateNextTestId() {
    return ++this.testId;
  }

  getIndentationLevel(chunk, indentationFactor = 2) {
    // count the number of whitespace tokens in the chunk, starting from the first token
    let whitespaceCount = 0;
    for (let i = 0; i < chunk.length; i++) {
      if (chunk[i].type === TokenKind.WHITESPACE) {
        whitespaceCount++;
      } else {
        break;
      }
    }

    // if the number of whitespace tokens is an exact multiple of "indentationFactor"
    // then return the level of the indentation
    if (whitespaceCount % indentationFactor === 0) {
      return whitespaceCount / indentationFactor;
    }

    // take into account any extra 2 whitespaces related to a YAML block
    const yamlIndentationFactor = 2;
    if ((whitespaceCount - yamlIndentationFactor) % indentationFactor === 0) {
      return (whitespaceCount - yamlIndentationFactor) / indentationFactor;
    }

    return 0;
  }

  emitTestPoint(testPoint) {
    const deep = (node, data, level) => {
      node.document = node.document || {};
      node.document.tests = node.document.tests || [];

      if (level === 0) {
        return node.document.tests.push(data);
      }
      deep(node.document, data, level - 1);
    };

    deep(this.output, testPoint, this.subTestLevel);
  }

  emitPlan(plan) {
    const deep = (node, data, level) => {
      node.document = node.document || {};

      if (level === 0) {
        return (node.document.plan = data);
      }
      deep(node.document, data, level - 1);
    };

    deep(this.output, plan, this.subTestLevel);
  }

  emitVersion(version) {
    const deep = (node, data, level) => {
      node.document = node.document || {};

      if (level === 0) {
        return (node.document.version = data);
      }
      deep(node.document, data, level - 1);
    };

    deep(this.output, version, this.subTestLevel);
  }

  emitComment(comment) {
    const deep = (node, data, level) => {
      node.document = node.document || {};

      if (level === 0) {
        node.document.comments = node.document.comments || [];
        return node.document.comments.push(data);
      }
      deep(node.document, data, level - 1);
    };

    deep(this.output, comment, this.subTestLevel);
  }

  emitSubtestName(name) {
    const deep = (node, data, level) => {
      node.document = node.document || {};

      // subtest name needs to be set on next node level
      // this is why we need one more level (hence -1)
      if (level === -1) {
        return node.document.name = data;
      }
      deep(node.document, data, level - 1);
    };

    deep(this.output, name, this.subTestLevel);
  }

  emitPragma(pragma) {
    const deep = (node, data, level) => {
      node.document = node.document || {};
      node.document.pragmas = node.document.pragmas || {};

      if (level === 0) {
        return (node.document.pragmas = {
          ...node.document.pragmas,
          ...data,
        });
      }
      deep(node.document, data, level - 1);
    };

    deep(this.output, pragma, this.subTestLevel);
  }

  emitYAMLBlock(yaml) {
    const deep = (node, data, level) => {
      node.document = node.document || {};
      node.document.tests = node.document.tests || [];

      if (level === 0) {
        const tests = node.document.tests;
        if (!tests.length) {
          node.document.tests = [{}];
        }
        return (node.document.tests.at(-1).diagnostics = data);
      }
      deep(node.document, data, level - 1);
    };

    deep(this.output, yaml, this.subTestLevel);
  }

  emiteBailout(message) {
    const deep = (node, data, level) => {
      node.document = node.document || {};

      if (level === 0) {
        return (node.document.bailout = data);
      }
      deep(node.document, data, level - 1);
    };

    deep(this.output, message, this.subTestLevel);
  }

  //--------------------------------------------------------------------------//
  //------------------------------ Parser rules ------------------------------//
  //--------------------------------------------------------------------------//

  // TAPDocument := Version Plan Body | Version Body Plan
  parse() {
    this.output = {};

    this.tokens.forEach((chunk) => {
      const subtestBlockIndentationFactor = 4;
      this.subTestLevel = this.getIndentationLevel(
        chunk,
        subtestBlockIndentationFactor
      );
      // if the chunk starts with a multiple of 4 whitespace tokens,
      // then it's a subtest
      if (this.subTestLevel > 0) {
        // remove the indentation block from the current chunk
        chunk = chunk.slice(this.subTestLevel * subtestBlockIndentationFactor);
      }

      this.parseTapBlocks(chunk);

      // move pointers to the next chunk and reset the index
      this.currentTokenChunk++;
      this.currentTokenIndex = 0;
    });

    return this.output;
  }

  parseTapBlocks(chunk) {
    const chunkAsString = chunk.map((token) => token.value).join('');
    const firstToken = chunk[0];
    const { type } = firstToken;

    switch (type) {
      case TokenKind.TAP:
        this.Version();
        break;
      case TokenKind.NUMERIC:
        this.Plan();
        break;
      case TokenKind.TAP_TEST_OK:
      case TokenKind.TAP_TEST_NOTOK:
        this.TestPoint();
        break;
      case TokenKind.COMMENT:
        this.Comment();
        break;
      case TokenKind.TAP_PRAGMA:
        this.Pragma();
        break;
      case TokenKind.WHITESPACE:
        if (this.getIndentationLevel(chunk, 2) === 1) {
          this.YAMLBlock();
        }
        break;
      case TokenKind.LITERAL:
        // check for "Bail out!" literal
        if (/^Bail\s+out!/i.test(chunkAsString)) {
          this.Bailout();
        }
        break;
      default:
        this.lexer.error(`Expected a valid token`, firstToken);
    }
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

    this.emitVersion(numberToken.value);
  }

  // ----------------Plan----------------
  // Plan := "1.." (Number) (" # " Reason)? "\n"
  Plan() {
    // even if specs mention plan starts at 1, we need to make sure we read the plan start value in case of a missing or invalid plan start value
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
    if (hashToken) {
      if (hashToken.type === TokenKind.HASH) {
        this.next(); // skip hash
        body.reason = this.readNextLiterals();
      } else if (hashToken.type === TokenKind.LITERAL) {
        this.lexer.error(`Expected "#"`, hashToken);
      }
    }

    this.emitPlan(body);
  }

  // ----------------TestPoint----------------
  // TestPoint := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?
  // Directive   := " # " ("todo" | "skip") (" " Reason)?
  // YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
  // YAMLLine    := "  " (YAML)* "\n"

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
    } else {
      // TODO(@manekinekko): handle case when test ID is not provided
      numberToken = this.generateNextTestId() + ''; // set as string
    }

    const body = {
      status: isTestFailed ? 'failed' : 'passed',
      id: numberToken,
    };

    // Read optional description prefix " - "
    const descriptionDashToken = this.peek();
    if (descriptionDashToken && descriptionDashToken.type === TokenKind.DASH) {
      this.next(); // skip dash
    }

    // Read optional description
    if (this.peek()) {
      body.description = this.readNextLiterals();
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
    const directive = {
      reason,
      directive: todoOrSkipToken,
    };

    if (directive) {
      if (directive.reason) {
        body.reason = directive.reason;
      }

      if (directive.directive === 'todo') {
        body.status = 'todo';
      }
      if (directive.directive === 'skip') {
        body.status = 'skipped';
      }
    }

    this.emitTestPoint(body);
  }

  // ----------------Bailout----------------
  // BailOut := "Bail out!" (" " Reason)? "\n"
  Bailout() {
    this.next(); // skip "Bail"
    this.next(); // skip "out!"

    // Read optional reason
    const hashToken = this.peek();
    if (hashToken && hashToken.type === TokenKind.HASH) {
      this.next(); // skip hash
    }
    this.emiteBailout(this.readNextLiterals());
  }

  // ----------------Comment----------------
  // Comment := ^ (" ")* "#" [^\n]* "\n"
  Comment() {
    const commentToken = this.next();
    if (commentToken.type !== TokenKind.COMMENT) {
      this.lexer.error(`Expected " # "`, commentToken);
    }

    const subtestKeyword = this.peek();
    if (/^Subtest:/i.test(subtestKeyword.value)) {
      this.next(); // skip subtest keyword
      this.emitSubtestName(this.readNextLiterals().trim());
    }
    else {
      this.emitComment(this.readNextLiterals().trim());
    }

  }

  // ----------------YAMLBlock----------------
  // YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
  // YAMLLine    := "  " (YAML)* "\n"
  // TODO(@manekinekko): YAML content is consumed as a string. Add support for YAML parsing in the future (if needed)
  YAMLBlock() {
    const yaml = this.peek();
    if (yaml && yaml.type === TokenKind.TAP_YAML) {
      this.emitYAMLBlock(this.next().value); // consume raw YAML
    }
  }

  // ----------------PRAGMA----------------
  // Pragma := "pragma " [+-] PragmaKey "\n"
  // PragmaKey   := ([a-zA-Z0-9_-])+
  Pragma() {
    const pragmaToken = this.next();
    if (pragmaToken.type !== TokenKind.TAP_PRAGMA) {
      this.lexer.error(`Expected "pragma"`, pragmaToken);
    }

    const pragmas = {};

    let nextToken = this.peek();
    while (nextToken) {
      let isEnabled = true;
      const pragmaKeySign = this.next();
      if (pragmaKeySign.type === TokenKind.PLUS) {
        isEnabled = true;
      } else if (pragmaKeySign.type === TokenKind.DASH) {
        isEnabled = false;
      } else {
        this.lexer.error(`Expected "+" or "-"`, pragmaKeySign);
      }

      const pragmaKeyToken = this.peek();
      if (pragmaKeyToken.type !== TokenKind.LITERAL) {
        this.lexer.error(`Expected pragma key`, pragmaKeyToken);
      }

      let pragmaKey = this.next().value;

      // in some cases, pragma key can be followed by a comma separator,
      // so we need to remove it
      pragmaKey = pragmaKey.replace(/,/g, '');

      pragmas[pragmaKey] = isEnabled;
      nextToken = this.peek();
    }

    this.emitPragma(pragmas);
  }
}

module.exports = { TapParser };
