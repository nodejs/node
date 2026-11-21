'use strict';

// Decoder for the scopes proposal of ECMA-426.
// https://tc39.es/ecma426/branch/proposal-scopes/

const {
  ArrayIsArray,
  ArrayPrototypePop,
  ArrayPrototypePush,
} = primordials;

const {
  StringCharIterator,
  decodeUnsignedVLQ,
  toSigned,
} = require('internal/source_map/vlq');

// https://tc39.es/ecma426/branch/proposal-scopes/#sec-scopes-grammar
const kTagOriginalScopeStart = 0x1; // 'B'
const kTagOriginalScopeEnd = 0x2; // 'C'
const kTagOriginalScopeVariables = 0x3; // 'D'
const kTagGeneratedRangeStart = 0x4; // 'E'
const kTagGeneratedRangeEnd = 0x5; // 'F'
const kTagGeneratedRangeBindings = 0x6; // 'G'
const kTagGeneratedRangeSubRangeBinding = 0x7; // 'H'
const kTagGeneratedRangeCallSite = 0x8; // 'I'
const kTagGeneratedRangeRelativeCallSite = 0x9; // 'J'

// https://tc39.es/ecma426/branch/proposal-scopes/#table-scope-flags
const kScopeFlagHasName = 0x1;
const kScopeFlagHasKind = 0x2;
const kScopeFlagIsStackFrame = 0x4;

// https://tc39.es/ecma426/branch/proposal-scopes/#table-range-flags
const kRangeFlagHasLine = 0x1;
const kRangeFlagHasDefinition = 0x2;
const kRangeFlagIsStackFrame = 0x4;
const kRangeFlagIsHidden = 0x8;

const kStackFrameTypeNone = 'none';
const kStackFrameTypeOriginal = 'original';
const kStackFrameTypeHidden = 'hidden';

/**
 * Decodes the comma separated item at the iterator's position, and moves the
 * iterator past the item and its comma.
 * @param {StringCharIterator} iterator
 * @param {number[]} values receives the VLQUnsignedValue of each Vlq
 * @returns {boolean} false if the item is malformed
 */
function decodeItem(iterator, values) {
  values.length = 0;
  while (iterator.hasNext() && iterator.peek() !== ',') {
    const value = decodeUnsignedVLQ(iterator);
    if (value === -1) return false;
    ArrayPrototypePush(values, value);
  }
  iterator.next();
  return true;
}

/**
 * AccumulateIndex ( _accumulator_: an Index Accumulator Record, _increment_: an integer, ): a non-negative integer
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-AccumulateIndex
 * @param {{ index: number }} accumulator
 * @param {number} increment
 * @returns {number}
 */
function accumulateIndex(accumulator, increment) {
  // 1. Set _accumulator_.[[Index]] to _accumulator_.[[Index]] + _increment_.
  accumulator.index += increment;
  // 2. If _accumulator_.[[Index]] < 0, then
  //   a. Optionally report an error.
  //   b. Set _accumulator_.[[Index]] to 0.
  if (accumulator.index < 0) accumulator.index = 0;
  // 3. Return _accumulator_.[[Index]].
  return accumulator.index;
}

/**
 * AccumulatePosition ( _accumulator_: a Position Accumulator Record, _lineIncrement_: a non-negative integer,
 *   _columnIncrement_: a non-negative integer, ): a Position Record
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-AccumulatePosition
 * @param {{ line: number, column: number }} accumulator
 * @param {number} lineIncrement
 * @param {number} columnIncrement
 * @returns {{ line: number, column: number }}
 */
function accumulatePosition(accumulator, lineIncrement, columnIncrement) {
  // 1. Set _accumulator_.[[Line]] to _accumulator_.[[Line]] + _lineIncrement_.
  accumulator.line += lineIncrement;
  if (lineIncrement === 0) {
    // 2. If _lineIncrement_ = 0, then
    //   a. Set _accumulator_.[[Column]] to _accumulator_.[[Column]] + _columnIncrement_.
    accumulator.column += columnIncrement;
  } else {
    // 3. Else,
    //   a. Set _accumulator_.[[Column]] to _columnIncrement_.
    accumulator.column = columnIncrement;
  }
  // 4. Return { [[Line]]: _accumulator_.[[Line]], [[Column]]: _accumulator_.[[Column]] }.
  return { line: accumulator.line, column: accumulator.column };
}

/**
 * RelativeName ( _vlq_: a |Vlq| Parse Node, _accumulator_: an Index Accumulator Record,
 *   _names_: a List of Strings, ): a String or *null*
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-RelativeName
 * @param {number} unsigned the VLQUnsignedValue of _vlq_
 * @param {{ index: number }} accumulator
 * @param {string[]} names
 * @returns {string | null}
 */
function relativeName(unsigned, accumulator, names) {
  // 1. Let _relativeIndex_ be the VLQSignedValue of _vlq_.
  // 2. Let _index_ be AccumulateIndex(_accumulator_, _relativeIndex_).
  const index = accumulateIndex(accumulator, toSigned(unsigned));
  // 3. If _index_ >= the length of _names_, then
  //   a. Optionally report an error.
  //   b. Return *null*.
  // 4. Return _names_[_index_].
  return index < names.length ? names[index] : null;
}

/**
 * BindingExpression ( _names_: a List of Strings, ): a String or *null*
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-BindingExpression
 * @param {number} unsigned the VLQUnsignedValue of |Vlq|
 * @param {string[]} names
 * @returns {string | null}
 */
function bindingExpression(unsigned, names) {
  // 1. Let _unadjustedBindingIndex_ be the VLQUnsignedValue of |Vlq|.
  // 2. If _unadjustedBindingIndex_ = 0, return *null*.
  // 3. Let _bindingIndex_ be _unadjustedBindingIndex_ - 1.
  // 4. If _bindingIndex_ >= the length of _names_, then
  //   a. Optionally report an error.
  //   b. Return *null*.
  if (unsigned === 0 || unsigned > names.length) return null;
  // 5. Return _names_[_bindingIndex_].
  return names[unsigned - 1];
}

/**
 * DecodeSourceScopes ( _scopes_: a String or *null*, _names_: a List of Strings, ): a List of Original Scope Records
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodeSourceScopes
 * @param {string | null} encoded
 * @param {string[]} names
 * @param {number} sourceIndex index of the source, recorded on each Original
 *   Scope Record as `sourceIndex` to resolve SourceOfScope
 * @param {object[]} flattened receives FlattenedScopes of the result
 * @returns {object[]} the root Original Scope Records of the source
 */
function decodeOriginalScopes(encoded, names, sourceIndex, flattened) {
  // 1. If _scopes_ is *null*, return << >>.
  if (typeof encoded !== 'string') return [];
  // 4. Let _scopePosition_ be a new Position Accumulator Record { [[Line]]: 0, [[Column]]: 0 }.
  const position = { line: 0, column: 0 };
  // 5. Let _scopeNameIndex_ be a new Index Accumulator Record { [[Index]]: 0 }.
  const nameIndex = { index: 0 };
  // 6. Let _scopeKindIndex_ be a new Index Accumulator Record { [[Index]]: 0 }.
  const kindIndex = { index: 0 };
  // 7. Let _scopeVariableIndex_ be a new Index Accumulator Record { [[Index]]: 0 }.
  const variableIndex = { index: 0 };
  const roots = [];
  const stack = [];
  // Non-recursive FlattenedScopes, so deeply nested scopes don't overflow the
  // stack.
  // https://tc39.es/ecma426/branch/proposal-scopes/#sec-FlattenedScopes
  const preorder = [];
  // 2. Let _parsedScopes_ be the root Parse Node when parsing _scopes_ using |Scopes| as the goal symbol.
  // 9. Return the DecodedOriginalScopeTrees of _parsedScopes_ with arguments _state_ and _names_.
  const iterator = new StringCharIterator(encoded);
  const values = [];
  while (iterator.hasNext()) {
    // 3. If parsing failed, then
    //   a. Optionally report an error.
    //   b. Return << >>.
    if (!decodeItem(iterator, values)) return [];
    if (values.length === 0) continue;
    const top = stack[stack.length - 1];
    switch (values[0]) {
      // https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodedOriginalScopeTrees
      // OriginalScopeTree :
      //   OriginalScopeStart OriginalScopeVariablesItem? OriginalScopeTreeList? `,` OriginalScopeEnd
      case kTagOriginalScopeStart: {
        if (values.length < 4) return [];
        // 4. Let _flags_ be the VLQUnsignedValue of |OriginalScopeStart|'s |ScopeFlags| nonterminal.
        const flags = values[1];
        // 1. Let _start_ be the DecodedPosition of |OriginalScopeStart| with argument _state_.[[ScopePosition]].
        const start = accumulatePosition(position, values[2], values[3]);
        let next = 4;
        let name = null;
        let kind = null;
        // 2. Let _name_ be the OriginalScopeName of |OriginalScopeStart| with arguments
        //    _state_.[[ScopeNameIndex]] and _names_.
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-OriginalScopeName

        //   2. If _flags_ & 0x1 != 0x1, return *null*.
        if (flags & kScopeFlagHasName) {
          if (next >= values.length) return [];
          //   4. Return RelativeName(|ScopeNameOrKind|, _accumulator_, _names_).
          name = relativeName(values[next++], nameIndex, names);
        }
        // 3. Let _kind_ be the OriginalScopeKind of |OriginalScopeStart| with arguments
        //    _state_.[[ScopeKindIndex]] and _names_.
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-OriginalScopeKind

        //   2. If _flags_ & 0x2 != 0x2, return *null*.
        if (flags & kScopeFlagHasKind) {
          if (next >= values.length) return [];
          //   4. If _flags_ & 0x1 = 0x1, then
          //     a. Assert: |ScopeKind| is present.
          //     b. Return RelativeName(|ScopeKind|, _accumulator_, _names_).
          //   5. Return RelativeName(|ScopeNameOrKind|, _accumulator_, _names_).
          kind = relativeName(values[next++], kindIndex, names);
        }
        // 5. Let _originalScope_ be the Original Scope Record { [[Start]]: _start_, [[End]]: _start_,

        //    [[Name]]: _name_, [[Kind]]: _kind_, [[Variables]]: << >>, [[Children]]: << >>,
        //    [[IsStackFrame]]: *false* }.
        const scope = {
          start,
          end: start,
          name,
          kind,
          // 6. If _flags_ & 0x4 = 0x4, set _originalScope_.[[IsStackFrame]] to *true*.
          isStackFrame: (flags & kScopeFlagIsStackFrame) !== 0,
          variables: [],
          children: [],
          sourceIndex,
        };
        // 8. If |OriginalScopeTreeList| is present, then
        //   a. Set _originalScope_.[[Children]] to the DecodedOriginalScopeTrees of |OriginalScopeTreeList|
        //      with arguments _state_ and _names_.
        ArrayPrototypePush(top ? top.children : roots, scope);
        ArrayPrototypePush(stack, scope);
        ArrayPrototypePush(preorder, scope);
        break;
      }
      case kTagOriginalScopeEnd:
        if (top === undefined || values.length < 3) return [];
        // 9. Set _originalScope_.[[End]] to the DecodedPosition of |OriginalScopeEnd| with argument
        //    _state_.[[ScopePosition]].
        top.end = accumulatePosition(position, values[1], values[2]);

        // 10. Return << _originalScope_ >>.
        ArrayPrototypePop(stack);
        break;
      case kTagOriginalScopeVariables:
        if (top === undefined) return [];
        // 7. If |OriginalScopeVariablesItem| is present, then
        //   a. Set _originalScope_.[[Variables]] to the OriginalScopeVariables of |OriginalScopeVariablesItem|
        //      with arguments _state_.[[ScopeVariableIndex]] and _names_.
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-OriginalScopeVariables
        for (let j = 1; j < values.length; ++j) {
          // ScopeVariable : Vlq
          //   1. Let _variable_ be RelativeName(|Vlq|, _accumulator_, _names_).
          const variable = relativeName(values[j], variableIndex, names);

          //   2. If _variable_ is *null*, return << "" >>.

          //   3. Return << _variable_ >>.
          ArrayPrototypePush(top.variables, variable ?? '');
        }
        break;
      default:
        // OriginalScopeTreeItem :
        //   VendorExtensionItem
        //   InvalidScopeItem

        //   1. Return << >>.
        break;
    }
  }
  if (stack.length !== 0) return [];
  for (let i = 0; i < preorder.length; ++i) {
    ArrayPrototypePush(flattened, preorder[i]);
  }
  return roots;
}

/**
 * ParentDefinition ( _state_: a Decode Range State Record, _parentRangeIndex_: a non-negative integer, ):
 *   an Original Scope Record
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-ParentDefinition
 * @param {object[]} stack the open Generated Range Records, ending with the
 *   current one
 * @param {number} parentRangeIndex
 * @returns {object | null}
 */
function parentDefinition(stack, parentRangeIndex) {
  // 1. Let _stack_ be _state_.[[DefinitionStack]].
  // 2. Let _length_ be the number of elements in _stack_.
  // 3. Return _stack_[_length_ - _parentRangeIndex_ - 1].
  for (let i = stack.length - 2; i >= 0; --i) {
    const definition = stack[i].definition;
    if (definition === null) continue;
    if (parentRangeIndex-- === 0) return definition;
  }
  return null;
}

/**
 * DecodeGeneratedRanges ( _ranges_: either a List of Strings, or *null*, _sources_: a List of Decoded Source Records,
 *   _names_: a List of Strings, ): a List of Generated Range Records, steps 3.a to 3.g
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodeGeneratedRanges
 * @param {string} segment
 * @param {object[][]} flattenedScopes FlattenedScopes of each source's
 *   [[RootScopes]], indexed by the source index local to this map
 * @param {string[]} names
 * @param {number} sourceBase index of this map's first source in the
 *   original scope records
 * @returns {object[] | undefined} undefined if the segment is malformed
 */
function decodeRangesSegment(segment, flattenedScopes, names, sourceBase) {
  //   c. Let _rangePosition_ be a new Position Accumulator Record { [[Line]]: 0, [[Column]]: 0 }.
  const position = { line: 0, column: 0 };
  //   d. Let _rangeDefinitionSourceIndex_ be a new Index Accumulator Record { [[Index]]: 0 }.
  const definitionSourceIndex = { index: 0 };
  //   e. Let _rangeDefinitionScopeIndex_ be a new Index Accumulator Record { [[Index]]: 0 }.
  const definitionScopeIndex = { index: 0 };
  const result = [];
  const stack = [];
  //   a. Let _parsedRanges_ be the root Parse Node when parsing _segment_ using |Ranges| as the goal symbol.
  //   g. Let _segmentResult_ be the DecodedGeneratedRangeTrees of _parsedRanges_ with arguments _state_,
  //      _sources_ and _names_.
  const iterator = new StringCharIterator(segment);
  const values = [];
  while (iterator.hasNext()) {
    //   b. If parsing failed, then
    //     i. Optionally report an error.
    //     ii. Return << >>.
    if (!decodeItem(iterator, values)) return;
    if (values.length === 0) continue;
    const top = stack[stack.length - 1];
    switch (values[0]) {
      // https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodedGeneratedRangeTrees
      // GeneratedRangeTree :
      //   GeneratedRangeStart GeneratedRangeBindingsItem? GeneratedRangeCallSiteItem? GeneratedRangeItemList?
      //   `,` GeneratedRangeEnd
      case kTagGeneratedRangeStart: {
        if (values.length < 3) return;
        // 3. Let _flags_ be the VLQUnsignedValue of |GeneratedRangeStart|'s |RangeFlags| nonterminal.
        const flags = values[1];
        let next = 2;
        // 1. Let _start_ be the DecodedPosition of |GeneratedRangeStart| with argument _state_.[[RangePosition]].
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodedPosition
        //   1. If |RangeLine| is present, let _relativeLine_ be the VLQUnsignedValue of |RangeLine|, otherwise
        //      let _relativeLine_ be 0.
        let line = 0;
        if (flags & kRangeFlagHasLine) line = values[next++];
        if (next >= values.length) return;
        //   2. Let _relativeColumn_ be the VLQUnsignedValue of |RangeColumn|.
        //   3. Return AccumulatePosition(_accumulator_, _relativeLine_, _relativeColumn_).
        const start = accumulatePosition(position, line, values[next++]);
        // 2. Let _definition_ be the GeneratedRangeDefinition of |GeneratedRangeStart| with arguments _state_
        //    and _sources_.
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-GeneratedRangeDefinition
        let definition = null;

        //   2. If _flags_ & 0x2 != 0x2, return *null*.
        if (flags & kRangeFlagHasDefinition) {
          if (next + 1 >= values.length) return;
          // RangeDefinition :
          //   DefinitionSourceIdx DefinitionScopeIdx
          //   1. Let _sourceOffset_ be the VLQSignedValue of |DefinitionSourceIdx|.
          const sourceOffset = toSigned(values[next++]);
          //   2. Let _sourceIndex_ be AccumulateIndex(_state_.[[RangeDefinitionSourceIndex]], _sourceOffset_).
          const sourceIndex =
            accumulateIndex(definitionSourceIndex, sourceOffset);
          let scopeOffset;
          if (sourceOffset === 0) {
            //   4. If _sourceOffset_ = 0, then
            //     a. Let _scopeOffset_ be the VLQSignedValue of |DefinitionScopeIdx|.
            scopeOffset = toSigned(values[next++]);
          } else {
            //   5. Else,
            //     a. Set _state_.[[RangeDefinitionScopeIndex]].[[Index]] to 0.
            definitionScopeIndex.index = 0;
            //     b. Let _scopeOffset_ be the VLQUnsignedValue of |DefinitionScopeIdx|.
            scopeOffset = values[next++];
          }
          //   6. Let _scopeIndex_ be AccumulateIndex(_state_.[[RangeDefinitionScopeIndex]], _scopeOffset_).
          const scopeIndex = accumulateIndex(definitionScopeIndex, scopeOffset);
          //   3. Let _source_ be _sources_[_sourceIndex_].
          //   7. Return ScopeForScopeIndex(_source_.[[RootScopes]], _scopeIndex_).
          // https://tc39.es/ecma426/branch/proposal-scopes/#sec-ScopeForScopeIndex
          //   1. Return FlattenedScopes(_scopes_)[_index_].
          if (sourceIndex < flattenedScopes.length) {
            definition = flattenedScopes[sourceIndex][scopeIndex] ?? null;
          }
        }
        // 6. Else,
        //   a. If _flags_ & 0x8 = 0x8, optionally report an error.
        //   b. Let _stackFrameType_ be ~none~.
        let stackFrameType = kStackFrameTypeNone;
        if (flags & kRangeFlagIsStackFrame) {
          // 4. If _flags_ & 0xc = 0xc, then
          //   a. Let _stackFrameType_ be ~hidden~.
          // 5. Else if _flags_ & 0x4 = 0x4, then
          //   a. Let _stackFrameType_ be ~original~.
          stackFrameType = (flags & kRangeFlagIsHidden) ?
            kStackFrameTypeHidden : kStackFrameTypeOriginal;
        }
        const range = {
          start,
          end: start,
          definition,
          stackFrameType,
          // 8. Else,

          //   a. Let _bindings_ be << >>.
          bindings: [],
          // 10. Else,
          //   a. Let _callSite_ be *null*.
          callSite: null,
          children: [],
        };
        // 11. If _definition_ is not *null*, then
        //   a. Append _definition_ to _state_.[[DefinitionStack]].
        // 12. If |GeneratedRangeItemList| is present, then
        //   a. Let _children_ be the DecodedGeneratedRangeTrees of |GeneratedRangeItemList| with arguments
        //      _state_, _sources_ and _names_.
        ArrayPrototypePush(top ? top.children : result, range);
        ArrayPrototypePush(stack, range);
        break;
      }
      case kTagGeneratedRangeEnd:
        if (top === undefined || values.length < 2 || values.length > 3) return;
        // 15. Let _end_ be the DecodedPosition of |GeneratedRangeEnd| with argument _state_.[[RangePosition]].
        top.end = accumulatePosition(position,
                                     values.length === 3 ? values[1] : 0,
                                     values[values.length - 1]);
        // 16. If _definition_ is not *null*, then
        //   a. Remove the last element of _state_.[[DefinitionStack]].
        ArrayPrototypePop(stack);
        break;
      case kTagGeneratedRangeBindings:
        if (top === undefined) return;
        // 7. If |GeneratedRangeBindingsItem| is present, then
        //   a. Let _bindings_ be the GeneratedRangeBindings of |GeneratedRangeBindingsItem| with arguments
        //      _start_ and _names_.
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-GeneratedRangeBindings
        for (let j = 1; j < values.length; ++j) {
          // BindingExpression : Vlq
          //   1. Let _binding_ be the BindingExpression of |BindingExpression| with argument _names_.

          //   2. Return << << { [[From]]: _start_, [[Binding]]: _binding_ } >> >>.
          ArrayPrototypePush(top.bindings, [{
            from: { line: top.start.line, column: top.start.column },
            binding: bindingExpression(values[j], names),
          }]);
        }
        break;
      case kTagGeneratedRangeSubRangeBinding: {
        if (top === undefined || values.length < 2) return;
        // 14. If |GeneratedRangeItemList| is present, then
        //   a. Let _subRangeBindings_ be the GeneratedSubRangeBindings of |GeneratedRangeItemList| with
        //      arguments _start_ and _names_.
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-GeneratedSubRangeBindings
        // GeneratedSubRangeBinding :
        //   `H` VariableIndex BindingFromList
        //   1. Let _variableIndex_ be the VLQUnsignedValue of |VariableIndex|.
        //   2. Let _from_ be a new Position Accumulator Record { [[Line]]: _start_.[[Line]],
        //      [[Column]]: _start_.[[Column]] }.
        const from = { line: top.start.line, column: top.start.column };
        //   b. For each Sub-Range Binding Record _subRangeBinding_ of _subRangeBindings_, do
        //     i. Let _index_ be _subRangeBinding_.[[VariableIndex]].
        //     ii. If _index_ < the length of _bindings_, then
        //       1. Set _bindings_[_index_] to the list-concatenation of _bindings_[_index_] and
        //          _subRangeBinding_.[[Bindings]].
        //     iii. Else,
        //       1. Optionally report an error.
        const bindings = top.bindings[values[1]];
        //   3. Let _bindings_ be the SubRangeBinding of |BindingFromList| with arguments _from_ and _names_.
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-SubRangeBinding
        for (let j = 2; j + 2 < values.length; j += 3) {
          // BindingFrom : BindingLine BindingColumn BindingExpression
          const binding = {
            //   1. Let _relativeLine_ be the VLQUnsignedValue of |BindingLine|.
            //   2. Let _relativeColumn_ be the VLQUnsignedValue of |BindingColumn|.
            //   3. Let _fromPosition_ be AccumulatePosition(_from_, _relativeLine_, _relativeColumn_).
            from: accumulatePosition(from, values[j], values[j + 1]),
            //   4. Let _binding_ be the BindingExpression of |BindingExpression| with argument _names_.
            binding: bindingExpression(values[j + 2], names),
          };

          //   5. Return << { [[From]]: _fromPosition_, [[Binding]]: _binding_ } >>.
          if (bindings !== undefined) ArrayPrototypePush(bindings, binding);
        }
        break;
      }
      case kTagGeneratedRangeCallSite:
        if (top === undefined || values.length < 4) return;
        // 9. If |GeneratedRangeCallSiteItem| is present, then
        //   a. Let _callSite_ be the GeneratedRangeCallSite of |GeneratedRangeCallSiteItem| with arguments
        //      _state_ and _sources_.
        // https://tc39.es/ecma426/branch/proposal-scopes/#sec-GeneratedRangeCallSite
        // GeneratedRangeCallSite :
        //   `I` CallSiteSourceIdx CallSiteLine CallSiteColumn
        //   1. Let _sourceIndex_ be the VLQUnsignedValue of |CallSiteSourceIdx|.
        //   2. Let _source_ be _sources_[_sourceIndex_].
        if (values[1] < flattenedScopes.length) {
          //   3. Let _line_ be the VLQUnsignedValue of |CallSiteLine|.
          //   4. Let _column_ be the VLQUnsignedValue of |CallSiteColumn|.
          //   5. Return a new Original Position Record { [[Source]]: _source_, [[Line]]: _line_,
          //      [[Column]]: _column_ }.
          top.callSite = {
            sourceIndex: sourceBase + values[1],
            line: values[2],
            column: values[3],
          };
        }
        break;
      case kTagGeneratedRangeRelativeCallSite: {
        if (top === undefined || values.length < 2 || values.length > 4) return;
        // `J` CallSiteRangeParentIdx CallSiteLineOffset CallSiteColumn
        //   1. Let _parentDefinitionIndex_ be the VLQUnsignedValue of |CallSiteRangeParentIdx|.
        //   2. Let _parentDefinition_ be ParentDefinition(_state_, _parentDefinitionIndex_).
        // `J` CallSiteLineOffset CallSiteColumn
        // `J` CallSiteColumnOffset
        //   1. Let _parentDefinition_ be ParentDefinition(_state_, 0).
        const parent = parentDefinition(stack, values.length === 4 ? values[1] : 0);
        if (parent === null) break;
        let line = parent.start.line;
        let column;
        if (values.length === 2) {
          // `J` CallSiteColumnOffset
          //   3. Let _line_ be _parentDefinition_.[[Start]].[[Line]].
          //   4. Let _column_ be _parentDefinition_.[[Start]].[[Column]] + the VLQUnsignedValue of
          //      |CallSiteColumnOffset|.
          column = parent.start.column + values[1];
        } else {
          // `J` CallSiteRangeParentIdx CallSiteLineOffset CallSiteColumn
          //   4. Let _line_ be _parentDefinition_.[[Start]].[[Line]] + the VLQUnsignedValue of
          //      |CallSiteLineOffset|.
          //   5. Let _column_ be the VLQUnsignedValue of |CallSiteColumn|.
          // `J` CallSiteLineOffset CallSiteColumn
          //   3. Let _line_ be _parentDefinition_.[[Start]].[[Line]] + the VLQUnsignedValue of
          //      |CallSiteLineOffset|.
          //   4. Let _column_ be the VLQUnsignedValue of |CallSiteColumn|.
          line += values[values.length - 2];
          column = values[values.length - 1];
        }
        top.callSite = { sourceIndex: parent.sourceIndex, line, column };
        break;
      }
      default:
        break;
    }
  }
  if (stack.length !== 0) return;
  return result;
}

/**
 * DecodeGeneratedRanges ( _ranges_: either a List of Strings, or *null*, _sources_: a List of Decoded Source Records,
 *   _names_: a List of Strings, ): a List of Generated Range Records
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodeGeneratedRanges
 * @param {string[]} ranges
 * @param {object[][]} flattenedScopes
 * @param {string[]} names
 * @param {number} sourceBase
 * @returns {object[]} the root Generated Range Records
 */
function decodeGeneratedRanges(ranges, flattenedScopes, names, sourceBase) {
  // 1. If _ranges_ is *null*, return << >>.
  if (!ArrayIsArray(ranges)) return [];

  // 2. Let _result_ be << >>.
  const result = [];
  // 3. For each String _segment_ of _ranges_, do
  for (let i = 0; i < ranges.length; ++i) {
    if (typeof ranges[i] !== 'string') continue;
    const segment =
      decodeRangesSegment(ranges[i], flattenedScopes, names, sourceBase);

    //   b. If parsing failed, then
    //     i. Optionally report an error.
    //     ii. Return << >>.
    if (segment === undefined) return [];
    //   i. Append each element of _segmentResult_ to _result_.
    for (let j = 0; j < segment.length; ++j) {
      ArrayPrototypePush(result, segment[j]);
    }
  }
  // 4. Return _result_.
  return result;
}

module.exports = {
  decodeGeneratedRanges,
  decodeOriginalScopes,
};
