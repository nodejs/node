// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATOR_H_
#define V8_COMPILER_OPERATOR_H_

#include <ostream>  // NOLINT(readability/streams)

#include "src/base/flags.h"
#include "src/base/functional.h"
#include "src/zone.h"

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

  // Constructor.
  Operator(Opcode opcode, Properties properties, const char* mnemonic,
           size_t value_in, size_t effect_in, size_t control_in,
           size_t value_out, size_t effect_out, size_t control_out);

  virtual ~Operator() {}

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
  virtual bool Equals(const Operator* that) const {
    return this->opcode() == that->opcode();
  }

  // Compute a hashcode to speed up equivalence-set checking.
  // Equal operators should always have equal hashcodes, and unequal operators
  // should have unequal hashcodes with high probability.
  virtual size_t HashCode() const { return base::hash<Opcode>()(opcode()); }

  // Check whether this operator has the given property.
  bool HasProperty(Property property) const {
    return (properties() & property) == property;
  }

  // Number of data inputs to the operator, for verifying graph structure.
  // TODO(titzer): convert callers to ValueInputCount();
  int InputCount() const { return ValueInputCount(); }

  // Number of data outputs from the operator, for verifying graph structure.
  // TODO(titzer): convert callers to ValueOutputCount();
  int OutputCount() const { return ValueOutputCount(); }

  Properties properties() const { return properties_; }

  // TODO(titzer): convert return values here to size_t.
  int ValueInputCount() const { return value_in_; }
  int EffectInputCount() const { return effect_in_; }
  int ControlInputCount() const { return control_in_; }

  int ValueOutputCount() const { return value_out_; }
  int EffectOutputCount() const { return effect_out_; }
  int ControlOutputCount() const { return control_out_; }

  static inline size_t ZeroIfPure(Properties properties) {
    return (properties & kPure) == kPure ? 0 : 1;
  }

  // TODO(titzer): API for input and output types, for typechecking graph.
 protected:
  // Print the full operator into the given stream, including any
  // static parameters. Useful for debugging and visualizing the IR.
  virtual void PrintTo(std::ostream& os) const;
  friend std::ostream& operator<<(std::ostream& os, const Operator& op);

 private:
  Opcode opcode_;
  Properties properties_;
  const char* mnemonic_;
  uint32_t value_in_;
  uint16_t effect_in_;
  uint16_t control_in_;
  uint16_t value_out_;
  uint8_t effect_out_;
  uint8_t control_out_;

  DISALLOW_COPY_AND_ASSIGN(Operator);
};

DEFINE_OPERATORS_FOR_FLAGS(Operator::Properties)

std::ostream& operator<<(std::ostream& os, const Operator& op);


// A templatized implementation of Operator that has one static parameter of
// type {T}.
template <typename T, typename Pred = std::equal_to<T>,
          typename Hash = base::hash<T>>
class Operator1 : public Operator {
 public:
  Operator1(Opcode opcode, Properties properties, const char* mnemonic,
            size_t value_in, size_t effect_in, size_t control_in,
            size_t value_out, size_t effect_out, size_t control_out,
            T parameter, Pred const& pred = Pred(), Hash const& hash = Hash())
      : Operator(opcode, properties, mnemonic, value_in, effect_in, control_in,
                 value_out, effect_out, control_out),
        parameter_(parameter),
        pred_(pred),
        hash_(hash) {}

  T const& parameter() const { return parameter_; }

  virtual bool Equals(const Operator* other) const FINAL {
    if (opcode() != other->opcode()) return false;
    const Operator1<T>* that = static_cast<const Operator1<T>*>(other);
    return this->pred_(this->parameter(), that->parameter());
  }
  virtual size_t HashCode() const FINAL {
    return base::hash_combine(this->opcode(), this->hash_(this->parameter()));
  }
  virtual void PrintParameter(std::ostream& os) const {
    os << "[" << this->parameter() << "]";
  }

 protected:
  virtual void PrintTo(std::ostream& os) const FINAL {
    os << mnemonic();
    PrintParameter(os);
  }

 private:
  T const parameter_;
  Pred const pred_;
  Hash const hash_;
};


// Helper to extract parameters from Operator1<*> operator.
template <typename T>
inline T const& OpParameter(const Operator* op) {
  return static_cast<const Operator1<T>*>(op)->parameter();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPERATOR_H_
