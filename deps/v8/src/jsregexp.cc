// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "ast.h"
#include "execution.h"
#include "factory.h"
#include "jsregexp-inl.h"
#include "platform.h"
#include "runtime.h"
#include "top.h"
#include "compilation-cache.h"
#include "string-stream.h"
#include "parser.h"
#include "regexp-macro-assembler.h"
#include "regexp-macro-assembler-tracer.h"
#include "regexp-macro-assembler-irregexp.h"
#include "regexp-stack.h"

#ifdef ARM
#include "regexp-macro-assembler-arm.h"
#else  // IA32
#include "macro-assembler-ia32.h"
#include "regexp-macro-assembler-ia32.h"
#endif

#include "interpreter-irregexp.h"


namespace v8 { namespace internal {


Handle<Object> RegExpImpl::CreateRegExpLiteral(Handle<JSFunction> constructor,
                                               Handle<String> pattern,
                                               Handle<String> flags,
                                               bool* has_pending_exception) {
  // Ensure that the constructor function has been loaded.
  if (!constructor->IsLoaded()) {
    LoadLazy(constructor, has_pending_exception);
    if (*has_pending_exception) return Handle<Object>();
  }
  // Call the construct code with 2 arguments.
  Object** argv[2] = { Handle<Object>::cast(pattern).location(),
                       Handle<Object>::cast(flags).location() };
  return Execution::New(constructor, 2, argv, has_pending_exception);
}


static JSRegExp::Flags RegExpFlagsFromString(Handle<String> str) {
  int flags = JSRegExp::NONE;
  for (int i = 0; i < str->length(); i++) {
    switch (str->Get(i)) {
      case 'i':
        flags |= JSRegExp::IGNORE_CASE;
        break;
      case 'g':
        flags |= JSRegExp::GLOBAL;
        break;
      case 'm':
        flags |= JSRegExp::MULTILINE;
        break;
    }
  }
  return JSRegExp::Flags(flags);
}


static inline void ThrowRegExpException(Handle<JSRegExp> re,
                                        Handle<String> pattern,
                                        Handle<String> error_text,
                                        const char* message) {
  Handle<JSArray> array = Factory::NewJSArray(2);
  SetElement(array, 0, pattern);
  SetElement(array, 1, error_text);
  Handle<Object> regexp_err = Factory::NewSyntaxError(message, array);
  Top::Throw(*regexp_err);
}


// Generic RegExp methods. Dispatches to implementation specific methods.


class OffsetsVector {
 public:
  inline OffsetsVector(int num_registers)
      : offsets_vector_length_(num_registers) {
    if (offsets_vector_length_ > kStaticOffsetsVectorSize) {
      vector_ = NewArray<int>(offsets_vector_length_);
    } else {
      vector_ = static_offsets_vector_;
    }
  }
  inline ~OffsetsVector() {
    if (offsets_vector_length_ > kStaticOffsetsVectorSize) {
      DeleteArray(vector_);
      vector_ = NULL;
    }
  }
  inline int* vector() { return vector_; }
  inline int length() { return offsets_vector_length_; }

 private:
  int* vector_;
  int offsets_vector_length_;
  static const int kStaticOffsetsVectorSize = 50;
  static int static_offsets_vector_[kStaticOffsetsVectorSize];
};


int OffsetsVector::static_offsets_vector_[
    OffsetsVector::kStaticOffsetsVectorSize];


Handle<Object> RegExpImpl::Compile(Handle<JSRegExp> re,
                                   Handle<String> pattern,
                                   Handle<String> flag_str) {
  JSRegExp::Flags flags = RegExpFlagsFromString(flag_str);
  Handle<FixedArray> cached = CompilationCache::LookupRegExp(pattern, flags);
  bool in_cache = !cached.is_null();
  LOG(RegExpCompileEvent(re, in_cache));

  Handle<Object> result;
  if (in_cache) {
    re->set_data(*cached);
    return re;
  }
  FlattenString(pattern);
  ZoneScope zone_scope(DELETE_ON_EXIT);
  RegExpCompileData parse_result;
  FlatStringReader reader(pattern);
  if (!ParseRegExp(&reader, flags.is_multiline(), &parse_result)) {
    // Throw an exception if we fail to parse the pattern.
    ThrowRegExpException(re,
                         pattern,
                         parse_result.error,
                         "malformed_regexp");
    return Handle<Object>::null();
  }

  if (parse_result.simple && !flags.is_ignore_case()) {
    // Parse-tree is a single atom that is equal to the pattern.
    AtomCompile(re, pattern, flags, pattern);
  } else if (parse_result.tree->IsAtom() &&
      !flags.is_ignore_case() &&
      parse_result.capture_count == 0) {
    RegExpAtom* atom = parse_result.tree->AsAtom();
    Vector<const uc16> atom_pattern = atom->data();
    Handle<String> atom_string = Factory::NewStringFromTwoByte(atom_pattern);
    AtomCompile(re, pattern, flags, atom_string);
  } else {
    IrregexpPrepare(re, pattern, flags, parse_result.capture_count);
  }
  ASSERT(re->data()->IsFixedArray());
  // Compilation succeeded so the data is set on the regexp
  // and we can store it in the cache.
  Handle<FixedArray> data(FixedArray::cast(re->data()));
  CompilationCache::PutRegExp(pattern, flags, data);

  return re;
}


Handle<Object> RegExpImpl::Exec(Handle<JSRegExp> regexp,
                                Handle<String> subject,
                                int index,
                                Handle<JSArray> last_match_info) {
  switch (regexp->TypeTag()) {
    case JSRegExp::ATOM:
      return AtomExec(regexp, subject, index, last_match_info);
    case JSRegExp::IRREGEXP: {
      Handle<Object> result =
          IrregexpExec(regexp, subject, index, last_match_info);
      ASSERT(!result.is_null() || Top::has_pending_exception());
      return result;
    }
    default:
      UNREACHABLE();
      return Handle<Object>::null();
  }
}


// RegExp Atom implementation: Simple string search using indexOf.


void RegExpImpl::AtomCompile(Handle<JSRegExp> re,
                             Handle<String> pattern,
                             JSRegExp::Flags flags,
                             Handle<String> match_pattern) {
  Factory::SetRegExpAtomData(re,
                             JSRegExp::ATOM,
                             pattern,
                             flags,
                             match_pattern);
}


static void SetAtomLastCapture(FixedArray* array,
                               String* subject,
                               int from,
                               int to) {
  NoHandleAllocation no_handles;
  RegExpImpl::SetLastCaptureCount(array, 2);
  RegExpImpl::SetLastSubject(array, subject);
  RegExpImpl::SetLastInput(array, subject);
  RegExpImpl::SetCapture(array, 0, from);
  RegExpImpl::SetCapture(array, 1, to);
}


Handle<Object> RegExpImpl::AtomExec(Handle<JSRegExp> re,
                                    Handle<String> subject,
                                    int index,
                                    Handle<JSArray> last_match_info) {
  Handle<String> needle(String::cast(re->DataAt(JSRegExp::kAtomPatternIndex)));

  uint32_t start_index = index;

  int value = Runtime::StringMatch(subject, needle, start_index);
  if (value == -1) return Factory::null_value();
  ASSERT(last_match_info->HasFastElements());

  {
    NoHandleAllocation no_handles;
    FixedArray* array = last_match_info->elements();
    SetAtomLastCapture(array, *subject, value, value + needle->length());
  }
  return last_match_info;
}


// Irregexp implementation.


// Ensures that the regexp object contains a compiled version of the
// source for either ASCII or non-ASCII strings.
// If the compiled version doesn't already exist, it is compiled
// from the source pattern.
// If compilation fails, an exception is thrown and this function
// returns false.
bool RegExpImpl::EnsureCompiledIrregexp(Handle<JSRegExp> re, bool is_ascii) {
  int index;
  if (is_ascii) {
    index = JSRegExp::kIrregexpASCIICodeIndex;
  } else {
    index = JSRegExp::kIrregexpUC16CodeIndex;
  }
  Object* entry = re->DataAt(index);
  if (!entry->IsTheHole()) {
    // A value has already been compiled.
    if (entry->IsJSObject()) {
      // If it's a JS value, it's an error.
      Top::Throw(entry);
      return false;
    }
    return true;
  }

  // Compile the RegExp.
  ZoneScope zone_scope(DELETE_ON_EXIT);

  JSRegExp::Flags flags = re->GetFlags();

  Handle<String> pattern(re->Pattern());
  if (!pattern->IsFlat()) {
    FlattenString(pattern);
  }

  RegExpCompileData compile_data;
  FlatStringReader reader(pattern);
  if (!ParseRegExp(&reader, flags.is_multiline(), &compile_data)) {
    // Throw an exception if we fail to parse the pattern.
    // THIS SHOULD NOT HAPPEN. We already parsed it successfully once.
    ThrowRegExpException(re,
                         pattern,
                         compile_data.error,
                         "malformed_regexp");
    return false;
  }
  RegExpEngine::CompilationResult result =
      RegExpEngine::Compile(&compile_data,
                            flags.is_ignore_case(),
                            flags.is_multiline(),
                            pattern,
                            is_ascii);
  if (result.error_message != NULL) {
    // Unable to compile regexp.
    Handle<JSArray> array = Factory::NewJSArray(2);
    SetElement(array, 0, pattern);
    SetElement(array,
               1,
               Factory::NewStringFromUtf8(CStrVector(result.error_message)));
    Handle<Object> regexp_err =
        Factory::NewSyntaxError("malformed_regexp", array);
    Top::Throw(*regexp_err);
    re->SetDataAt(index, *regexp_err);
    return false;
  }

  NoHandleAllocation no_handles;

  FixedArray* data = FixedArray::cast(re->data());
  data->set(index, result.code);
  int register_max = IrregexpMaxRegisterCount(data);
  if (result.num_registers > register_max) {
    SetIrregexpMaxRegisterCount(data, result.num_registers);
  }

  return true;
}


int RegExpImpl::IrregexpMaxRegisterCount(FixedArray* re) {
  return Smi::cast(
      re->get(JSRegExp::kIrregexpMaxRegisterCountIndex))->value();
}


void RegExpImpl::SetIrregexpMaxRegisterCount(FixedArray* re, int value) {
  re->set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::FromInt(value));
}


int RegExpImpl::IrregexpNumberOfCaptures(FixedArray* re) {
  return Smi::cast(re->get(JSRegExp::kIrregexpCaptureCountIndex))->value();
}


int RegExpImpl::IrregexpNumberOfRegisters(FixedArray* re) {
  return Smi::cast(re->get(JSRegExp::kIrregexpMaxRegisterCountIndex))->value();
}


ByteArray* RegExpImpl::IrregexpByteCode(FixedArray* re, bool is_ascii) {
  int index;
  if (is_ascii) {
    index = JSRegExp::kIrregexpASCIICodeIndex;
  } else {
    index = JSRegExp::kIrregexpUC16CodeIndex;
  }
  return ByteArray::cast(re->get(index));
}


Code* RegExpImpl::IrregexpNativeCode(FixedArray* re, bool is_ascii) {
  int index;
  if (is_ascii) {
    index = JSRegExp::kIrregexpASCIICodeIndex;
  } else {
    index = JSRegExp::kIrregexpUC16CodeIndex;
  }
  return Code::cast(re->get(index));
}


void RegExpImpl::IrregexpPrepare(Handle<JSRegExp> re,
                                 Handle<String> pattern,
                                 JSRegExp::Flags flags,
                                 int capture_count) {
  // Initialize compiled code entries to null.
  Factory::SetRegExpIrregexpData(re,
                                 JSRegExp::IRREGEXP,
                                 pattern,
                                 flags,
                                 capture_count);
}


Handle<Object> RegExpImpl::IrregexpExec(Handle<JSRegExp> jsregexp,
                                        Handle<String> subject,
                                        int previous_index,
                                        Handle<JSArray> last_match_info) {
  ASSERT_EQ(jsregexp->TypeTag(), JSRegExp::IRREGEXP);

  // Prepare space for the return values.
  int number_of_capture_registers =
      (IrregexpNumberOfCaptures(FixedArray::cast(jsregexp->data())) + 1) * 2;
  OffsetsVector offsets(number_of_capture_registers);

#ifdef DEBUG
  if (FLAG_trace_regexp_bytecodes) {
    String* pattern = jsregexp->Pattern();
    PrintF("\n\nRegexp match:   /%s/\n\n", *(pattern->ToCString()));
    PrintF("\n\nSubject string: '%s'\n\n", *(subject->ToCString()));
  }
#endif

  if (!subject->IsFlat()) {
    FlattenString(subject);
  }

  last_match_info->EnsureSize(number_of_capture_registers + kLastMatchOverhead);

  int* offsets_vector = offsets.vector();
  bool rc;

  // Dispatch to the correct RegExp implementation.

  Handle<String> original_subject = subject;
  Handle<FixedArray> regexp(FixedArray::cast(jsregexp->data()));
  if (UseNativeRegexp()) {
#ifdef ARM
    UNREACHABLE();
#else
    RegExpMacroAssemblerIA32::Result res;
    do {
      bool is_ascii = StringShape(*subject).IsAsciiRepresentation();
      if (!EnsureCompiledIrregexp(jsregexp, is_ascii)) {
        return Handle<Object>::null();
      }
      Handle<Code> code(RegExpImpl::IrregexpNativeCode(*regexp, is_ascii));
      res = RegExpMacroAssemblerIA32::Match(code,
                                            subject,
                                            offsets_vector,
                                            offsets.length(),
                                            previous_index);
      // If result is RETRY, the string have changed representation, and we
      // must restart from scratch.
    } while (res == RegExpMacroAssemblerIA32::RETRY);
    if (res == RegExpMacroAssemblerIA32::EXCEPTION) {
      ASSERT(Top::has_pending_exception());
      return Handle<Object>::null();
    }
    ASSERT(res == RegExpMacroAssemblerIA32::SUCCESS
        || res == RegExpMacroAssemblerIA32::FAILURE);

    rc = (res == RegExpMacroAssemblerIA32::SUCCESS);
#endif
  } else {
    bool is_ascii = StringShape(*subject).IsAsciiRepresentation();
    if (!EnsureCompiledIrregexp(jsregexp, is_ascii)) {
      return Handle<Object>::null();
    }
    for (int i = number_of_capture_registers - 1; i >= 0; i--) {
      offsets_vector[i] = -1;
    }
    Handle<ByteArray> byte_codes(IrregexpByteCode(*regexp, is_ascii));

    rc = IrregexpInterpreter::Match(byte_codes,
                                    subject,
                                    offsets_vector,
                                    previous_index);
  }

  // Handle results from RegExp implementation.

  if (!rc) {
    return Factory::null_value();
  }

  FixedArray* array = last_match_info->elements();
  ASSERT(array->length() >= number_of_capture_registers + kLastMatchOverhead);
  // The captures come in (start, end+1) pairs.
  SetLastCaptureCount(array, number_of_capture_registers);
  SetLastSubject(array, *original_subject);
  SetLastInput(array, *original_subject);
  for (int i = 0; i < number_of_capture_registers; i+=2) {
    SetCapture(array, i, offsets_vector[i]);
    SetCapture(array, i + 1, offsets_vector[i + 1]);
  }
  return last_match_info;
}


// -------------------------------------------------------------------
// Implementation of the Irregexp regular expression engine.
//
// The Irregexp regular expression engine is intended to be a complete
// implementation of ECMAScript regular expressions.  It generates either
// bytecodes or native code.

//   The Irregexp regexp engine is structured in three steps.
//   1) The parser generates an abstract syntax tree.  See ast.cc.
//   2) From the AST a node network is created.  The nodes are all
//      subclasses of RegExpNode.  The nodes represent states when
//      executing a regular expression.  Several optimizations are
//      performed on the node network.
//   3) From the nodes we generate either byte codes or native code
//      that can actually execute the regular expression (perform
//      the search).  The code generation step is described in more
//      detail below.

// Code generation.
//
//   The nodes are divided into four main categories.
//   * Choice nodes
//        These represent places where the regular expression can
//        match in more than one way.  For example on entry to an
//        alternation (foo|bar) or a repetition (*, +, ? or {}).
//   * Action nodes
//        These represent places where some action should be
//        performed.  Examples include recording the current position
//        in the input string to a register (in order to implement
//        captures) or other actions on register for example in order
//        to implement the counters needed for {} repetitions.
//   * Matching nodes
//        These attempt to match some element part of the input string.
//        Examples of elements include character classes, plain strings
//        or back references.
//   * End nodes
//        These are used to implement the actions required on finding
//        a successful match or failing to find a match.
//
//   The code generated (whether as byte codes or native code) maintains
//   some state as it runs.  This consists of the following elements:
//
//   * The capture registers.  Used for string captures.
//   * Other registers.  Used for counters etc.
//   * The current position.
//   * The stack of backtracking information.  Used when a matching node
//     fails to find a match and needs to try an alternative.
//
// Conceptual regular expression execution model:
//
//   There is a simple conceptual model of regular expression execution
//   which will be presented first.  The actual code generated is a more
//   efficient simulation of the simple conceptual model:
//
//   * Choice nodes are implemented as follows:
//     For each choice except the last {
//       push current position
//       push backtrack code location
//       <generate code to test for choice>
//       backtrack code location:
//       pop current position
//     }
//     <generate code to test for last choice>
//
//   * Actions nodes are generated as follows
//     <push affected registers on backtrack stack>
//     <generate code to perform action>
//     push backtrack code location
//     <generate code to test for following nodes>
//     backtrack code location:
//     <pop affected registers to restore their state>
//     <pop backtrack location from stack and go to it>
//
//   * Matching nodes are generated as follows:
//     if input string matches at current position
//       update current position
//       <generate code to test for following nodes>
//     else
//       <pop backtrack location from stack and go to it>
//
//   Thus it can be seen that the current position is saved and restored
//   by the choice nodes, whereas the registers are saved and restored by
//   by the action nodes that manipulate them.
//
//   The other interesting aspect of this model is that nodes are generated
//   at the point where they are needed by a recursive call to Emit().  If
//   the node has already been code generated then the Emit() call will
//   generate a jump to the previously generated code instead.  In order to
//   limit recursion it is possible for the Emit() function to put the node
//   on a work list for later generation and instead generate a jump.  The
//   destination of the jump is resolved later when the code is generated.
//
// Actual regular expression code generation.
//
//   Code generation is actually more complicated than the above.  In order
//   to improve the efficiency of the generated code some optimizations are
//   performed
//
//   * Choice nodes have 1-character lookahead.
//     A choice node looks at the following character and eliminates some of
//     the choices immediately based on that character.  This is not yet
//     implemented.
//   * Simple greedy loops store reduced backtracking information.
//     A quantifier like /.*foo/m will greedily match the whole input.  It will
//     then need to backtrack to a point where it can match "foo".  The naive
//     implementation of this would push each character position onto the
//     backtracking stack, then pop them off one by one.  This would use space
//     proportional to the length of the input string.  However since the "."
//     can only match in one way and always has a constant length (in this case
//     of 1) it suffices to store the current position on the top of the stack
//     once.  Matching now becomes merely incrementing the current position and
//     backtracking becomes decrementing the current position and checking the
//     result against the stored current position.  This is faster and saves
//     space.
//   * The current state is virtualized.
//     This is used to defer expensive operations until it is clear that they
//     are needed and to generate code for a node more than once, allowing
//     specialized an efficient versions of the code to be created. This is
//     explained in the section below.
//
// Execution state virtualization.
//
//   Instead of emitting code, nodes that manipulate the state can record their
//   manipulation in an object called the Trace.  The Trace object can record a
//   current position offset, an optional backtrack code location on the top of
//   the virtualized backtrack stack and some register changes.  When a node is
//   to be emitted it can flush the Trace or update it.  Flushing the Trace
//   will emit code to bring the actual state into line with the virtual state.
//   Avoiding flushing the state can postpone some work (eg updates of capture
//   registers).  Postponing work can save time when executing the regular
//   expression since it may be found that the work never has to be done as a
//   failure to match can occur.  In addition it is much faster to jump to a
//   known backtrack code location than it is to pop an unknown backtrack
//   location from the stack and jump there.
//
//   The virtual state found in the Trace affects code generation.  For example
//   the virtual state contains the difference between the actual current
//   position and the virtual current position, and matching code needs to use
//   this offset to attempt a match in the correct location of the input
//   string.  Therefore code generated for a non-trivial trace is specialized
//   to that trace.  The code generator therefore has the ability to generate
//   code for each node several times.  In order to limit the size of the
//   generated code there is an arbitrary limit on how many specialized sets of
//   code may be generated for a given node.  If the limit is reached, the
//   trace is flushed and a generic version of the code for a node is emitted.
//   This is subsequently used for that node.  The code emitted for non-generic
//   trace is not recorded in the node and so it cannot currently be reused in
//   the event that code generation is requested for an identical trace.


void RegExpTree::AppendToText(RegExpText* text) {
  UNREACHABLE();
}


void RegExpAtom::AppendToText(RegExpText* text) {
  text->AddElement(TextElement::Atom(this));
}


void RegExpCharacterClass::AppendToText(RegExpText* text) {
  text->AddElement(TextElement::CharClass(this));
}


void RegExpText::AppendToText(RegExpText* text) {
  for (int i = 0; i < elements()->length(); i++)
    text->AddElement(elements()->at(i));
}


TextElement TextElement::Atom(RegExpAtom* atom) {
  TextElement result = TextElement(ATOM);
  result.data.u_atom = atom;
  return result;
}


TextElement TextElement::CharClass(
      RegExpCharacterClass* char_class) {
  TextElement result = TextElement(CHAR_CLASS);
  result.data.u_char_class = char_class;
  return result;
}


int TextElement::length() {
  if (type == ATOM) {
    return data.u_atom->length();
  } else {
    ASSERT(type == CHAR_CLASS);
    return 1;
  }
}


DispatchTable* ChoiceNode::GetTable(bool ignore_case) {
  if (table_ == NULL) {
    table_ = new DispatchTable();
    DispatchTableConstructor cons(table_, ignore_case);
    cons.BuildTable(this);
  }
  return table_;
}


class RegExpCompiler {
 public:
  RegExpCompiler(int capture_count, bool ignore_case, bool is_ascii);

  int AllocateRegister() {
    if (next_register_ >= RegExpMacroAssembler::kMaxRegister) {
      reg_exp_too_big_ = true;
      return next_register_;
    }
    return next_register_++;
  }

  RegExpEngine::CompilationResult Assemble(RegExpMacroAssembler* assembler,
                                           RegExpNode* start,
                                           int capture_count,
                                           Handle<String> pattern);

  inline void AddWork(RegExpNode* node) { work_list_->Add(node); }

  static const int kImplementationOffset = 0;
  static const int kNumberOfRegistersOffset = 0;
  static const int kCodeOffset = 1;

  RegExpMacroAssembler* macro_assembler() { return macro_assembler_; }
  EndNode* accept() { return accept_; }

  static const int kMaxRecursion = 100;
  inline int recursion_depth() { return recursion_depth_; }
  inline void IncrementRecursionDepth() { recursion_depth_++; }
  inline void DecrementRecursionDepth() { recursion_depth_--; }

  void SetRegExpTooBig() { reg_exp_too_big_ = true; }

  inline bool ignore_case() { return ignore_case_; }
  inline bool ascii() { return ascii_; }

  static const int kNoRegister = -1;
 private:
  EndNode* accept_;
  int next_register_;
  List<RegExpNode*>* work_list_;
  int recursion_depth_;
  RegExpMacroAssembler* macro_assembler_;
  bool ignore_case_;
  bool ascii_;
  bool reg_exp_too_big_;
};


class RecursionCheck {
 public:
  explicit RecursionCheck(RegExpCompiler* compiler) : compiler_(compiler) {
    compiler->IncrementRecursionDepth();
  }
  ~RecursionCheck() { compiler_->DecrementRecursionDepth(); }
 private:
  RegExpCompiler* compiler_;
};


static RegExpEngine::CompilationResult IrregexpRegExpTooBig() {
  return RegExpEngine::CompilationResult("RegExp too big");
}


// Attempts to compile the regexp using an Irregexp code generator.  Returns
// a fixed array or a null handle depending on whether it succeeded.
RegExpCompiler::RegExpCompiler(int capture_count, bool ignore_case, bool ascii)
    : next_register_(2 * (capture_count + 1)),
      work_list_(NULL),
      recursion_depth_(0),
      ignore_case_(ignore_case),
      ascii_(ascii),
      reg_exp_too_big_(false) {
  accept_ = new EndNode(EndNode::ACCEPT);
  ASSERT(next_register_ - 1 <= RegExpMacroAssembler::kMaxRegister);
}


RegExpEngine::CompilationResult RegExpCompiler::Assemble(
    RegExpMacroAssembler* macro_assembler,
    RegExpNode* start,
    int capture_count,
    Handle<String> pattern) {
#ifdef DEBUG
  if (FLAG_trace_regexp_assembler)
    macro_assembler_ = new RegExpMacroAssemblerTracer(macro_assembler);
  else
#endif
    macro_assembler_ = macro_assembler;
  List <RegExpNode*> work_list(0);
  work_list_ = &work_list;
  Label fail;
  macro_assembler_->PushBacktrack(&fail);
  Trace new_trace;
  start->Emit(this, &new_trace);
  macro_assembler_->Bind(&fail);
  macro_assembler_->Fail();
  while (!work_list.is_empty()) {
    work_list.RemoveLast()->Emit(this, &new_trace);
  }
  if (reg_exp_too_big_) return IrregexpRegExpTooBig();

  Handle<Object> code = macro_assembler_->GetCode(pattern);

  work_list_ = NULL;
#ifdef DEBUG
  if (FLAG_trace_regexp_assembler) {
    delete macro_assembler_;
  }
#endif
  return RegExpEngine::CompilationResult(*code, next_register_);
}


bool Trace::DeferredAction::Mentions(int that) {
  if (type() == ActionNode::CLEAR_CAPTURES) {
    Interval range = static_cast<DeferredClearCaptures*>(this)->range();
    return range.Contains(that);
  } else {
    return reg() == that;
  }
}


bool Trace::mentions_reg(int reg) {
  for (DeferredAction* action = actions_;
       action != NULL;
       action = action->next()) {
    if (action->Mentions(reg))
      return true;
  }
  return false;
}


bool Trace::GetStoredPosition(int reg, int* cp_offset) {
  ASSERT_EQ(0, *cp_offset);
  for (DeferredAction* action = actions_;
       action != NULL;
       action = action->next()) {
    if (action->Mentions(reg)) {
      if (action->type() == ActionNode::STORE_POSITION) {
        *cp_offset = static_cast<DeferredCapture*>(action)->cp_offset();
        return true;
      } else {
        return false;
      }
    }
  }
  return false;
}


int Trace::FindAffectedRegisters(OutSet* affected_registers) {
  int max_register = RegExpCompiler::kNoRegister;
  for (DeferredAction* action = actions_;
       action != NULL;
       action = action->next()) {
    if (action->type() == ActionNode::CLEAR_CAPTURES) {
      Interval range = static_cast<DeferredClearCaptures*>(action)->range();
      for (int i = range.from(); i <= range.to(); i++)
        affected_registers->Set(i);
      if (range.to() > max_register) max_register = range.to();
    } else {
      affected_registers->Set(action->reg());
      if (action->reg() > max_register) max_register = action->reg();
    }
  }
  return max_register;
}


void Trace::RestoreAffectedRegisters(RegExpMacroAssembler* assembler,
                                     int max_register,
                                     OutSet& registers_to_pop,
                                     OutSet& registers_to_clear) {
  for (int reg = max_register; reg >= 0; reg--) {
    if (registers_to_pop.Get(reg)) assembler->PopRegister(reg);
    else if (registers_to_clear.Get(reg)) {
      int clear_to = reg;
      while (reg > 0 && registers_to_clear.Get(reg - 1)) {
        reg--;
      }
      assembler->ClearRegisters(reg, clear_to);
    }
  }
}


void Trace::PerformDeferredActions(RegExpMacroAssembler* assembler,
                                   int max_register,
                                   OutSet& affected_registers,
                                   OutSet* registers_to_pop,
                                   OutSet* registers_to_clear) {
  // The "+1" is to avoid a push_limit of zero if stack_limit_slack() is 1.
  const int push_limit = (assembler->stack_limit_slack() + 1) / 2;

  for (int reg = 0; reg <= max_register; reg++) {
    if (!affected_registers.Get(reg)) {
      continue;
    }
    // Count pushes performed to force a stack limit check occasionally.
    int pushes = 0;

    // The chronologically first deferred action in the trace
    // is used to infer the action needed to restore a register
    // to its previous state (or not, if it's safe to ignore it).
    enum DeferredActionUndoType { IGNORE, RESTORE, CLEAR };
    DeferredActionUndoType undo_action = IGNORE;

    int value = 0;
    bool absolute = false;
    bool clear = false;
    int store_position = -1;
    // This is a little tricky because we are scanning the actions in reverse
    // historical order (newest first).
    for (DeferredAction* action = actions_;
         action != NULL;
         action = action->next()) {
      if (action->Mentions(reg)) {
        switch (action->type()) {
          case ActionNode::SET_REGISTER: {
            Trace::DeferredSetRegister* psr =
                static_cast<Trace::DeferredSetRegister*>(action);
            if (!absolute) {
              value += psr->value();
              absolute = true;
            }
            // SET_REGISTER is currently only used for newly introduced loop
            // counters. They can have a significant previous value if they
            // occour in a loop. TODO(lrn): Propagate this information, so
            // we can set undo_action to IGNORE if we know there is no value to
            // restore.
            undo_action = RESTORE;
            ASSERT_EQ(store_position, -1);
            ASSERT(!clear);
            break;
          }
          case ActionNode::INCREMENT_REGISTER:
            if (!absolute) {
              value++;
            }
            ASSERT_EQ(store_position, -1);
            ASSERT(!clear);
            undo_action = RESTORE;
            break;
          case ActionNode::STORE_POSITION: {
            Trace::DeferredCapture* pc =
                static_cast<Trace::DeferredCapture*>(action);
            if (!clear && store_position == -1) {
              store_position = pc->cp_offset();
            }

            // For captures we know that stores and clears alternate.
            // Other register, are never cleared, and if the occur
            // inside a loop, they might be assigned more than once.
            if (reg <= 1) {
              // Registers zero and one, aka "capture zero", is
              // always set correctly if we succeed. There is no
              // need to undo a setting on backtrack, because we
              // will set it again or fail.
              undo_action = IGNORE;
            } else {
              undo_action = pc->is_capture() ? CLEAR : RESTORE;
            }
            ASSERT(!absolute);
            ASSERT_EQ(value, 0);
            break;
          }
          case ActionNode::CLEAR_CAPTURES: {
            // Since we're scanning in reverse order, if we've already
            // set the position we have to ignore historically earlier
            // clearing operations.
            if (store_position == -1) {
              clear = true;
            }
            undo_action = RESTORE;
            ASSERT(!absolute);
            ASSERT_EQ(value, 0);
            break;
          }
          default:
            UNREACHABLE();
            break;
        }
      }
    }
    // Prepare for the undo-action (e.g., push if it's going to be popped).
    if (undo_action == RESTORE) {
      pushes++;
      RegExpMacroAssembler::StackCheckFlag stack_check =
          RegExpMacroAssembler::kNoStackLimitCheck;
      if (pushes == push_limit) {
        stack_check = RegExpMacroAssembler::kCheckStackLimit;
        pushes = 0;
      }

      assembler->PushRegister(reg, stack_check);
      registers_to_pop->Set(reg);
    } else if (undo_action == CLEAR) {
      registers_to_clear->Set(reg);
    }
    // Perform the chronologically last action (or accumulated increment)
    // for the register.
    if (store_position != -1) {
      assembler->WriteCurrentPositionToRegister(reg, store_position);
    } else if (clear) {
      assembler->ClearRegisters(reg, reg);
    } else if (absolute) {
      assembler->SetRegister(reg, value);
    } else if (value != 0) {
      assembler->AdvanceRegister(reg, value);
    }
  }
}


// This is called as we come into a loop choice node and some other tricky
// nodes.  It normalizes the state of the code generator to ensure we can
// generate generic code.
void Trace::Flush(RegExpCompiler* compiler, RegExpNode* successor) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();

  ASSERT(!is_trivial());

  if (actions_ == NULL && backtrack() == NULL) {
    // Here we just have some deferred cp advances to fix and we are back to
    // a normal situation.  We may also have to forget some information gained
    // through a quick check that was already performed.
    if (cp_offset_ != 0) assembler->AdvanceCurrentPosition(cp_offset_);
    // Create a new trivial state and generate the node with that.
    Trace new_state;
    successor->Emit(compiler, &new_state);
    return;
  }

  // Generate deferred actions here along with code to undo them again.
  OutSet affected_registers;

  if (backtrack() != NULL) {
    // Here we have a concrete backtrack location.  These are set up by choice
    // nodes and so they indicate that we have a deferred save of the current
    // position which we may need to emit here.
    assembler->PushCurrentPosition();
  }

  int max_register = FindAffectedRegisters(&affected_registers);
  OutSet registers_to_pop;
  OutSet registers_to_clear;
  PerformDeferredActions(assembler,
                         max_register,
                         affected_registers,
                         &registers_to_pop,
                         &registers_to_clear);
  if (cp_offset_ != 0) {
    assembler->AdvanceCurrentPosition(cp_offset_);
  }

  // Create a new trivial state and generate the node with that.
  Label undo;
  assembler->PushBacktrack(&undo);
  Trace new_state;
  successor->Emit(compiler, &new_state);

  // On backtrack we need to restore state.
  assembler->Bind(&undo);
  RestoreAffectedRegisters(assembler,
                           max_register,
                           registers_to_pop,
                           registers_to_clear);
  if (backtrack() == NULL) {
    assembler->Backtrack();
  } else {
    assembler->PopCurrentPosition();
    assembler->GoTo(backtrack());
  }
}


void NegativeSubmatchSuccess::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();

  // Omit flushing the trace. We discard the entire stack frame anyway.

  if (!label()->is_bound()) {
    // We are completely independent of the trace, since we ignore it,
    // so this code can be used as the generic version.
    assembler->Bind(label());
  }

  // Throw away everything on the backtrack stack since the start
  // of the negative submatch and restore the character position.
  assembler->ReadCurrentPositionFromRegister(current_position_register_);
  assembler->ReadStackPointerFromRegister(stack_pointer_register_);
  if (clear_capture_count_ > 0) {
    // Clear any captures that might have been performed during the success
    // of the body of the negative look-ahead.
    int clear_capture_end = clear_capture_start_ + clear_capture_count_ - 1;
    assembler->ClearRegisters(clear_capture_start_, clear_capture_end);
  }
  // Now that we have unwound the stack we find at the top of the stack the
  // backtrack that the BeginSubmatch node got.
  assembler->Backtrack();
}


void EndNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  if (!trace->is_trivial()) {
    trace->Flush(compiler, this);
    return;
  }
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  if (!label()->is_bound()) {
    assembler->Bind(label());
  }
  switch (action_) {
    case ACCEPT:
      assembler->Succeed();
      return;
    case BACKTRACK:
      assembler->GoTo(trace->backtrack());
      return;
    case NEGATIVE_SUBMATCH_SUCCESS:
      // This case is handled in a different virtual method.
      UNREACHABLE();
  }
  UNIMPLEMENTED();
}


void GuardedAlternative::AddGuard(Guard* guard) {
  if (guards_ == NULL)
    guards_ = new ZoneList<Guard*>(1);
  guards_->Add(guard);
}


ActionNode* ActionNode::SetRegister(int reg,
                                    int val,
                                    RegExpNode* on_success) {
  ActionNode* result = new ActionNode(SET_REGISTER, on_success);
  result->data_.u_store_register.reg = reg;
  result->data_.u_store_register.value = val;
  return result;
}


ActionNode* ActionNode::IncrementRegister(int reg, RegExpNode* on_success) {
  ActionNode* result = new ActionNode(INCREMENT_REGISTER, on_success);
  result->data_.u_increment_register.reg = reg;
  return result;
}


ActionNode* ActionNode::StorePosition(int reg,
                                      bool is_capture,
                                      RegExpNode* on_success) {
  ActionNode* result = new ActionNode(STORE_POSITION, on_success);
  result->data_.u_position_register.reg = reg;
  result->data_.u_position_register.is_capture = is_capture;
  return result;
}


ActionNode* ActionNode::ClearCaptures(Interval range,
                                      RegExpNode* on_success) {
  ActionNode* result = new ActionNode(CLEAR_CAPTURES, on_success);
  result->data_.u_clear_captures.range_from = range.from();
  result->data_.u_clear_captures.range_to = range.to();
  return result;
}


ActionNode* ActionNode::BeginSubmatch(int stack_reg,
                                      int position_reg,
                                      RegExpNode* on_success) {
  ActionNode* result = new ActionNode(BEGIN_SUBMATCH, on_success);
  result->data_.u_submatch.stack_pointer_register = stack_reg;
  result->data_.u_submatch.current_position_register = position_reg;
  return result;
}


ActionNode* ActionNode::PositiveSubmatchSuccess(int stack_reg,
                                                int position_reg,
                                                int clear_register_count,
                                                int clear_register_from,
                                                RegExpNode* on_success) {
  ActionNode* result = new ActionNode(POSITIVE_SUBMATCH_SUCCESS, on_success);
  result->data_.u_submatch.stack_pointer_register = stack_reg;
  result->data_.u_submatch.current_position_register = position_reg;
  result->data_.u_submatch.clear_register_count = clear_register_count;
  result->data_.u_submatch.clear_register_from = clear_register_from;
  return result;
}


ActionNode* ActionNode::EmptyMatchCheck(int start_register,
                                        int repetition_register,
                                        int repetition_limit,
                                        RegExpNode* on_success) {
  ActionNode* result = new ActionNode(EMPTY_MATCH_CHECK, on_success);
  result->data_.u_empty_match_check.start_register = start_register;
  result->data_.u_empty_match_check.repetition_register = repetition_register;
  result->data_.u_empty_match_check.repetition_limit = repetition_limit;
  return result;
}


#define DEFINE_ACCEPT(Type)                                          \
  void Type##Node::Accept(NodeVisitor* visitor) {                    \
    visitor->Visit##Type(this);                                      \
  }
FOR_EACH_NODE_TYPE(DEFINE_ACCEPT)
#undef DEFINE_ACCEPT


void LoopChoiceNode::Accept(NodeVisitor* visitor) {
  visitor->VisitLoopChoice(this);
}


// -------------------------------------------------------------------
// Emit code.


void ChoiceNode::GenerateGuard(RegExpMacroAssembler* macro_assembler,
                               Guard* guard,
                               Trace* trace) {
  switch (guard->op()) {
    case Guard::LT:
      ASSERT(!trace->mentions_reg(guard->reg()));
      macro_assembler->IfRegisterGE(guard->reg(),
                                    guard->value(),
                                    trace->backtrack());
      break;
    case Guard::GEQ:
      ASSERT(!trace->mentions_reg(guard->reg()));
      macro_assembler->IfRegisterLT(guard->reg(),
                                    guard->value(),
                                    trace->backtrack());
      break;
  }
}


static unibrow::Mapping<unibrow::Ecma262UnCanonicalize> uncanonicalize;
static unibrow::Mapping<unibrow::CanonicalizationRange> canonrange;


// Returns the number of characters in the equivalence class, omitting those
// that cannot occur in the source string because it is ASCII.
static int GetCaseIndependentLetters(uc16 character,
                                     bool ascii_subject,
                                     unibrow::uchar* letters) {
  int length = uncanonicalize.get(character, '\0', letters);
  // Unibrow returns 0 or 1 for characters where case independependence is
  // trivial.
  if (length == 0) {
    letters[0] = character;
    length = 1;
  }
  if (!ascii_subject || character <= String::kMaxAsciiCharCode) {
    return length;
  }
  // The standard requires that non-ASCII characters cannot have ASCII
  // character codes in their equivalence class.
  return 0;
}


static inline bool EmitSimpleCharacter(RegExpCompiler* compiler,
                                       uc16 c,
                                       Label* on_failure,
                                       int cp_offset,
                                       bool check,
                                       bool preloaded) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  bool bound_checked = false;
  if (!preloaded) {
    assembler->LoadCurrentCharacter(
        cp_offset,
        on_failure,
        check);
    bound_checked = true;
  }
  assembler->CheckNotCharacter(c, on_failure);
  return bound_checked;
}


// Only emits non-letters (things that don't have case).  Only used for case
// independent matches.
static inline bool EmitAtomNonLetter(RegExpCompiler* compiler,
                                     uc16 c,
                                     Label* on_failure,
                                     int cp_offset,
                                     bool check,
                                     bool preloaded) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  bool ascii = compiler->ascii();
  unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
  int length = GetCaseIndependentLetters(c, ascii, chars);
  if (length < 1) {
    // This can't match.  Must be an ASCII subject and a non-ASCII character.
    // We do not need to do anything since the ASCII pass already handled this.
    return false;  // Bounds not checked.
  }
  bool checked = false;
  // We handle the length > 1 case in a later pass.
  if (length == 1) {
    if (ascii && c > String::kMaxAsciiCharCodeU) {
      // Can't match - see above.
      return false;  // Bounds not checked.
    }
    if (!preloaded) {
      macro_assembler->LoadCurrentCharacter(cp_offset, on_failure, check);
      checked = check;
    }
    macro_assembler->CheckNotCharacter(c, on_failure);
  }
  return checked;
}


static bool ShortCutEmitCharacterPair(RegExpMacroAssembler* macro_assembler,
                                      bool ascii,
                                      uc16 c1,
                                      uc16 c2,
                                      Label* on_failure) {
  uc16 char_mask;
  if (ascii) {
    char_mask = String::kMaxAsciiCharCode;
  } else {
    char_mask = String::kMaxUC16CharCode;
  }
  uc16 exor = c1 ^ c2;
  // Check whether exor has only one bit set.
  if (((exor - 1) & exor) == 0) {
    // If c1 and c2 differ only by one bit.
    // Ecma262UnCanonicalize always gives the highest number last.
    ASSERT(c2 > c1);
    uc16 mask = char_mask ^ exor;
    macro_assembler->CheckNotCharacterAfterAnd(c1, mask, on_failure);
    return true;
  }
  ASSERT(c2 > c1);
  uc16 diff = c2 - c1;
  if (((diff - 1) & diff) == 0 && c1 >= diff) {
    // If the characters differ by 2^n but don't differ by one bit then
    // subtract the difference from the found character, then do the or
    // trick.  We avoid the theoretical case where negative numbers are
    // involved in order to simplify code generation.
    uc16 mask = char_mask ^ diff;
    macro_assembler->CheckNotCharacterAfterMinusAnd(c1 - diff,
                                                    diff,
                                                    mask,
                                                    on_failure);
    return true;
  }
  return false;
}


typedef bool EmitCharacterFunction(RegExpCompiler* compiler,
                                   uc16 c,
                                   Label* on_failure,
                                   int cp_offset,
                                   bool check,
                                   bool preloaded);

// Only emits letters (things that have case).  Only used for case independent
// matches.
static inline bool EmitAtomLetter(RegExpCompiler* compiler,
                                  uc16 c,
                                  Label* on_failure,
                                  int cp_offset,
                                  bool check,
                                  bool preloaded) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  bool ascii = compiler->ascii();
  unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
  int length = GetCaseIndependentLetters(c, ascii, chars);
  if (length <= 1) return false;
  // We may not need to check against the end of the input string
  // if this character lies before a character that matched.
  if (!preloaded) {
    macro_assembler->LoadCurrentCharacter(cp_offset, on_failure, check);
  }
  Label ok;
  ASSERT(unibrow::Ecma262UnCanonicalize::kMaxWidth == 4);
  switch (length) {
    case 2: {
      if (ShortCutEmitCharacterPair(macro_assembler,
                                    ascii,
                                    chars[0],
                                    chars[1],
                                    on_failure)) {
      } else {
        macro_assembler->CheckCharacter(chars[0], &ok);
        macro_assembler->CheckNotCharacter(chars[1], on_failure);
        macro_assembler->Bind(&ok);
      }
      break;
    }
    case 4:
      macro_assembler->CheckCharacter(chars[3], &ok);
      // Fall through!
    case 3:
      macro_assembler->CheckCharacter(chars[0], &ok);
      macro_assembler->CheckCharacter(chars[1], &ok);
      macro_assembler->CheckNotCharacter(chars[2], on_failure);
      macro_assembler->Bind(&ok);
      break;
    default:
      UNREACHABLE();
      break;
  }
  return true;
}


static void EmitCharClass(RegExpMacroAssembler* macro_assembler,
                          RegExpCharacterClass* cc,
                          bool ascii,
                          Label* on_failure,
                          int cp_offset,
                          bool check_offset,
                          bool preloaded) {
  if (cc->is_standard() &&
      macro_assembler->CheckSpecialCharacterClass(cc->standard_type(),
                                                  cp_offset,
                                                  check_offset,
                                                  on_failure)) {
    return;
  }

  ZoneList<CharacterRange>* ranges = cc->ranges();
  int max_char;
  if (ascii) {
    max_char = String::kMaxAsciiCharCode;
  } else {
    max_char = String::kMaxUC16CharCode;
  }

  Label success;

  Label* char_is_in_class =
      cc->is_negated() ? on_failure : &success;

  int range_count = ranges->length();

  int last_valid_range = range_count - 1;
  while (last_valid_range >= 0) {
    CharacterRange& range = ranges->at(last_valid_range);
    if (range.from() <= max_char) {
      break;
    }
    last_valid_range--;
  }

  if (last_valid_range < 0) {
    if (!cc->is_negated()) {
      // TODO(plesner): We can remove this when the node level does our
      // ASCII optimizations for us.
      macro_assembler->GoTo(on_failure);
    }
    if (check_offset) {
      macro_assembler->CheckPosition(cp_offset, on_failure);
    }
    return;
  }

  if (last_valid_range == 0 &&
      !cc->is_negated() &&
      ranges->at(0).IsEverything(max_char)) {
    // This is a common case hit by non-anchored expressions.
    if (check_offset) {
      macro_assembler->CheckPosition(cp_offset, on_failure);
    }
    return;
  }

  if (!preloaded) {
    macro_assembler->LoadCurrentCharacter(cp_offset, on_failure, check_offset);
  }

  for (int i = 0; i < last_valid_range; i++) {
    CharacterRange& range = ranges->at(i);
    Label next_range;
    uc16 from = range.from();
    uc16 to = range.to();
    if (from > max_char) {
      continue;
    }
    if (to > max_char) to = max_char;
    if (to == from) {
      macro_assembler->CheckCharacter(to, char_is_in_class);
    } else {
      if (from != 0) {
        macro_assembler->CheckCharacterLT(from, &next_range);
      }
      if (to != max_char) {
        macro_assembler->CheckCharacterLT(to + 1, char_is_in_class);
      } else {
        macro_assembler->GoTo(char_is_in_class);
      }
    }
    macro_assembler->Bind(&next_range);
  }

  CharacterRange& range = ranges->at(last_valid_range);
  uc16 from = range.from();
  uc16 to = range.to();

  if (to > max_char) to = max_char;
  ASSERT(to >= from);

  if (to == from) {
    if (cc->is_negated()) {
      macro_assembler->CheckCharacter(to, on_failure);
    } else {
      macro_assembler->CheckNotCharacter(to, on_failure);
    }
  } else {
    if (from != 0) {
      if (cc->is_negated()) {
        macro_assembler->CheckCharacterLT(from, &success);
      } else {
        macro_assembler->CheckCharacterLT(from, on_failure);
      }
    }
    if (to != String::kMaxUC16CharCode) {
      if (cc->is_negated()) {
        macro_assembler->CheckCharacterLT(to + 1, on_failure);
      } else {
        macro_assembler->CheckCharacterGT(to, on_failure);
      }
    } else {
      if (cc->is_negated()) {
        macro_assembler->GoTo(on_failure);
      }
    }
  }
  macro_assembler->Bind(&success);
}


RegExpNode::~RegExpNode() {
}


RegExpNode::LimitResult RegExpNode::LimitVersions(RegExpCompiler* compiler,
                                                  Trace* trace) {
  // If we are generating a greedy loop then don't stop and don't reuse code.
  if (trace->stop_node() != NULL) {
    return CONTINUE;
  }

  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  if (trace->is_trivial()) {
    if (label_.is_bound()) {
      // We are being asked to generate a generic version, but that's already
      // been done so just go to it.
      macro_assembler->GoTo(&label_);
      return DONE;
    }
    if (compiler->recursion_depth() >= RegExpCompiler::kMaxRecursion) {
      // To avoid too deep recursion we push the node to the work queue and just
      // generate a goto here.
      compiler->AddWork(this);
      macro_assembler->GoTo(&label_);
      return DONE;
    }
    // Generate generic version of the node and bind the label for later use.
    macro_assembler->Bind(&label_);
    return CONTINUE;
  }

  // We are being asked to make a non-generic version.  Keep track of how many
  // non-generic versions we generate so as not to overdo it.
  trace_count_++;
  if (FLAG_regexp_optimization &&
      trace_count_ < kMaxCopiesCodeGenerated &&
      compiler->recursion_depth() <= RegExpCompiler::kMaxRecursion) {
    return CONTINUE;
  }

  // If we get here code has been generated for this node too many times or
  // recursion is too deep.  Time to switch to a generic version.  The code for
  // generic versions above can handle deep recursion properly.
  trace->Flush(compiler, this);
  return DONE;
}


int ActionNode::EatsAtLeast(int still_to_find, int recursion_depth) {
  if (recursion_depth > RegExpCompiler::kMaxRecursion) return 0;
  if (type_ == POSITIVE_SUBMATCH_SUCCESS) return 0;  // Rewinds input!
  return on_success()->EatsAtLeast(still_to_find, recursion_depth + 1);
}


int AssertionNode::EatsAtLeast(int still_to_find, int recursion_depth) {
  if (recursion_depth > RegExpCompiler::kMaxRecursion) return 0;
  return on_success()->EatsAtLeast(still_to_find, recursion_depth + 1);
}


int BackReferenceNode::EatsAtLeast(int still_to_find, int recursion_depth) {
  if (recursion_depth > RegExpCompiler::kMaxRecursion) return 0;
  return on_success()->EatsAtLeast(still_to_find, recursion_depth + 1);
}


int TextNode::EatsAtLeast(int still_to_find, int recursion_depth) {
  int answer = Length();
  if (answer >= still_to_find) return answer;
  if (recursion_depth > RegExpCompiler::kMaxRecursion) return answer;
  return answer + on_success()->EatsAtLeast(still_to_find - answer,
                                            recursion_depth + 1);
}


int NegativeLookaheadChoiceNode:: EatsAtLeast(int still_to_find,
                                              int recursion_depth) {
  if (recursion_depth > RegExpCompiler::kMaxRecursion) return 0;
  // Alternative 0 is the negative lookahead, alternative 1 is what comes
  // afterwards.
  RegExpNode* node = alternatives_->at(1).node();
  return node->EatsAtLeast(still_to_find, recursion_depth + 1);
}


void NegativeLookaheadChoiceNode::GetQuickCheckDetails(
    QuickCheckDetails* details,
    RegExpCompiler* compiler,
    int filled_in,
    bool not_at_start) {
  // Alternative 0 is the negative lookahead, alternative 1 is what comes
  // afterwards.
  RegExpNode* node = alternatives_->at(1).node();
  return node->GetQuickCheckDetails(details, compiler, filled_in, not_at_start);
}


int ChoiceNode::EatsAtLeastHelper(int still_to_find,
                                  int recursion_depth,
                                  RegExpNode* ignore_this_node) {
  if (recursion_depth > RegExpCompiler::kMaxRecursion) return 0;
  int min = 100;
  int choice_count = alternatives_->length();
  for (int i = 0; i < choice_count; i++) {
    RegExpNode* node = alternatives_->at(i).node();
    if (node == ignore_this_node) continue;
    int node_eats_at_least = node->EatsAtLeast(still_to_find,
                                               recursion_depth + 1);
    if (node_eats_at_least < min) min = node_eats_at_least;
  }
  return min;
}


int LoopChoiceNode::EatsAtLeast(int still_to_find, int recursion_depth) {
  return EatsAtLeastHelper(still_to_find, recursion_depth, loop_node_);
}


int ChoiceNode::EatsAtLeast(int still_to_find, int recursion_depth) {
  return EatsAtLeastHelper(still_to_find, recursion_depth, NULL);
}


// Takes the left-most 1-bit and smears it out, setting all bits to its right.
static inline uint32_t SmearBitsRight(uint32_t v) {
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v;
}


bool QuickCheckDetails::Rationalize(bool asc) {
  bool found_useful_op = false;
  uint32_t char_mask;
  if (asc) {
    char_mask = String::kMaxAsciiCharCode;
  } else {
    char_mask = String::kMaxUC16CharCode;
  }
  mask_ = 0;
  value_ = 0;
  int char_shift = 0;
  for (int i = 0; i < characters_; i++) {
    Position* pos = &positions_[i];
    if ((pos->mask & String::kMaxAsciiCharCode) != 0) {
      found_useful_op = true;
    }
    mask_ |= (pos->mask & char_mask) << char_shift;
    value_ |= (pos->value & char_mask) << char_shift;
    char_shift += asc ? 8 : 16;
  }
  return found_useful_op;
}


bool RegExpNode::EmitQuickCheck(RegExpCompiler* compiler,
                                Trace* trace,
                                bool preload_has_checked_bounds,
                                Label* on_possible_success,
                                QuickCheckDetails* details,
                                bool fall_through_on_failure) {
  if (details->characters() == 0) return false;
  GetQuickCheckDetails(details, compiler, 0, trace->at_start() == Trace::FALSE);
  if (details->cannot_match()) return false;
  if (!details->Rationalize(compiler->ascii())) return false;
  uint32_t mask = details->mask();
  uint32_t value = details->value();

  RegExpMacroAssembler* assembler = compiler->macro_assembler();

  if (trace->characters_preloaded() != details->characters()) {
    assembler->LoadCurrentCharacter(trace->cp_offset(),
                                    trace->backtrack(),
                                    !preload_has_checked_bounds,
                                    details->characters());
  }


  bool need_mask = true;

  if (details->characters() == 1) {
    // If number of characters preloaded is 1 then we used a byte or 16 bit
    // load so the value is already masked down.
    uint32_t char_mask;
    if (compiler->ascii()) {
      char_mask = String::kMaxAsciiCharCode;
    } else {
      char_mask = String::kMaxUC16CharCode;
    }
    if ((mask & char_mask) == char_mask) need_mask = false;
    mask &= char_mask;
  } else {
    // For 2-character preloads in ASCII mode we also use a 16 bit load with
    // zero extend.
    if (details->characters() == 2 && compiler->ascii()) {
      if ((mask & 0xffff) == 0xffff) need_mask = false;
    } else {
      if (mask == 0xffffffff) need_mask = false;
    }
  }

  if (fall_through_on_failure) {
    if (need_mask) {
      assembler->CheckCharacterAfterAnd(value, mask, on_possible_success);
    } else {
      assembler->CheckCharacter(value, on_possible_success);
    }
  } else {
    if (need_mask) {
      assembler->CheckNotCharacterAfterAnd(value, mask, trace->backtrack());
    } else {
      assembler->CheckNotCharacter(value, trace->backtrack());
    }
  }
  return true;
}


// Here is the meat of GetQuickCheckDetails (see also the comment on the
// super-class in the .h file).
//
// We iterate along the text object, building up for each character a
// mask and value that can be used to test for a quick failure to match.
// The masks and values for the positions will be combined into a single
// machine word for the current character width in order to be used in
// generating a quick check.
void TextNode::GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start) {
  ASSERT(characters_filled_in < details->characters());
  int characters = details->characters();
  int char_mask;
  int char_shift;
  if (compiler->ascii()) {
    char_mask = String::kMaxAsciiCharCode;
    char_shift = 8;
  } else {
    char_mask = String::kMaxUC16CharCode;
    char_shift = 16;
  }
  for (int k = 0; k < elms_->length(); k++) {
    TextElement elm = elms_->at(k);
    if (elm.type == TextElement::ATOM) {
      Vector<const uc16> quarks = elm.data.u_atom->data();
      for (int i = 0; i < characters && i < quarks.length(); i++) {
        QuickCheckDetails::Position* pos =
            details->positions(characters_filled_in);
        uc16 c = quarks[i];
        if (c > char_mask) {
          // If we expect a non-ASCII character from an ASCII string,
          // there is no way we can match. Not even case independent
          // matching can turn an ASCII character into non-ASCII or
          // vice versa.
          details->set_cannot_match();
          pos->determines_perfectly = false;
          return;
        }
        if (compiler->ignore_case()) {
          unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
          int length = GetCaseIndependentLetters(c, compiler->ascii(), chars);
          ASSERT(length != 0);  // Can only happen if c > char_mask (see above).
          if (length == 1) {
            // This letter has no case equivalents, so it's nice and simple
            // and the mask-compare will determine definitely whether we have
            // a match at this character position.
            pos->mask = char_mask;
            pos->value = c;
            pos->determines_perfectly = true;
          } else {
            uint32_t common_bits = char_mask;
            uint32_t bits = chars[0];
            for (int j = 1; j < length; j++) {
              uint32_t differing_bits = ((chars[j] & common_bits) ^ bits);
              common_bits ^= differing_bits;
              bits &= common_bits;
            }
            // If length is 2 and common bits has only one zero in it then
            // our mask and compare instruction will determine definitely
            // whether we have a match at this character position.  Otherwise
            // it can only be an approximate check.
            uint32_t one_zero = (common_bits | ~char_mask);
            if (length == 2 && ((~one_zero) & ((~one_zero) - 1)) == 0) {
              pos->determines_perfectly = true;
            }
            pos->mask = common_bits;
            pos->value = bits;
          }
        } else {
          // Don't ignore case.  Nice simple case where the mask-compare will
          // determine definitely whether we have a match at this character
          // position.
          pos->mask = char_mask;
          pos->value = c;
          pos->determines_perfectly = true;
        }
        characters_filled_in++;
        ASSERT(characters_filled_in <= details->characters());
        if (characters_filled_in == details->characters()) {
          return;
        }
      }
    } else {
      QuickCheckDetails::Position* pos =
          details->positions(characters_filled_in);
      RegExpCharacterClass* tree = elm.data.u_char_class;
      ZoneList<CharacterRange>* ranges = tree->ranges();
      if (tree->is_negated()) {
        // A quick check uses multi-character mask and compare.  There is no
        // useful way to incorporate a negative char class into this scheme
        // so we just conservatively create a mask and value that will always
        // succeed.
        pos->mask = 0;
        pos->value = 0;
      } else {
        int first_range = 0;
        while (ranges->at(first_range).from() > char_mask) {
          first_range++;
          if (first_range == ranges->length()) {
            details->set_cannot_match();
            pos->determines_perfectly = false;
            return;
          }
        }
        CharacterRange range = ranges->at(first_range);
        uc16 from = range.from();
        uc16 to = range.to();
        if (to > char_mask) {
          to = char_mask;
        }
        uint32_t differing_bits = (from ^ to);
        // A mask and compare is only perfect if the differing bits form a
        // number like 00011111 with one single block of trailing 1s.
        if ((differing_bits & (differing_bits + 1)) == 0) {
          pos->determines_perfectly = true;
        }
        uint32_t common_bits = ~SmearBitsRight(differing_bits);
        uint32_t bits = (from & common_bits);
        for (int i = first_range + 1; i < ranges->length(); i++) {
          CharacterRange range = ranges->at(i);
          uc16 from = range.from();
          uc16 to = range.to();
          if (from > char_mask) continue;
          if (to > char_mask) to = char_mask;
          // Here we are combining more ranges into the mask and compare
          // value.  With each new range the mask becomes more sparse and
          // so the chances of a false positive rise.  A character class
          // with multiple ranges is assumed never to be equivalent to a
          // mask and compare operation.
          pos->determines_perfectly = false;
          uint32_t new_common_bits = (from ^ to);
          new_common_bits = ~SmearBitsRight(new_common_bits);
          common_bits &= new_common_bits;
          bits &= new_common_bits;
          uint32_t differing_bits = (from & common_bits) ^ bits;
          common_bits ^= differing_bits;
          bits &= common_bits;
        }
        pos->mask = common_bits;
        pos->value = bits;
      }
      characters_filled_in++;
      ASSERT(characters_filled_in <= details->characters());
      if (characters_filled_in == details->characters()) {
        return;
      }
    }
  }
  ASSERT(characters_filled_in != details->characters());
  on_success()-> GetQuickCheckDetails(details,
                                      compiler,
                                      characters_filled_in,
                                      true);
}


void QuickCheckDetails::Clear() {
  for (int i = 0; i < characters_; i++) {
    positions_[i].mask = 0;
    positions_[i].value = 0;
    positions_[i].determines_perfectly = false;
  }
  characters_ = 0;
}


void QuickCheckDetails::Advance(int by, bool ascii) {
  ASSERT(by >= 0);
  if (by >= characters_) {
    Clear();
    return;
  }
  for (int i = 0; i < characters_ - by; i++) {
    positions_[i] = positions_[by + i];
  }
  for (int i = characters_ - by; i < characters_; i++) {
    positions_[i].mask = 0;
    positions_[i].value = 0;
    positions_[i].determines_perfectly = false;
  }
  characters_ -= by;
  // We could change mask_ and value_ here but we would never advance unless
  // they had already been used in a check and they won't be used again because
  // it would gain us nothing.  So there's no point.
}


void QuickCheckDetails::Merge(QuickCheckDetails* other, int from_index) {
  ASSERT(characters_ == other->characters_);
  if (other->cannot_match_) {
    return;
  }
  if (cannot_match_) {
    *this = *other;
    return;
  }
  for (int i = from_index; i < characters_; i++) {
    QuickCheckDetails::Position* pos = positions(i);
    QuickCheckDetails::Position* other_pos = other->positions(i);
    if (pos->mask != other_pos->mask ||
        pos->value != other_pos->value ||
        !other_pos->determines_perfectly) {
      // Our mask-compare operation will be approximate unless we have the
      // exact same operation on both sides of the alternation.
      pos->determines_perfectly = false;
    }
    pos->mask &= other_pos->mask;
    pos->value &= pos->mask;
    other_pos->value &= pos->mask;
    uc16 differing_bits = (pos->value ^ other_pos->value);
    pos->mask &= ~differing_bits;
    pos->value &= pos->mask;
  }
}


class VisitMarker {
 public:
  explicit VisitMarker(NodeInfo* info) : info_(info) {
    ASSERT(!info->visited);
    info->visited = true;
  }
  ~VisitMarker() {
    info_->visited = false;
  }
 private:
  NodeInfo* info_;
};


void LoopChoiceNode::GetQuickCheckDetails(QuickCheckDetails* details,
                                          RegExpCompiler* compiler,
                                          int characters_filled_in,
                                          bool not_at_start) {
  if (body_can_be_zero_length_ || info()->visited) return;
  VisitMarker marker(info());
  return ChoiceNode::GetQuickCheckDetails(details,
                                          compiler,
                                          characters_filled_in,
                                          not_at_start);
}


void ChoiceNode::GetQuickCheckDetails(QuickCheckDetails* details,
                                      RegExpCompiler* compiler,
                                      int characters_filled_in,
                                      bool not_at_start) {
  not_at_start = (not_at_start || not_at_start_);
  int choice_count = alternatives_->length();
  ASSERT(choice_count > 0);
  alternatives_->at(0).node()->GetQuickCheckDetails(details,
                                                    compiler,
                                                    characters_filled_in,
                                                    not_at_start);
  for (int i = 1; i < choice_count; i++) {
    QuickCheckDetails new_details(details->characters());
    RegExpNode* node = alternatives_->at(i).node();
    node->GetQuickCheckDetails(&new_details, compiler,
                               characters_filled_in,
                               not_at_start);
    // Here we merge the quick match details of the two branches.
    details->Merge(&new_details, characters_filled_in);
  }
}


// Check for [0-9A-Z_a-z].
static void EmitWordCheck(RegExpMacroAssembler* assembler,
                          Label* word,
                          Label* non_word,
                          bool fall_through_on_word) {
  assembler->CheckCharacterGT('z', non_word);
  assembler->CheckCharacterLT('0', non_word);
  assembler->CheckCharacterGT('a' - 1, word);
  assembler->CheckCharacterLT('9' + 1, word);
  assembler->CheckCharacterLT('A', non_word);
  assembler->CheckCharacterLT('Z' + 1, word);
  if (fall_through_on_word) {
    assembler->CheckNotCharacter('_', non_word);
  } else {
    assembler->CheckCharacter('_', word);
  }
}


// Emit the code to check for a ^ in multiline mode (1-character lookbehind
// that matches newline or the start of input).
static void EmitHat(RegExpCompiler* compiler,
                    RegExpNode* on_success,
                    Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  // We will be loading the previous character into the current character
  // register.
  Trace new_trace(*trace);
  new_trace.InvalidateCurrentCharacter();

  Label ok;
  if (new_trace.cp_offset() == 0) {
    // The start of input counts as a newline in this context, so skip to
    // ok if we are at the start.
    assembler->CheckAtStart(&ok);
  }
  // We already checked that we are not at the start of input so it must be
  // OK to load the previous character.
  assembler->LoadCurrentCharacter(new_trace.cp_offset() -1,
                                  new_trace.backtrack(),
                                  false);
  // Newline means \n, \r, 0x2028 or 0x2029.
  if (!compiler->ascii()) {
    assembler->CheckCharacterAfterAnd(0x2028, 0xfffe, &ok);
  }
  assembler->CheckCharacter('\n', &ok);
  assembler->CheckNotCharacter('\r', new_trace.backtrack());
  assembler->Bind(&ok);
  on_success->Emit(compiler, &new_trace);
}


// Emit the code to handle \b and \B (word-boundary or non-word-boundary).
static void EmitBoundaryCheck(AssertionNode::AssertionNodeType type,
                              RegExpCompiler* compiler,
                              RegExpNode* on_success,
                              Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  Label before_non_word;
  Label before_word;
  if (trace->characters_preloaded() != 1) {
    assembler->LoadCurrentCharacter(trace->cp_offset(), &before_non_word);
  }
  // Fall through on non-word.
  EmitWordCheck(assembler, &before_word, &before_non_word, false);

  // We will be loading the previous character into the current character
  // register.
  Trace new_trace(*trace);
  new_trace.InvalidateCurrentCharacter();

  Label ok;
  Label* boundary;
  Label* not_boundary;
  if (type == AssertionNode::AT_BOUNDARY) {
    boundary = &ok;
    not_boundary = new_trace.backtrack();
  } else {
    not_boundary = &ok;
    boundary = new_trace.backtrack();
  }

  // Next character is not a word character.
  assembler->Bind(&before_non_word);
  if (new_trace.cp_offset() == 0) {
    // The start of input counts as a non-word character, so the question is
    // decided if we are at the start.
    assembler->CheckAtStart(not_boundary);
  }
  // We already checked that we are not at the start of input so it must be
  // OK to load the previous character.
  assembler->LoadCurrentCharacter(new_trace.cp_offset() - 1,
                                  &ok,  // Unused dummy label in this call.
                                  false);
  // Fall through on non-word.
  EmitWordCheck(assembler, boundary, not_boundary, false);
  assembler->GoTo(not_boundary);

  // Next character is a word character.
  assembler->Bind(&before_word);
  if (new_trace.cp_offset() == 0) {
    // The start of input counts as a non-word character, so the question is
    // decided if we are at the start.
    assembler->CheckAtStart(boundary);
  }
  // We already checked that we are not at the start of input so it must be
  // OK to load the previous character.
  assembler->LoadCurrentCharacter(new_trace.cp_offset() - 1,
                                  &ok,  // Unused dummy label in this call.
                                  false);
  bool fall_through_on_word = (type == AssertionNode::AT_NON_BOUNDARY);
  EmitWordCheck(assembler, not_boundary, boundary, fall_through_on_word);

  assembler->Bind(&ok);

  on_success->Emit(compiler, &new_trace);
}


void AssertionNode::GetQuickCheckDetails(QuickCheckDetails* details,
                                         RegExpCompiler* compiler,
                                         int filled_in,
                                         bool not_at_start) {
  if (type_ == AT_START && not_at_start) {
    details->set_cannot_match();
    return;
  }
  return on_success()->GetQuickCheckDetails(details,
                                            compiler,
                                            filled_in,
                                            not_at_start);
}


void AssertionNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  switch (type_) {
    case AT_END: {
      Label ok;
      assembler->CheckPosition(trace->cp_offset(), &ok);
      assembler->GoTo(trace->backtrack());
      assembler->Bind(&ok);
      break;
    }
    case AT_START: {
      if (trace->at_start() == Trace::FALSE) {
        assembler->GoTo(trace->backtrack());
        return;
      }
      if (trace->at_start() == Trace::UNKNOWN) {
        assembler->CheckNotAtStart(trace->backtrack());
        Trace at_start_trace = *trace;
        at_start_trace.set_at_start(true);
        on_success()->Emit(compiler, &at_start_trace);
        return;
      }
    }
    break;
    case AFTER_NEWLINE:
      EmitHat(compiler, on_success(), trace);
      return;
    case AT_NON_BOUNDARY:
    case AT_BOUNDARY:
      EmitBoundaryCheck(type_, compiler, on_success(), trace);
      return;
  }
  on_success()->Emit(compiler, trace);
}


static bool DeterminedAlready(QuickCheckDetails* quick_check, int offset) {
  if (quick_check == NULL) return false;
  if (offset >= quick_check->characters()) return false;
  return quick_check->positions(offset)->determines_perfectly;
}


static void UpdateBoundsCheck(int index, int* checked_up_to) {
  if (index > *checked_up_to) {
    *checked_up_to = index;
  }
}


// We call this repeatedly to generate code for each pass over the text node.
// The passes are in increasing order of difficulty because we hope one
// of the first passes will fail in which case we are saved the work of the
// later passes.  for example for the case independent regexp /%[asdfghjkl]a/
// we will check the '%' in the first pass, the case independent 'a' in the
// second pass and the character class in the last pass.
//
// The passes are done from right to left, so for example to test for /bar/
// we will first test for an 'r' with offset 2, then an 'a' with offset 1
// and then a 'b' with offset 0.  This means we can avoid the end-of-input
// bounds check most of the time.  In the example we only need to check for
// end-of-input when loading the putative 'r'.
//
// A slight complication involves the fact that the first character may already
// be fetched into a register by the previous node.  In this case we want to
// do the test for that character first.  We do this in separate passes.  The
// 'preloaded' argument indicates that we are doing such a 'pass'.  If such a
// pass has been performed then subsequent passes will have true in
// first_element_checked to indicate that that character does not need to be
// checked again.
//
// In addition to all this we are passed a Trace, which can
// contain an AlternativeGeneration object.  In this AlternativeGeneration
// object we can see details of any quick check that was already passed in
// order to get to the code we are now generating.  The quick check can involve
// loading characters, which means we do not need to recheck the bounds
// up to the limit the quick check already checked.  In addition the quick
// check can have involved a mask and compare operation which may simplify
// or obviate the need for further checks at some character positions.
void TextNode::TextEmitPass(RegExpCompiler* compiler,
                            TextEmitPassType pass,
                            bool preloaded,
                            Trace* trace,
                            bool first_element_checked,
                            int* checked_up_to) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  bool ascii = compiler->ascii();
  Label* backtrack = trace->backtrack();
  QuickCheckDetails* quick_check = trace->quick_check_performed();
  int element_count = elms_->length();
  for (int i = preloaded ? 0 : element_count - 1; i >= 0; i--) {
    TextElement elm = elms_->at(i);
    int cp_offset = trace->cp_offset() + elm.cp_offset;
    if (elm.type == TextElement::ATOM) {
      Vector<const uc16> quarks = elm.data.u_atom->data();
      for (int j = preloaded ? 0 : quarks.length() - 1; j >= 0; j--) {
        if (first_element_checked && i == 0 && j == 0) continue;
        if (DeterminedAlready(quick_check, elm.cp_offset + j)) continue;
        EmitCharacterFunction* emit_function = NULL;
        switch (pass) {
          case NON_ASCII_MATCH:
            ASSERT(ascii);
            if (quarks[j] > String::kMaxAsciiCharCode) {
              assembler->GoTo(backtrack);
              return;
            }
            break;
          case NON_LETTER_CHARACTER_MATCH:
            emit_function = &EmitAtomNonLetter;
            break;
          case SIMPLE_CHARACTER_MATCH:
            emit_function = &EmitSimpleCharacter;
            break;
          case CASE_CHARACTER_MATCH:
            emit_function = &EmitAtomLetter;
            break;
          default:
            break;
        }
        if (emit_function != NULL) {
          bool bound_checked = emit_function(compiler,
                                             quarks[j],
                                             backtrack,
                                             cp_offset + j,
                                             *checked_up_to < cp_offset + j,
                                             preloaded);
          if (bound_checked) UpdateBoundsCheck(cp_offset + j, checked_up_to);
        }
      }
    } else {
      ASSERT_EQ(elm.type, TextElement::CHAR_CLASS);
      if (pass == CHARACTER_CLASS_MATCH) {
        if (first_element_checked && i == 0) continue;
        if (DeterminedAlready(quick_check, elm.cp_offset)) continue;
        RegExpCharacterClass* cc = elm.data.u_char_class;
        EmitCharClass(assembler,
                      cc,
                      ascii,
                      backtrack,
                      cp_offset,
                      *checked_up_to < cp_offset,
                      preloaded);
        UpdateBoundsCheck(cp_offset, checked_up_to);
      }
    }
  }
}


int TextNode::Length() {
  TextElement elm = elms_->last();
  ASSERT(elm.cp_offset >= 0);
  if (elm.type == TextElement::ATOM) {
    return elm.cp_offset + elm.data.u_atom->data().length();
  } else {
    return elm.cp_offset + 1;
  }
}


bool TextNode::SkipPass(int int_pass, bool ignore_case) {
  TextEmitPassType pass = static_cast<TextEmitPassType>(int_pass);
  if (ignore_case) {
    return pass == SIMPLE_CHARACTER_MATCH;
  } else {
    return pass == NON_LETTER_CHARACTER_MATCH || pass == CASE_CHARACTER_MATCH;
  }
}


// This generates the code to match a text node.  A text node can contain
// straight character sequences (possibly to be matched in a case-independent
// way) and character classes.  For efficiency we do not do this in a single
// pass from left to right.  Instead we pass over the text node several times,
// emitting code for some character positions every time.  See the comment on
// TextEmitPass for details.
void TextNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  LimitResult limit_result = LimitVersions(compiler, trace);
  if (limit_result == DONE) return;
  ASSERT(limit_result == CONTINUE);

  if (trace->cp_offset() + Length() > RegExpMacroAssembler::kMaxCPOffset) {
    compiler->SetRegExpTooBig();
    return;
  }

  if (compiler->ascii()) {
    int dummy = 0;
    TextEmitPass(compiler, NON_ASCII_MATCH, false, trace, false, &dummy);
  }

  bool first_elt_done = false;
  int bound_checked_to = trace->cp_offset() - 1;
  bound_checked_to += trace->bound_checked_up_to();

  // If a character is preloaded into the current character register then
  // check that now.
  if (trace->characters_preloaded() == 1) {
    for (int pass = kFirstRealPass; pass <= kLastPass; pass++) {
      if (!SkipPass(pass, compiler->ignore_case())) {
        TextEmitPass(compiler,
                     static_cast<TextEmitPassType>(pass),
                     true,
                     trace,
                     false,
                     &bound_checked_to);
      }
    }
    first_elt_done = true;
  }

  for (int pass = kFirstRealPass; pass <= kLastPass; pass++) {
    if (!SkipPass(pass, compiler->ignore_case())) {
      TextEmitPass(compiler,
                   static_cast<TextEmitPassType>(pass),
                   false,
                   trace,
                   first_elt_done,
                   &bound_checked_to);
    }
  }

  Trace successor_trace(*trace);
  successor_trace.set_at_start(false);
  successor_trace.AdvanceCurrentPositionInTrace(Length(), compiler);
  RecursionCheck rc(compiler);
  on_success()->Emit(compiler, &successor_trace);
}


void Trace::InvalidateCurrentCharacter() {
  characters_preloaded_ = 0;
}


void Trace::AdvanceCurrentPositionInTrace(int by, RegExpCompiler* compiler) {
  ASSERT(by > 0);
  // We don't have an instruction for shifting the current character register
  // down or for using a shifted value for anything so lets just forget that
  // we preloaded any characters into it.
  characters_preloaded_ = 0;
  // Adjust the offsets of the quick check performed information.  This
  // information is used to find out what we already determined about the
  // characters by means of mask and compare.
  quick_check_performed_.Advance(by, compiler->ascii());
  cp_offset_ += by;
  if (cp_offset_ > RegExpMacroAssembler::kMaxCPOffset) {
    compiler->SetRegExpTooBig();
    cp_offset_ = 0;
  }
  bound_checked_up_to_ = Max(0, bound_checked_up_to_ - by);
}


void TextNode::MakeCaseIndependent() {
  int element_count = elms_->length();
  for (int i = 0; i < element_count; i++) {
    TextElement elm = elms_->at(i);
    if (elm.type == TextElement::CHAR_CLASS) {
      RegExpCharacterClass* cc = elm.data.u_char_class;
      ZoneList<CharacterRange>* ranges = cc->ranges();
      int range_count = ranges->length();
      for (int i = 0; i < range_count; i++) {
        ranges->at(i).AddCaseEquivalents(ranges);
      }
    }
  }
}


int TextNode::GreedyLoopTextLength() {
  TextElement elm = elms_->at(elms_->length() - 1);
  if (elm.type == TextElement::CHAR_CLASS) {
    return elm.cp_offset + 1;
  } else {
    return elm.cp_offset + elm.data.u_atom->data().length();
  }
}


// Finds the fixed match length of a sequence of nodes that goes from
// this alternative and back to this choice node.  If there are variable
// length nodes or other complications in the way then return a sentinel
// value indicating that a greedy loop cannot be constructed.
int ChoiceNode::GreedyLoopTextLength(GuardedAlternative* alternative) {
  int length = 0;
  RegExpNode* node = alternative->node();
  // Later we will generate code for all these text nodes using recursion
  // so we have to limit the max number.
  int recursion_depth = 0;
  while (node != this) {
    if (recursion_depth++ > RegExpCompiler::kMaxRecursion) {
      return kNodeIsTooComplexForGreedyLoops;
    }
    int node_length = node->GreedyLoopTextLength();
    if (node_length == kNodeIsTooComplexForGreedyLoops) {
      return kNodeIsTooComplexForGreedyLoops;
    }
    length += node_length;
    SeqRegExpNode* seq_node = static_cast<SeqRegExpNode*>(node);
    node = seq_node->on_success();
  }
  return length;
}


void LoopChoiceNode::AddLoopAlternative(GuardedAlternative alt) {
  ASSERT_EQ(loop_node_, NULL);
  AddAlternative(alt);
  loop_node_ = alt.node();
}


void LoopChoiceNode::AddContinueAlternative(GuardedAlternative alt) {
  ASSERT_EQ(continue_node_, NULL);
  AddAlternative(alt);
  continue_node_ = alt.node();
}


void LoopChoiceNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  if (trace->stop_node() == this) {
    int text_length = GreedyLoopTextLength(&(alternatives_->at(0)));
    ASSERT(text_length != kNodeIsTooComplexForGreedyLoops);
    // Update the counter-based backtracking info on the stack.  This is an
    // optimization for greedy loops (see below).
    ASSERT(trace->cp_offset() == text_length);
    macro_assembler->AdvanceCurrentPosition(text_length);
    macro_assembler->GoTo(trace->loop_label());
    return;
  }
  ASSERT(trace->stop_node() == NULL);
  if (!trace->is_trivial()) {
    trace->Flush(compiler, this);
    return;
  }
  ChoiceNode::Emit(compiler, trace);
}


int ChoiceNode::CalculatePreloadCharacters(RegExpCompiler* compiler) {
  int preload_characters = EatsAtLeast(4, 0);
#ifdef CAN_READ_UNALIGNED
  bool ascii = compiler->ascii();
  if (ascii) {
    if (preload_characters > 4) preload_characters = 4;
    // We can't preload 3 characters because there is no machine instruction
    // to do that.  We can't just load 4 because we could be reading
    // beyond the end of the string, which could cause a memory fault.
    if (preload_characters == 3) preload_characters = 2;
  } else {
    if (preload_characters > 2) preload_characters = 2;
  }
#else
  if (preload_characters > 1) preload_characters = 1;
#endif
  return preload_characters;
}


// This class is used when generating the alternatives in a choice node.  It
// records the way the alternative is being code generated.
class AlternativeGeneration: public Malloced {
 public:
  AlternativeGeneration()
      : possible_success(),
        expects_preload(false),
        after(),
        quick_check_details() { }
  Label possible_success;
  bool expects_preload;
  Label after;
  QuickCheckDetails quick_check_details;
};


// Creates a list of AlternativeGenerations.  If the list has a reasonable
// size then it is on the stack, otherwise the excess is on the heap.
class AlternativeGenerationList {
 public:
  explicit AlternativeGenerationList(int count)
      : alt_gens_(count) {
    for (int i = 0; i < count && i < kAFew; i++) {
      alt_gens_.Add(a_few_alt_gens_ + i);
    }
    for (int i = kAFew; i < count; i++) {
      alt_gens_.Add(new AlternativeGeneration());
    }
  }
  ~AlternativeGenerationList() {
    for (int i = kAFew; i < alt_gens_.length(); i++) {
      delete alt_gens_[i];
      alt_gens_[i] = NULL;
    }
  }

  AlternativeGeneration* at(int i) {
    return alt_gens_[i];
  }
 private:
  static const int kAFew = 10;
  ZoneList<AlternativeGeneration*> alt_gens_;
  AlternativeGeneration a_few_alt_gens_[kAFew];
};


/* Code generation for choice nodes.
 *
 * We generate quick checks that do a mask and compare to eliminate a
 * choice.  If the quick check succeeds then it jumps to the continuation to
 * do slow checks and check subsequent nodes.  If it fails (the common case)
 * it falls through to the next choice.
 *
 * Here is the desired flow graph.  Nodes directly below each other imply
 * fallthrough.  Alternatives 1 and 2 have quick checks.  Alternative
 * 3 doesn't have a quick check so we have to call the slow check.
 * Nodes are marked Qn for quick checks and Sn for slow checks.  The entire
 * regexp continuation is generated directly after the Sn node, up to the
 * next GoTo if we decide to reuse some already generated code.  Some
 * nodes expect preload_characters to be preloaded into the current
 * character register.  R nodes do this preloading.  Vertices are marked
 * F for failures and S for success (possible success in the case of quick
 * nodes).  L, V, < and > are used as arrow heads.
 *
 * ----------> R
 *             |
 *             V
 *            Q1 -----> S1
 *             |   S   /
 *            F|      /
 *             |    F/
 *             |    /
 *             |   R
 *             |  /
 *             V L
 *            Q2 -----> S2
 *             |   S   /
 *            F|      /
 *             |    F/
 *             |    /
 *             |   R
 *             |  /
 *             V L
 *            S3
 *             |
 *            F|
 *             |
 *             R
 *             |
 * backtrack   V
 * <----------Q4
 *   \    F    |
 *    \        |S
 *     \   F   V
 *      \-----S4
 *
 * For greedy loops we reverse our expectation and expect to match rather
 * than fail. Therefore we want the loop code to look like this (U is the
 * unwind code that steps back in the greedy loop).  The following alternatives
 * look the same as above.
 *              _____
 *             /     \
 *             V     |
 * ----------> S1    |
 *            /|     |
 *           / |S    |
 *         F/  \_____/
 *         /
 *        |<-----------
 *        |            \
 *        V             \
 *        Q2 ---> S2     \
 *        |  S   /       |
 *       F|     /        |
 *        |   F/         |
 *        |   /          |
 *        |  R           |
 *        | /            |
 *   F    VL             |
 * <------U              |
 * back   |S             |
 *        \______________/
 */


void ChoiceNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  int choice_count = alternatives_->length();
#ifdef DEBUG
  for (int i = 0; i < choice_count - 1; i++) {
    GuardedAlternative alternative = alternatives_->at(i);
    ZoneList<Guard*>* guards = alternative.guards();
    int guard_count = (guards == NULL) ? 0 : guards->length();
    for (int j = 0; j < guard_count; j++) {
      ASSERT(!trace->mentions_reg(guards->at(j)->reg()));
    }
  }
#endif

  LimitResult limit_result = LimitVersions(compiler, trace);
  if (limit_result == DONE) return;
  ASSERT(limit_result == CONTINUE);

  int new_flush_budget = trace->flush_budget() / choice_count;
  if (trace->flush_budget() == 0 && trace->actions() != NULL) {
    trace->Flush(compiler, this);
    return;
  }

  RecursionCheck rc(compiler);

  Trace* current_trace = trace;

  int text_length = GreedyLoopTextLength(&(alternatives_->at(0)));
  bool greedy_loop = false;
  Label greedy_loop_label;
  Trace counter_backtrack_trace;
  counter_backtrack_trace.set_backtrack(&greedy_loop_label);
  if (not_at_start()) counter_backtrack_trace.set_at_start(false);

  if (choice_count > 1 && text_length != kNodeIsTooComplexForGreedyLoops) {
    // Here we have special handling for greedy loops containing only text nodes
    // and other simple nodes.  These are handled by pushing the current
    // position on the stack and then incrementing the current position each
    // time around the switch.  On backtrack we decrement the current position
    // and check it against the pushed value.  This avoids pushing backtrack
    // information for each iteration of the loop, which could take up a lot of
    // space.
    greedy_loop = true;
    ASSERT(trace->stop_node() == NULL);
    macro_assembler->PushCurrentPosition();
    current_trace = &counter_backtrack_trace;
    Label greedy_match_failed;
    Trace greedy_match_trace;
    if (not_at_start()) greedy_match_trace.set_at_start(false);
    greedy_match_trace.set_backtrack(&greedy_match_failed);
    Label loop_label;
    macro_assembler->Bind(&loop_label);
    greedy_match_trace.set_stop_node(this);
    greedy_match_trace.set_loop_label(&loop_label);
    alternatives_->at(0).node()->Emit(compiler, &greedy_match_trace);
    macro_assembler->Bind(&greedy_match_failed);
  }

  Label second_choice;  // For use in greedy matches.
  macro_assembler->Bind(&second_choice);

  int first_normal_choice = greedy_loop ? 1 : 0;

  int preload_characters = CalculatePreloadCharacters(compiler);
  bool preload_is_current =
      (current_trace->characters_preloaded() == preload_characters);
  bool preload_has_checked_bounds = preload_is_current;

  AlternativeGenerationList alt_gens(choice_count);

  // For now we just call all choices one after the other.  The idea ultimately
  // is to use the Dispatch table to try only the relevant ones.
  for (int i = first_normal_choice; i < choice_count; i++) {
    GuardedAlternative alternative = alternatives_->at(i);
    AlternativeGeneration* alt_gen = alt_gens.at(i);
    alt_gen->quick_check_details.set_characters(preload_characters);
    ZoneList<Guard*>* guards = alternative.guards();
    int guard_count = (guards == NULL) ? 0 : guards->length();
    Trace new_trace(*current_trace);
    new_trace.set_characters_preloaded(preload_is_current ?
                                         preload_characters :
                                         0);
    if (preload_has_checked_bounds) {
      new_trace.set_bound_checked_up_to(preload_characters);
    }
    new_trace.quick_check_performed()->Clear();
    if (not_at_start_) new_trace.set_at_start(Trace::FALSE);
    alt_gen->expects_preload = preload_is_current;
    bool generate_full_check_inline = false;
    if (FLAG_regexp_optimization &&
        try_to_emit_quick_check_for_alternative(i) &&
        alternative.node()->EmitQuickCheck(compiler,
                                           &new_trace,
                                           preload_has_checked_bounds,
                                           &alt_gen->possible_success,
                                           &alt_gen->quick_check_details,
                                           i < choice_count - 1)) {
      // Quick check was generated for this choice.
      preload_is_current = true;
      preload_has_checked_bounds = true;
      // On the last choice in the ChoiceNode we generated the quick
      // check to fall through on possible success.  So now we need to
      // generate the full check inline.
      if (i == choice_count - 1) {
        macro_assembler->Bind(&alt_gen->possible_success);
        new_trace.set_quick_check_performed(&alt_gen->quick_check_details);
        new_trace.set_characters_preloaded(preload_characters);
        new_trace.set_bound_checked_up_to(preload_characters);
        generate_full_check_inline = true;
      }
    } else if (alt_gen->quick_check_details.cannot_match()) {
      if (i == choice_count - 1 && !greedy_loop) {
        macro_assembler->GoTo(trace->backtrack());
      }
      continue;
    } else {
      // No quick check was generated.  Put the full code here.
      // If this is not the first choice then there could be slow checks from
      // previous cases that go here when they fail.  There's no reason to
      // insist that they preload characters since the slow check we are about
      // to generate probably can't use it.
      if (i != first_normal_choice) {
        alt_gen->expects_preload = false;
        new_trace.set_characters_preloaded(0);
      }
      if (i < choice_count - 1) {
        new_trace.set_backtrack(&alt_gen->after);
      }
      generate_full_check_inline = true;
    }
    if (generate_full_check_inline) {
      if (new_trace.actions() != NULL) {
        new_trace.set_flush_budget(new_flush_budget);
      }
      for (int j = 0; j < guard_count; j++) {
        GenerateGuard(macro_assembler, guards->at(j), &new_trace);
      }
      alternative.node()->Emit(compiler, &new_trace);
      preload_is_current = false;
    }
    macro_assembler->Bind(&alt_gen->after);
  }
  if (greedy_loop) {
    macro_assembler->Bind(&greedy_loop_label);
    // If we have unwound to the bottom then backtrack.
    macro_assembler->CheckGreedyLoop(trace->backtrack());
    // Otherwise try the second priority at an earlier position.
    macro_assembler->AdvanceCurrentPosition(-text_length);
    macro_assembler->GoTo(&second_choice);
  }

  // At this point we need to generate slow checks for the alternatives where
  // the quick check was inlined.  We can recognize these because the associated
  // label was bound.
  for (int i = first_normal_choice; i < choice_count - 1; i++) {
    AlternativeGeneration* alt_gen = alt_gens.at(i);
    Trace new_trace(*current_trace);
    // If there are actions to be flushed we have to limit how many times
    // they are flushed.  Take the budget of the parent trace and distribute
    // it fairly amongst the children.
    if (new_trace.actions() != NULL) {
      new_trace.set_flush_budget(new_flush_budget);
    }
    EmitOutOfLineContinuation(compiler,
                              &new_trace,
                              alternatives_->at(i),
                              alt_gen,
                              preload_characters,
                              alt_gens.at(i + 1)->expects_preload);
  }
}


void ChoiceNode::EmitOutOfLineContinuation(RegExpCompiler* compiler,
                                           Trace* trace,
                                           GuardedAlternative alternative,
                                           AlternativeGeneration* alt_gen,
                                           int preload_characters,
                                           bool next_expects_preload) {
  if (!alt_gen->possible_success.is_linked()) return;

  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  macro_assembler->Bind(&alt_gen->possible_success);
  Trace out_of_line_trace(*trace);
  out_of_line_trace.set_characters_preloaded(preload_characters);
  out_of_line_trace.set_quick_check_performed(&alt_gen->quick_check_details);
  if (not_at_start_) out_of_line_trace.set_at_start(Trace::FALSE);
  ZoneList<Guard*>* guards = alternative.guards();
  int guard_count = (guards == NULL) ? 0 : guards->length();
  if (next_expects_preload) {
    Label reload_current_char;
    out_of_line_trace.set_backtrack(&reload_current_char);
    for (int j = 0; j < guard_count; j++) {
      GenerateGuard(macro_assembler, guards->at(j), &out_of_line_trace);
    }
    alternative.node()->Emit(compiler, &out_of_line_trace);
    macro_assembler->Bind(&reload_current_char);
    // Reload the current character, since the next quick check expects that.
    // We don't need to check bounds here because we only get into this
    // code through a quick check which already did the checked load.
    macro_assembler->LoadCurrentCharacter(trace->cp_offset(),
                                          NULL,
                                          false,
                                          preload_characters);
    macro_assembler->GoTo(&(alt_gen->after));
  } else {
    out_of_line_trace.set_backtrack(&(alt_gen->after));
    for (int j = 0; j < guard_count; j++) {
      GenerateGuard(macro_assembler, guards->at(j), &out_of_line_trace);
    }
    alternative.node()->Emit(compiler, &out_of_line_trace);
  }
}


void ActionNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  LimitResult limit_result = LimitVersions(compiler, trace);
  if (limit_result == DONE) return;
  ASSERT(limit_result == CONTINUE);

  RecursionCheck rc(compiler);

  switch (type_) {
    case STORE_POSITION: {
      Trace::DeferredCapture
          new_capture(data_.u_position_register.reg,
                      data_.u_position_register.is_capture,
                      trace);
      Trace new_trace = *trace;
      new_trace.add_action(&new_capture);
      on_success()->Emit(compiler, &new_trace);
      break;
    }
    case INCREMENT_REGISTER: {
      Trace::DeferredIncrementRegister
          new_increment(data_.u_increment_register.reg);
      Trace new_trace = *trace;
      new_trace.add_action(&new_increment);
      on_success()->Emit(compiler, &new_trace);
      break;
    }
    case SET_REGISTER: {
      Trace::DeferredSetRegister
          new_set(data_.u_store_register.reg, data_.u_store_register.value);
      Trace new_trace = *trace;
      new_trace.add_action(&new_set);
      on_success()->Emit(compiler, &new_trace);
      break;
    }
    case CLEAR_CAPTURES: {
      Trace::DeferredClearCaptures
        new_capture(Interval(data_.u_clear_captures.range_from,
                             data_.u_clear_captures.range_to));
      Trace new_trace = *trace;
      new_trace.add_action(&new_capture);
      on_success()->Emit(compiler, &new_trace);
      break;
    }
    case BEGIN_SUBMATCH:
      if (!trace->is_trivial()) {
        trace->Flush(compiler, this);
      } else {
        assembler->WriteCurrentPositionToRegister(
            data_.u_submatch.current_position_register, 0);
        assembler->WriteStackPointerToRegister(
            data_.u_submatch.stack_pointer_register);
        on_success()->Emit(compiler, trace);
      }
      break;
    case EMPTY_MATCH_CHECK: {
      int start_pos_reg = data_.u_empty_match_check.start_register;
      int stored_pos = 0;
      int rep_reg = data_.u_empty_match_check.repetition_register;
      bool has_minimum = (rep_reg != RegExpCompiler::kNoRegister);
      bool know_dist = trace->GetStoredPosition(start_pos_reg, &stored_pos);
      if (know_dist && !has_minimum && stored_pos == trace->cp_offset()) {
        // If we know we haven't advanced and there is no minimum we
        // can just backtrack immediately.
        assembler->GoTo(trace->backtrack());
      } else if (know_dist && stored_pos < trace->cp_offset()) {
        // If we know we've advanced we can generate the continuation
        // immediately.
        on_success()->Emit(compiler, trace);
      } else if (!trace->is_trivial()) {
        trace->Flush(compiler, this);
      } else {
        Label skip_empty_check;
        // If we have a minimum number of repetitions we check the current
        // number first and skip the empty check if it's not enough.
        if (has_minimum) {
          int limit = data_.u_empty_match_check.repetition_limit;
          assembler->IfRegisterLT(rep_reg, limit, &skip_empty_check);
        }
        // If the match is empty we bail out, otherwise we fall through
        // to the on-success continuation.
        assembler->IfRegisterEqPos(data_.u_empty_match_check.start_register,
                                   trace->backtrack());
        assembler->Bind(&skip_empty_check);
        on_success()->Emit(compiler, trace);
      }
      break;
    }
    case POSITIVE_SUBMATCH_SUCCESS: {
      if (!trace->is_trivial()) {
        trace->Flush(compiler, this);
        return;
      }
      assembler->ReadCurrentPositionFromRegister(
          data_.u_submatch.current_position_register);
      assembler->ReadStackPointerFromRegister(
          data_.u_submatch.stack_pointer_register);
      int clear_register_count = data_.u_submatch.clear_register_count;
      if (clear_register_count == 0) {
        on_success()->Emit(compiler, trace);
        return;
      }
      int clear_registers_from = data_.u_submatch.clear_register_from;
      Label clear_registers_backtrack;
      Trace new_trace = *trace;
      new_trace.set_backtrack(&clear_registers_backtrack);
      on_success()->Emit(compiler, &new_trace);

      assembler->Bind(&clear_registers_backtrack);
      int clear_registers_to = clear_registers_from + clear_register_count - 1;
      assembler->ClearRegisters(clear_registers_from, clear_registers_to);

      ASSERT(trace->backtrack() == NULL);
      assembler->Backtrack();
      return;
    }
    default:
      UNREACHABLE();
  }
}


void BackReferenceNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  if (!trace->is_trivial()) {
    trace->Flush(compiler, this);
    return;
  }

  LimitResult limit_result = LimitVersions(compiler, trace);
  if (limit_result == DONE) return;
  ASSERT(limit_result == CONTINUE);

  RecursionCheck rc(compiler);

  ASSERT_EQ(start_reg_ + 1, end_reg_);
  if (compiler->ignore_case()) {
    assembler->CheckNotBackReferenceIgnoreCase(start_reg_,
                                               trace->backtrack());
  } else {
    assembler->CheckNotBackReference(start_reg_, trace->backtrack());
  }
  on_success()->Emit(compiler, trace);
}


// -------------------------------------------------------------------
// Dot/dotty output


#ifdef DEBUG


class DotPrinter: public NodeVisitor {
 public:
  explicit DotPrinter(bool ignore_case)
      : ignore_case_(ignore_case),
        stream_(&alloc_) { }
  void PrintNode(const char* label, RegExpNode* node);
  void Visit(RegExpNode* node);
  void PrintAttributes(RegExpNode* from);
  StringStream* stream() { return &stream_; }
  void PrintOnFailure(RegExpNode* from, RegExpNode* to);
#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that);
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
 private:
  bool ignore_case_;
  HeapStringAllocator alloc_;
  StringStream stream_;
};


void DotPrinter::PrintNode(const char* label, RegExpNode* node) {
  stream()->Add("digraph G {\n  graph [label=\"");
  for (int i = 0; label[i]; i++) {
    switch (label[i]) {
      case '\\':
        stream()->Add("\\\\");
        break;
      case '"':
        stream()->Add("\"");
        break;
      default:
        stream()->Put(label[i]);
        break;
    }
  }
  stream()->Add("\"];\n");
  Visit(node);
  stream()->Add("}\n");
  printf("%s", *(stream()->ToCString()));
}


void DotPrinter::Visit(RegExpNode* node) {
  if (node->info()->visited) return;
  node->info()->visited = true;
  node->Accept(this);
}


void DotPrinter::PrintOnFailure(RegExpNode* from, RegExpNode* on_failure) {
  stream()->Add("  n%p -> n%p [style=dotted];\n", from, on_failure);
  Visit(on_failure);
}


class TableEntryBodyPrinter {
 public:
  TableEntryBodyPrinter(StringStream* stream, ChoiceNode* choice)
      : stream_(stream), choice_(choice) { }
  void Call(uc16 from, DispatchTable::Entry entry) {
    OutSet* out_set = entry.out_set();
    for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
      if (out_set->Get(i)) {
        stream()->Add("    n%p:s%io%i -> n%p;\n",
                      choice(),
                      from,
                      i,
                      choice()->alternatives()->at(i).node());
      }
    }
  }
 private:
  StringStream* stream() { return stream_; }
  ChoiceNode* choice() { return choice_; }
  StringStream* stream_;
  ChoiceNode* choice_;
};


class TableEntryHeaderPrinter {
 public:
  explicit TableEntryHeaderPrinter(StringStream* stream)
      : first_(true), stream_(stream) { }
  void Call(uc16 from, DispatchTable::Entry entry) {
    if (first_) {
      first_ = false;
    } else {
      stream()->Add("|");
    }
    stream()->Add("{\\%k-\\%k|{", from, entry.to());
    OutSet* out_set = entry.out_set();
    int priority = 0;
    for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
      if (out_set->Get(i)) {
        if (priority > 0) stream()->Add("|");
        stream()->Add("<s%io%i> %i", from, i, priority);
        priority++;
      }
    }
    stream()->Add("}}");
  }
 private:
  bool first_;
  StringStream* stream() { return stream_; }
  StringStream* stream_;
};


class AttributePrinter {
 public:
  explicit AttributePrinter(DotPrinter* out)
      : out_(out), first_(true) { }
  void PrintSeparator() {
    if (first_) {
      first_ = false;
    } else {
      out_->stream()->Add("|");
    }
  }
  void PrintBit(const char* name, bool value) {
    if (!value) return;
    PrintSeparator();
    out_->stream()->Add("{%s}", name);
  }
  void PrintPositive(const char* name, int value) {
    if (value < 0) return;
    PrintSeparator();
    out_->stream()->Add("{%s|%x}", name, value);
  }
 private:
  DotPrinter* out_;
  bool first_;
};


void DotPrinter::PrintAttributes(RegExpNode* that) {
  stream()->Add("  a%p [shape=Mrecord, color=grey, fontcolor=grey, "
                "margin=0.1, fontsize=10, label=\"{",
                that);
  AttributePrinter printer(this);
  NodeInfo* info = that->info();
  printer.PrintBit("NI", info->follows_newline_interest);
  printer.PrintBit("WI", info->follows_word_interest);
  printer.PrintBit("SI", info->follows_start_interest);
  Label* label = that->label();
  if (label->is_bound())
    printer.PrintPositive("@", label->pos());
  stream()->Add("}\"];\n");
  stream()->Add("  a%p -> n%p [style=dashed, color=grey, "
                "arrowhead=none];\n", that, that);
}


static const bool kPrintDispatchTable = false;
void DotPrinter::VisitChoice(ChoiceNode* that) {
  if (kPrintDispatchTable) {
    stream()->Add("  n%p [shape=Mrecord, label=\"", that);
    TableEntryHeaderPrinter header_printer(stream());
    that->GetTable(ignore_case_)->ForEach(&header_printer);
    stream()->Add("\"]\n", that);
    PrintAttributes(that);
    TableEntryBodyPrinter body_printer(stream(), that);
    that->GetTable(ignore_case_)->ForEach(&body_printer);
  } else {
    stream()->Add("  n%p [shape=Mrecord, label=\"?\"];\n", that);
    for (int i = 0; i < that->alternatives()->length(); i++) {
      GuardedAlternative alt = that->alternatives()->at(i);
      stream()->Add("  n%p -> n%p;\n", that, alt.node());
    }
  }
  for (int i = 0; i < that->alternatives()->length(); i++) {
    GuardedAlternative alt = that->alternatives()->at(i);
    alt.node()->Accept(this);
  }
}


void DotPrinter::VisitText(TextNode* that) {
  stream()->Add("  n%p [label=\"", that);
  for (int i = 0; i < that->elements()->length(); i++) {
    if (i > 0) stream()->Add(" ");
    TextElement elm = that->elements()->at(i);
    switch (elm.type) {
      case TextElement::ATOM: {
        stream()->Add("'%w'", elm.data.u_atom->data());
        break;
      }
      case TextElement::CHAR_CLASS: {
        RegExpCharacterClass* node = elm.data.u_char_class;
        stream()->Add("[");
        if (node->is_negated())
          stream()->Add("^");
        for (int j = 0; j < node->ranges()->length(); j++) {
          CharacterRange range = node->ranges()->at(j);
          stream()->Add("%k-%k", range.from(), range.to());
        }
        stream()->Add("]");
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  stream()->Add("\", shape=box, peripheries=2];\n");
  PrintAttributes(that);
  stream()->Add("  n%p -> n%p;\n", that, that->on_success());
  Visit(that->on_success());
}


void DotPrinter::VisitBackReference(BackReferenceNode* that) {
  stream()->Add("  n%p [label=\"$%i..$%i\", shape=doubleoctagon];\n",
                that,
                that->start_register(),
                that->end_register());
  PrintAttributes(that);
  stream()->Add("  n%p -> n%p;\n", that, that->on_success());
  Visit(that->on_success());
}


void DotPrinter::VisitEnd(EndNode* that) {
  stream()->Add("  n%p [style=bold, shape=point];\n", that);
  PrintAttributes(that);
}


void DotPrinter::VisitAssertion(AssertionNode* that) {
  stream()->Add("  n%p [", that);
  switch (that->type()) {
    case AssertionNode::AT_END:
      stream()->Add("label=\"$\", shape=septagon");
      break;
    case AssertionNode::AT_START:
      stream()->Add("label=\"^\", shape=septagon");
      break;
    case AssertionNode::AT_BOUNDARY:
      stream()->Add("label=\"\\b\", shape=septagon");
      break;
    case AssertionNode::AT_NON_BOUNDARY:
      stream()->Add("label=\"\\B\", shape=septagon");
      break;
    case AssertionNode::AFTER_NEWLINE:
      stream()->Add("label=\"(?<=\\n)\", shape=septagon");
      break;
  }
  stream()->Add("];\n");
  PrintAttributes(that);
  RegExpNode* successor = that->on_success();
  stream()->Add("  n%p -> n%p;\n", that, successor);
  Visit(successor);
}


void DotPrinter::VisitAction(ActionNode* that) {
  stream()->Add("  n%p [", that);
  switch (that->type_) {
    case ActionNode::SET_REGISTER:
      stream()->Add("label=\"$%i:=%i\", shape=octagon",
                    that->data_.u_store_register.reg,
                    that->data_.u_store_register.value);
      break;
    case ActionNode::INCREMENT_REGISTER:
      stream()->Add("label=\"$%i++\", shape=octagon",
                    that->data_.u_increment_register.reg);
      break;
    case ActionNode::STORE_POSITION:
      stream()->Add("label=\"$%i:=$pos\", shape=octagon",
                    that->data_.u_position_register.reg);
      break;
    case ActionNode::BEGIN_SUBMATCH:
      stream()->Add("label=\"$%i:=$pos,begin\", shape=septagon",
                    that->data_.u_submatch.current_position_register);
      break;
    case ActionNode::POSITIVE_SUBMATCH_SUCCESS:
      stream()->Add("label=\"escape\", shape=septagon");
      break;
    case ActionNode::EMPTY_MATCH_CHECK:
      stream()->Add("label=\"$%i=$pos?,$%i<%i?\", shape=septagon",
                    that->data_.u_empty_match_check.start_register,
                    that->data_.u_empty_match_check.repetition_register,
                    that->data_.u_empty_match_check.repetition_limit);
      break;
    case ActionNode::CLEAR_CAPTURES: {
      stream()->Add("label=\"clear $%i to $%i\", shape=septagon",
                    that->data_.u_clear_captures.range_from,
                    that->data_.u_clear_captures.range_to);
      break;
    }
  }
  stream()->Add("];\n");
  PrintAttributes(that);
  RegExpNode* successor = that->on_success();
  stream()->Add("  n%p -> n%p;\n", that, successor);
  Visit(successor);
}


class DispatchTableDumper {
 public:
  explicit DispatchTableDumper(StringStream* stream) : stream_(stream) { }
  void Call(uc16 key, DispatchTable::Entry entry);
  StringStream* stream() { return stream_; }
 private:
  StringStream* stream_;
};


void DispatchTableDumper::Call(uc16 key, DispatchTable::Entry entry) {
  stream()->Add("[%k-%k]: {", key, entry.to());
  OutSet* set = entry.out_set();
  bool first = true;
  for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
    if (set->Get(i)) {
      if (first) {
        first = false;
      } else {
        stream()->Add(", ");
      }
      stream()->Add("%i", i);
    }
  }
  stream()->Add("}\n");
}


void DispatchTable::Dump() {
  HeapStringAllocator alloc;
  StringStream stream(&alloc);
  DispatchTableDumper dumper(&stream);
  tree()->ForEach(&dumper);
  OS::PrintError("%s", *stream.ToCString());
}


void RegExpEngine::DotPrint(const char* label,
                            RegExpNode* node,
                            bool ignore_case) {
  DotPrinter printer(ignore_case);
  printer.PrintNode(label, node);
}


#endif  // DEBUG


// -------------------------------------------------------------------
// Tree to graph conversion

static const int kSpaceRangeCount = 20;
static const int kSpaceRangeAsciiCount = 4;
static const uc16 kSpaceRanges[kSpaceRangeCount] = { 0x0009, 0x000D, 0x0020,
    0x0020, 0x00A0, 0x00A0, 0x1680, 0x1680, 0x180E, 0x180E, 0x2000, 0x200A,
    0x2028, 0x2029, 0x202F, 0x202F, 0x205F, 0x205F, 0x3000, 0x3000 };

static const int kWordRangeCount = 8;
static const uc16 kWordRanges[kWordRangeCount] = { '0', '9', 'A', 'Z', '_',
    '_', 'a', 'z' };

static const int kDigitRangeCount = 2;
static const uc16 kDigitRanges[kDigitRangeCount] = { '0', '9' };

static const int kLineTerminatorRangeCount = 6;
static const uc16 kLineTerminatorRanges[kLineTerminatorRangeCount] = { 0x000A,
    0x000A, 0x000D, 0x000D, 0x2028, 0x2029 };

RegExpNode* RegExpAtom::ToNode(RegExpCompiler* compiler,
                               RegExpNode* on_success) {
  ZoneList<TextElement>* elms = new ZoneList<TextElement>(1);
  elms->Add(TextElement::Atom(this));
  return new TextNode(elms, on_success);
}


RegExpNode* RegExpText::ToNode(RegExpCompiler* compiler,
                               RegExpNode* on_success) {
  return new TextNode(elements(), on_success);
}

static bool CompareInverseRanges(ZoneList<CharacterRange>* ranges,
                                 const uc16* special_class,
                                 int length) {
  ASSERT(ranges->length() != 0);
  ASSERT(length != 0);
  ASSERT(special_class[0] != 0);
  if (ranges->length() != (length >> 1) + 1) {
    return false;
  }
  CharacterRange range = ranges->at(0);
  if (range.from() != 0) {
    return false;
  }
  for (int i = 0; i < length; i += 2) {
    if (special_class[i] != (range.to() + 1)) {
      return false;
    }
    range = ranges->at((i >> 1) + 1);
    if (special_class[i+1] != range.from() - 1) {
      return false;
    }
  }
  if (range.to() != 0xffff) {
    return false;
  }
  return true;
}


static bool CompareRanges(ZoneList<CharacterRange>* ranges,
                          const uc16* special_class,
                          int length) {
  if (ranges->length() * 2 != length) {
    return false;
  }
  for (int i = 0; i < length; i += 2) {
    CharacterRange range = ranges->at(i >> 1);
    if (range.from() != special_class[i] || range.to() != special_class[i+1]) {
      return false;
    }
  }
  return true;
}


bool RegExpCharacterClass::is_standard() {
  // TODO(lrn): Remove need for this function, by not throwing away information
  // along the way.
  if (is_negated_) {
    return false;
  }
  if (set_.is_standard()) {
    return true;
  }
  if (CompareRanges(set_.ranges(), kSpaceRanges, kSpaceRangeCount)) {
    set_.set_standard_set_type('s');
    return true;
  }
  if (CompareInverseRanges(set_.ranges(), kSpaceRanges, kSpaceRangeCount)) {
    set_.set_standard_set_type('S');
    return true;
  }
  if (CompareInverseRanges(set_.ranges(),
                           kLineTerminatorRanges,
                           kLineTerminatorRangeCount)) {
    set_.set_standard_set_type('.');
    return true;
  }
  return false;
}


RegExpNode* RegExpCharacterClass::ToNode(RegExpCompiler* compiler,
                                         RegExpNode* on_success) {
  return new TextNode(this, on_success);
}


RegExpNode* RegExpDisjunction::ToNode(RegExpCompiler* compiler,
                                      RegExpNode* on_success) {
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  int length = alternatives->length();
  ChoiceNode* result = new ChoiceNode(length);
  for (int i = 0; i < length; i++) {
    GuardedAlternative alternative(alternatives->at(i)->ToNode(compiler,
                                                               on_success));
    result->AddAlternative(alternative);
  }
  return result;
}


RegExpNode* RegExpQuantifier::ToNode(RegExpCompiler* compiler,
                                     RegExpNode* on_success) {
  return ToNode(min(),
                max(),
                is_greedy(),
                body(),
                compiler,
                on_success);
}


RegExpNode* RegExpQuantifier::ToNode(int min,
                                     int max,
                                     bool is_greedy,
                                     RegExpTree* body,
                                     RegExpCompiler* compiler,
                                     RegExpNode* on_success,
                                     bool not_at_start) {
  // x{f, t} becomes this:
  //
  //             (r++)<-.
  //               |     `
  //               |     (x)
  //               v     ^
  //      (r=0)-->(?)---/ [if r < t]
  //               |
  //   [if r >= f] \----> ...
  //

  // 15.10.2.5 RepeatMatcher algorithm.
  // The parser has already eliminated the case where max is 0.  In the case
  // where max_match is zero the parser has removed the quantifier if min was
  // > 0 and removed the atom if min was 0.  See AddQuantifierToAtom.

  // If we know that we cannot match zero length then things are a little
  // simpler since we don't need to make the special zero length match check
  // from step 2.1.  If the min and max are small we can unroll a little in
  // this case.
  static const int kMaxUnrolledMinMatches = 3;  // Unroll (foo)+ and (foo){3,}
  static const int kMaxUnrolledMaxMatches = 3;  // Unroll (foo)? and (foo){x,3}
  if (max == 0) return on_success;  // This can happen due to recursion.
  bool body_can_be_empty = (body->min_match() == 0);
  int body_start_reg = RegExpCompiler::kNoRegister;
  Interval capture_registers = body->CaptureRegisters();
  bool needs_capture_clearing = !capture_registers.is_empty();
  if (body_can_be_empty) {
    body_start_reg = compiler->AllocateRegister();
  } else if (FLAG_regexp_optimization && !needs_capture_clearing) {
    // Only unroll if there are no captures and the body can't be
    // empty.
    if (min > 0 && min <= kMaxUnrolledMinMatches) {
      int new_max = (max == kInfinity) ? max : max - min;
      // Recurse once to get the loop or optional matches after the fixed ones.
      RegExpNode* answer = ToNode(
          0, new_max, is_greedy, body, compiler, on_success, true);
      // Unroll the forced matches from 0 to min.  This can cause chains of
      // TextNodes (which the parser does not generate).  These should be
      // combined if it turns out they hinder good code generation.
      for (int i = 0; i < min; i++) {
        answer = body->ToNode(compiler, answer);
      }
      return answer;
    }
    if (max <= kMaxUnrolledMaxMatches) {
      ASSERT(min == 0);
      // Unroll the optional matches up to max.
      RegExpNode* answer = on_success;
      for (int i = 0; i < max; i++) {
        ChoiceNode* alternation = new ChoiceNode(2);
        if (is_greedy) {
          alternation->AddAlternative(GuardedAlternative(body->ToNode(compiler,
                                                                      answer)));
          alternation->AddAlternative(GuardedAlternative(on_success));
        } else {
          alternation->AddAlternative(GuardedAlternative(on_success));
          alternation->AddAlternative(GuardedAlternative(body->ToNode(compiler,
                                                                      answer)));
        }
        answer = alternation;
        if (not_at_start) alternation->set_not_at_start();
      }
      return answer;
    }
  }
  bool has_min = min > 0;
  bool has_max = max < RegExpTree::kInfinity;
  bool needs_counter = has_min || has_max;
  int reg_ctr = needs_counter
      ? compiler->AllocateRegister()
      : RegExpCompiler::kNoRegister;
  LoopChoiceNode* center = new LoopChoiceNode(body->min_match() == 0);
  if (not_at_start) center->set_not_at_start();
  RegExpNode* loop_return = needs_counter
      ? static_cast<RegExpNode*>(ActionNode::IncrementRegister(reg_ctr, center))
      : static_cast<RegExpNode*>(center);
  if (body_can_be_empty) {
    // If the body can be empty we need to check if it was and then
    // backtrack.
    loop_return = ActionNode::EmptyMatchCheck(body_start_reg,
                                              reg_ctr,
                                              min,
                                              loop_return);
  }
  RegExpNode* body_node = body->ToNode(compiler, loop_return);
  if (body_can_be_empty) {
    // If the body can be empty we need to store the start position
    // so we can bail out if it was empty.
    body_node = ActionNode::StorePosition(body_start_reg, false, body_node);
  }
  if (needs_capture_clearing) {
    // Before entering the body of this loop we need to clear captures.
    body_node = ActionNode::ClearCaptures(capture_registers, body_node);
  }
  GuardedAlternative body_alt(body_node);
  if (has_max) {
    Guard* body_guard = new Guard(reg_ctr, Guard::LT, max);
    body_alt.AddGuard(body_guard);
  }
  GuardedAlternative rest_alt(on_success);
  if (has_min) {
    Guard* rest_guard = new Guard(reg_ctr, Guard::GEQ, min);
    rest_alt.AddGuard(rest_guard);
  }
  if (is_greedy) {
    center->AddLoopAlternative(body_alt);
    center->AddContinueAlternative(rest_alt);
  } else {
    center->AddContinueAlternative(rest_alt);
    center->AddLoopAlternative(body_alt);
  }
  if (needs_counter) {
    return ActionNode::SetRegister(reg_ctr, 0, center);
  } else {
    return center;
  }
}


RegExpNode* RegExpAssertion::ToNode(RegExpCompiler* compiler,
                                    RegExpNode* on_success) {
  NodeInfo info;
  switch (type()) {
    case START_OF_LINE:
      return AssertionNode::AfterNewline(on_success);
    case START_OF_INPUT:
      return AssertionNode::AtStart(on_success);
    case BOUNDARY:
      return AssertionNode::AtBoundary(on_success);
    case NON_BOUNDARY:
      return AssertionNode::AtNonBoundary(on_success);
    case END_OF_INPUT:
      return AssertionNode::AtEnd(on_success);
    case END_OF_LINE: {
      // Compile $ in multiline regexps as an alternation with a positive
      // lookahead in one side and an end-of-input on the other side.
      // We need two registers for the lookahead.
      int stack_pointer_register = compiler->AllocateRegister();
      int position_register = compiler->AllocateRegister();
      // The ChoiceNode to distinguish between a newline and end-of-input.
      ChoiceNode* result = new ChoiceNode(2);
      // Create a newline atom.
      ZoneList<CharacterRange>* newline_ranges =
          new ZoneList<CharacterRange>(3);
      CharacterRange::AddClassEscape('n', newline_ranges);
      RegExpCharacterClass* newline_atom = new RegExpCharacterClass('n');
      TextNode* newline_matcher = new TextNode(
         newline_atom,
         ActionNode::PositiveSubmatchSuccess(stack_pointer_register,
                                             position_register,
                                             0,  // No captures inside.
                                             -1,  // Ignored if no captures.
                                             on_success));
      // Create an end-of-input matcher.
      RegExpNode* end_of_line = ActionNode::BeginSubmatch(
          stack_pointer_register,
          position_register,
          newline_matcher);
      // Add the two alternatives to the ChoiceNode.
      GuardedAlternative eol_alternative(end_of_line);
      result->AddAlternative(eol_alternative);
      GuardedAlternative end_alternative(AssertionNode::AtEnd(on_success));
      result->AddAlternative(end_alternative);
      return result;
    }
    default:
      UNREACHABLE();
  }
  return on_success;
}


RegExpNode* RegExpBackReference::ToNode(RegExpCompiler* compiler,
                                        RegExpNode* on_success) {
  return new BackReferenceNode(RegExpCapture::StartRegister(index()),
                               RegExpCapture::EndRegister(index()),
                               on_success);
}


RegExpNode* RegExpEmpty::ToNode(RegExpCompiler* compiler,
                                RegExpNode* on_success) {
  return on_success;
}


RegExpNode* RegExpLookahead::ToNode(RegExpCompiler* compiler,
                                    RegExpNode* on_success) {
  int stack_pointer_register = compiler->AllocateRegister();
  int position_register = compiler->AllocateRegister();

  const int registers_per_capture = 2;
  const int register_of_first_capture = 2;
  int register_count = capture_count_ * registers_per_capture;
  int register_start =
    register_of_first_capture + capture_from_ * registers_per_capture;

  RegExpNode* success;
  if (is_positive()) {
    RegExpNode* node = ActionNode::BeginSubmatch(
        stack_pointer_register,
        position_register,
        body()->ToNode(
            compiler,
            ActionNode::PositiveSubmatchSuccess(stack_pointer_register,
                                                position_register,
                                                register_count,
                                                register_start,
                                                on_success)));
    return node;
  } else {
    // We use a ChoiceNode for a negative lookahead because it has most of
    // the characteristics we need.  It has the body of the lookahead as its
    // first alternative and the expression after the lookahead of the second
    // alternative.  If the first alternative succeeds then the
    // NegativeSubmatchSuccess will unwind the stack including everything the
    // choice node set up and backtrack.  If the first alternative fails then
    // the second alternative is tried, which is exactly the desired result
    // for a negative lookahead.  The NegativeLookaheadChoiceNode is a special
    // ChoiceNode that knows to ignore the first exit when calculating quick
    // checks.
    GuardedAlternative body_alt(
        body()->ToNode(
            compiler,
            success = new NegativeSubmatchSuccess(stack_pointer_register,
                                                  position_register,
                                                  register_count,
                                                  register_start)));
    ChoiceNode* choice_node =
        new NegativeLookaheadChoiceNode(body_alt,
                                        GuardedAlternative(on_success));
    return ActionNode::BeginSubmatch(stack_pointer_register,
                                     position_register,
                                     choice_node);
  }
}


RegExpNode* RegExpCapture::ToNode(RegExpCompiler* compiler,
                                  RegExpNode* on_success) {
  return ToNode(body(), index(), compiler, on_success);
}


RegExpNode* RegExpCapture::ToNode(RegExpTree* body,
                                  int index,
                                  RegExpCompiler* compiler,
                                  RegExpNode* on_success) {
  int start_reg = RegExpCapture::StartRegister(index);
  int end_reg = RegExpCapture::EndRegister(index);
  RegExpNode* store_end = ActionNode::StorePosition(end_reg, true, on_success);
  RegExpNode* body_node = body->ToNode(compiler, store_end);
  return ActionNode::StorePosition(start_reg, true, body_node);
}


RegExpNode* RegExpAlternative::ToNode(RegExpCompiler* compiler,
                                      RegExpNode* on_success) {
  ZoneList<RegExpTree*>* children = nodes();
  RegExpNode* current = on_success;
  for (int i = children->length() - 1; i >= 0; i--) {
    current = children->at(i)->ToNode(compiler, current);
  }
  return current;
}


static void AddClass(const uc16* elmv,
                     int elmc,
                     ZoneList<CharacterRange>* ranges) {
  for (int i = 0; i < elmc; i += 2) {
    ASSERT(elmv[i] <= elmv[i + 1]);
    ranges->Add(CharacterRange(elmv[i], elmv[i + 1]));
  }
}


static void AddClassNegated(const uc16 *elmv,
                            int elmc,
                            ZoneList<CharacterRange>* ranges) {
  ASSERT(elmv[0] != 0x0000);
  ASSERT(elmv[elmc-1] != String::kMaxUC16CharCode);
  uc16 last = 0x0000;
  for (int i = 0; i < elmc; i += 2) {
    ASSERT(last <= elmv[i] - 1);
    ASSERT(elmv[i] <= elmv[i + 1]);
    ranges->Add(CharacterRange(last, elmv[i] - 1));
    last = elmv[i + 1] + 1;
  }
  ranges->Add(CharacterRange(last, String::kMaxUC16CharCode));
}


void CharacterRange::AddClassEscape(uc16 type,
                                    ZoneList<CharacterRange>* ranges) {
  switch (type) {
    case 's':
      AddClass(kSpaceRanges, kSpaceRangeCount, ranges);
      break;
    case 'S':
      AddClassNegated(kSpaceRanges, kSpaceRangeCount, ranges);
      break;
    case 'w':
      AddClass(kWordRanges, kWordRangeCount, ranges);
      break;
    case 'W':
      AddClassNegated(kWordRanges, kWordRangeCount, ranges);
      break;
    case 'd':
      AddClass(kDigitRanges, kDigitRangeCount, ranges);
      break;
    case 'D':
      AddClassNegated(kDigitRanges, kDigitRangeCount, ranges);
      break;
    case '.':
      AddClassNegated(kLineTerminatorRanges,
                      kLineTerminatorRangeCount,
                      ranges);
      break;
    // This is not a character range as defined by the spec but a
    // convenient shorthand for a character class that matches any
    // character.
    case '*':
      ranges->Add(CharacterRange::Everything());
      break;
    // This is the set of characters matched by the $ and ^ symbols
    // in multiline mode.
    case 'n':
      AddClass(kLineTerminatorRanges,
               kLineTerminatorRangeCount,
               ranges);
      break;
    default:
      UNREACHABLE();
  }
}


Vector<const uc16> CharacterRange::GetWordBounds() {
  return Vector<const uc16>(kWordRanges, kWordRangeCount);
}


class CharacterRangeSplitter {
 public:
  CharacterRangeSplitter(ZoneList<CharacterRange>** included,
                          ZoneList<CharacterRange>** excluded)
      : included_(included),
        excluded_(excluded) { }
  void Call(uc16 from, DispatchTable::Entry entry);

  static const int kInBase = 0;
  static const int kInOverlay = 1;

 private:
  ZoneList<CharacterRange>** included_;
  ZoneList<CharacterRange>** excluded_;
};


void CharacterRangeSplitter::Call(uc16 from, DispatchTable::Entry entry) {
  if (!entry.out_set()->Get(kInBase)) return;
  ZoneList<CharacterRange>** target = entry.out_set()->Get(kInOverlay)
    ? included_
    : excluded_;
  if (*target == NULL) *target = new ZoneList<CharacterRange>(2);
  (*target)->Add(CharacterRange(entry.from(), entry.to()));
}


void CharacterRange::Split(ZoneList<CharacterRange>* base,
                           Vector<const uc16> overlay,
                           ZoneList<CharacterRange>** included,
                           ZoneList<CharacterRange>** excluded) {
  ASSERT_EQ(NULL, *included);
  ASSERT_EQ(NULL, *excluded);
  DispatchTable table;
  for (int i = 0; i < base->length(); i++)
    table.AddRange(base->at(i), CharacterRangeSplitter::kInBase);
  for (int i = 0; i < overlay.length(); i += 2) {
    table.AddRange(CharacterRange(overlay[i], overlay[i+1]),
                   CharacterRangeSplitter::kInOverlay);
  }
  CharacterRangeSplitter callback(included, excluded);
  table.ForEach(&callback);
}


void CharacterRange::AddCaseEquivalents(ZoneList<CharacterRange>* ranges) {
  unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
  if (IsSingleton()) {
    // If this is a singleton we just expand the one character.
    int length = uncanonicalize.get(from(), '\0', chars);
    for (int i = 0; i < length; i++) {
      uc32 chr = chars[i];
      if (chr != from()) {
        ranges->Add(CharacterRange::Singleton(chars[i]));
      }
    }
  } else if (from() <= kRangeCanonicalizeMax &&
             to() <= kRangeCanonicalizeMax) {
    // If this is a range we expand the characters block by block,
    // expanding contiguous subranges (blocks) one at a time.
    // The approach is as follows.  For a given start character we
    // look up the block that contains it, for instance 'a' if the
    // start character is 'c'.  A block is characterized by the property
    // that all characters uncanonicalize in the same way as the first
    // element, except that each entry in the result is incremented
    // by the distance from the first element.  So a-z is a block
    // because 'a' uncanonicalizes to ['a', 'A'] and the k'th letter
    // uncanonicalizes to ['a' + k, 'A' + k].
    // Once we've found the start point we look up its uncanonicalization
    // and produce a range for each element.  For instance for [c-f]
    // we look up ['a', 'A'] and produce [c-f] and [C-F].  We then only
    // add a range if it is not already contained in the input, so [c-f]
    // will be skipped but [C-F] will be added.  If this range is not
    // completely contained in a block we do this for all the blocks
    // covered by the range.
    unibrow::uchar range[unibrow::Ecma262UnCanonicalize::kMaxWidth];
    // First, look up the block that contains the 'from' character.
    int length = canonrange.get(from(), '\0', range);
    if (length == 0) {
      range[0] = from();
    } else {
      ASSERT_EQ(1, length);
    }
    int pos = from();
    // The start of the current block.  Note that except for the first
    // iteration 'start' is always equal to 'pos'.
    int start;
    // If it is not the start point of a block the entry contains the
    // offset of the character from the start point.
    if ((range[0] & kStartMarker) == 0) {
      start = pos - range[0];
    } else {
      start = pos;
    }
    // Then we add the ranges on at a time, incrementing the current
    // position to be after the last block each time.  The position
    // always points to the start of a block.
    while (pos < to()) {
      length = canonrange.get(start, '\0', range);
      if (length == 0) {
        range[0] = start;
      } else {
        ASSERT_EQ(1, length);
      }
      ASSERT((range[0] & kStartMarker) != 0);
      // The start point of a block contains the distance to the end
      // of the range.
      int block_end = start + (range[0] & kPayloadMask) - 1;
      int end = (block_end > to()) ? to() : block_end;
      length = uncanonicalize.get(start, '\0', range);
      for (int i = 0; i < length; i++) {
        uc32 c = range[i];
        uc16 range_from = c + (pos - start);
        uc16 range_to = c + (end - start);
        if (!(from() <= range_from && range_to <= to())) {
          ranges->Add(CharacterRange(range_from, range_to));
        }
      }
      start = pos = block_end + 1;
    }
  } else {
    // TODO(plesner) when we've fixed the 2^11 bug in unibrow.
  }
}


ZoneList<CharacterRange>* CharacterSet::ranges() {
  if (ranges_ == NULL) {
    ranges_ = new ZoneList<CharacterRange>(2);
    CharacterRange::AddClassEscape(standard_set_type_, ranges_);
  }
  return ranges_;
}



// -------------------------------------------------------------------
// Interest propagation


RegExpNode* RegExpNode::TryGetSibling(NodeInfo* info) {
  for (int i = 0; i < siblings_.length(); i++) {
    RegExpNode* sibling = siblings_.Get(i);
    if (sibling->info()->Matches(info))
      return sibling;
  }
  return NULL;
}


RegExpNode* RegExpNode::EnsureSibling(NodeInfo* info, bool* cloned) {
  ASSERT_EQ(false, *cloned);
  siblings_.Ensure(this);
  RegExpNode* result = TryGetSibling(info);
  if (result != NULL) return result;
  result = this->Clone();
  NodeInfo* new_info = result->info();
  new_info->ResetCompilationState();
  new_info->AddFromPreceding(info);
  AddSibling(result);
  *cloned = true;
  return result;
}


template <class C>
static RegExpNode* PropagateToEndpoint(C* node, NodeInfo* info) {
  NodeInfo full_info(*node->info());
  full_info.AddFromPreceding(info);
  bool cloned = false;
  return RegExpNode::EnsureSibling(node, &full_info, &cloned);
}


// -------------------------------------------------------------------
// Splay tree


OutSet* OutSet::Extend(unsigned value) {
  if (Get(value))
    return this;
  if (successors() != NULL) {
    for (int i = 0; i < successors()->length(); i++) {
      OutSet* successor = successors()->at(i);
      if (successor->Get(value))
        return successor;
    }
  } else {
    successors_ = new ZoneList<OutSet*>(2);
  }
  OutSet* result = new OutSet(first_, remaining_);
  result->Set(value);
  successors()->Add(result);
  return result;
}


void OutSet::Set(unsigned value) {
  if (value < kFirstLimit) {
    first_ |= (1 << value);
  } else {
    if (remaining_ == NULL)
      remaining_ = new ZoneList<unsigned>(1);
    if (remaining_->is_empty() || !remaining_->Contains(value))
      remaining_->Add(value);
  }
}


bool OutSet::Get(unsigned value) {
  if (value < kFirstLimit) {
    return (first_ & (1 << value)) != 0;
  } else if (remaining_ == NULL) {
    return false;
  } else {
    return remaining_->Contains(value);
  }
}


const uc16 DispatchTable::Config::kNoKey = unibrow::Utf8::kBadChar;
const DispatchTable::Entry DispatchTable::Config::kNoValue;


void DispatchTable::AddRange(CharacterRange full_range, int value) {
  CharacterRange current = full_range;
  if (tree()->is_empty()) {
    // If this is the first range we just insert into the table.
    ZoneSplayTree<Config>::Locator loc;
    ASSERT_RESULT(tree()->Insert(current.from(), &loc));
    loc.set_value(Entry(current.from(), current.to(), empty()->Extend(value)));
    return;
  }
  // First see if there is a range to the left of this one that
  // overlaps.
  ZoneSplayTree<Config>::Locator loc;
  if (tree()->FindGreatestLessThan(current.from(), &loc)) {
    Entry* entry = &loc.value();
    // If we've found a range that overlaps with this one, and it
    // starts strictly to the left of this one, we have to fix it
    // because the following code only handles ranges that start on
    // or after the start point of the range we're adding.
    if (entry->from() < current.from() && entry->to() >= current.from()) {
      // Snap the overlapping range in half around the start point of
      // the range we're adding.
      CharacterRange left(entry->from(), current.from() - 1);
      CharacterRange right(current.from(), entry->to());
      // The left part of the overlapping range doesn't overlap.
      // Truncate the whole entry to be just the left part.
      entry->set_to(left.to());
      // The right part is the one that overlaps.  We add this part
      // to the map and let the next step deal with merging it with
      // the range we're adding.
      ZoneSplayTree<Config>::Locator loc;
      ASSERT_RESULT(tree()->Insert(right.from(), &loc));
      loc.set_value(Entry(right.from(),
                          right.to(),
                          entry->out_set()));
    }
  }
  while (current.is_valid()) {
    if (tree()->FindLeastGreaterThan(current.from(), &loc) &&
        (loc.value().from() <= current.to()) &&
        (loc.value().to() >= current.from())) {
      Entry* entry = &loc.value();
      // We have overlap.  If there is space between the start point of
      // the range we're adding and where the overlapping range starts
      // then we have to add a range covering just that space.
      if (current.from() < entry->from()) {
        ZoneSplayTree<Config>::Locator ins;
        ASSERT_RESULT(tree()->Insert(current.from(), &ins));
        ins.set_value(Entry(current.from(),
                            entry->from() - 1,
                            empty()->Extend(value)));
        current.set_from(entry->from());
      }
      ASSERT_EQ(current.from(), entry->from());
      // If the overlapping range extends beyond the one we want to add
      // we have to snap the right part off and add it separately.
      if (entry->to() > current.to()) {
        ZoneSplayTree<Config>::Locator ins;
        ASSERT_RESULT(tree()->Insert(current.to() + 1, &ins));
        ins.set_value(Entry(current.to() + 1,
                            entry->to(),
                            entry->out_set()));
        entry->set_to(current.to());
      }
      ASSERT(entry->to() <= current.to());
      // The overlapping range is now completely contained by the range
      // we're adding so we can just update it and move the start point
      // of the range we're adding just past it.
      entry->AddValue(value);
      // Bail out if the last interval ended at 0xFFFF since otherwise
      // adding 1 will wrap around to 0.
      if (entry->to() == String::kMaxUC16CharCode)
        break;
      ASSERT(entry->to() + 1 > current.from());
      current.set_from(entry->to() + 1);
    } else {
      // There is no overlap so we can just add the range
      ZoneSplayTree<Config>::Locator ins;
      ASSERT_RESULT(tree()->Insert(current.from(), &ins));
      ins.set_value(Entry(current.from(),
                          current.to(),
                          empty()->Extend(value)));
      break;
    }
  }
}


OutSet* DispatchTable::Get(uc16 value) {
  ZoneSplayTree<Config>::Locator loc;
  if (!tree()->FindGreatestLessThan(value, &loc))
    return empty();
  Entry* entry = &loc.value();
  if (value <= entry->to())
    return entry->out_set();
  else
    return empty();
}


// -------------------------------------------------------------------
// Analysis


void Analysis::EnsureAnalyzed(RegExpNode* that) {
  if (that->info()->been_analyzed || that->info()->being_analyzed)
    return;
  that->info()->being_analyzed = true;
  that->Accept(this);
  that->info()->being_analyzed = false;
  that->info()->been_analyzed = true;
}


void Analysis::VisitEnd(EndNode* that) {
  // nothing to do
}


void TextNode::CalculateOffsets() {
  int element_count = elements()->length();
  // Set up the offsets of the elements relative to the start.  This is a fixed
  // quantity since a TextNode can only contain fixed-width things.
  int cp_offset = 0;
  for (int i = 0; i < element_count; i++) {
    TextElement& elm = elements()->at(i);
    elm.cp_offset = cp_offset;
    if (elm.type == TextElement::ATOM) {
      cp_offset += elm.data.u_atom->data().length();
    } else {
      cp_offset++;
      Vector<const uc16> quarks = elm.data.u_atom->data();
    }
  }
}


void Analysis::VisitText(TextNode* that) {
  if (ignore_case_) {
    that->MakeCaseIndependent();
  }
  EnsureAnalyzed(that->on_success());
  that->CalculateOffsets();
}


void Analysis::VisitAction(ActionNode* that) {
  RegExpNode* target = that->on_success();
  EnsureAnalyzed(target);
  // If the next node is interested in what it follows then this node
  // has to be interested too so it can pass the information on.
  that->info()->AddFromFollowing(target->info());
}


void Analysis::VisitChoice(ChoiceNode* that) {
  NodeInfo* info = that->info();
  for (int i = 0; i < that->alternatives()->length(); i++) {
    RegExpNode* node = that->alternatives()->at(i).node();
    EnsureAnalyzed(node);
    // Anything the following nodes need to know has to be known by
    // this node also, so it can pass it on.
    info->AddFromFollowing(node->info());
  }
}


void Analysis::VisitLoopChoice(LoopChoiceNode* that) {
  NodeInfo* info = that->info();
  for (int i = 0; i < that->alternatives()->length(); i++) {
    RegExpNode* node = that->alternatives()->at(i).node();
    if (node != that->loop_node()) {
      EnsureAnalyzed(node);
      info->AddFromFollowing(node->info());
    }
  }
  // Check the loop last since it may need the value of this node
  // to get a correct result.
  EnsureAnalyzed(that->loop_node());
  info->AddFromFollowing(that->loop_node()->info());
}


void Analysis::VisitBackReference(BackReferenceNode* that) {
  EnsureAnalyzed(that->on_success());
}


void Analysis::VisitAssertion(AssertionNode* that) {
  EnsureAnalyzed(that->on_success());
}


// -------------------------------------------------------------------
// Dispatch table construction


void DispatchTableConstructor::VisitEnd(EndNode* that) {
  AddRange(CharacterRange::Everything());
}


void DispatchTableConstructor::BuildTable(ChoiceNode* node) {
  node->set_being_calculated(true);
  ZoneList<GuardedAlternative>* alternatives = node->alternatives();
  for (int i = 0; i < alternatives->length(); i++) {
    set_choice_index(i);
    alternatives->at(i).node()->Accept(this);
  }
  node->set_being_calculated(false);
}


class AddDispatchRange {
 public:
  explicit AddDispatchRange(DispatchTableConstructor* constructor)
    : constructor_(constructor) { }
  void Call(uc32 from, DispatchTable::Entry entry);
 private:
  DispatchTableConstructor* constructor_;
};


void AddDispatchRange::Call(uc32 from, DispatchTable::Entry entry) {
  CharacterRange range(from, entry.to());
  constructor_->AddRange(range);
}


void DispatchTableConstructor::VisitChoice(ChoiceNode* node) {
  if (node->being_calculated())
    return;
  DispatchTable* table = node->GetTable(ignore_case_);
  AddDispatchRange adder(this);
  table->ForEach(&adder);
}


void DispatchTableConstructor::VisitBackReference(BackReferenceNode* that) {
  // TODO(160): Find the node that we refer back to and propagate its start
  // set back to here.  For now we just accept anything.
  AddRange(CharacterRange::Everything());
}


void DispatchTableConstructor::VisitAssertion(AssertionNode* that) {
  RegExpNode* target = that->on_success();
  target->Accept(this);
}



static int CompareRangeByFrom(const CharacterRange* a,
                              const CharacterRange* b) {
  return Compare<uc16>(a->from(), b->from());
}


void DispatchTableConstructor::AddInverse(ZoneList<CharacterRange>* ranges) {
  ranges->Sort(CompareRangeByFrom);
  uc16 last = 0;
  for (int i = 0; i < ranges->length(); i++) {
    CharacterRange range = ranges->at(i);
    if (last < range.from())
      AddRange(CharacterRange(last, range.from() - 1));
    if (range.to() >= last) {
      if (range.to() == String::kMaxUC16CharCode) {
        return;
      } else {
        last = range.to() + 1;
      }
    }
  }
  AddRange(CharacterRange(last, String::kMaxUC16CharCode));
}


void DispatchTableConstructor::VisitText(TextNode* that) {
  TextElement elm = that->elements()->at(0);
  switch (elm.type) {
    case TextElement::ATOM: {
      uc16 c = elm.data.u_atom->data()[0];
      AddRange(CharacterRange(c, c));
      break;
    }
    case TextElement::CHAR_CLASS: {
      RegExpCharacterClass* tree = elm.data.u_char_class;
      ZoneList<CharacterRange>* ranges = tree->ranges();
      if (tree->is_negated()) {
        AddInverse(ranges);
      } else {
        for (int i = 0; i < ranges->length(); i++)
          AddRange(ranges->at(i));
      }
      break;
    }
    default: {
      UNIMPLEMENTED();
    }
  }
}


void DispatchTableConstructor::VisitAction(ActionNode* that) {
  RegExpNode* target = that->on_success();
  target->Accept(this);
}


RegExpEngine::CompilationResult RegExpEngine::Compile(RegExpCompileData* data,
                                                      bool ignore_case,
                                                      bool is_multiline,
                                                      Handle<String> pattern,
                                                      bool is_ascii) {
  if ((data->capture_count + 1) * 2 - 1 > RegExpMacroAssembler::kMaxRegister) {
    return IrregexpRegExpTooBig();
  }
  RegExpCompiler compiler(data->capture_count, ignore_case, is_ascii);
  // Wrap the body of the regexp in capture #0.
  RegExpNode* captured_body = RegExpCapture::ToNode(data->tree,
                                                    0,
                                                    &compiler,
                                                    compiler.accept());
  RegExpNode* node = captured_body;
  if (!data->tree->IsAnchored()) {
    // Add a .*? at the beginning, outside the body capture, unless
    // this expression is anchored at the beginning.
    RegExpNode* loop_node =
        RegExpQuantifier::ToNode(0,
                                 RegExpTree::kInfinity,
                                 false,
                                 new RegExpCharacterClass('*'),
                                 &compiler,
                                 captured_body,
                                 data->contains_anchor);

    if (data->contains_anchor) {
      // Unroll loop once, to take care of the case that might start
      // at the start of input.
      ChoiceNode* first_step_node = new ChoiceNode(2);
      first_step_node->AddAlternative(GuardedAlternative(captured_body));
      first_step_node->AddAlternative(GuardedAlternative(
          new TextNode(new RegExpCharacterClass('*'), loop_node)));
      node = first_step_node;
    } else {
      node = loop_node;
    }
  }
  data->node = node;
  Analysis analysis(ignore_case);
  analysis.EnsureAnalyzed(node);

  NodeInfo info = *node->info();

  if (RegExpImpl::UseNativeRegexp()) {
#ifdef ARM
    UNREACHABLE();
#else  // IA32
    RegExpMacroAssemblerIA32::Mode mode;
    if (is_ascii) {
      mode = RegExpMacroAssemblerIA32::ASCII;
    } else {
      mode = RegExpMacroAssemblerIA32::UC16;
    }
    RegExpMacroAssemblerIA32 macro_assembler(mode,
                                             (data->capture_count + 1) * 2);
    return compiler.Assemble(&macro_assembler,
                             node,
                             data->capture_count,
                             pattern);
#endif
  }
  EmbeddedVector<byte, 1024> codes;
  RegExpMacroAssemblerIrregexp macro_assembler(codes);
  return compiler.Assemble(&macro_assembler,
                           node,
                           data->capture_count,
                           pattern);
}


}}  // namespace v8::internal
