'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePop,
  ArrayPrototypePush,
  Boolean,
  Number,
  RegExpPrototypeExec,
  String,
  StringPrototypeEndsWith,
  StringPrototypeReplaceAll,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeTrim,
} = primordials;
const Transform = require('internal/streams/transform');
const { TapLexer, TokenKind } = require('internal/test_runner/tap_lexer');
const { TapChecker } = require('internal/test_runner/tap_checker');
const {
  codes: { ERR_TAP_VALIDATION_ERROR, ERR_TAP_PARSER_ERROR },
} = require('internal/errors');
const { kEmptyObject } = require('internal/util');
/**
 *
 * TAP14 specifications
 *
 * See https://testanything.org/tap-version-14-specification.html
 *
 * Note that the following grammar is intended as a rough "pseudocode" guidance.
 * It is not strict EBNF:
 *
 * TAPDocument := Version Plan Body | Version Body Plan
 * Version     := "TAP version 14\n"
 * Plan        := "1.." (Number) (" # " Reason)? "\n"
 * Body        := (TestPoint | BailOut | Pragma | Comment | Anything | Empty | Subtest)*
 * TestPoint   := ("not ")? "ok" (" " Number)? ((" -")? (" " Description) )? (" " Directive)? "\n" (YAMLBlock)?
 * Directive   := " # " ("todo" | "skip") (" " Reason)?
 * YAMLBlock   := "  ---\n" (YAMLLine)* "  ...\n"
 * YAMLLine    := "  " (YAML)* "\n"
 * BailOut     := "Bail out!" (" " Reason)? "\n"
 * Reason      := [^\n]+
 * Pragma      := "pragma " [+-] PragmaKey "\n"
 * PragmaKey   := ([a-zA-Z0-9_-])+
 * Subtest     := ("# Subtest" (": " SubtestName)?)? "\n" SubtestDocument TestPoint
 * Comment     := ^ (" ")* "#" [^\n]* "\n"
 * Empty       := [\s\t]* "\n"
 * Anything    := [^\n]+ "\n"
 *
 */

/**
 * An LL(1) parser for TAP14/TAP13.
 */
class TapParser extends Transform {
  #checker = null;
  #lexer = null;
  #currentToken = null;

  #input = '';
  #currentChunkAsString = '';
  #lastLine = '';

  #tokens = [[]];
  #flatAST = [];
  #bufferedComments = [];
  #bufferedTestPoints = [];
  #lastTestPointDetails = {};
  #yamlBlockBuffer = [];

  #currentTokenIndex = 0;
  #currentTokenChunk = 0;
  #subTestNestingLevel = 0;
  #yamlCurrentIndentationLevel = 0;
  #kSubtestBlockIndentationFactor = 4;

  #isYAMLBlock = false;
  #isSyncParsingEnabled = false;

  constructor({ specs = TapChecker.TAP13 } = kEmptyObject) {
    super({ __proto__: null, readableObjectMode: true });

    this.#checker = new TapChecker({ specs });
  }

  // ----------------------------------------------------------------------//
  // ----------------------------- Public API -----------------------------//
  // ----------------------------------------------------------------------//

  parse(chunkAsString = '', callback = null) {
    this.#isSyncParsingEnabled = false;
    this.#currentTokenChunk = 0;
    this.#currentTokenIndex = 0;
    // Note: we are overwriting the input on each stream call
    // This is fine because we don't want to parse previous chunks
    this.#input = chunkAsString;
    this.#lexer = new TapLexer(chunkAsString);

    try {
      this.#tokens = this.#scanTokens();
      this.#parseTokens(callback);
    } catch (error) {
      callback(null, error);
    }
  }

  parseSync(input = '', callback = null) {
    if (typeof input !== 'string' || input === '') {
      return [];
    }

    this.#isSyncParsingEnabled = true;
    this.#input = input;
    this.#lexer = new TapLexer(input);
    this.#tokens = this.#scanTokens();

    this.#parseTokens(callback);

    if (this.#isYAMLBlock) {
      // Looks like we have a non-ending YAML block
      this.#error('Expected end of YAML block');
    }

    // Manually flush the remaining buffered comments and test points
    this._flush();

    return this.#flatAST;
  }

  // Check if the TAP content is semantically valid
  // Note: Validating the TAP content requires the whole AST to be available.
  check() {
    if (this.#isSyncParsingEnabled) {
      return this.#checker.check(this.#flatAST);
    }

    // TODO(@manekinekko): when running in async mode, it doesn't make sense to
    // validate the current chunk. Validation needs to whole AST to be available.
    throw new ERR_TAP_VALIDATION_ERROR(
      'TAP validation is not supported for async parsing',
    );
  }
  // ----------------------------------------------------------------------//
  // --------------------------- Transform API ----------------------------//
  // ----------------------------------------------------------------------//

  processChunk(chunk) {
    const str = this.#lastLine + chunk.toString('utf8');
    const lines = StringPrototypeSplit(str, '\n');
    this.#lastLine = ArrayPrototypePop(lines);

    let chunkAsString = ArrayPrototypeJoin(lines, '\n');
    // Special case where chunk is emitted by a child process
    chunkAsString = StringPrototypeReplaceAll(
      chunkAsString,
      '[out] ',
      '',
    );
    chunkAsString = StringPrototypeReplaceAll(
      chunkAsString,
      '[err] ',
      '',
    );
    if (StringPrototypeEndsWith(chunkAsString, '\n')) {
      chunkAsString = StringPrototypeSlice(chunkAsString, 0, -1);
    }
    if (StringPrototypeEndsWith(chunkAsString, 'EOF')) {
      chunkAsString = StringPrototypeSlice(chunkAsString, 0, -3);
    }

    return chunkAsString;
  }

  _transform(chunk, _encoding, next) {
    const chunkAsString = this.processChunk(chunk);

    if (!chunkAsString) {
      // Ignore empty chunks
      next();
      return;
    }

    this.parse(chunkAsString, (node, error) => {
      if (error) {
        next(error);
        return;
      }

      if (node.kind === TokenKind.EOF) {
        // Emit when the current chunk is fully processed and consumed
        next();
      }
    });
  }

  // Flush the remaining buffered comments and test points
  // This will be called automatically when the stream is closed
  // We also call this method manually when we reach the end of the sync parsing
  _flush(next = null) {
    if (!this.#lastLine) {
      this.#__flushPendingTestPointsAndComments();
      next?.();
      return;
    }
    // Parse the remaining line
    this.parse(this.#lastLine, (node, error) => {
      this.#lastLine = '';

      if (error) {
        next?.(error);
        return;
      }

      if (node.kind === TokenKind.EOF) {
        this.#__flushPendingTestPointsAndComments();
        next?.();
      }
    });
  }

  #__flushPendingTestPointsAndComments() {
    ArrayPrototypeForEach(this.#bufferedTestPoints, (node) => {
      this.#emit(node);
    });
    ArrayPrototypeForEach(this.#bufferedComments, (node) => {
      this.#emit(node);
    });

    // Clean up
    this.#bufferedTestPoints = [];
    this.#bufferedComments = [];
  }

  // ----------------------------------------------------------------------//
  // ----------------------------- Private API ----------------------------//
  // ----------------------------------------------------------------------//

  #scanTokens() {
    return this.#lexer.scan();
  }

  #parseTokens(callback = null) {
    for (let index = 0; index < this.#tokens.length; index++) {
      const chunk = this.#tokens[index];
      this.#parseChunk(chunk);
    }

    callback?.({ kind: TokenKind.EOF });
  }

  #parseChunk(chunk) {
    this.#subTestNestingLevel = this.#getCurrentIndentationLevel(chunk);
    // We compute the current index of the token in the chunk
    // based on the indentation level (number of spaces).
    // We also need to take into account if we are in a YAML block or not.
    // If we are in a YAML block, we compute the current index of the token
    // based on the indentation level of the YAML block (start block).

    if (this.#isYAMLBlock) {
      this.#currentTokenIndex =
        this.#yamlCurrentIndentationLevel *
        this.#kSubtestBlockIndentationFactor;
    } else {
      this.#currentTokenIndex =
        this.#subTestNestingLevel * this.#kSubtestBlockIndentationFactor;
      this.#yamlCurrentIndentationLevel = this.#subTestNestingLevel;
    }

    let node;

    // Parse current chunk
    try {
      node = this.#TAPDocument(chunk);
    } catch {
      node = {
        kind: TokenKind.UNKNOWN,
        node: {
          value: this.#currentChunkAsString,
        },
      };
    }

    // Emit the parsed node to both the stream and the AST
    this.#emitOrBufferCurrentNode(node);

    // Move pointers to the next chunk and reset the current token index
    this.#currentTokenChunk++;
    this.#currentTokenIndex = 0;
  }

  #error(message) {
    const token = this.#currentToken || { value: '', kind: '' };
    // Escape NewLine characters
    if (token.value === '\n') {
      token.value = '\\n';
    }

    throw new ERR_TAP_PARSER_ERROR(
      message,
      `, received "${token.value}" (${token.kind})`,
      token,
      this.#input,
    );
  }

  #peek(shouldSkipBlankTokens = true) {
    if (shouldSkipBlankTokens) {
      this.#skip(TokenKind.WHITESPACE);
    }

    return this.#tokens[this.#currentTokenChunk][this.#currentTokenIndex];
  }

  #next(shouldSkipBlankTokens = true) {
    if (shouldSkipBlankTokens) {
      this.#skip(TokenKind.WHITESPACE);
    }

    if (this.#tokens[this.#currentTokenChunk]) {
      this.#currentToken =
        this.#tokens[this.#currentTokenChunk][this.#currentTokenIndex++];
    } else {
      this.#currentToken = null;
    }

    return this.#currentToken;
  }

  // Skip the provided tokens in the current chunk
  #skip(...tokensToSkip) {
    let token = this.#tokens[this.#currentTokenChunk][this.#currentTokenIndex];
    while (token && ArrayPrototypeIncludes(tokensToSkip, token.kind)) {
      // pre-increment to skip current tokens but make sure we don't advance index on the last iteration
      token = this.#tokens[this.#currentTokenChunk][++this.#currentTokenIndex];
    }
  }

  #readNextLiterals() {
    const literals = [];
    let nextToken = this.#peek(false);

    // Read all literal, numeric, whitespace and escape tokens until we hit a different token
    // or reach end of current chunk
    while (
      nextToken &&
      ArrayPrototypeIncludes(
        [
          TokenKind.LITERAL,
          TokenKind.NUMERIC,
          TokenKind.DASH,
          TokenKind.PLUS,
          TokenKind.WHITESPACE,
          TokenKind.ESCAPE,
        ],
        nextToken.kind,
      )
    ) {
      const word = this.#next(false).value;

      // Don't output escaped characters
      if (nextToken.kind !== TokenKind.ESCAPE) {
        ArrayPrototypePush(literals, word);
      }

      nextToken = this.#peek(false);
    }

    return ArrayPrototypeJoin(literals, '');
  }

  #countLeadingSpacesInCurrentChunk(chunk) {
    // Count the number of whitespace tokens in the chunk, starting from the first token
    let whitespaceCount = 0;
    while (chunk?.[whitespaceCount]?.kind === TokenKind.WHITESPACE) {
      whitespaceCount++;
    }
    return whitespaceCount;
  }

  #addDiagnosticsToLastTestPoint(currentNode) {
    const { length, [length - 1]: lastTestPoint } = this.#bufferedTestPoints;

    // Diagnostic nodes are only added to Test points of the same nesting level
    if (lastTestPoint && lastTestPoint.nesting === currentNode.nesting) {
      lastTestPoint.node.time = this.#lastTestPointDetails.duration;

      // TODO(@manekinekko): figure out where to put the other diagnostic properties
      // See https://github.com/nodejs/node/pull/44952
      lastTestPoint.node.diagnostics ||= [];

      ArrayPrototypeForEach(currentNode.node.diagnostics, (diagnostic) => {
        // Avoid adding empty diagnostics
        if (diagnostic) {
          ArrayPrototypePush(lastTestPoint.node.diagnostics, diagnostic);
        }
      });

      this.#bufferedTestPoints = [];
    }

    return lastTestPoint;
  }

  #flushBufferedTestPointNode(shouldClearBuffer = true) {
    if (this.#bufferedTestPoints.length > 0) {
      this.#emit(this.#bufferedTestPoints[0]);

      if (shouldClearBuffer) {
        this.#bufferedTestPoints = [];
      }
    }
  }

  #addCommentsToCurrentNode(currentNode) {
    if (this.#bufferedComments.length > 0) {
      currentNode.comments = ArrayPrototypeMap(
        this.#bufferedComments,
        (c) => c.node.comment,
      );
      this.#bufferedComments = [];
    }

    return currentNode;
  }

  #flushBufferedComments(shouldClearBuffer = true) {
    if (this.#bufferedComments.length > 0) {
      ArrayPrototypeForEach(this.#bufferedComments, (node) => {
        this.#emit(node);
      });

      if (shouldClearBuffer) {
        this.#bufferedComments = [];
      }
    }
  }

  #getCurrentIndentationLevel(chunk) {
    const whitespaceCount = this.#countLeadingSpacesInCurrentChunk(chunk);
    return (whitespaceCount / this.#kSubtestBlockIndentationFactor) | 0;
  }

  #emit(node) {
    if (node.kind !== TokenKind.EOF) {
      ArrayPrototypePush(this.#flatAST, node);
      this.push({
        __proto__: null,
        ...node,
      });
    }
  }

  #emitOrBufferCurrentNode(currentNode) {
    currentNode = {
      ...currentNode,
      nesting: this.#subTestNestingLevel,
      lexeme: this.#currentChunkAsString,
    };

    switch (currentNode.kind) {
      // Emit these nodes
      case TokenKind.UNKNOWN:
        if (!currentNode.node.value) {
          // Ignore unrecognized and empty nodes
          break;
        }
        // falls through

      case TokenKind.TAP_PLAN:
      case TokenKind.TAP_PRAGMA:
      case TokenKind.TAP_VERSION:
      case TokenKind.TAP_BAIL_OUT:
      case TokenKind.TAP_SUBTEST_POINT:
        // Check if we have a buffered test point, and if so, emit it
        this.#flushBufferedTestPointNode();

        // If we have buffered comments, add them to the current node
        currentNode = this.#addCommentsToCurrentNode(currentNode);

        // Emit the current node
        this.#emit(currentNode);
        break;

      // By default, we buffer the next test point node in case we have a diagnostic
      // to add to it in the next iteration
      // Note: in case we hit and EOF, we flush the comments buffer (see _flush())
      case TokenKind.TAP_TEST_POINT:
        // In case of an already buffered test point, we flush it and buffer the current one
        // Because diagnostic nodes are only added to the last processed test point
        this.#flushBufferedTestPointNode();

        // Buffer this node (and also add any pending comments to it)
        ArrayPrototypePush(
          this.#bufferedTestPoints,
          this.#addCommentsToCurrentNode(currentNode),
        );
        break;

      // Keep buffering comments until we hit a non-comment node, then add them to the that node
      // Note: in case we hit and EOF, we flush the comments buffer (see _flush())
      case TokenKind.COMMENT:
        ArrayPrototypePush(this.#bufferedComments, currentNode);
        break;

      // Diagnostic nodes are added to Test points of the same nesting level
      case TokenKind.TAP_YAML_END:
        // Emit either the last updated test point (w/ diagnostics) or the current diagnostics node alone
        this.#emit(
          this.#addDiagnosticsToLastTestPoint(currentNode) || currentNode,
        );
        break;

      // In case we hit an EOF, we emit it to indicate the end of the stream
      case TokenKind.EOF:
        this.#emit(currentNode);
        break;
    }
  }

  #serializeChunk(chunk) {
    return ArrayPrototypeJoin(
      ArrayPrototypeMap(
        // Exclude NewLine and EOF tokens
        ArrayPrototypeFilter(
          chunk,
          (token) =>
            token.kind !== TokenKind.NEWLINE && token.kind !== TokenKind.EOF,
        ),
        (token) => token.value,
      ),
      '',
    );
  }

  // --------------------------------------------------------------------------//
  // ------------------------------ Parser rules ------------------------------//
  // --------------------------------------------------------------------------//

  // TAPDocument := Version Plan Body | Version Body Plan
  #TAPDocument(tokenChunks) {
    this.#currentChunkAsString = this.#serializeChunk(tokenChunks);
    const firstToken = this.#peek(false);

    if (firstToken) {
      const { kind } = firstToken;

      switch (kind) {
        case TokenKind.TAP:
          return this.#Version();
        case TokenKind.NUMERIC:
          return this.#Plan();
        case TokenKind.TAP_TEST_OK:
        case TokenKind.TAP_TEST_NOTOK:
          return this.#TestPoint();
        case TokenKind.COMMENT:
        case TokenKind.HASH:
          return this.#Comment();
        case TokenKind.TAP_PRAGMA:
          return this.#Pragma();
        case TokenKind.WHITESPACE:
          return this.#YAMLBlock();
        case TokenKind.LITERAL:
          // Check for "Bail out!" literal (case insensitive)
          if (
            RegExpPrototypeExec(/^Bail\s+out!/i, this.#currentChunkAsString)
          ) {
            return this.#Bailout();
          } else if (this.#isYAMLBlock) {
            return this.#YAMLBlock();
          }

          // Read token because error needs the last token details
          this.#next(false);
          this.#error('Expected a valid token');

          break;
        case TokenKind.EOF:
          return firstToken;

        case TokenKind.NEWLINE:
          // Consume and ignore NewLine token
          return this.#next(false);
        default:
          // Read token because error needs the last token details
          this.#next(false);
          this.#error('Expected a valid token');
      }
    }

    const node = {
      kind: TokenKind.UNKNOWN,
      node: {
        value: this.#currentChunkAsString,
      },
    };

    // We make sure the emitted node has the same shape
    // both in sync and async parsing (for the stream interface)
    return node;
  }

  // ----------------Version----------------
  // Version := "TAP version Number\n"
  #Version() {
    const tapToken = this.#peek();

    if (tapToken.kind === TokenKind.TAP) {
      this.#next(); // Consume the TAP token
    } else {
      this.#error('Expected "TAP" keyword');
    }

    const versionToken = this.#peek();
    if (versionToken?.kind === TokenKind.TAP_VERSION) {
      this.#next(); // Consume the version token
    } else {
      this.#error('Expected "version" keyword');
    }

    const numberToken = this.#peek();
    if (numberToken?.kind === TokenKind.NUMERIC) {
      const version = this.#next().value;
      const node = { kind: TokenKind.TAP_VERSION, node: { version } };
      return node;
    }
    this.#error('Expected a version number');
  }

  // ----------------Plan----------------
  // Plan := "1.." (Number) (" # " Reason)? "\n"
  #Plan() {
    // Even if specs mention plan starts at 1, we need to make sure we read the plan start value
    // in case of a missing or invalid plan start value
    const planStart = this.#next();

    if (planStart.kind !== TokenKind.NUMERIC) {
      this.#error('Expected a plan start count');
    }

    const planToken = this.#next();
    if (planToken?.kind !== TokenKind.TAP_PLAN) {
      this.#error('Expected ".." symbol');
    }

    const planEnd = this.#next();
    if (planEnd?.kind !== TokenKind.NUMERIC) {
      this.#error('Expected a plan end count');
    }

    const plan = {
      start: planStart.value,
      end: planEnd.value,
    };

    // Read optional reason
    const hashToken = this.#peek();
    if (hashToken) {
      if (hashToken.kind === TokenKind.HASH) {
        this.#next(); // skip hash
        plan.reason = StringPrototypeTrim(this.#readNextLiterals());
      } else if (hashToken.kind === TokenKind.LITERAL) {
        this.#error('Expected "#" symbol before a reason');
      }
    }

    const node = {
      kind: TokenKind.TAP_PLAN,
      node: plan,
    };

    return node;
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
  #TestPoint() {
    const notToken = this.#peek();
    let isTestFailed = false;

    if (notToken.kind === TokenKind.TAP_TEST_NOTOK) {
      this.#next(); // skip "not" token
      isTestFailed = true;
    }

    const okToken = this.#next();
    if (okToken.kind !== TokenKind.TAP_TEST_OK) {
      this.#error('Expected "ok" or "not ok" keyword');
    }

    // Read optional test number
    let numberToken = this.#peek();
    if (numberToken && numberToken.kind === TokenKind.NUMERIC) {
      numberToken = this.#next().value;
    } else {
      numberToken = ''; // Set an empty ID to indicate that the test hasn't provider an ID
    }

    const test = {
      // Output both failed and passed properties to make it easier for the checker to detect the test status
      status: {
        fail: isTestFailed,
        pass: !isTestFailed,
        todo: false,
        skip: false,
      },
      id: numberToken,
      description: '',
      reason: '',
      time: 0,
      diagnostics: [],
    };

    // Read optional description prefix " - "
    const descriptionDashToken = this.#peek();
    if (descriptionDashToken && descriptionDashToken.kind === TokenKind.DASH) {
      this.#next(); // skip dash
    }

    // Read optional description
    if (this.#peek()) {
      const description = StringPrototypeTrim(this.#readNextLiterals());
      if (description) {
        test.description = description;
      }
    }

    // Read optional directive and reason
    const hashToken = this.#peek();
    if (hashToken && hashToken.kind === TokenKind.HASH) {
      this.#next(); // skip hash
    }

    let todoOrSkipToken = this.#peek();
    if (todoOrSkipToken && todoOrSkipToken.kind === TokenKind.LITERAL) {
      if (RegExpPrototypeExec(/todo/i, todoOrSkipToken.value)) {
        todoOrSkipToken = 'todo';
        this.#next(); // skip token
      } else if (RegExpPrototypeExec(/skip/i, todoOrSkipToken.value)) {
        todoOrSkipToken = 'skip';
        this.#next(); // skip token
      }
    }

    const reason = StringPrototypeTrim(this.#readNextLiterals());
    if (todoOrSkipToken) {
      if (reason) {
        test.reason = reason;
      }

      test.status.todo = todoOrSkipToken === 'todo';
      test.status.skip = todoOrSkipToken === 'skip';
    }

    const node = {
      kind: TokenKind.TAP_TEST_POINT,
      node: test,
    };

    return node;
  }

  // ----------------Bailout----------------
  // BailOut := "Bail out!" (" " Reason)? "\n"
  #Bailout() {
    this.#next(); // skip "Bail"
    this.#next(); // skip "out!"

    // Read optional reason
    const hashToken = this.#peek();
    if (hashToken && hashToken.kind === TokenKind.HASH) {
      this.#next(); // skip hash
    }

    const reason = StringPrototypeTrim(this.#readNextLiterals());

    const node = {
      kind: TokenKind.TAP_BAIL_OUT,
      node: { bailout: true, reason },
    };

    return node;
  }

  // ----------------Comment----------------
  // Comment := ^ (" ")* "#" [^\n]* "\n"
  #Comment() {
    const commentToken = this.#next();
    if (
      commentToken.kind !== TokenKind.COMMENT &&
      commentToken.kind !== TokenKind.HASH
    ) {
      this.#error('Expected "#" symbol');
    }

    const commentContent = this.#peek();
    if (commentContent) {
      if (RegExpPrototypeExec(/^Subtest:/i, commentContent.value) !== null) {
        this.#next(); // skip subtest keyword
        const name = StringPrototypeTrim(this.#readNextLiterals());
        const node = {
          kind: TokenKind.TAP_SUBTEST_POINT,
          node: {
            name,
          },
        };

        return node;
      }

      const comment = StringPrototypeTrim(this.#readNextLiterals());
      const node = {
        kind: TokenKind.COMMENT,
        node: { comment },
      };

      return node;
    }

    // If there is no comment content, then we ignore the current node
  }

  // ----------------YAMLBlock----------------
  // YAMLBlock := "  ---\n" (YAMLLine)* "  ...\n"
  #YAMLBlock() {
    const space1 = this.#peek(false);
    if (space1 && space1.kind === TokenKind.WHITESPACE) {
      this.#next(false); // skip 1st space
    }

    const space2 = this.#peek(false);
    if (space2 && space2.kind === TokenKind.WHITESPACE) {
      this.#next(false); // skip 2nd space
    }

    const yamlBlockSymbol = this.#peek(false);

    if (yamlBlockSymbol.kind === TokenKind.WHITESPACE) {
      if (this.#isYAMLBlock === false) {
        this.#next(false); // skip 3rd space
        this.#error('Expected valid YAML indentation (2 spaces)');
      }
    }

    if (yamlBlockSymbol.kind === TokenKind.TAP_YAML_START) {
      if (this.#isYAMLBlock) {
        // Looks like we have another YAML start block, but we didn't close the previous one
        this.#error('Unexpected YAML start marker');
      }

      this.#isYAMLBlock = true;
      this.#yamlCurrentIndentationLevel = this.#subTestNestingLevel;
      this.#lastTestPointDetails = {};

      // Consume the YAML start marker
      this.#next(false); // skip "---"

      // No need to pass this token to the stream interface
      return;
    } else if (yamlBlockSymbol.kind === TokenKind.TAP_YAML_END) {
      this.#next(false); // skip "..."

      if (!this.#isYAMLBlock) {
        // Looks like we have an YAML end block, but we didn't encounter any YAML start marker
        this.#error('Unexpected YAML end marker');
      }

      this.#isYAMLBlock = false;

      const diagnostics = this.#yamlBlockBuffer;
      this.#yamlBlockBuffer = []; // Free the buffer for the next YAML block

      const node = {
        kind: TokenKind.TAP_YAML_END,
        node: {
          diagnostics,
        },
      };

      return node;
    }

    if (this.#isYAMLBlock) {
      this.#YAMLLine();
    } else {
      return {
        kind: TokenKind.UNKNOWN,
        node: {
          value: yamlBlockSymbol.value,
        },
      };
    }
  }

  // ----------------YAMLLine----------------
  // YAMLLine := "  " (YAML)* "\n"
  #YAMLLine() {
    const yamlLiteral = this.#readNextLiterals();
    const { 0: key, 1: value } = StringPrototypeSplit(yamlLiteral, ':', 2);

    // Note that this.#lastTestPointDetails has been cleared when we encounter a YAML start marker

    switch (key) {
      case 'duration_ms':
        this.#lastTestPointDetails.duration = Number(value);
        break;
      // Below are diagnostic properties introduced in https://github.com/nodejs/node/pull/44952
      case 'expected':
        this.#lastTestPointDetails.expected = Boolean(value);
        break;
      case 'actual':
        this.#lastTestPointDetails.actual = Boolean(value);
        break;
      case 'operator':
        this.#lastTestPointDetails.operator = String(value);
        break;
    }

    ArrayPrototypePush(this.#yamlBlockBuffer, yamlLiteral);
  }

  // ----------------PRAGMA----------------
  // Pragma := "pragma " [+-] PragmaKey "\n"
  // PragmaKey := ([a-zA-Z0-9_-])+
  // TODO(@manekinekko): pragmas are parsed but not used yet! TapChecker() should take care of that.
  #Pragma() {
    const pragmaToken = this.#next();
    if (pragmaToken.kind !== TokenKind.TAP_PRAGMA) {
      this.#error('Expected "pragma" keyword');
    }

    const pragmas = {};

    let nextToken = this.#peek();
    while (
      nextToken &&
      ArrayPrototypeIncludes(
        [TokenKind.NEWLINE, TokenKind.EOF, TokenKind.EOL],
        nextToken.kind,
      ) === false
    ) {
      let isEnabled = true;
      const pragmaKeySign = this.#next();
      if (pragmaKeySign.kind === TokenKind.PLUS) {
        isEnabled = true;
      } else if (pragmaKeySign.kind === TokenKind.DASH) {
        isEnabled = false;
      } else {
        this.#error('Expected "+" or "-" before pragma keys');
      }

      const pragmaKeyToken = this.#peek();
      if (pragmaKeyToken.kind !== TokenKind.LITERAL) {
        this.#error('Expected pragma key');
      }

      let pragmaKey = this.#next().value;

      // In some cases, pragma key can be followed by a comma separator,
      // so we need to remove it
      pragmaKey = StringPrototypeReplaceAll(pragmaKey, ',', '');

      pragmas[pragmaKey] = isEnabled;
      nextToken = this.#peek();
    }

    const node = {
      kind: TokenKind.TAP_PRAGMA,
      node: {
        pragmas,
      },
    };

    return node;
  }
}

module.exports = { TapParser };
