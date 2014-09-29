// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/utils/random-number-generator.h"
#include "test/cctest/compiler/codegen-tester.h"

#if V8_TURBOFAN_TARGET

using namespace v8::internal;
using namespace v8::internal::compiler;

typedef StructuredMachineAssembler::IfBuilder IfBuilder;
typedef StructuredMachineAssembler::LoopBuilder Loop;

static const int32_t kUninitializedVariableOffset = -1;
static const int32_t kUninitializedOutput = -1;
static const int32_t kVerifiedOutput = -2;

static const int32_t kInitalVar = 1013;
static const int32_t kConjunctionInc = 1069;
static const int32_t kDisjunctionInc = 1151;
static const int32_t kThenInc = 1223;
static const int32_t kElseInc = 1291;
static const int32_t kIfInc = 1373;

class IfBuilderModel {
 public:
  explicit IfBuilderModel(Zone* zone)
      : zone_(zone),
        variable_offset_(0),
        root_(new (zone_) Node(NULL)),
        current_node_(root_),
        current_expression_(NULL) {}

  void If() {
    if (current_node_->else_node != NULL) {
      current_node_ = current_node_->else_node;
    } else if (current_node_->then_node != NULL) {
      current_node_ = current_node_->then_node;
    }
    DCHECK(current_expression_ == NULL);
    current_expression_ = new (zone_) Expression(zone_, NULL);
    current_node_->condition = current_expression_;
  }
  void IfNode() { LastChild()->variable_offset = variable_offset_++; }

  void OpenParen() { current_expression_ = LastChild(); }
  void CloseParen() { current_expression_ = current_expression_->parent; }

  void And() { NewChild()->conjunction = true; }
  void Or() { NewChild()->disjunction = true; }

  void Then() {
    DCHECK(current_expression_ == NULL || current_expression_->parent == NULL);
    current_expression_ = NULL;
    DCHECK(current_node_->then_node == NULL);
    current_node_->then_node = new (zone_) Node(current_node_);
  }
  void Else() {
    DCHECK(current_expression_ == NULL || current_expression_->parent == NULL);
    current_expression_ = NULL;
    DCHECK(current_node_->else_node == NULL);
    current_node_->else_node = new (zone_) Node(current_node_);
  }
  void Return() {
    if (current_node_->else_node != NULL) {
      current_node_->else_node->returns = true;
    } else if (current_node_->then_node != NULL) {
      current_node_->then_node->returns = true;
    } else {
      CHECK(false);
    }
  }
  void End() {}

  void Print(std::vector<char>* v) { PrintRecursive(v, root_); }

  struct VerificationState {
    int32_t* inputs;
    int32_t* outputs;
    int32_t var;
  };

  int32_t Verify(int length, int32_t* inputs, int32_t* outputs) {
    CHECK_EQ(variable_offset_, length);
    // Input/Output verification.
    for (int i = 0; i < length; ++i) {
      CHECK(inputs[i] == 0 || inputs[i] == 1);
      CHECK(outputs[i] == kUninitializedOutput || outputs[i] >= 0);
    }
    // Do verification.
    VerificationState state;
    state.inputs = inputs;
    state.outputs = outputs;
    state.var = kInitalVar;
    VerifyRecursive(root_, &state);
    // Verify all outputs marked.
    for (int i = 0; i < length; ++i) {
      CHECK(outputs[i] == kUninitializedOutput ||
            outputs[i] == kVerifiedOutput);
    }
    return state.var;
  }

 private:
  struct Expression;
  typedef std::vector<Expression*, zone_allocator<Expression*> > Expressions;

  struct Expression : public ZoneObject {
    Expression(Zone* zone, Expression* p)
        : variable_offset(kUninitializedVariableOffset),
          disjunction(false),
          conjunction(false),
          parent(p),
          children(Expressions::allocator_type(zone)) {}
    int variable_offset;
    bool disjunction;
    bool conjunction;
    Expression* parent;
    Expressions children;

   private:
    DISALLOW_COPY_AND_ASSIGN(Expression);
  };

  struct Node : public ZoneObject {
    explicit Node(Node* p)
        : parent(p),
          condition(NULL),
          then_node(NULL),
          else_node(NULL),
          returns(false) {}
    Node* parent;
    Expression* condition;
    Node* then_node;
    Node* else_node;
    bool returns;

   private:
    DISALLOW_COPY_AND_ASSIGN(Node);
  };

  Expression* LastChild() {
    if (current_expression_->children.empty()) {
      current_expression_->children.push_back(
          new (zone_) Expression(zone_, current_expression_));
    }
    return current_expression_->children.back();
  }

  Expression* NewChild() {
    Expression* child = new (zone_) Expression(zone_, current_expression_);
    current_expression_->children.push_back(child);
    return child;
  }

  static void PrintRecursive(std::vector<char>* v, Expression* expression) {
    CHECK(expression != NULL);
    if (expression->conjunction) {
      DCHECK(!expression->disjunction);
      v->push_back('&');
    } else if (expression->disjunction) {
      v->push_back('|');
    }
    if (expression->variable_offset != kUninitializedVariableOffset) {
      v->push_back('v');
    }
    Expressions& children = expression->children;
    if (children.empty()) return;
    v->push_back('(');
    for (Expressions::iterator i = children.begin(); i != children.end(); ++i) {
      PrintRecursive(v, *i);
    }
    v->push_back(')');
  }

  static void PrintRecursive(std::vector<char>* v, Node* node) {
    // Termination condition.
    if (node->condition == NULL) {
      CHECK(node->then_node == NULL && node->else_node == NULL);
      if (node->returns) v->push_back('r');
      return;
    }
    CHECK(!node->returns);
    v->push_back('i');
    PrintRecursive(v, node->condition);
    if (node->then_node != NULL) {
      v->push_back('t');
      PrintRecursive(v, node->then_node);
    }
    if (node->else_node != NULL) {
      v->push_back('e');
      PrintRecursive(v, node->else_node);
    }
  }

  static bool VerifyRecursive(Expression* expression,
                              VerificationState* state) {
    bool result = false;
    bool first_iteration = true;
    Expressions& children = expression->children;
    CHECK(!children.empty());
    for (Expressions::iterator i = children.begin(); i != children.end(); ++i) {
      Expression* child = *i;
      // Short circuit evaluation,
      // but mixes of &&s and ||s have weird semantics.
      if ((child->conjunction && !result) || (child->disjunction && result)) {
        continue;
      }
      if (child->conjunction) state->var += kConjunctionInc;
      if (child->disjunction) state->var += kDisjunctionInc;
      bool child_result;
      if (child->variable_offset != kUninitializedVariableOffset) {
        // Verify output
        CHECK_EQ(state->var, state->outputs[child->variable_offset]);
        state->outputs[child->variable_offset] = kVerifiedOutput;  // Mark seen.
        child_result = state->inputs[child->variable_offset];
        CHECK(child->children.empty());
        state->var += kIfInc;
      } else {
        child_result = VerifyRecursive(child, state);
      }
      if (child->conjunction) {
        result &= child_result;
      } else if (child->disjunction) {
        result |= child_result;
      } else {
        CHECK(first_iteration);
        result = child_result;
      }
      first_iteration = false;
    }
    return result;
  }

  static void VerifyRecursive(Node* node, VerificationState* state) {
    if (node->condition == NULL) return;
    bool result = VerifyRecursive(node->condition, state);
    if (result) {
      if (node->then_node) {
        state->var += kThenInc;
        return VerifyRecursive(node->then_node, state);
      }
    } else {
      if (node->else_node) {
        state->var += kElseInc;
        return VerifyRecursive(node->else_node, state);
      }
    }
  }

  Zone* zone_;
  int variable_offset_;
  Node* root_;
  Node* current_node_;
  Expression* current_expression_;
  DISALLOW_COPY_AND_ASSIGN(IfBuilderModel);
};


class IfBuilderGenerator : public StructuredMachineAssemblerTester<int32_t> {
 public:
  IfBuilderGenerator()
      : StructuredMachineAssemblerTester<int32_t>(
            MachineOperatorBuilder::pointer_rep(),
            MachineOperatorBuilder::pointer_rep()),
        var_(NewVariable(Int32Constant(kInitalVar))),
        c_(this),
        m_(this->zone()),
        one_(Int32Constant(1)),
        offset_(0) {}

  static void GenerateExpression(v8::base::RandomNumberGenerator* rng,
                                 std::vector<char>* v, int n_vars) {
    int depth = 1;
    v->push_back('(');
    bool need_if = true;
    bool populated = false;
    while (n_vars != 0) {
      if (need_if) {
        // can nest a paren or do a variable
        if (rng->NextBool()) {
          v->push_back('v');
          n_vars--;
          need_if = false;
          populated = true;
        } else {
          v->push_back('(');
          depth++;
          populated = false;
        }
      } else {
        // can pop, do && or do ||
        int options = 3;
        if (depth == 1 || !populated) {
          options--;
        }
        switch (rng->NextInt(options)) {
          case 0:
            v->push_back('&');
            need_if = true;
            break;
          case 1:
            v->push_back('|');
            need_if = true;
            break;
          case 2:
            v->push_back(')');
            depth--;
            break;
        }
      }
    }
    CHECK(!need_if);
    while (depth != 0) {
      v->push_back(')');
      depth--;
    }
  }

  static void GenerateIfThenElse(v8::base::RandomNumberGenerator* rng,
                                 std::vector<char>* v, int n_ifs,
                                 int max_exp_length) {
    CHECK_GT(n_ifs, 0);
    CHECK_GT(max_exp_length, 0);
    bool have_env = true;
    bool then_done = false;
    bool else_done = false;
    bool first_iteration = true;
    while (n_ifs != 0) {
      if (have_env) {
        int options = 3;
        if (else_done || first_iteration) {  // Don't do else or return
          options -= 2;
          first_iteration = false;
        }
        switch (rng->NextInt(options)) {
          case 0:
            v->push_back('i');
            n_ifs--;
            have_env = false;
            GenerateExpression(rng, v, rng->NextInt(max_exp_length) + 1);
            break;
          case 1:
            v->push_back('r');
            have_env = false;
            break;
          case 2:
            v->push_back('e');
            else_done = true;
            then_done = false;
            break;
          default:
            CHECK(false);
        }
      } else {  // Can only do then or else
        int options = 2;
        if (then_done) options--;
        switch (rng->NextInt(options)) {
          case 0:
            v->push_back('e');
            else_done = true;
            then_done = false;
            break;
          case 1:
            v->push_back('t');
            then_done = true;
            else_done = false;
            break;
          default:
            CHECK(false);
        }
        have_env = true;
      }
    }
    // Last instruction must have been an if, can complete it in several ways.
    int options = 2;
    if (then_done && !else_done) options++;
    switch (rng->NextInt(3)) {
      case 0:
        // Do nothing.
        break;
      case 1:
        v->push_back('t');
        switch (rng->NextInt(3)) {
          case 0:
            v->push_back('r');
            break;
          case 1:
            v->push_back('e');
            break;
          case 2:
            v->push_back('e');
            v->push_back('r');
            break;
          default:
            CHECK(false);
        }
        break;
      case 2:
        v->push_back('e');
        if (rng->NextBool()) v->push_back('r');
        break;
      default:
        CHECK(false);
    }
  }

  std::string::const_iterator ParseExpression(std::string::const_iterator it,
                                              std::string::const_iterator end) {
    // Prepare for expression.
    m_.If();
    c_.If();
    int depth = 0;
    for (; it != end; ++it) {
      switch (*it) {
        case 'v':
          m_.IfNode();
          {
            Node* offset = Int32Constant(offset_ * 4);
            Store(kMachineWord32, Parameter(1), offset, var_.Get());
            var_.Set(Int32Add(var_.Get(), Int32Constant(kIfInc)));
            c_.If(Load(kMachineWord32, Parameter(0), offset));
            offset_++;
          }
          break;
        case '&':
          m_.And();
          c_.And();
          var_.Set(Int32Add(var_.Get(), Int32Constant(kConjunctionInc)));
          break;
        case '|':
          m_.Or();
          c_.Or();
          var_.Set(Int32Add(var_.Get(), Int32Constant(kDisjunctionInc)));
          break;
        case '(':
          if (depth != 0) {
            m_.OpenParen();
            c_.OpenParen();
          }
          depth++;
          break;
        case ')':
          depth--;
          if (depth == 0) return it;
          m_.CloseParen();
          c_.CloseParen();
          break;
        default:
          CHECK(false);
      }
    }
    CHECK(false);
    return it;
  }

  void ParseIfThenElse(const std::string& str) {
    int n_vars = 0;
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
      if (*it == 'v') n_vars++;
    }
    InitializeConstants(n_vars);
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
      switch (*it) {
        case 'i': {
          it++;
          CHECK(it != str.end());
          CHECK_EQ('(', *it);
          it = ParseExpression(it, str.end());
          CHECK_EQ(')', *it);
          break;
        }
        case 't':
          m_.Then();
          c_.Then();
          var_.Set(Int32Add(var_.Get(), Int32Constant(kThenInc)));
          break;
        case 'e':
          m_.Else();
          c_.Else();
          var_.Set(Int32Add(var_.Get(), Int32Constant(kElseInc)));
          break;
        case 'r':
          m_.Return();
          Return(var_.Get());
          break;
        default:
          CHECK(false);
      }
    }
    m_.End();
    c_.End();
    Return(var_.Get());
    // Compare generated model to parsed version.
    {
      std::vector<char> v;
      m_.Print(&v);
      std::string m_str(v.begin(), v.end());
      CHECK(m_str == str);
    }
  }

  void ParseExpression(const std::string& str) {
    CHECK(inputs_.is_empty());
    std::string wrapped = "i(" + str + ")te";
    ParseIfThenElse(wrapped);
  }

  void ParseRandomIfThenElse(v8::base::RandomNumberGenerator* rng, int n_ifs,
                             int n_vars) {
    std::vector<char> v;
    GenerateIfThenElse(rng, &v, n_ifs, n_vars);
    std::string str(v.begin(), v.end());
    ParseIfThenElse(str);
  }

  void RunRandom(v8::base::RandomNumberGenerator* rng) {
    // TODO(dcarney): permute inputs via model.
    // TODO(dcarney): compute test_cases from n_ifs and n_vars.
    int test_cases = 100;
    for (int test = 0; test < test_cases; test++) {
      Initialize();
      for (int i = 0; i < offset_; i++) {
        inputs_[i] = rng->NextBool();
      }
      DoCall();
    }
  }

  void Run(const std::string& str, int32_t expected) {
    Initialize();
    int offset = 0;
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
      switch (*it) {
        case 't':
          inputs_[offset++] = 1;
          break;
        case 'f':
          inputs_[offset++] = 0;
          break;
        default:
          CHECK(false);
      }
    }
    CHECK_EQ(offset_, offset);
    // Call.
    int32_t result = DoCall();
    CHECK_EQ(result, expected);
  }

 private:
  typedef std::vector<int32_t, zone_allocator<int32_t> > IOVector;

  void InitializeConstants(int n_vars) {
    CHECK(inputs_.is_empty());
    inputs_.Reset(new int32_t[n_vars]);
    outputs_.Reset(new int32_t[n_vars]);
  }

  void Initialize() {
    for (int i = 0; i < offset_; i++) {
      inputs_[i] = 0;
      outputs_[i] = kUninitializedOutput;
    }
  }

  int32_t DoCall() {
    int32_t result = Call(inputs_.get(), outputs_.get());
    int32_t expected = m_.Verify(offset_, inputs_.get(), outputs_.get());
    CHECK_EQ(result, expected);
    return result;
  }

  const v8::internal::compiler::Variable var_;
  IfBuilder c_;
  IfBuilderModel m_;
  Node* one_;
  int32_t offset_;
  SmartArrayPointer<int32_t> inputs_;
  SmartArrayPointer<int32_t> outputs_;
};


TEST(RunExpressionString) {
  IfBuilderGenerator m;
  m.ParseExpression("((v|v)|v)");
  m.Run("ttt", kInitalVar + 1 * kIfInc + kThenInc);
  m.Run("ftt", kInitalVar + 2 * kIfInc + kDisjunctionInc + kThenInc);
  m.Run("fft", kInitalVar + 3 * kIfInc + 2 * kDisjunctionInc + kThenInc);
  m.Run("fff", kInitalVar + 3 * kIfInc + 2 * kDisjunctionInc + kElseInc);
}


TEST(RunExpressionStrings) {
  const char* strings[] = {
      "v",       "(v)",     "((v))",     "v|v",
      "(v|v)",   "((v|v))", "v&v",       "(v&v)",
      "((v&v))", "v&(v)",   "v&(v|v)",   "v&(v|v)&v",
      "v|(v)",   "v|(v&v)", "v|(v&v)|v", "v|(((v)|(v&v)|(v)|v)&(v))|v",
  };
  v8::base::RandomNumberGenerator rng;
  for (size_t i = 0; i < ARRAY_SIZE(strings); i++) {
    IfBuilderGenerator m;
    m.ParseExpression(strings[i]);
    m.RunRandom(&rng);
  }
}


TEST(RunSimpleIfElseTester) {
  const char* tests[] = {
      "i(v)",   "i(v)t",   "i(v)te",
      "i(v)er", "i(v)ter", "i(v)ti(v)trei(v)ei(v)ei(v)ei(v)ei(v)ei(v)ei(v)e"};
  v8::base::RandomNumberGenerator rng;
  for (size_t i = 0; i < ARRAY_SIZE(tests); ++i) {
    IfBuilderGenerator m;
    m.ParseIfThenElse(tests[i]);
    m.RunRandom(&rng);
  }
}


TEST(RunRandomExpressions) {
  v8::base::RandomNumberGenerator rng;
  for (int n_vars = 1; n_vars < 12; n_vars++) {
    for (int i = 0; i < n_vars * n_vars + 10; i++) {
      IfBuilderGenerator m;
      m.ParseRandomIfThenElse(&rng, 1, n_vars);
      m.RunRandom(&rng);
    }
  }
}


TEST(RunRandomIfElse) {
  v8::base::RandomNumberGenerator rng;
  for (int n_ifs = 1; n_ifs < 12; n_ifs++) {
    for (int i = 0; i < n_ifs * n_ifs + 10; i++) {
      IfBuilderGenerator m;
      m.ParseRandomIfThenElse(&rng, n_ifs, 1);
      m.RunRandom(&rng);
    }
  }
}


TEST(RunRandomIfElseExpressions) {
  v8::base::RandomNumberGenerator rng;
  for (int n_vars = 2; n_vars < 6; n_vars++) {
    for (int n_ifs = 2; n_ifs < 7; n_ifs++) {
      for (int i = 0; i < n_ifs * n_vars + 10; i++) {
        IfBuilderGenerator m;
        m.ParseRandomIfThenElse(&rng, n_ifs, n_vars);
        m.RunRandom(&rng);
      }
    }
  }
}

#endif
