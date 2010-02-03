// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_FRAMES_H_
#define V8_FRAMES_H_

namespace v8 {
namespace internal {

typedef uint32_t RegList;

// Get the number of registers in a given register list.
int NumRegs(RegList list);

// Return the code of the n-th saved register available to JavaScript.
int JSCallerSavedCode(int n);


// Forward declarations.
class StackFrameIterator;
class Top;
class ThreadLocalTop;


class StackHandler BASE_EMBEDDED {
 public:
  enum State {
    ENTRY,
    TRY_CATCH,
    TRY_FINALLY
  };

  // Get the address of this stack handler.
  inline Address address() const;

  // Get the next stack handler in the chain.
  inline StackHandler* next() const;

  // Tells whether the given address is inside this handler.
  inline bool includes(Address address) const;

  // Garbage collection support.
  inline void Iterate(ObjectVisitor* v) const;

  // Conversion support.
  static inline StackHandler* FromAddress(Address address);

  // Testers
  bool is_entry() { return state() == ENTRY; }
  bool is_try_catch() { return state() == TRY_CATCH; }
  bool is_try_finally() { return state() == TRY_FINALLY; }

  // Garbage collection support.
  void Cook(Code* code);
  void Uncook(Code* code);

 private:
  // Accessors.
  inline State state() const;

  inline Address pc() const;
  inline void set_pc(Address value);

  DISALLOW_IMPLICIT_CONSTRUCTORS(StackHandler);
};


#define STACK_FRAME_TYPE_LIST(V)              \
  V(ENTRY,             EntryFrame)            \
  V(ENTRY_CONSTRUCT,   EntryConstructFrame)   \
  V(EXIT,              ExitFrame)             \
  V(JAVA_SCRIPT,       JavaScriptFrame)       \
  V(INTERNAL,          InternalFrame)         \
  V(CONSTRUCT,         ConstructFrame)        \
  V(ARGUMENTS_ADAPTOR, ArgumentsAdaptorFrame)


// Abstract base class for all stack frames.
class StackFrame BASE_EMBEDDED {
 public:
#define DECLARE_TYPE(type, ignore) type,
  enum Type {
    NONE = 0,
    STACK_FRAME_TYPE_LIST(DECLARE_TYPE)
    NUMBER_OF_TYPES
  };
#undef DECLARE_TYPE

  // Opaque data type for identifying stack frames. Used extensively
  // by the debugger.
  enum Id { NO_ID = 0 };

  // Type testers.
  bool is_entry() const { return type() == ENTRY; }
  bool is_entry_construct() const { return type() == ENTRY_CONSTRUCT; }
  bool is_exit() const { return type() == EXIT; }
  bool is_java_script() const { return type() == JAVA_SCRIPT; }
  bool is_arguments_adaptor() const { return type() == ARGUMENTS_ADAPTOR; }
  bool is_internal() const { return type() == INTERNAL; }
  bool is_construct() const { return type() == CONSTRUCT; }
  virtual bool is_standard() const { return false; }

  // Accessors.
  Address sp() const { return state_.sp; }
  Address fp() const { return state_.fp; }
  Address caller_sp() const { return GetCallerStackPointer(); }

  Address pc() const { return *pc_address(); }
  void set_pc(Address pc) { *pc_address() = pc; }

  Address* pc_address() const { return state_.pc_address; }

  // Get the id of this stack frame.
  Id id() const { return static_cast<Id>(OffsetFrom(caller_sp())); }

  // Checks if this frame includes any stack handlers.
  bool HasHandler() const;

  // Get the type of this frame.
  virtual Type type() const = 0;

  // Get the code associated with this frame.
  virtual Code* code() const = 0;

  // Garbage collection support.
  static void CookFramesForThread(ThreadLocalTop* thread);
  static void UncookFramesForThread(ThreadLocalTop* thread);

  virtual void Iterate(ObjectVisitor* v) const { }

  // Printing support.
  enum PrintMode { OVERVIEW, DETAILS };
  virtual void Print(StringStream* accumulator,
                     PrintMode mode,
                     int index) const { }

 protected:
  struct State {
    Address sp;
    Address fp;
    Address* pc_address;
  };

  explicit StackFrame(StackFrameIterator* iterator) : iterator_(iterator) { }
  virtual ~StackFrame() { }

  // Compute the stack pointer for the calling frame.
  virtual Address GetCallerStackPointer() const = 0;

  // Printing support.
  static void PrintIndex(StringStream* accumulator,
                         PrintMode mode,
                         int index);

  // Get the top handler from the current stack iterator.
  inline StackHandler* top_handler() const;

  // Compute the stack frame type for the given state.
  static Type ComputeType(State* state);

 private:
  const StackFrameIterator* iterator_;
  State state_;

  // Fill in the state of the calling frame.
  virtual void ComputeCallerState(State* state) const = 0;

  // Get the type and the state of the calling frame.
  virtual Type GetCallerState(State* state) const;

  // Cooking/uncooking support.
  void Cook();
  void Uncook();

  friend class StackFrameIterator;
  friend class StackHandlerIterator;
  friend class SafeStackFrameIterator;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StackFrame);
};


// Entry frames are used to enter JavaScript execution from C.
class EntryFrame: public StackFrame {
 public:
  virtual Type type() const { return ENTRY; }

  virtual Code* code() const;

  // Garbage collection support.
  virtual void Iterate(ObjectVisitor* v) const;

  static EntryFrame* cast(StackFrame* frame) {
    ASSERT(frame->is_entry());
    return static_cast<EntryFrame*>(frame);
  }

 protected:
  explicit EntryFrame(StackFrameIterator* iterator) : StackFrame(iterator) { }

  // The caller stack pointer for entry frames is always zero. The
  // real information about the caller frame is available through the
  // link to the top exit frame.
  virtual Address GetCallerStackPointer() const { return 0; }

 private:
  virtual void ComputeCallerState(State* state) const;
  virtual Type GetCallerState(State* state) const;

  friend class StackFrameIterator;
};


class EntryConstructFrame: public EntryFrame {
 public:
  virtual Type type() const { return ENTRY_CONSTRUCT; }

  virtual Code* code() const;

  static EntryConstructFrame* cast(StackFrame* frame) {
    ASSERT(frame->is_entry_construct());
    return static_cast<EntryConstructFrame*>(frame);
  }

 protected:
  explicit EntryConstructFrame(StackFrameIterator* iterator)
      : EntryFrame(iterator) { }

 private:
  friend class StackFrameIterator;
};


// Exit frames are used to exit JavaScript execution and go to C.
class ExitFrame: public StackFrame {
 public:
  enum Mode { MODE_NORMAL, MODE_DEBUG };
  virtual Type type() const { return EXIT; }

  virtual Code* code() const;

  Object*& code_slot() const;

  // Garbage collection support.
  virtual void Iterate(ObjectVisitor* v) const;

  static ExitFrame* cast(StackFrame* frame) {
    ASSERT(frame->is_exit());
    return static_cast<ExitFrame*>(frame);
  }

  // Compute the state and type of an exit frame given a frame
  // pointer. Used when constructing the first stack frame seen by an
  // iterator and the frames following entry frames.
  static Type GetStateForFramePointer(Address fp, State* state);

 protected:
  explicit ExitFrame(StackFrameIterator* iterator) : StackFrame(iterator) { }

  virtual Address GetCallerStackPointer() const;

 private:
  virtual void ComputeCallerState(State* state) const;

  friend class StackFrameIterator;
};


class StandardFrame: public StackFrame {
 public:
  // Testers.
  virtual bool is_standard() const { return true; }

  // Accessors.
  inline Object* context() const;

  // Access the expressions in the stack frame including locals.
  inline Object* GetExpression(int index) const;
  inline void SetExpression(int index, Object* value);
  int ComputeExpressionsCount() const;

  static StandardFrame* cast(StackFrame* frame) {
    ASSERT(frame->is_standard());
    return static_cast<StandardFrame*>(frame);
  }

 protected:
  explicit StandardFrame(StackFrameIterator* iterator)
      : StackFrame(iterator) { }

  virtual void ComputeCallerState(State* state) const;

  // Accessors.
  inline Address caller_fp() const;
  inline Address caller_pc() const;

  // Computes the address of the PC field in the standard frame given
  // by the provided frame pointer.
  static inline Address ComputePCAddress(Address fp);

  // Iterate over expression stack including stack handlers, locals,
  // and parts of the fixed part including context and code fields.
  void IterateExpressions(ObjectVisitor* v) const;

  // Returns the address of the n'th expression stack element.
  Address GetExpressionAddress(int n) const;

  // Determines if the n'th expression stack element is in a stack
  // handler or not. Requires traversing all handlers in this frame.
  bool IsExpressionInsideHandler(int n) const;

  // Determines if the standard frame for the given frame pointer is
  // an arguments adaptor frame.
  static inline bool IsArgumentsAdaptorFrame(Address fp);

  // Determines if the standard frame for the given frame pointer is a
  // construct frame.
  static inline bool IsConstructFrame(Address fp);

 private:
  friend class StackFrame;
};


class JavaScriptFrame: public StandardFrame {
 public:
  virtual Type type() const { return JAVA_SCRIPT; }

  // Accessors.
  inline Object* function() const;
  inline Object* receiver() const;
  inline void set_receiver(Object* value);

  // Access the parameters.
  Object* GetParameter(int index) const;
  int ComputeParametersCount() const;

  // Temporary way of getting access to the number of parameters
  // passed on the stack by the caller. Once argument adaptor frames
  // has been introduced on ARM, this number will always match the
  // computed parameters count.
  int GetProvidedParametersCount() const;

  // Check if this frame is a constructor frame invoked through 'new'.
  bool IsConstructor() const;

  // Check if this frame has "adapted" arguments in the sense that the
  // actual passed arguments are available in an arguments adaptor
  // frame below it on the stack.
  inline bool has_adapted_arguments() const;

  // Garbage collection support.
  virtual void Iterate(ObjectVisitor* v) const;

  // Printing support.
  virtual void Print(StringStream* accumulator,
                     PrintMode mode,
                     int index) const;

  // Determine the code for the frame.
  virtual Code* code() const;

  static JavaScriptFrame* cast(StackFrame* frame) {
    ASSERT(frame->is_java_script());
    return static_cast<JavaScriptFrame*>(frame);
  }

 protected:
  explicit JavaScriptFrame(StackFrameIterator* iterator)
      : StandardFrame(iterator), disable_heap_access_(false) { }

  virtual Address GetCallerStackPointer() const;

  // When this mode is enabled it is not allowed to access heap objects.
  // This is a special mode used when gathering stack samples in profiler.
  // A shortcoming is that caller's SP value will be calculated incorrectly
  // (see GetCallerStackPointer implementation), but it is not used for stack
  // sampling.
  void DisableHeapAccess() { disable_heap_access_ = true; }

 private:
  bool disable_heap_access_;
  inline Object* function_slot_object() const;

  friend class StackFrameIterator;
};


// Arguments adaptor frames are automatically inserted below
// JavaScript frames when the actual number of parameters does not
// match the formal number of parameters.
class ArgumentsAdaptorFrame: public JavaScriptFrame {
 public:
  virtual Type type() const { return ARGUMENTS_ADAPTOR; }

  // Determine the code for the frame.
  virtual Code* code() const;

  static ArgumentsAdaptorFrame* cast(StackFrame* frame) {
    ASSERT(frame->is_arguments_adaptor());
    return static_cast<ArgumentsAdaptorFrame*>(frame);
  }

  // Printing support.
  virtual void Print(StringStream* accumulator,
                     PrintMode mode,
                     int index) const;
 protected:
  explicit ArgumentsAdaptorFrame(StackFrameIterator* iterator)
      : JavaScriptFrame(iterator) { }

  virtual Address GetCallerStackPointer() const;

 private:
  friend class StackFrameIterator;
};


class InternalFrame: public StandardFrame {
 public:
  virtual Type type() const { return INTERNAL; }

  // Garbage collection support.
  virtual void Iterate(ObjectVisitor* v) const;

  // Determine the code for the frame.
  virtual Code* code() const;

  static InternalFrame* cast(StackFrame* frame) {
    ASSERT(frame->is_internal());
    return static_cast<InternalFrame*>(frame);
  }

 protected:
  explicit InternalFrame(StackFrameIterator* iterator)
      : StandardFrame(iterator) { }

  virtual Address GetCallerStackPointer() const;

 private:
  friend class StackFrameIterator;
};


// Construct frames are special trampoline frames introduced to handle
// function invocations through 'new'.
class ConstructFrame: public InternalFrame {
 public:
  virtual Type type() const { return CONSTRUCT; }

  static ConstructFrame* cast(StackFrame* frame) {
    ASSERT(frame->is_construct());
    return static_cast<ConstructFrame*>(frame);
  }

 protected:
  explicit ConstructFrame(StackFrameIterator* iterator)
      : InternalFrame(iterator) { }

 private:
  friend class StackFrameIterator;
};


class StackFrameIterator BASE_EMBEDDED {
 public:
  // An iterator that iterates over the current thread's stack.
  StackFrameIterator();

  // An iterator that iterates over a given thread's stack.
  explicit StackFrameIterator(ThreadLocalTop* thread);

  // An iterator that can start from a given FP address.
  // If use_top, then work as usual, if fp isn't NULL, use it,
  // otherwise, do nothing.
  StackFrameIterator(bool use_top, Address fp, Address sp);

  StackFrame* frame() const {
    ASSERT(!done());
    return frame_;
  }

  bool done() const { return frame_ == NULL; }
  void Advance() { (this->*advance_)(); }

  // Go back to the first frame.
  void Reset();

 private:
#define DECLARE_SINGLETON(ignore, type) type type##_;
  STACK_FRAME_TYPE_LIST(DECLARE_SINGLETON)
#undef DECLARE_SINGLETON
  StackFrame* frame_;
  StackHandler* handler_;
  ThreadLocalTop* thread_;
  Address fp_;
  Address sp_;
  void (StackFrameIterator::*advance_)();

  StackHandler* handler() const {
    ASSERT(!done());
    return handler_;
  }

  // Get the type-specific frame singleton in a given state.
  StackFrame* SingletonFor(StackFrame::Type type, StackFrame::State* state);
  // A helper function, can return a NULL pointer.
  StackFrame* SingletonFor(StackFrame::Type type);

  void AdvanceWithHandler();
  void AdvanceWithoutHandler();

  friend class StackFrame;
  friend class SafeStackFrameIterator;
  DISALLOW_COPY_AND_ASSIGN(StackFrameIterator);
};


// Iterator that supports iterating through all JavaScript frames.
template<typename Iterator>
class JavaScriptFrameIteratorTemp BASE_EMBEDDED {
 public:
  JavaScriptFrameIteratorTemp() { if (!done()) Advance(); }

  explicit JavaScriptFrameIteratorTemp(ThreadLocalTop* thread) :
      iterator_(thread) {
    if (!done()) Advance();
  }

  // Skip frames until the frame with the given id is reached.
  explicit JavaScriptFrameIteratorTemp(StackFrame::Id id);

  JavaScriptFrameIteratorTemp(Address fp, Address sp,
                              Address low_bound, Address high_bound) :
      iterator_(fp, sp, low_bound, high_bound) {
    if (!done()) Advance();
  }

  inline JavaScriptFrame* frame() const;

  bool done() const { return iterator_.done(); }
  void Advance();

  // Advance to the frame holding the arguments for the current
  // frame. This only affects the current frame if it has adapted
  // arguments.
  void AdvanceToArgumentsFrame();

  // Go back to the first frame.
  void Reset();

 private:
  Iterator iterator_;
};


typedef JavaScriptFrameIteratorTemp<StackFrameIterator> JavaScriptFrameIterator;


// NOTE: The stack trace frame iterator is an iterator that only
// traverse proper JavaScript frames; that is JavaScript frames that
// have proper JavaScript functions. This excludes the problematic
// functions in runtime.js.
class StackTraceFrameIterator: public JavaScriptFrameIterator {
 public:
  StackTraceFrameIterator();
  void Advance();

 private:
  bool IsValidFrame();
};


class SafeStackFrameIterator BASE_EMBEDDED {
 public:
  SafeStackFrameIterator(Address fp, Address sp,
                         Address low_bound, Address high_bound);

  StackFrame* frame() const {
    ASSERT(is_working_iterator_);
    return iterator_.frame();
  }

  bool done() const { return iteration_done_ ? true : iterator_.done(); }

  void Advance();
  void Reset();

  static bool IsWithinBounds(
      Address low_bound, Address high_bound, Address addr) {
    return low_bound <= addr && addr <= high_bound;
  }

 private:
  bool IsValidStackAddress(Address addr) const {
    return IsWithinBounds(low_bound_, high_bound_, addr);
  }
  bool CanIterateHandles(StackFrame* frame, StackHandler* handler);
  bool IsValidFrame(StackFrame* frame) const;
  bool IsValidCaller(StackFrame* frame);

  Address low_bound_;
  Address high_bound_;
  const bool is_valid_top_;
  const bool is_valid_fp_;
  const bool is_working_iterator_;
  bool iteration_done_;
  StackFrameIterator iterator_;
};


#ifdef ENABLE_LOGGING_AND_PROFILING
typedef JavaScriptFrameIteratorTemp<SafeStackFrameIterator>
    SafeJavaScriptFrameIterator;


class SafeStackTraceFrameIterator: public SafeJavaScriptFrameIterator {
 public:
  explicit SafeStackTraceFrameIterator(Address fp, Address sp,
                                       Address low_bound, Address high_bound);
  void Advance();
};
#endif


class StackFrameLocator BASE_EMBEDDED {
 public:
  // Find the nth JavaScript frame on the stack. The caller must
  // guarantee that such a frame exists.
  JavaScriptFrame* FindJavaScriptFrame(int n);

 private:
  StackFrameIterator iterator_;
};


} }  // namespace v8::internal

#endif  // V8_FRAMES_H_
