// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATOR_H_
#define V8_COMPILER_OPERATOR_H_

#include "src/base/flags.h"
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
  typedef uint8_t Opcode;

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
  typedef base::Flags<Property, uint8_t> Properties;

  Operator(Opcode opcode, Properties properties, const char* mnemonic)
      : opcode_(opcode), properties_(properties), mnemonic_(mnemonic) {}
  virtual ~Operator();

  // A small integer unique to all instances of a particular kind of operator,
  // useful for quick matching for specific kinds of operators. For fast access
  // the opcode is stored directly in the operator object.
  Opcode opcode() const { return opcode_; }

  // Returns a constant string representing the mnemonic of the operator,
  // without the static parameters. Useful for debugging.
  const char* mnemonic() const { return mnemonic_; }

  // Check if this operator equals another operator. Equivalent operators can
  // be merged, and nodes with equivalent operators and equivalent inputs
  // can be merged.
  virtual bool Equals(const Operator* other) const = 0;

  // Compute a hashcode to speed up equivalence-set checking.
  // Equal operators should always have equal hashcodes, and unequal operators
  // should have unequal hashcodes with high probability.
  virtual int HashCode() const = 0;

  // Check whether this operator has the given property.
  bool HasProperty(Property property) const {
    return (properties() & property) == property;
  }

  // Number of data inputs to the operator, for verifying graph structure.
  virtual int InputCount() const = 0;

  // Number of data outputs from the operator, for verifying graph structure.
  virtual int OutputCount() const = 0;

  Properties properties() const { return properties_; }

  // TODO(titzer): API for input and output types, for typechecking graph.
 protected:
  // Print the full operator into the given stream, including any
  // static parameters. Useful for debugging and visualizing the IR.
  virtual OStream& PrintTo(OStream& os) const = 0;  // NOLINT
  friend OStream& operator<<(OStream& os, const Operator& op);

 private:
  Opcode opcode_;
  Properties properties_;
  const char* mnemonic_;

  DISALLOW_COPY_AND_ASSIGN(Operator);
};

DEFINE_OPERATORS_FOR_FLAGS(Operator::Properties)

OStream& operator<<(OStream& os, const Operator& op);

// An implementation of Operator that has no static parameters. Such operators
// have just a name, an opcode, and a fixed number of inputs and outputs.
// They can represented by singletons and shared globally.
class SimpleOperator : public Operator {
 public:
  SimpleOperator(Opcode opcode, Properties properties, int input_count,
                 int output_count, const char* mnemonic);
  ~SimpleOperator();

  virtual bool Equals(const Operator* that) const FINAL {
    return opcode() == that->opcode();
  }
  virtual int HashCode() const FINAL { return opcode(); }
  virtual int InputCount() const FINAL { return input_count_; }
  virtual int OutputCount() const FINAL { return output_count_; }

 private:
  virtual OStream& PrintTo(OStream& os) const FINAL {  // NOLINT
    return os << mnemonic();
  }

  int input_count_;
  int output_count_;

  DISALLOW_COPY_AND_ASSIGN(SimpleOperator);
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
    return static_cast<int>(bit_cast<int64_t>(a));
  }
  static bool Equals(double a, double b) {
    return bit_cast<int64_t>(a) == bit_cast<int64_t>(b);
  }
};

// Specialization for static parameters of type {Unique<Object>}.
template <>
struct StaticParameterTraits<Unique<Object> > {
  static OStream& PrintTo(OStream& os, Unique<Object> val) {  // NOLINT
    return os << Brief(*val.handle());
  }
  static int HashCode(Unique<Object> a) {
    return static_cast<int>(a.Hashcode());
  }
  static bool Equals(Unique<Object> a, Unique<Object> b) { return a == b; }
};

// Specialization for static parameters of type {Unique<Name>}.
template <>
struct StaticParameterTraits<Unique<Name> > {
  static OStream& PrintTo(OStream& os, Unique<Name> val) {  // NOLINT
    return os << Brief(*val.handle());
  }
  static int HashCode(Unique<Name> a) { return static_cast<int>(a.Hashcode()); }
  static bool Equals(Unique<Name> a, Unique<Name> b) { return a == b; }
};

#if DEBUG
// Specialization for static parameters of type {Handle<Object>} to prevent any
// direct usage of Handles in constants.
template <>
struct StaticParameterTraits<Handle<Object> > {
  static OStream& PrintTo(OStream& os, Handle<Object> val) {  // NOLINT
    UNREACHABLE();  // Should use Unique<Object> instead
    return os;
  }
  static int HashCode(Handle<Object> a) {
    UNREACHABLE();  // Should use Unique<Object> instead
    return 0;
  }
  static bool Equals(Handle<Object> a, Handle<Object> b) {
    UNREACHABLE();  // Should use Unique<Object> instead
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
  Operator1(Opcode opcode, Properties properties, int input_count,
            int output_count, const char* mnemonic, T parameter)
      : Operator(opcode, properties, mnemonic),
        input_count_(input_count),
        output_count_(output_count),
        parameter_(parameter) {}

  const T& parameter() const { return parameter_; }

  virtual bool Equals(const Operator* other) const OVERRIDE {
    if (opcode() != other->opcode()) return false;
    const Operator1<T>* that = static_cast<const Operator1<T>*>(other);
    return StaticParameterTraits<T>::Equals(this->parameter_, that->parameter_);
  }
  virtual int HashCode() const OVERRIDE {
    return opcode() + 33 * StaticParameterTraits<T>::HashCode(this->parameter_);
  }
  virtual int InputCount() const OVERRIDE { return input_count_; }
  virtual int OutputCount() const OVERRIDE { return output_count_; }
  virtual OStream& PrintParameter(OStream& os) const {  // NOLINT
    return StaticParameterTraits<T>::PrintTo(os << "[", parameter_) << "]";
  }

 protected:
  virtual OStream& PrintTo(OStream& os) const FINAL {  // NOLINT
    return PrintParameter(os << mnemonic());
  }

 private:
  int input_count_;
  int output_count_;
  T parameter_;
};


// Helper to extract parameters from Operator1<*> operator.
template <typename T>
static inline const T& OpParameter(const Operator* op) {
  return reinterpret_cast<const Operator1<T>*>(op)->parameter();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPERATOR_H_
