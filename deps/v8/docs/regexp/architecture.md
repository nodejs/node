# V8 RegExp Architecture

V8's regular expression subsystem (`src/regexp/`) compiles and executes ECMAScript regular expressions across both one-byte (`LATIN1`) and two-byte (`UC16` / UTF-16) strings. Rather than relying on a single execution strategy for all patterns, V8 dispatches regular expressions across **three specialized engines** (`ATOM`, `EXPERIMENTAL`, and `IRREGEXP`) and supports dynamic tier-up from interpreted bytecode to architecture-specific native machine code.

---

## 1. Heap Representation and Sandbox Isolation (`JSRegExp` and `RegExpData`)

To protect V8 against in-sandbox memory corruption, regular expression metadata, bytecode, and code pointers are separated from the JavaScript-visible `JSRegExp` object (`src/objects/js-regexp.h`):

```
In-Sandbox Heap (Main Cage)                TrustedSpace (Outside Main Cage)
+---------------------------+              +------------------------------------+
| JSRegExp                  |              | RegExpData (ExposedTrustedObject)  |
|  - flags_ (Smi)           |  TrustedPtr  |  - type_tag_: ATOM | IRREGEXP      |
|  - last_index (in-object) | -----------> |               | EXPERIMENTAL       |
|  - data_ (TrustedPointer) |              |  - original_source_, escaped_src_  |
+---------------------------+              |  - quick_check_mask_ / _value_     |
                                           |  - quick_check_reject_bitset_[8]   |
                                           +------------------------------------+
                                                   ▲                    ▲
                                                   │                    │
                                    +--------------+----+     +---------+--------------+
                                    | AtomRegExpData    |     | IrRegExpData           |
                                    |  - pattern_       |     |  - latin1_bytecode_    |
                                    +-------------------+     |  - uc16_bytecode_      |
                                                              |  - latin1_code_ (CPT)  |
                                                              |  - uc16_code_ (CPT)    |
                                                              |  - capture_name_map_   |
                                                              |  - ticks_until_tier_up_|
                                                              |  - backtrack_limit_    |
                                                              +------------------------+
```

- **`JSRegExp`**: Lives in the regular V8 heap and holds `lastIndex`, `flags`, and a `TrustedPointerMember<RegExpData, kRegExpDataTrustedPointerTag> data_` resolved via the Isolate's `TrustedPointerTable`.
- **`RegExpData` (`ExposedTrustedObject`)**: Allocated in `TrustedSpace`. Carries `type_tag()` (`RegExpData::Type::ATOM`, `IRREGEXP`, or `EXPERIMENTAL`), `original_source`, `escaped_source`, `flags`, a back-reference `RegExpDataWrapper`, and inline quick-check filter fields (`quick_check_mask_`, `quick_check_value_`, and the 256-bit `quick_check_reject_bitset_` covering all Latin-1 start characters).
- **`AtomRegExpData`**: Subclass of `RegExpData` for `Type::ATOM`; stores the literal `pattern_` `String` to search for.
- **`IrRegExpData`**: Subclass of `RegExpData` used by both `Type::IRREGEXP` and `Type::EXPERIMENTAL`. Because character widths and case-folding rules differ between 1-byte and 2-byte strings, `IrRegExpData` maintains **separate caches for `LATIN1` (one-byte) and `UC16` (two-byte) subjects**:
  - `latin1_bytecode_` and `uc16_bytecode_`: Protected pointers (`ProtectedTaggedMember<TrustedByteArray>`) to compiled bytecode in `TrustedSpace`.
  - `latin1_code_` and `uc16_code_`: Indirect code pointers (`CodePointerMember`, resolved via the `CodePointerTable`) to either a trampoline (`RegExpInterpreterTrampoline` or `RegExpExperimentalTrampoline`) or compiled native `Code`.

---

## 2. The Three RegExp Engines in `RegExp::Compile`

When a regular expression is initialized (`RegExp::Compile` in `src/regexp/regexp.cc`), V8 first checks the `CompilationCache` (`CompilationCacheRegExp`). On a cache miss, `regexp::Parser::ParseRegExpFromHeapString` (`src/regexp/regexp-parser.cc`) parses the pattern into a `regexp::Tree` AST (`src/regexp/regexp-ast.h`) and selects one of three engines:

### 1. `ATOM` (`RegExpData::Type::ATOM`)
- **Selection criteria**: Selected when the parsed AST is a simple unescaped literal or a single `Atom` node (`parse_result.tree->IsAtom()`) with:
  - No captures (`capture_count == 0`),
  - Not case-insensitive (`!IsIgnoreCase(flags)`),
  - Not sticky (`!IsSticky(flags)`), and
  - `!HasFewDifferentCharacters(atom_string)` (if a pattern of length `> 2` has 3x as many characters as distinct characters, e.g. `/aaaaaaa/`, Irregexp's lookahead tables outperform generic substring search).
- **Execution (`RegExpImpl::AtomExecRaw` in `src/regexp/regexp.cc` and CSA fast paths)**: Uses `StringSearch<PChar, SChar>` (`src/strings/string-search.h`), which dispatches between single-character `memchr`, linear search for very short patterns, and **Boyer-Moore-Horspool** / full **Boyer-Moore** string search without any regex state machine overhead.

### 2. `EXPERIMENTAL` (`RegExpData::Type::EXPERIMENTAL` in `src/regexp/experimental/`)
- **Design**: A **linear-time $O(n)$ non-backtracking engine** based on Thompson NFA simulation over a dedicated bytecode (`RegExpInstruction` in `src/regexp/experimental/experimental-bytecode.h`).
- **Eligibility (`ExperimentalRegExp::CanBeHandled`)**: `ExperimentalCompiler::CanBeHandled` walks the `regexp::Tree` AST to verify that the pattern contains no backreferences, no lookarounds, no atomic groups, and bounded quantifier ranges within configurable limits. The result is cached on `IrRegExpData` as `is_linear_executable()`.
- **Selection & Fallback**:
  1. **Explicit `l` flag (`JSRegExp::kLinear` / `--enable-experimental-regexp-engine`)**: Compiles directly with `ExperimentalRegExp::Initialize`; throws a `SyntaxError` (`Error::kNotLinear`) if `!is_linear_executable`.
  2. **Default flag (`--default-to-experimental-regexp-engine`)**: Routes all compatible patterns to `EXPERIMENTAL`.
  3. **Automatic Catastrophic-Backtracking Fallback (`--enable-experimental-regexp-engine-on-excessive-backtracks`)**: When a compatible pattern is compiled with `IRREGEXP`, `SetBacktrackAndExperimentalFallback` sets `backtrack_limit` on `IrRegExpData` to `--regexp-backtracks-before-fallback` (default 50,000) and enables `can_fallback` on the macro assembler. If Irregexp exceeds that backtrack count during execution, it bails out to `Runtime_RegExpExperimentalOneshotExec` -> `RegExp::ExperimentalOneshotExec`, which compiles and finishes the match in linear time using `NfaInterpreter` (`src/regexp/experimental/experimental-interpreter.cc`).

### 3. `IRREGEXP` (`RegExpData::Type::IRREGEXP`)
- **Design**: V8's full-featured backtracking regular expression engine supporting all ECMAScript syntax (captures, named groups, backreferences, positive/negative lookahead and lookbehind, Unicode property escapes, and `v`-mode character sets).
- **Lazy Compilation**: `RegExpImpl::IrregexpInitialize` only allocates the `IrRegExpData` with uninitialized code/bytecode slots. Actual compilation for `LATIN1` or `UC16` happens lazily on the first `exec`/`test` call with a one-byte or two-byte subject string via `RegExp::EnsureFullyCompiled` -> `RegExpImpl::IrregexpPrepare` -> `RegExpImpl::EnsureCompiledIrregexp`.

---

## 3. Dynamic Tier-Up (`--regexp-tier-up`) and Bytecode-to-Native Compilation

Compiling every `IRREGEXP` pattern directly to native machine code wastes memory and compile time on regexes that only run once or twice on short strings. When `--regexp-tier-up` is enabled (the default):

```
First execution on LATIN1 (or UC16) subject
   │
   ├─► If subject.length() >= JSRegExp::kTierUpForSubjectLengthValue (1000)
   │   or global/replace heuristics trigger eager tier-up:
   │      MarkTierUpForNextExec() immediately.
   │
   ▼
Tier 0: Irregexp Bytecode (TrustedByteArray)
   │  CompileIrregexpFromSource(..., CompilationTarget::kBytecode)
   │  - Emits bytecode via RegExpBytecodeGenerator
   │  - Optimizes bytecode via RegExpBytecodePeephole
   │  - Installs RegExpInterpreterTrampoline in re_data->code(is_one_byte)
   │  - Initializes ticks_until_tier_up = --regexp-tier-up-ticks
   ▼
Executed by IrregexpInterpreter (src/regexp/regexp-interpreter.cc)
   │  Each execution (or backtrack tick) decrements ticks_until_tier_up.
   │  When ticks_until_tier_up reaches 0 -> MarkTierUpForNextExec().
   ▼
Tier 1: Native Machine Code (Code)
      CompileIrregexpFromBytecode (when --regexp-assemble-from-bytecode is on)
        or CompileIrregexpFromSource(..., CompilationTarget::kNative)
      - Installs compiled Code in re_data->code(is_one_byte)
      - Clears re_data->bytecode(is_one_byte) to reclaim memory
```

### Peephole Optimization (`RegExpBytecodePeephole`)
When `RegExpBytecodeGenerator` (`src/regexp/regexp-bytecode-generator.cc`) finishes emitting raw bytecode, `RegExpBytecodePeephole::OptimizeBytecode` (`src/regexp/regexp-bytecode-peephole.cc`) scans the instruction stream for common multi-instruction idioms and fuses them into super-bytecodes (such as `SkipUntilChar`, `SkipUntilCharAnd`, `SkipUntilCharPosChecked`, `SkipUntilBitInTable`, `SkipUntilGtOrNotBitInTable`, `SkipUntilOneOfMasked`, and `SkipUntilOneOfMasked3`), fixing up all internal jump targets.

### Assembling Native Code Directly from Bytecode (`CompileIrregexpFromBytecode`)
When `--regexp-assemble-from-bytecode` is enabled, tier-up does **not** need to re-parse the pattern string or rebuild the Irregexp `Node` graph:
- `RegExpImpl::CompileIrregexpFromBytecode` (`src/regexp/regexp.cc`) creates the target architecture's `NativeRegExpMacroAssembler` (`RegExpMacroAssemblerX64`, `RegExpMacroAssemblerARM64`, etc.).
- `regexp::CodeGenerator::Assemble` (`src/regexp/regexp-code-generator.{h,cc}`) iterates over the existing `TrustedByteArray` bytecode and lowers each bytecode instruction directly to `RegExpMacroAssembler` calls.
- Once native `Code` is installed on `IrRegExpData`, `re_data->clear_bytecode(is_one_byte)` frees the bytecode array.

---

## 4. Irregexp Compiler Internals: Graph Nodes, `Trace`, Quick Checks, and Boyer-Moore

When `RegExpImpl::Compile` (`src/regexp/regexp-compiler.cc`) compiles a `regexp::Tree` AST, it first lowers the AST via `ToNode` into a cyclic directed graph of `regexp::Node`s (`src/regexp/regexp-nodes.h`):

### 1. Irregexp Graph Nodes (`src/regexp/regexp-nodes.h`)
| Node Class | Role in Automaton |
|---|---|
| `TextNode` | Matches a sequence of literal atoms (`Atom`) and/or character classes (`ClassRanges`), either forward or backward (`read_backward_` for lookbehind). |
| `ActionNode` | Performs a side effect on registers or the backtrack stack without consuming input: `STORE_POSITION` (captures), `RESTORE_POSITION`, `CLEAR_CAPTURES`, `SET_REGISTER_FOR_LOOP`, `INCREMENT_REGISTER`, `EMPTY_MATCH_CHECK`, `BEGIN_POSITIVE_SUBMATCH`, `BEGIN_NEGATIVE_SUBMATCH`, `POSITIVE_SUBMATCH_SUCCESS`, and `EATS_AT_LEAST`. |
| `ChoiceNode` | Ordered alternation (`a|b|c`) holding a `ZoneList<GuardedAlternative>`. Tries alternatives in order, pushing backtrack continuations for remaining branches. |
| `LoopChoiceNode` | Subclass of `ChoiceNode` representing a quantifier loop (`*`, `+`, `{m,n}`) with a `loop_node()` body alternative and a `continue_node()` exit alternative. Classifies fixed-length greedy loops via `AtomicLoopKind` (`kAtEnd`, `kTotal`, `kBoundary`, `kDisjoint`) and `DrainMode` (`kFull`, `kRetryAtEntry`, `kRestoreOnly`, `kOmit`) to avoid $O(n^2)$ backtracking drains. |
| `NegativeLookaroundChoiceNode` | Subclass of `ChoiceNode` for `(?!...)` and `(?<!...)` where `lookaround_node()` must fail before continuing to `continue_node()`. |
| `AssertionNode` | Zero-width boundary checks: `AT_START` (`^`), `AT_END` (`$`), `AT_BOUNDARY` (`\b`), `AT_NON_BOUNDARY` (`\B`), and `AFTER_NEWLINE`. |
| `BackReferenceNode` | Matches the substring previously captured in `[start_reg_, end_reg_]`. |
| `EndNode` | Terminal node: `ACCEPT` (match succeeded), `BACKTRACK` (match failed), or `NEGATIVE_SUBMATCH_SUCCESS`. |

Before code generation, `Analysis` (`src/regexp/regexp-compiler.cc`) walks the node graph to compute `NodeInfo` (`follows_word_interest`, `follows_newline_interest`, `follows_start_interest`) and `EatsAtLeastInfo` (`from_possibly_start`, `from_not_start`), which records the minimum number of characters any successful match starting at each node must consume.

### 2. `Trace` and Deferred Actions (`src/regexp/regexp-compiler.h`)
Code generation traverses the `Node` graph using an immutable linked chain of `Trace` objects passed to `Node::Emit(Compiler*, Trace*)`:
- **Current Position Offset (`cp_offset_`)**: Instead of emitting an instruction to advance the current input pointer (`cp`) after every character, `TextNode::Emit` advances `trace.cp_offset_` at compile time (`AdvanceCurrentPositionInTrace`) and emits character checks at `cp + cp_offset_`.
- **Bounds-Check Coalescing (`bound_checked_up_to_`) & Preloading (`characters_preloaded_`)**: A single bounds check at the start of a trace (using `EatsAtLeast`) covers multiple subsequent character loads up to `bound_checked_up_to_`.
- **Deferred Actions (`action_`)**: Register updates such as `ActionNode::STORE_POSITION` for capture groups are appended to the `Trace` rather than emitted immediately. If a character check later in the trace fails, no work was wasted writing capture registers that would have to be undone; if the trace succeeds or flushes, `Trace::Flush` emits the deferred actions with their exact relative `cp_offset`s.
- **Direct Backtrack Labels (`backtrack_`)**: Non-trivial traces carry a direct `Label* backtrack_` to jump to on failure, avoiding pushing/popping the runtime backtrack stack until `Trace::Flush` is required (e.g., at loop headers or when `Node::LimitVersions` hits `kMaxCopiesCodeGenerated`).

### 3. `QuickCheckDetails` (Wide Mask-and-Compare & Reject Bitset)
To reject non-matching positions in a single wide memory load:
- `Node::GetQuickCheckDetails` (`src/regexp/regexp-compiler.h`) inspects up to `kMaxPositions = 4` upcoming characters (4 bytes in `LATIN1`, 2 `uc16` characters in `UC16`), computing a combined 32-bit `mask_` and `value_` across all alternatives.
- At runtime, `EmitQuickCheck` loads up to 4 characters at once, computes `(chars & mask) == value`, and immediately skips or backtracks on mismatch. Even when `mask == 0`, `RegExpData::quick_check_reject_bitset_` stores a 256-bit Latin-1 rejection bitmap checked by `RegExpExecInternal` before entering the engine.

### 4. `BoyerMooreLookahead` (`src/regexp/regexp-compiler.h`)
For unanchored patterns (which V8 wraps in an implicit `.*?` search `LoopChoiceNode`) and greedy/non-greedy `.*` loops:
- `Node::FillInBMInfo` populates a `BoyerMooreLookahead` table of up to `kMaxLookaheadForBoyerMoore = 8` positions (`BoyerMoorePositionInfo`, tracking a 128-bit character bitmap mod 128).
- `BoyerMooreLookahead::FindWorthwhileInterval` selects the most selective lookahead window `[from, to]` and `EmitSkipInstructions` (or `BuildSkipTable`) emits a fast skip-table scan (`CheckBitInTable` / SIMD nibble table lookup) that advances the input cursor by `to - from + 1` characters per iteration until a candidate character is found.

---

## 5. Key Files Reference

| File | Purpose |
|---|---|
| `src/objects/js-regexp.{h,cc}` & `js-regexp-inl.h` | `JSRegExp`, `RegExpData`, `AtomRegExpData`, `IrRegExpData`, and `JSRegExpResult` heap definitions. |
| `src/regexp/regexp.{h,cc}` | Top-level `RegExp::Compile`, `RegExp::Exec`, `AtomCompile`/`AtomExec`, `EnsureCompiledIrregexp`, and `CompileIrregexpFromBytecode`. |
| `src/regexp/regexp-parser.{h,cc}` | ECMAScript RegExp parser producing `regexp::Tree` AST nodes. |
| `src/regexp/regexp-ast.{h,cc}` | `regexp::Tree` AST hierarchy (`Alternative`, `Disjunction`, `Atom`, `ClassRanges`, `Quantifier`, `Capture`, `Lookaround`). |
| `src/regexp/regexp-nodes.h` | Automaton graph `Node` classes (`TextNode`, `ActionNode`, `ChoiceNode`, `LoopChoiceNode`, `AssertionNode`) and drain optimizations (`DrainMode`, `AtomicLoopKind`, `ParkedGrant`). |
| `src/regexp/regexp-compiler.{h,cc}` | `Compiler`, `Trace`, `QuickCheckDetails`, `BoyerMooreLookahead`, `Analysis`, and `ToNode` AST-to-graph lowering. |
| `src/regexp/regexp-compiler-tonode.cc` | Lowering of `regexp::Tree` nodes, character classes, case folding, and Unicode surrogates into `regexp::Node` graphs. |
| `src/regexp/regexp-macro-assembler.{h,cc}` | Abstract `RegExpMacroAssembler` interface implemented by the bytecode generator and all native backends (`src/regexp/<arch>/`). |
| `src/regexp/regexp-bytecode-generator.{h,cc}` | `RegExpBytecodeGenerator` emitting Irregexp bytecode. |
| `src/regexp/regexp-bytecode-peephole.{h,cc}` | `RegExpBytecodePeephole` fusing multi-instruction bytecode idioms into `SkipUntil*` super-bytecodes. |
| `src/regexp/regexp-interpreter.{h,cc}` | `IrregexpInterpreter` executing Irregexp bytecode. |
| `src/regexp/regexp-code-generator.{h,cc}` | `regexp::CodeGenerator` assembling native machine code directly from Irregexp bytecode (`--regexp-assemble-from-bytecode`). |
| `src/regexp/experimental/experimental.{h,cc}` | `ExperimentalRegExp`, `ExperimentalCompiler`, and `NfaInterpreter` implementing the linear-time non-backtracking Thompson NFA engine. |
