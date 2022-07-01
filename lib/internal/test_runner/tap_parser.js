'use strict';

const util = require('util');

/** TAP14 specifications

See https://testanything.org/tap-version-14-specification.html

Note that the following grammar is intended as a rough “pseudocode” guidance.
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
const { TapLexer, TokenKind } = require('internal/test_runner/tap_lexer');

/**
 * An LL(1) parser for TAP14.
 */
class TapParser {
  constructor(input, options = {}) {
    this.lexer = new TapLexer(input);
    this.tokens = this.chunk(this.lexer.scan());
    this.currentTokenIndex = 0;
    this.currentTokenChunk = 0;
    this.currentToken = null;
    this.output = {};
    this.documents = [];
    this.subTestLevel = 0;

    this.yamlBlock = [];
    this.isYAMLBlock = false;

    // use this stack to keep track of the beginning of each subtest
    // the top of the stack is the current subtest
    // everytime we enter a subtest, we push a new subtest onto the stack
    // when the stack is empty, we assume all subtests have been terminated
    this.subtestsStack = [];

    // TODO(@manekinekko): should we use a large number to start with, so we don't collide with existing test ids?
    this.testId = 90000;

    if (options.debug) {
      this.debug();
    }
  }

  debug() {
    console.log(util.inspect(this.tokens, { colors: true, depth: 10 }));
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
    while (token && tokensToSkip.includes(token.kind)) {
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
        TokenKind.DASH,
        TokenKind.PLUS,
        TokenKind.WHITESPACE,
        TokenKind.ESCAPE,
      ].includes(nextToken.kind)
    ) {
      const word = this.next(false).value;

      // don't output escaped characters
      if (nextToken.kind !== TokenKind.ESCAPE) {
        literals.push(word);
      }

      nextToken = this.peek(false);
    }

    return literals.join('').trim();
  }

  // split all tokens by EOL: [[token1, token2, ...], [token1, token2, ...]]
  // this would simplify the parsing logic
  chunk(tokens) {
    return [...tokens]
      .reduce(
        (acc, token) => {
          if (token.kind === TokenKind.EOL) {
            acc.push([]);
          } else {
            acc[acc.length - 1].push(token);
          }
          return acc;
        },
        [[]]
      )
      .filter((chunk) => chunk.length > 0 && chunk[0].kind !== TokenKind.EOF);
  }

  generateNextTestId() {
    return ++this.testId;
  }

  countSpaces(chunk) {
    // count the number of whitespace tokens in the chunk, starting from the first token
    let whitespaceCount = 0;
    for (let i = 0; i < chunk.length; i++) {
      if (chunk[i].kind === TokenKind.WHITESPACE) {
        whitespaceCount++;
      } else {
        break;
      }
    }
    return whitespaceCount;
  }

  getIndentationLevel(chunk, indentationFactor = 2) {
    const whitespaceCount = this.countSpaces(chunk);

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

  // run a depth-first traversal of each node in the current AST
  // until we hit a certain indentation level.
  // we also create a new "documents" entry for each level we hit
  // this will be used to host the coming subtest entries
  dfs(node, currentLevel, fn, stopLevel = 0) {
    node.documents ||= [{}];

    if (currentLevel === stopLevel) {
      return fn(node);
    }

    for (const document of node.documents) {
      this.dfs(document, currentLevel - 1, fn, stopLevel);
    }
  }

  //----------------------------------------------------------------------//
  //------------------------------ Emitters ------------------------------//
  //----------------------------------------------------------------------//

  emitTestPoint(value) {
    this.dfs(this.documents, this.subTestLevel, (node) => {
      // if we are at the parent level, check if the current test is terminating any
      // subtests that are still open
      if (this.subtestsStack.length > 0) {
        // peek most recent subtest on the stack
        const { name, level } = this.subtestsStack.at(-1);

        // if the current test is terminating a subtest, then we need to close it
        if (level === this.subTestLevel + 1 && name === value.description) {
          // terminate the most recent subtest
          this.subtestsStack.pop();

          // mark the subtest as terminated in the most recent child document
          node.documents.at(-1).documents.at(-1).terminated = true;

          // create a sub documents entry in current documents for the next subtest (if any)
          // this will allow us to make sure any new subtests are created in the new document (context)
          node.documents.at(-1).documents.push({});

          // add the test point entry to the most recent parent document
          node.documents.at(-1).tests ||= [];
          node.documents.at(-1).tests.push(value);
        }

        // if no subtest is terminating, then we need to add the test point to the most recent subtest
        else {
          node.documents.at(-1).tests ||= [];
          node.documents.at(-1).tests.push(value);
        }
      }
      // if no subtests were parsed, then we need to add the test point to the most recent document
      // this is the case when we are parsing a test that is at the root level
      else {
        node.documents.at(-1).tests ||= [];
        node.documents.at(-1).tests.push(value);
      }
    });
  }

  emitPlan(value) {
    this.dfs(this.documents, this.subTestLevel, (node) => {
      node.documents.at(-1).plan = value;
    });
  }

  emitVersion(value) {
    this.dfs(this.documents, this.subTestLevel, (node) => {
      node.documents.at(-1).version = value;
    });
  }

  emitComment(value) {
    this.dfs(this.documents, this.subTestLevel, (node) => {
      node.documents.at(-1).comments ||= [];
      node.documents.at(-1).comments.push(value);
    });
  }

  emitSubtestName(value) {
    this.dfs(
      this.documents,
      this.subTestLevel,
      (node) => {
        node.documents.at(-1).name = value;
        node.documents.at(-1).terminated = false;

        // we store the name of the coming subtest, and its level.
        // the subtest level is the level of the current indentation level + 1
        this.subtestsStack.push({ name: value, level: this.subTestLevel + 1 });
      },
      // subtest name declared in comment is usually encountered before the subtest block starts.
      // we need to emit the name on the node that comes after the current node.
      // this is why we set the stop level to -1.
      // This will allow us to create a new document node for the coming subtest.
      -1
    );
  }

  emitPragma(value) {
    this.dfs(this.documents, this.subTestLevel, (node) => {
      node.documents.at(-1).pragmas ||= {};
      node.documents.at(-1).pragmas = {
        ...node.documents.at(-1).pragmas,
        ...value,
      };
    });
  }

  emitYAMLBlock(value) {
    this.dfs(this.documents, this.subTestLevel, (node) => {
      node.documents.at(-1).tests ||= [{}];
      node.documents.at(-1).tests.at(-1).diagnostics = value;
    });
  }

  emiteBailout(value) {
    this.dfs(this.documents, this.subTestLevel, (node) => {
      node.documents.at(-1).bailout = value;
    });
  }

  //--------------------------------------------------------------------------//
  //------------------------------ Parser rules ------------------------------//
  //--------------------------------------------------------------------------//

  // TAPDocument := Version Plan Body | Version Body Plan
  parse() {
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

      this.TAPDocument(chunk);

      // move pointers to the next chunk and reset the index
      this.currentTokenChunk++;
      this.currentTokenIndex = 0;
    });

    if (this.isYAMLBlock) {
      // looks like we have a non-ending YAML block
      this.lexer.error(`Expected end of YAML block`, this.tokens.at(-1).at(-1));
    }

    this.output = {
      root: { ...this.documents },
    };

    return this.output;
  }

  TAPDocument(chunk) {
    const chunkAsString = chunk.map((token) => token.value).join('');
    const firstToken = chunk[0];
    const { kind } = firstToken;

    // only even number of spaces are considered indentation
    const nbSpaces = this.countSpaces(chunk);
    if (nbSpaces % 2 !== 0) {
      this.lexer.error(
        `Invalid indentation level. Expected ${nbSpaces - 1} spaces`,
        chunk[nbSpaces - 1]
      );
    }

    switch (kind) {
      case TokenKind.TAP:
        return this.Version();
      case TokenKind.NUMERIC:
        return this.Plan();
      case TokenKind.TAP_TEST_OK:
      case TokenKind.TAP_TEST_NOTOK:
        return this.TestPoint();
      case TokenKind.COMMENT:
        return this.Comment();
      case TokenKind.TAP_PRAGMA:
        return this.Pragma();
      case TokenKind.WHITESPACE:
        return this.YAMLBlock();
      case TokenKind.LITERAL:
        // check for "Bail out!" literal (case insensitive)
        if (/^Bail\s+out!/i.test(chunkAsString)) {
          return this.Bailout();
        }
      default:
        this.lexer.error(`Expected a valid token`, firstToken);
    }
  }

  // ----------------Version----------------
  // Version := "TAP version 14\n"
  Version() {
    const tapToken = this.next();

    if (tapToken.kind !== TokenKind.TAP) {
      this.lexer.error(`Expected "TAP"`, tapToken);
    }

    const versionToken = this.next();
    if (versionToken.kind !== TokenKind.TAP_VERSION) {
      this.lexer.error(`Expected "version"`, versionToken);
    }

    const numberToken = this.next();
    if (numberToken.kind !== TokenKind.NUMERIC) {
      this.lexer.error(`Expected Numeric`, numberToken);
    }

    this.emitVersion(numberToken.value);
  }

  // ----------------Plan----------------
  // Plan := "1.." (Number) (" # " Reason)? "\n"
  Plan() {
    // even if specs mention plan starts at 1, we need to make sure we read the plan start value in case of a missing or invalid plan start value
    const planStart = this.next();

    if (planStart.kind !== TokenKind.NUMERIC) {
      this.lexer.error(`Expected a Numeric`, planStart);
    }

    const planToken = this.next();
    if (planToken.kind !== TokenKind.TAP_PLAN) {
      this.lexer.error(`Expected ".."`, planToken);
    }

    const planEnd = this.next();
    if (planEnd.kind !== TokenKind.NUMERIC) {
      this.lexer.error(`Expected a Numeric`, planEnd);
    }

    const body = {
      start: planStart.value,
      end: planEnd.value,
    };

    // Read optional reason
    const hashToken = this.peek();
    if (hashToken) {
      if (hashToken.kind === TokenKind.HASH) {
        this.next(); // skip hash
        body.reason = this.readNextLiterals();
      } else if (hashToken.kind === TokenKind.LITERAL) {
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

    if (notToken.kind === TokenKind.TAP_TEST_NOTOK) {
      this.next(); // skip "not" token
      isTestFailed = true;
    }

    const okToken = this.next();
    if (okToken.kind !== TokenKind.TAP_TEST_OK) {
      this.lexer.error(`Expected "ok" or "not ok"`, okToken);
    }

    // Read optional test number
    let numberToken = this.peek();
    if (numberToken && numberToken.kind === TokenKind.NUMERIC) {
      numberToken = this.next().value;
    } else {
      // TODO(@manekinekko): handle case when test ID is not provided
      numberToken = this.generateNextTestId() + ''; // set as string
    }

    const body = {
      // output both failed and passed properties to make it easier for the checker to detect the test status
      status: {
        fail: isTestFailed,
        pass: !isTestFailed,
        todo: false,
        skip: false,
      },
      id: numberToken,
      description: '',
      reason: '',
    };

    // Read optional description prefix " - "
    const descriptionDashToken = this.peek();
    if (descriptionDashToken && descriptionDashToken.kind === TokenKind.DASH) {
      this.next(); // skip dash
    }

    // Read optional description
    if (this.peek()) {
      const description = this.readNextLiterals();
      if (description) {
        body.description = description;
      }
    }

    // Read optional directive and reason
    const hashToken = this.peek();
    if (hashToken && hashToken.kind === TokenKind.HASH) {
      this.next(); // skip hash
    }

    let todoOrSkipToken = this.peek();
    if (todoOrSkipToken && todoOrSkipToken.kind === TokenKind.LITERAL) {
      if (/todo/i.test(todoOrSkipToken.value)) {
        todoOrSkipToken = 'todo';
        this.next(); // skip token
      } else if (/skip/i.test(todoOrSkipToken.value)) {
        todoOrSkipToken = 'skip';
        this.next(); // skip token
      }
    }

    const reason = this.readNextLiterals();
    if (todoOrSkipToken) {
      if (reason) {
        body.reason = reason;
      }

      body.status.todo = todoOrSkipToken === 'todo';
      body.status.skip = todoOrSkipToken === 'skip';
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
    if (hashToken && hashToken.kind === TokenKind.HASH) {
      this.next(); // skip hash
    }
    this.emiteBailout(this.readNextLiterals());
  }

  // ----------------Comment----------------
  // Comment := ^ (" ")* "#" [^\n]* "\n"
  Comment() {
    const commentToken = this.next();
    if (commentToken.kind !== TokenKind.COMMENT) {
      this.lexer.error(`Expected " # "`, commentToken);
    }

    const subtestKeyword = this.peek();
    if (subtestKeyword) {
      if (/^Subtest:/i.test(subtestKeyword.value)) {
        this.next(); // skip subtest keyword
        this.emitSubtestName(this.readNextLiterals().trim());
      } else {
        this.emitComment(this.readNextLiterals().trim());
      }
    }
  }

  // ----------------YAMLBlock----------------
  // YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
  // YAMLLine    := "  " (YAML)* "\n"
  // TODO(@manekinekko): Add support for YAML parsing in the future (if needed)
  YAMLBlock() {
    const token = this.peek();

    switch (token.kind) {
      case TokenKind.TAP_YAML_START:
        this.isYAMLBlock = true;
        this.next(); // skip
        return;
      case TokenKind.TAP_YAML_END:
        if (!this.isYAMLBlock) {
          // looks like we have a YAML end block, but we didn't have a start block
          this.lexer.error(`Unexpected end of YAML block`, token);
        }

        this.isYAMLBlock = false;
        this.next(); // skip
        this.emitYAMLBlock(this.yamlBlock); // consume raw YAML
        return;
      default:
        this.yamlBlock.push(this.readNextLiterals());
    }
  }

  // ----------------PRAGMA----------------
  // Pragma := "pragma " [+-] PragmaKey "\n"
  // PragmaKey := ([a-zA-Z0-9_-])+
  Pragma() {
    const pragmaToken = this.next();
    if (pragmaToken.kind !== TokenKind.TAP_PRAGMA) {
      this.lexer.error(`Expected "pragma"`, pragmaToken);
    }

    const pragmas = {};

    let nextToken = this.peek();
    while (
      nextToken &&
      [TokenKind.EOL, TokenKind.EOF].includes(nextToken.kind) === false
    ) {
      let isEnabled = true;
      const pragmaKeySign = this.next();
      if (pragmaKeySign.kind === TokenKind.PLUS) {
        isEnabled = true;
      } else if (pragmaKeySign.kind === TokenKind.DASH) {
        isEnabled = false;
      } else {
        this.lexer.error(`Expected "+" or "-"`, pragmaKeySign);
      }

      const pragmaKeyToken = this.peek();
      if (pragmaKeyToken.kind !== TokenKind.LITERAL) {
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
