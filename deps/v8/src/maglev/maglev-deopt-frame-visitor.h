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

template <typename DeoptInfoT>
class DeoptInfoVisitor {
 public:
  template <typename Function>
  static void ForEager(DeoptInfoT* deopt_info, Function&& f) {
    DeoptInfoVisitor<DeoptInfoT> visitor(deopt_info);
    visitor.Visit(deopt_info->top_frame(), f);
  }

  template <typename Function>
  static void ForLazy(DeoptInfoT* deopt_info, Function&& f) {
    DeoptInfoVisitor<DeoptInfoT> visitor(deopt_info);
    if (deopt_info->top_frame().parent()) {
      visitor.Visit(*deopt_info->top_frame().parent(), f);
    }
    const bool kSkipResultLocation = true;
    visitor.VisitSingleFrame<kSkipResultLocation>(deopt_info->top_frame(), f);
  }

 private:
  const DeoptInfoT* deopt_info_;
  const VirtualObjectList virtual_objects_;

  using DeoptFrameT = std::conditional_t<std::is_const_v<DeoptInfoT>,
                                         const DeoptFrame, DeoptFrame>;
  using ValueNodeT =
      std::conditional_t<std::is_const_v<DeoptInfoT>, ValueNode*, ValueNode*&>;

  explicit DeoptInfoVisitor(DeoptInfoT* deopt_info)
      : deopt_info_(deopt_info),
        virtual_objects_(deopt_info->top_frame().GetVirtualObjects()) {}

  template <typename Function>
  void Visit(DeoptFrameT& frame, Function&& f) {
    if (frame.parent()) {
      Visit(*frame.parent(), f);
    }
    VisitSingleFrame(frame, f);
  }

  template <bool skip_frame_result = false, typename Function>
  void VisitSingleFrame(DeoptFrameT& frame, Function&& f) {
    auto updated_f = [&](ValueNodeT node) {
      DCHECK(!node->template Is<VirtualObject>());
      if (node->template Is<Identity>()) {
        node = node->input(0).node();
      }
      if (auto alloc = node->template TryCast<InlinedAllocation>()) {
        VirtualObject* vobject = virtual_objects_.FindAllocatedWith(alloc);
        if (vobject && (!alloc->HasBeenAnalysed() || alloc->HasBeenElided())) {
          return vobject->ForEachNestedRuntimeInput(virtual_objects_, f);
        }
      }
      f(node);
    };
    switch (frame.type()) {
      case DeoptFrame::FrameType::kInterpretedFrame:
        updated_f(frame.as_interpreted().closure());
        frame.as_interpreted().frame_state()->ForEachValue(
            frame.as_interpreted().unit(),
            [&](ValueNode*& node, interpreter::Register reg) {
              if constexpr (std::is_same_v<DeoptInfoT, LazyDeoptInfo>) {
                // Skip over the result location for lazy deopts, since it is
                // irrelevant for lazy deopts (unoptimized code will recreate
                // the result).
                if (skip_frame_result && deopt_info_->IsResultRegister(reg)) {
                  return;
                }
              }
              updated_f(node);
            });
        break;
      case DeoptFrame::FrameType::kInlinedArgumentsFrame: {
        // The inlined arguments frame can never be the top frame.
        updated_f(frame.as_inlined_arguments().closure());
        for (ValueNode*& node : frame.as_inlined_arguments().arguments()) {
          updated_f(node);
        }
        break;
      }
      case DeoptFrame::FrameType::kConstructInvokeStubFrame: {
        updated_f(frame.as_construct_stub().receiver());
        updated_f(frame.as_construct_stub().context());
        break;
      }
      case DeoptFrame::FrameType::kBuiltinContinuationFrame:
        for (ValueNode*& node : frame.as_builtin_continuation().parameters()) {
          updated_f(node);
        }
        updated_f(frame.as_builtin_continuation().context());
        break;
    }
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

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_DEOPT_FRAME_VISITOR_H_
