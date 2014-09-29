// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATOR_H_
#define V8_COMPILER_OPERATOR_H_

#include "src/v8.h"

#include "src/assembler.h"
#include "src/ostreams.h"
#include "src/unique.h"

namespace v8 {
namespace internal {
namespace compiler {

// An operator represents description of the "computation" of a node in the
// compiler IR. A computation takes values (i.e. data) as input and produces
// zero or more values as output. The side-effects of a computation must be
// captured by additional control and data dependencies which are part of the
// IR graph.
// Operators are immutable and describe the statically-known parts of a
// computation. Thus they can be safely shared by many different nodes in the
// IR graph, or even globally between graphs. Operators can have "static
// parameters" which are compile-time constant parameters to the operator, such
// as the name for a named field access, the ID of a runtime function, etc.
// Static parameters are private to the operator and only semantically
// meaningful to the operator itself.
class Operator : public ZoneObject {
 public:
  Operator(uint8_t opcode, uint16_t properties)
      : opcode_(opcode), properties_(properties) {}
  virtual ~Operator() {}

  // Properties inform the operator-independent optimizer about legal
  // transformations for nodes that have this operator.
  enum Property {
    kNoProperties = 0,
    kReducible = 1 << 0,    // Participates in strength reduction.
    kCommutative = 1 << 1,  // OP(a, b) == OP(b, a) for all inputs.
    kAssociative = 1 << 2,  // OP(a, OP(b,c)) == OP(OP(a,b), c) for all inputs.
    kIdempotent = 1 << 3,   // OP(a); OP(a) == OP(a).
    kNoRead = 1 << 4,       // Has no scheduling dependency on Effects
    kNoWrite = 1 << 5,      // Does not modify any Effects and thereby
                            // create new scheduling dependencies.
    kNoThrow = 1 << 6,      // Can never generate an exception.
    kFoldable = kNoRead | kNoWrite,
    kEliminatable = kNoWrite | kNoThrow,
    kPure = kNoRead | kNoWrite | kNoThrow | kIdempotent
  };

  // A small integer unique to all instances of a particular kind of operator,
  // useful for quick matching for specific kinds of operators. For fast access
  // the opcode is stored directly in the operator object.
  inline uint8_t opcode() const { return opcode_; }

  // Returns a constant string representing the mnemonic of the operator,
  // without the static parameters. Useful for debugging.
  virtual const char* mnemonic() = 0;

  // Check if this operator equals another operator. Equivalent operators can
  // be merged, and nodes with equivalent operators and equivalent inputs
  // can be merged.
  virtual bool Equals(Operator* other) = 0;

  // Compute a hashcode to speed up equivalence-set checking.
  // Equal operators should always have equal hashcodes, and unequal operators
  // should have unequal hashcodes with high probability.
  virtual int HashCode() = 0;

  // Check whether this operator has the given property.
  inline bool HasProperty(Property property) const {
    return (properties_ & static_cast<int>(property)) == property;
  }

  // Number of data inputs to the operator, for verifying graph structure.
  virtual int InputCount() = 0;

  // Number of data outputs from the operator, for verifying graph structure.
  virtual int OutputCount() = 0;

  inline Property properties() { return static_cast<Property>(properties_); }

  // TODO(titzer): API for input and output types, for typechecking graph.
 private:
  // Print the full operator into the given stream, including any
  // static parameters. Useful for debugging and visualizing the IR.
  virtual OStream& PrintTo(OStream& os) const = 0;  // NOLINT
  friend OStream& operator<<(OStream& os, const Operator& op);

  uint8_t opcode_;
  uint16_t properties_;
};

OStream& operator<<(OStream& os, const Operator& op);

// An implementation of Operator that has no static parameters. Such operators
// have just a name, an opcode, and a fixed number of inputs and outputs.
// They can represented by singletons and shared globally.
class SimpleOperator : public Operator {
 public:
  SimpleOperator(uint8_t opcode, uint16_t properties, int input_count,
                 int output_count, const char* mnemonic)
      : Operator(opcode, properties),
        input_count_(input_count),
        output_count_(output_count),
        mnemonic_(mnemonic) {}

  virtual const char* mnemonic() { return mnemonic_; }
  virtual bool Equals(Operator* that) { return opcode() == that->opcode(); }
  virtual int HashCode() { return opcode(); }
  virtual int InputCount() { return input_count_; }
  virtual int OutputCount() { return output_count_; }

 private:
  virtual OStream& PrintTo(OStream& os) const {  // NOLINT
    return os << mnemonic_;
  }

  int input_count_;
  int output_count_;
  const char* mnemonic_;
};

// Template specialization implements a kind of type class for dealing with the
// static parameters of Operator1 automatically.
template <typename T>
struct StaticParameterTraits {
  static OStream& PrintTo(OStream& os, T val) {  // NOLINT
    return os << "??";
  }
  static int HashCode(T a) { return 0; }
  static bool Equals(T a, T b) {
    return false;  // Not every T has a ==. By default, be conservative.
  }
};

template <>
struct StaticParameterTraits<ExternalReference> {
  static OStream& PrintTo(OStream& os, ExternalReference val) {  // NOLINT
    os << val.address();
    const Runtime::Function* function =
        Runtime::FunctionForEntry(val.address());
    if (function != NULL) {
      os << " <" << function->name << ".entry>";
    }
    return os;
  }
  static int HashCode(ExternalReference a) {
    return reinterpret_cast<intptr_t>(a.address()) & 0xFFFFFFFF;
  }
  static bool Equals(ExternalReference a, ExternalReference b) {
    return a == b;
  }
};

// Specialization for static parameters of type {int}.
template <>
struct StaticParameterTraits<int> {
  static OStream& PrintTo(OStream& os, int val) {  // NOLINT
    return os << val;
  }
  static int HashCode(int a) { return a; }
  static bool Equals(int a, int b) { return a == b; }
};

// Specialization for static parameters of type {double}.
template <>
struct StaticParameterTraits<double> {
  static OStream& PrintTo(OStream& os, double val) {  // NOLINT
    return os << val;
  }
  static int HashCode(double a) {
    return static_cast<int>(BitCast<int64_t>(a));
  }
  static bool Equals(double a, double b) {
    return BitCast<int64_t>(a) == BitCast<int64_t>(b);
  }
};

// Specialization for static parameters of type {PrintableUnique<Object>}.
template <>
struct StaticParameterTraits<PrintableUnique<Object> > {
  static OStream& PrintTo(OStream& os, PrintableUnique<Object> val) {  // NOLINT
    return os << val.string();
  }
  static int HashCode(PrintableUnique<Object> a) {
    return static_cast<int>(a.Hashcode());
  }
  static bool Equals(PrintableUnique<Object> a, PrintableUnique<Object> b) {
    return a == b;
  }
};

// Specialization for static parameters of type {PrintableUnique<Name>}.
template <>
struct StaticParameterTraits<PrintableUnique<Name> > {
  static OStream& PrintTo(OStream& os, PrintableUnique<Name> val) {  // NOLINT
    return os << val.string();
  }
  static int HashCode(PrintableUnique<Name> a) {
    return static_cast<int>(a.Hashcode());
  }
  static bool Equals(PrintableUnique<Name> a, PrintableUnique<Name> b) {
    return a == b;
  }
};

#if DEBUG
// Specialization for static parameters of type {Handle<Object>} to prevent any
// direct usage of Handles in constants.
template <>
struct StaticParameterTraits<Handle<Object> > {
  static OStream& PrintTo(OStream& os, Handle<Object> val) {  // NOLINT
    UNREACHABLE();  // Should use PrintableUnique<Object> instead
    return os;
  }
  static int HashCode(Handle<Object> a) {
    UNREACHABLE();  // Should use PrintableUnique<Object> instead
    return 0;
  }
  static bool Equals(Handle<Object> a, Handle<Object> b) {
    UNREACHABLE();  // Should use PrintableUnique<Object> instead
    return false;
  }
};
#endif

// A templatized implementation of Operator that has one static parameter of
// type {T}. If a specialization of StaticParameterTraits<{T}> exists, then
// operators of this kind can automatically be hashed, compared, and printed.
template <typename T>
class Operator1 : public Operator {
 public:
  Operator1(uint8_t opcode, uint16_t properties, int input_count,
            int output_count, const char* mnemonic, T parameter)
      : Operator(opcode, properties),
        input_count_(input_count),
        output_count_(output_count),
        mnemonic_(mnemonic),
        parameter_(parameter) {}

  const T& parameter() const { return parameter_; }

  virtual const char* mnemonic() { return mnemonic_; }
  virtual bool Equals(Operator* other) {
    if (opcode() != other->opcode()) return false;
    Operator1<T>* that = static_cast<Operator1<T>*>(other);
    T temp1 = this->parameter_;
    T temp2 = that->parameter_;
    return StaticParameterTraits<T>::Equals(temp1, temp2);
  }
  virtual int HashCode() {
    return opcode() + 33 * StaticParameterTraits<T>::HashCode(this->parameter_);
  }
  virtual int InputCount() { return input_count_; }
  virtual int OutputCount() { return output_count_; }
  virtual OStream& PrintParameter(OStream& os) const {  // NOLINT
    return StaticParameterTraits<T>::PrintTo(os << "[", parameter_) << "]";
  }

 private:
  virtual OStream& PrintTo(OStream& os) const {  // NOLINT
    return PrintParameter(os << mnemonic_);
  }

  int input_count_;
  int output_count_;
  const char* mnemonic_;
  T parameter_;
};

// Type definitions for operators with specific types of parameters.
typedef Operator1<PrintableUnique<Name> > NameOperator;
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_OPERATOR_H_
