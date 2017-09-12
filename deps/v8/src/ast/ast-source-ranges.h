// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_SOURCE_RANGES_H_
#define V8_AST_AST_SOURCE_RANGES_H_

#include "src/ast/ast.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

// Specifies a range within the source code. {start} is 0-based and inclusive,
// {end} is 0-based and exclusive.
struct SourceRange {
  SourceRange() : SourceRange(kNoSourcePosition, kNoSourcePosition) {}
  SourceRange(int start, int end) : start(start), end(end) {}
  bool IsEmpty() const { return start == kNoSourcePosition; }
  static SourceRange Empty() { return SourceRange(); }
  static SourceRange OpenEnded(int32_t start) {
    return SourceRange(start, kNoSourcePosition);
  }
  static SourceRange ContinuationOf(const SourceRange& that) {
    return that.IsEmpty() ? Empty() : OpenEnded(that.end);
  }
  int32_t start, end;
};

// The list of ast node kinds that have associated source ranges.
#define AST_SOURCE_RANGE_LIST(V) \
  V(Block)                       \
  V(CaseClause)                  \
  V(Conditional)                 \
  V(IfStatement)                 \
  V(IterationStatement)          \
  V(JumpStatement)               \
  V(SwitchStatement)             \
  V(Throw)                       \
  V(TryCatchStatement)           \
  V(TryFinallyStatement)

enum class SourceRangeKind {
  kBody,
  kCatch,
  kContinuation,
  kElse,
  kFinally,
  kThen,
};

class AstNodeSourceRanges : public ZoneObject {
 public:
  virtual ~AstNodeSourceRanges() {}
  virtual SourceRange GetRange(SourceRangeKind kind) = 0;
};

class ContinuationSourceRanges : public AstNodeSourceRanges {
 public:
  explicit ContinuationSourceRanges(int32_t continuation_position)
      : continuation_position_(continuation_position) {}

  SourceRange GetRange(SourceRangeKind kind) {
    DCHECK(kind == SourceRangeKind::kContinuation);
    return SourceRange::OpenEnded(continuation_position_);
  }

 private:
  int32_t continuation_position_;
};

class BlockSourceRanges final : public ContinuationSourceRanges {
 public:
  explicit BlockSourceRanges(int32_t continuation_position)
      : ContinuationSourceRanges(continuation_position) {}
};

class CaseClauseSourceRanges final : public AstNodeSourceRanges {
 public:
  explicit CaseClauseSourceRanges(const SourceRange& body_range)
      : body_range_(body_range) {}

  SourceRange GetRange(SourceRangeKind kind) {
    DCHECK(kind == SourceRangeKind::kBody);
    return body_range_;
  }

 private:
  SourceRange body_range_;
};

class ConditionalSourceRanges final : public AstNodeSourceRanges {
 public:
  explicit ConditionalSourceRanges(const SourceRange& then_range,
                                   const SourceRange& else_range)
      : then_range_(then_range), else_range_(else_range) {}

  SourceRange GetRange(SourceRangeKind kind) {
    switch (kind) {
      case SourceRangeKind::kThen:
        return then_range_;
      case SourceRangeKind::kElse:
        return else_range_;
      default:
        UNREACHABLE();
    }
  }

 private:
  SourceRange then_range_;
  SourceRange else_range_;
};

class IfStatementSourceRanges final : public AstNodeSourceRanges {
 public:
  explicit IfStatementSourceRanges(const SourceRange& then_range,
                                   const SourceRange& else_range)
      : then_range_(then_range), else_range_(else_range) {}

  SourceRange GetRange(SourceRangeKind kind) {
    switch (kind) {
      case SourceRangeKind::kElse:
        return else_range_;
      case SourceRangeKind::kThen:
        return then_range_;
      case SourceRangeKind::kContinuation: {
        const SourceRange& trailing_range =
            else_range_.IsEmpty() ? then_range_ : else_range_;
        return SourceRange::ContinuationOf(trailing_range);
      }
      default:
        UNREACHABLE();
    }
  }

 private:
  SourceRange then_range_;
  SourceRange else_range_;
};

class IterationStatementSourceRanges final : public AstNodeSourceRanges {
 public:
  explicit IterationStatementSourceRanges(const SourceRange& body_range)
      : body_range_(body_range) {}

  SourceRange GetRange(SourceRangeKind kind) {
    switch (kind) {
      case SourceRangeKind::kBody:
        return body_range_;
      case SourceRangeKind::kContinuation:
        return SourceRange::ContinuationOf(body_range_);
      default:
        UNREACHABLE();
    }
  }

 private:
  SourceRange body_range_;
};

class JumpStatementSourceRanges final : public ContinuationSourceRanges {
 public:
  explicit JumpStatementSourceRanges(int32_t continuation_position)
      : ContinuationSourceRanges(continuation_position) {}
};

class SwitchStatementSourceRanges final : public ContinuationSourceRanges {
 public:
  explicit SwitchStatementSourceRanges(int32_t continuation_position)
      : ContinuationSourceRanges(continuation_position) {}
};

class ThrowSourceRanges final : public ContinuationSourceRanges {
 public:
  explicit ThrowSourceRanges(int32_t continuation_position)
      : ContinuationSourceRanges(continuation_position) {}
};

class TryCatchStatementSourceRanges final : public AstNodeSourceRanges {
 public:
  explicit TryCatchStatementSourceRanges(const SourceRange& catch_range)
      : catch_range_(catch_range) {}

  SourceRange GetRange(SourceRangeKind kind) {
    DCHECK(kind == SourceRangeKind::kCatch);
    return catch_range_;
  }

 private:
  SourceRange catch_range_;
};

class TryFinallyStatementSourceRanges final : public AstNodeSourceRanges {
 public:
  explicit TryFinallyStatementSourceRanges(const SourceRange& finally_range)
      : finally_range_(finally_range) {}

  SourceRange GetRange(SourceRangeKind kind) {
    DCHECK(kind == SourceRangeKind::kFinally);
    return finally_range_;
  }

 private:
  SourceRange finally_range_;
};

// Maps ast node pointers to associated source ranges. The parser creates these
// mappings and the bytecode generator consumes them.
class SourceRangeMap final : public ZoneObject {
 public:
  explicit SourceRangeMap(Zone* zone) : map_(zone) {}

  AstNodeSourceRanges* Find(AstNode* node) {
    auto it = map_.find(node);
    if (it == map_.end()) return nullptr;
    return it->second;
  }

// Type-checked insertion.
#define DEFINE_MAP_INSERT(type)                         \
  void Insert(type* node, type##SourceRanges* ranges) { \
    map_.emplace(node, ranges);                         \
  }
  AST_SOURCE_RANGE_LIST(DEFINE_MAP_INSERT)
#undef DEFINE_MAP_INSERT

 private:
  ZoneMap<AstNode*, AstNodeSourceRanges*> map_;
};

#undef AST_SOURCE_RANGE_LIST

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_SOURCE_RANGES_H_
