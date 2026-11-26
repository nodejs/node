// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_DEOPT_FRAME_VISITOR_H_
#define V8_MAGLEV_MAGLEV_DEOPT_FRAME_VISITOR_H_

#include <type_traits>

#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

template <typename DeoptFrameT, typename Function>
inline void DeoptFrame::ForEachValueImpl(DeoptFrameT& frame, Function&& f) {
  switch (frame.type()) {
    case FrameType::kInterpretedFrame:
      f(frame.as_interpreted().closure());
      frame.as_interpreted().frame_state()->ForEachValue(
          frame.as_interpreted().unit(),
          [&](ValueNode*& node, interpreter::Register) { f(node); });
      break;
    case FrameType::kInlinedArgumentsFrame:
      f(frame.as_inlined_arguments().closure());
      for (ValueNode*& node : frame.as_inlined_arguments().arguments()) {
        f(node);
      }
      break;
    case FrameType::kConstructInvokeStubFrame:
      f(frame.as_construct_stub().receiver());
      f(frame.as_construct_stub().context());
      break;
    case FrameType::kBuiltinContinuationFrame:
      for (ValueNode*& node : frame.as_builtin_continuation().parameters()) {
        f(node);
      }
      f(frame.as_builtin_continuation().context());
      break;
  }
}

template <typename Function>
inline void DeoptFrame::ForEachValue(Function&& f) {
  ForEachValueImpl(*this, std::forward<Function>(f));
}

template <typename Function>
inline void DeoptFrame::ForEachValue(Function&& f) const {
  ForEachValueImpl(*this, std::forward<Function>(f));
}

template <typename DeoptInfoT>
class DeoptInfoVisitor {
 public:
  template <typename Function>
  static void ForEager(DeoptInfoT* deopt_info, Function&& f) {
    Visit(deopt_info->top_frame(), f);
  }

  template <typename Function>
  static void ForLazy(DeoptInfoT* deopt_info, Function&& f) {
    if (deopt_info->top_frame().parent()) {
      Visit(*deopt_info->top_frame().parent(), f);
    }
    VisitSingleFrame(deopt_info->top_frame(), f);
  }

 private:
  using DeoptFrameT = std::conditional_t<std::is_const_v<DeoptInfoT>,
                                         const DeoptFrame, DeoptFrame>;
  using ValueNodeT =
      std::conditional_t<std::is_const_v<DeoptInfoT>, ValueNode*, ValueNode*&>;
  using VirtualObjectT = std::conditional_t<std::is_const_v<DeoptInfoT>,
                                            const VirtualObject, VirtualObject>;

  template <typename Function>
  static void Visit(DeoptFrameT& frame, Function&& f) {
    if (frame.parent()) {
      Visit(*frame.parent(), f);
    }
    VisitSingleFrame(frame, f);
  }

  template <typename Function>
  static void VisitSingleFrame(DeoptFrameT& frame, Function&& f) {
    auto updated_f = [&](ValueNodeT node) {
      if constexpr (std::is_same_v<ValueNodeT, ValueNode*&>) {
        node = node->UnwrapAndUpdateUseCountForDeopt();
      } else {
        node = node->Unwrap();
      }
      if (VirtualObjectT* vobject = node->TryGetDeoptVirtualObject()) {
        return vobject->ForEachNestedRuntimeInput(
            f, VirtualObject::ForEachSlotIterationMode::kForDeopt);
      }
      f(node);
    };
    frame.ForEachValue(updated_f);
  }
};

template <typename Function>
void EagerDeoptInfo::ForEachInput(Function&& f) {
  DeoptInfoVisitor<EagerDeoptInfo>::ForEager(this, f);
}

template <typename Function>
void EagerDeoptInfo::ForEachInput(Function&& f) const {
  DeoptInfoVisitor<const EagerDeoptInfo>::ForEager(this, f);
}

template <typename Function>
void LazyDeoptInfo::ForEachInput(Function&& f) {
  DeoptInfoVisitor<LazyDeoptInfo>::ForLazy(this, f);
}

template <typename Function>
void LazyDeoptInfo::ForEachInput(Function&& f) const {
  DeoptInfoVisitor<const LazyDeoptInfo>::ForLazy(this, f);
}

inline void EagerDeoptInfo::Unwrap() {
  // The visitor automatically unwrap conversion, identities and ReturnedValues.
  ForEachInput([&](ValueNode*) {});
}

inline void LazyDeoptInfo::Unwrap() {
  // The visitor automatically unwrap conversion, identities and ReturnedValues.
  ForEachInput([](ValueNode*) {});
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_DEOPT_FRAME_VISITOR_H_
