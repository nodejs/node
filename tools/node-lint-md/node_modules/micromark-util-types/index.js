/**
 * @typedef {number|null} Code
 *   A character code.
 *
 *   This is often the same as what `String#charCodeAt()` yields but micromark
 *   adds meaning to certain other values.
 *
 *   `null` represents the end of the input stream (called eof).
 *   Negative integers are used instead of certain sequences of characters (such
 *   as line endings and tabs).
 *
 * @typedef {Code|string} Chunk
 *   A chunk is either a character code or a slice of a buffer in the form of a
 *   string.
 *
 *   Chunks are used because strings are more efficient storage that character
 *   codes, but limited in what they can represent.
 *
 * @typedef {'document'|'flow'|'content'|'text'|'string'} ContentType
 *   Enumeration of the content types.
 *
 *   Technically `document` is also a content type, which includes containers
 *   (lists, block quotes) and flow.
 *   As `ContentType` is used on tokens to define the type of subcontent but
 *   `document` is the highest level of content, so it’s not listed here.
 *
 *   Containers in markdown come from the margin and include more constructs
 *   on the lines that define them.
 *   Take for example a block quote with a paragraph inside it (such as
 *   `> asd`).
 *
 *   `flow` represents the sections, such as headings, code, and content, which
 *   is also parsed per line
 *   An example is HTML, which has a certain starting condition (such as
 *   `<script>` on its own line), then continues for a while, until an end
 *   condition is found (such as `</style>`).
 *   If that line with an end condition is never found, that flow goes until
 *   the end.
 *
 *   `content` is zero or more definitions, and then zero or one paragraph.
 *   It’s a weird one, and needed to make certain edge cases around definitions
 *   spec compliant.
 *   Definitions are unlike other things in markdown, in that they behave like
 *   `text` in that they can contain arbitrary line endings, but *have* to end
 *   at a line ending.
 *   If they end in something else, the whole definition instead is seen as a
 *   paragraph.
 *
 *   The content in markdown first needs to be parsed up to this level to
 *   figure out which things are defined, for the whole document, before
 *   continuing on with `text`, as whether a link or image reference forms or
 *   not depends on whether it’s defined.
 *   This unfortunately prevents a true streaming markdown to HTML compiler.
 *
 *   `text` contains phrasing content such as attention (emphasis, strong),
 *   media (links, images), and actual text.
 *
 *   `string` is a limited `text` like content type which only allows character
 *   references and character escapes.
 *   It exists in things such as identifiers (media references, definitions),
 *   titles, or URLs.
 *
 * @typedef Point
 *   A location in the document (`line`/`column`/`offset`) and chunk (`_index`,
 *   `_bufferIndex`).
 *
 *   `_bufferIndex` is `-1` when `_index` points to a code chunk and it’s a
 *   non-negative integer when pointing to a string chunk.
 *
 *   The interface for the location in the document comes from unist `Point`:
 *   <https://github.com/syntax-tree/unist#point>
 * @property {number} line
 *   1-indexed line number
 * @property {number} column
 *   1-indexed column number
 * @property {number} offset
 *   0-indexed position in the document
 * @property {number} _index
 *   Position in a list of chunks
 * @property {number} _bufferIndex
 *   Position in a string chunk (or `-1` when pointing to a numeric chunk)
 *
 * @typedef Token
 *   A token: a span of chunks.
 *   Tokens are what the core of micromark produces: the built in HTML compiler
 *   or other tools can turn them into different things.
 *
 *   Tokens are essentially names attached to a slice of chunks, such as
 *   `lineEndingBlank` for certain line endings, or `codeFenced` for a whole
 *   fenced code.
 *
 *   Sometimes, more info is attached to tokens, such as `_open` and `_close`
 *   by `attention` (strong, emphasis) to signal whether the sequence can open
 *   or close an attention run.
 *
 *   Linked tokens are used because outer constructs are parsed first.
 *   Take for example:
 *
 *   ```markdown
 *   > *a
 *     b*.
 *   ```
 *
 *   1.  The block quote marker and the space after it is parsed first
 *   2.  The rest of the line is a `chunkFlow` token
 *   3.  The two spaces on the second line are a `linePrefix`
 *   4.  The rest of the line is another `chunkFlow` token
 *
 *   The two `chunkFlow` tokens are linked together.
 *   The chunks they span are then passed through the flow tokenizer.
 *
 * @property {string} type
 * @property {Point} start
 * @property {Point} end
 * @property {Token} [previous]
 *   The previous token in a list of linked tokens.
 * @property {Token} [next]
 *   The next token in a list of linked tokens
 * @property {ContentType} [contentType]
 *   Declares a token as having content of a certain type.
 * @property {TokenizeContext} [_tokenizer]
 *   Used when dealing with linked tokens.
 *   A child tokenizer is needed to tokenize them, which is stored on those
 *   tokens.
 * @property {boolean} [_open]
 *   A marker used to parse attention, depending on the characters before
 *   sequences (`**`), the sequence can open, close, both, or none
 * @property {boolean} [_close]
 *   A marker used to parse attention, depending on the characters after
 *   sequences (`**`), the sequence can open, close, both, or none
 * @property {boolean} [_isInFirstContentOfListItem]
 *   A boolean used internally to figure out if a token is in the first content
 *   of a list item construct.
 * @property {boolean} [_container]
 *   A boolean used internally to figure out if a token is a container token.
 * @property {boolean} [_loose]
 *   A boolean used internally to figure out if a list is loose or not.
 * @property {boolean} [_inactive]
 *   A boolean used internally to figure out if a link opening can’t be used
 *   (because links in links are incorrect).
 * @property {boolean} [_balanced]
 *   A boolean used internally to figure out if a link opening is balanced: it’s
 *   not a link opening but has a balanced closing.
 *
 * @typedef {['enter'|'exit', Token, TokenizeContext]} Event
 *   An event is the start or end of a token amongst other events.
 *   Tokens can “contain” other tokens, even though they are stored in a flat
 *   list, through `enter`ing before them, and `exit`ing after them.
 *
 * @callback Enter
 *   Open a token.
 * @param {string} type
 *   Token to enter.
 * @param {Record<string, unknown>} [fields]
 *   Fields to patch on the token
 * @returns {Token}
 *
 * @callback Exit
 *   Close a token.
 * @param {string} type
 *   Token to close.
 *   Should match the current open token.
 * @returns {Token}
 *
 * @callback Consume
 *   Deal with the character and move to the next.
 * @param {Code} code
 *   Code that was given to the state function
 * @returns {void}
 *
 * @callback Attempt
 *   Attempt deals with several values, and tries to parse according to those
 *   values.
 *   If a value resulted in `ok`, it worked, the tokens that were made are used,
 *   and `returnState` is switched to.
 *   If the result is `nok`, the attempt failed, so we revert to the original
 *   state, and `bogusState` is used.
 * @param {Construct|Construct[]|ConstructRecord} construct
 * @param {State} returnState
 * @param {State} [bogusState]
 * @returns {(code: Code) => void}
 *
 * @typedef Effects
 *   A context object to transition the state machine.
 * @property {Enter} enter
 *   Start a new token.
 * @property {Exit} exit
 *   End a started token.
 * @property {Consume} consume
 *   Deal with the character and move to the next.
 * @property {Attempt} attempt
 *   Try to tokenize a construct.
 * @property {Attempt} interrupt
 *   Interrupt is used for stuff right after a line of content.
 * @property {Attempt} check
 *   Attempt, then revert.
 *
 * @callback State
 *   The main unit in the state machine: a function that gets a character code
 *   and has certain effects.
 *
 *   A state function should return another function: the next
 *   state-as-a-function to go to.
 *
 *   But there is one case where they return void: for the eof character code
 *   (at the end of a value).
 *   The reason being: well, there isn’t any state that makes sense, so void
 *   works well.
 *   Practically that has also helped: if for some reason it was a mistake, then
 *   an exception is throw because there is no next function, meaning it
 *   surfaces early.
 * @param {Code} code
 * @returns {State|void}
 *
 * @callback Resolver
 *   A resolver handles and cleans events coming from `tokenize`.
 * @param {Event[]} events
 *   List of events.
 * @param {TokenizeContext} context
 *   Context.
 * @returns {Event[]}
 *
 * @typedef {(this: TokenizeContext, effects: Effects, ok: State, nok: State) => State} Tokenizer
 *   A tokenize function sets up a state machine to handle character codes streaming in.
 *
 * @typedef {(this: TokenizeContext, effects: Effects) => State} Initializer
 *   Like a tokenizer, but without `ok` or `nok`.
 *
 * @typedef {(this: TokenizeContext, effects: Effects) => void} Exiter
 *   Like a tokenizer, but without `ok` or `nok`, and returning void.
 *   This is the final hook when a container must be closed.
 *
 * @typedef {(this: TokenizeContext, code: Code) => boolean} Previous
 *   Guard whether `code` can come before the construct.
 *   In certain cases a construct can hook into many potential start characters.
 *   Instead of setting up an attempt to parse that construct for most
 *   characters, this is a speedy way to reduce that.
 *
 * @typedef Construct
 *   An object descibing how to parse a markdown construct.
 * @property {Tokenizer} tokenize
 * @property {Previous} [previous]
 *   Guard whether the previous character can come before the construct
 * @property {Construct} [continuation]
 *   For containers, a continuation construct.
 * @property {Exiter} [exit]
 *   For containers, a final hook.
 * @property {string} [name]
 *   Name of the construct, used to toggle constructs off.
 *   Named constructs must not be `partial`.
 * @property {boolean} [partial=false]
 *   Whether this construct represents a partial construct.
 *   Partial constructs must not have a `name`.
 * @property {Resolver} [resolve]
 *   Resolve the events parsed by `tokenize`.
 *
 *   For example, if we’re currently parsing a link title and this construct
 *   parses character references, then `resolve` is called with the events
 *   ranging from the start to the end of a character reference each time one is
 *   found.
 * @property {Resolver} [resolveTo]
 *   Resolve the events from the start of the content (which includes other
 *   constructs) to the last one parsed by `tokenize`.
 *
 *   For example, if we’re currently parsing a link title and this construct
 *   parses character references, then `resolveTo` is called with the events
 *   ranging from the start of the link title to the end of a character
 *   reference each time one is found.
 * @property {Resolver} [resolveAll]
 *   Resolve all events when the content is complete, from the start to the end.
 *   Only used if `tokenize` is successful once in the content.
 *
 *   For example, if we’re currently parsing a link title and this construct
 *   parses character references, then `resolveAll` is called *if* at least one
 *   character reference is found, ranging from the start to the end of the link
 *   title to the end.
 * @property {boolean} [concrete]
 *   Concrete constructs cannot be interrupted by more containers.
 *
 *   For example, when parsing the document (containers, such as block quotes
 *   and lists) and this construct is parsing fenced code:
 *
 *   ````markdown
 *   > ```js
 *   > - list?
 *   ````
 *
 *   …then `- list?` cannot form if this fenced code construct is concrete.
 *
 *   An example of a construct that is not concrete is a GFM table:
 *
 *   ````markdown
 *   | a |
 *   | - |
 *   > | b |
 *   ````
 *
 *   …`b` is not part of the table.
 * @property {'before'|'after'} [add='before']
 *   Whether the construct, when in a `ConstructRecord`, precedes over existing
 *   constructs for the same character code when merged
 *   The default is that new constructs precede over existing ones.
 *
 * @typedef {Construct & {tokenize: Initializer}} InitialConstruct
 *   Like a construct, but `tokenize` does not accept `ok` or `nok`.
 *
 * @typedef {Record<string, undefined|Construct|Construct[]>} ConstructRecord
 *   Several constructs, mapped from their initial codes.
 *
 * @typedef TokenizeContext
 *   A context object that helps w/ tokenizing markdown constructs.
 * @property {Code} previous
 *   The previous code.
 * @property {Code} code
 *   Current code.
 * @property {boolean} [interrupt]
 *   Whether we’re currently interrupting.
 *   Take for example:
 *
 *   ```markdown
 *   a
 *   # b
 *   ```
 *
 *   At 2:1, we’re “interrupting”.
 * @property {Construct} [currentConstruct]
 *   The current construct.
 *   Constructs that are not `partial` are set here.
 * @property {Record<string, unknown> & {_closeFlow?: boolean}} [containerState]
 *   Info set when parsing containers.
 *   Containers are parsed in separate phases: their first line (`tokenize`),
 *   continued lines (`continuation.tokenize`), and finally `exit`.
 *   This record can be used to store some information between these hooks.
 * @property {Event[]} events
 *   Current list of events.
 * @property {ParseContext} parser
 *   The relevant parsing context.
 * @property {(token: Pick<Token, 'start'|'end'>) => Chunk[]} sliceStream
 *   Get the chunks that span a token (or location).
 * @property {(token: Pick<Token, 'start'|'end'>, expandTabs?: boolean) => string} sliceSerialize
 *   Get the source text that spans a token (or location).
 * @property {() => Point} now
 *   Get the current place.
 * @property {(value: Point) => void} defineSkip
 *   Define a skip: as containers (block quotes, lists), “nibble” a prefix from
 *   the margins, where a line starts after that prefix is defined here.
 *   When the tokenizers moves after consuming a line ending corresponding to
 *   the line number in the given point, the tokenizer shifts past the prefix
 *   based on the column in the shifted point.
 * @property {(slice: Chunk[]) => Event[]} write
 *   Write a slice of chunks.
 *   The eof code (`null`) can be used to signal the end of the stream.
 * @property {boolean} [_gfmTasklistFirstContentOfListItem]
 *   Internal boolean shared with `micromark-extension-gfm-task-list-item` to
 *   signal whether the tokenizer is tokenizing the first content of a list item
 *   construct.
 */

/**
 * @typedef {'ascii'|'utf8'|'utf-8'|'utf16le'|'ucs2'|'ucs-2'|'base64'|'latin1'|'binary'|'hex'} Encoding
 *   Encodings supported by the buffer class.
 *   This is a copy of the typing from Node, copied to prevent Node globals from
 *   being needed.
 *   Copied from: <https://github.com/DefinitelyTyped/DefinitelyTyped/blob/a2bc1d8/types/node/globals.d.ts#L174>
 *
 * @typedef {string|Uint8Array} Value
 *   Contents of the file.
 *   Can either be text, or a `Buffer` like structure.
 *   This does not directly use type `Buffer`, because it can also be used in a
 *   browser context.
 *   Instead this leverages `Uint8Array` which is the base type for `Buffer`,
 *   and a native JavaScript construct.
 */

/**
 * @typedef _ExtensionFields
 * @property {ConstructRecord} document
 * @property {ConstructRecord} contentInitial
 * @property {ConstructRecord} flowInitial
 * @property {ConstructRecord} flow
 * @property {ConstructRecord} string
 * @property {ConstructRecord} text
 * @property {{null?: string[]}} disable
 * @property {{null?: Pick<Construct, 'resolveAll'>[]}} insideSpan
 * @property {{null?: Code[]}} attentionMarkers
 *
 * @typedef _NormalizedExtensionFields
 * @property {Record<string, Construct[]>} document
 * @property {Record<string, Construct[]>} contentInitial
 * @property {Record<string, Construct[]>} flowInitial
 * @property {Record<string, Construct[]>} flow
 * @property {Record<string, Construct[]>} string
 * @property {Record<string, Construct[]>} text
 * @property {{null: string[]}} disable
 * @property {{null: Pick<Construct, 'resolveAll'>[]}} insideSpan
 * @property {{null: Code[]}} attentionMarkers
 *
 * @typedef {Record<string, Record<string, unknown>> & Partial<_ExtensionFields>} Extension
 *   A syntax extension changes how markdown is tokenized.
 *   See: <https://github.com/micromark/micromark#syntaxextension>
 *
 * @typedef {Record<string, Record<string, unknown[]>> & _NormalizedExtensionFields} FullNormalizedExtension
 * @typedef {Record<string, Record<string, unknown[]|undefined>> & Partial<_NormalizedExtensionFields>} NormalizedExtension
 *
 * @callback Create
 *   Set up a tokenizer for a content type.
 * @param {Omit<Point, '_index'|'_bufferIndex'>} [from]
 * @returns {TokenizeContext}
 *
 * @typedef ParseOptions
 *   Parse options.
 * @property {Extension[]} [extensions] Array of syntax extensions
 *
 * @typedef ParseContext
 *   A context object that helps w/ parsing markdown.
 * @property {FullNormalizedExtension} constructs
 * @property {Create} content
 * @property {Create} document
 * @property {Create} flow
 * @property {Create} string
 * @property {Create} text
 * @property {string[]} defined List of defined identifiers.
 * @property {Record<number, boolean>} lazy
 *   Map of line numbers to whether they are lazy (as opposed to the line before
 *   them).
 *   Take for example:
 *
 *   ```markdown
 *   > a
 *   b
 *   ```
 *
 *   L1 here is not lazy, L2 is.
 */

/**
 * @typedef CompileContext
 *   HTML compiler context
 * @property {CompileOptions} options
 *   Configuration passed by the user.
 * @property {(key: string, value?: unknown) => void} setData
 *   Set data into the key-value store.
 * @property {<K extends string>(key: K) => CompileData[K]} getData
 *   Get data from the key-value store.
 * @property {() => void} lineEndingIfNeeded
 *   Output an extra line ending if the previous value wasn’t EOF/EOL.
 * @property {(value: string) => string} encode
 *   Make a value safe for injection in HTML (except w/ `ignoreEncode`).
 * @property {() => void} buffer
 *   Capture some of the output data.
 * @property {() => string} resume
 *   Stop capturing and access the output data.
 * @property {(value: string) => void} raw
 *   Output raw data.
 * @property {(value: string) => void} tag
 *   Output (parts of) HTML tags.
 * @property {TokenizeContext['sliceSerialize']} sliceSerialize
 *   Get the string value of a token
 *
 * @callback Compile
 *   Serialize micromark events as HTML
 * @param {Event[]} events
 * @returns {string}
 *
 * @typedef {(this: CompileContext, token: Token) => void} Handle
 *   Handle one token
 *
 * @typedef {(this: Omit<CompileContext, 'sliceSerialize'>) => void} DocumentHandle
 *   Handle the whole
 *
 * @typedef {Record<string, Handle> & {null?: DocumentHandle}} Handles
 *   Token types mapping to handles
 *
 * @typedef {Record<string, Record<string, unknown>> & {enter: Handles, exit: Handles}} NormalizedHtmlExtension
 *
 * @typedef {Partial<NormalizedHtmlExtension>} HtmlExtension
 *   An HTML extension changes how markdown tokens are serialized.
 *
 * @typedef _CompileDataFields
 * @property {boolean} lastWasTag
 * @property {boolean} expectFirstItem
 * @property {boolean} slurpOneLineEnding
 * @property {boolean} slurpAllLineEndings
 * @property {boolean} fencedCodeInside
 * @property {number} fencesCount
 * @property {boolean} flowCodeSeenData
 * @property {boolean} ignoreEncode
 * @property {number} headingRank
 * @property {boolean} inCodeText
 * @property {string} characterReferenceType
 * @property {boolean[]} tightStack
 *
 * @typedef {Record<string, unknown> & Partial<_CompileDataFields>} CompileData
 *
 * @typedef CompileOptions
 *   Compile options
 * @property {'\r'|'\n'|'\r\n'} [defaultLineEnding]
 *   Value to use for line endings not in `doc` (`string`, default: first line
 *   ending or `'\n'`).
 *
 *   Generally, micromark copies line endings (`'\r'`, `'\n'`, `'\r\n'`) in the
 *   markdown document over to the compiled HTML.
 *   In some cases, such as `> a`, CommonMark requires that extra line endings
 *   are added: `<blockquote>\n<p>a</p>\n</blockquote>`.
 * @property {boolean} [allowDangerousHtml=false]
 *   Whether to allow embedded HTML (`boolean`, default: `false`).
 * @property {boolean} [allowDangerousProtocol=false]
 *   Whether to allow potentially dangerous protocols in links and images
 *   (`boolean`, default: `false`).
 *   URLs relative to the current protocol are always allowed (such as,
 *   `image.jpg`).
 *   For links, the allowed protocols are `http`, `https`, `irc`, `ircs`,
 *   `mailto`, and `xmpp`.
 *   For images, the allowed protocols are `http` and `https`.
 * @property {HtmlExtension[]} [htmlExtensions=[]]
 *   Array of HTML extensions
 */

/**
 * @typedef {ParseOptions & CompileOptions} Options
 */

export {}
