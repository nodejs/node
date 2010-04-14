// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "codegen-inl.h"
#include "register-allocator-inl.h"
#include "virtual-frame-inl.h"

namespace v8 {
namespace internal {

void VirtualFrame::SetElementAt(int index, Result* value) {
  int frame_index = element_count() - index - 1;
  ASSERT(frame_index >= 0);
  ASSERT(frame_index < element_count());
  ASSERT(value->is_valid());
  FrameElement original = elements_[frame_index];

  // Early exit if the element is the same as the one being set.
  bool same_register = original.is_register()
      && value->is_register()
      && original.reg().is(value->reg());
  bool same_constant = original.is_constant()
      && value->is_constant()
      && original.handle().is_identical_to(value->handle());
  if (same_register || same_constant) {
    value->Unuse();
    return;
  }

  InvalidateFrameSlotAt(frame_index);

  if (value->is_register()) {
    if (is_used(value->reg())) {
      // The register already appears on the frame.  Either the existing
      // register element, or the new element at frame_index, must be made
      // a copy.
      int i = register_location(value->reg());

      if (i < frame_index) {
        // The register FrameElement is lower in the frame than the new copy.
        elements_[frame_index] = CopyElementAt(i);
      } else {
        // There was an early bailout for the case of setting a
        // register element to itself.
        ASSERT(i != frame_index);
        elements_[frame_index] = elements_[i];
        elements_[i] = CopyElementAt(frame_index);
        if (elements_[frame_index].is_synced()) {
          elements_[i].set_sync();
        }
        elements_[frame_index].clear_sync();
        set_register_location(value->reg(), frame_index);
        for (int j = i + 1; j < element_count(); j++) {
          if (elements_[j].is_copy() && elements_[j].index() == i) {
            elements_[j].set_index(frame_index);
          }
        }
      }
    } else {
      // The register value->reg() was not already used on the frame.
      Use(value->reg(), frame_index);
      elements_[frame_index] =
          FrameElement::RegisterElement(value->reg(),
                                        FrameElement::NOT_SYNCED,
                                        value->type_info());
    }
  } else {
    ASSERT(value->is_constant());
    elements_[frame_index] =
        FrameElement::ConstantElement(value->handle(),
                                      FrameElement::NOT_SYNCED);
  }
  value->Unuse();
}


// Create a duplicate of an existing valid frame element.
// We can pass an optional number type information that will override the
// existing information about the backing element. The new information must
// not conflict with the existing type information and must be equally or
// more precise. The default parameter value kUninitialized means that there
// is no additional information.
FrameElement VirtualFrame::CopyElementAt(int index, TypeInfo info) {
  ASSERT(index >= 0);
  ASSERT(index < element_count());

  FrameElement target = elements_[index];
  FrameElement result;

  switch (target.type()) {
    case FrameElement::CONSTANT:
      // We do not copy constants and instead return a fresh unsynced
      // constant.
      result = FrameElement::ConstantElement(target.handle(),
                                             FrameElement::NOT_SYNCED);
      break;

    case FrameElement::COPY:
      // We do not allow copies of copies, so we follow one link to
      // the actual backing store of a copy before making a copy.
      index = target.index();
      ASSERT(elements_[index].is_memory() || elements_[index].is_register());
      // Fall through.

    case FrameElement::MEMORY:  // Fall through.
    case FrameElement::REGISTER: {
      // All copies are backed by memory or register locations.
      result.set_type(FrameElement::COPY);
      result.clear_copied();
      result.clear_sync();
      result.set_index(index);
      elements_[index].set_copied();
      // Update backing element's number information.
      TypeInfo existing = elements_[index].type_info();
      ASSERT(!existing.IsUninitialized());
      // Assert that the new type information (a) does not conflict with the
      // existing one and (b) is equally or more precise.
      ASSERT((info.ToInt() & existing.ToInt()) == existing.ToInt());
      ASSERT((info.ToInt() | existing.ToInt()) == info.ToInt());

      elements_[index].set_type_info(!info.IsUninitialized()
                                       ? info
                                       : existing);
      break;
    }
    case FrameElement::INVALID:
      // We should not try to copy invalid elements.
      UNREACHABLE();
      break;
  }
  return result;
}


// Modify the state of the virtual frame to match the actual frame by adding
// extra in-memory elements to the top of the virtual frame.  The extra
// elements will be externally materialized on the actual frame (eg, by
// pushing an exception handler).  No code is emitted.
void VirtualFrame::Adjust(int count) {
  ASSERT(count >= 0);
  ASSERT(stack_pointer_ == element_count() - 1);

  for (int i = 0; i < count; i++) {
    elements_.Add(FrameElement::MemoryElement(TypeInfo::Unknown()));
  }
  stack_pointer_ += count;
}


void VirtualFrame::ForgetElements(int count) {
  ASSERT(count >= 0);
  ASSERT(element_count() >= count);

  for (int i = 0; i < count; i++) {
    FrameElement last = elements_.RemoveLast();
    if (last.is_register()) {
      // A hack to properly count register references for the code
      // generator's current frame and also for other frames.  The
      // same code appears in PrepareMergeTo.
      if (cgen()->frame() == this) {
        Unuse(last.reg());
      } else {
        set_register_location(last.reg(), kIllegalIndex);
      }
    }
  }
}


// Make the type of the element at a given index be MEMORY.
void VirtualFrame::SpillElementAt(int index) {
  if (!elements_[index].is_valid()) return;

  SyncElementAt(index);
  // Number type information is preserved.
  // Copies get their number information from their backing element.
  TypeInfo info;
  if (!elements_[index].is_copy()) {
    info = elements_[index].type_info();
  } else {
    info = elements_[elements_[index].index()].type_info();
  }
  // The element is now in memory.  Its copied flag is preserved.
  FrameElement new_element = FrameElement::MemoryElement(info);
  if (elements_[index].is_copied()) {
    new_element.set_copied();
  }
  if (elements_[index].is_untagged_int32()) {
    new_element.set_untagged_int32(true);
  }
  if (elements_[index].is_register()) {
    Unuse(elements_[index].reg());
  }
  elements_[index] = new_element;
}


// Clear the dirty bit for the element at a given index.
void VirtualFrame::SyncElementAt(int index) {
  if (index <= stack_pointer_) {
    if (!elements_[index].is_synced()) SyncElementBelowStackPointer(index);
  } else if (index == stack_pointer_ + 1) {
    SyncElementByPushing(index);
  } else {
    SyncRange(stack_pointer_ + 1, index);
  }
}


void VirtualFrame::PrepareMergeTo(VirtualFrame* expected) {
  // Perform state changes on this frame that will make merge to the
  // expected frame simpler or else increase the likelihood that his
  // frame will match another.
  for (int i = 0; i < element_count(); i++) {
    FrameElement source = elements_[i];
    FrameElement target = expected->elements_[i];

    if (!target.is_valid() ||
        (target.is_memory() && !source.is_memory() && source.is_synced())) {
      // No code needs to be generated to invalidate valid elements.
      // No code needs to be generated to move values to memory if
      // they are already synced.  We perform those moves here, before
      // merging.
      if (source.is_register()) {
        // If the frame is the code generator's current frame, we have
        // to decrement both the frame-internal and global register
        // counts.
        if (cgen()->frame() == this) {
          Unuse(source.reg());
        } else {
          set_register_location(source.reg(), kIllegalIndex);
        }
      }
      elements_[i] = target;
    } else if (target.is_register() && !target.is_synced() &&
               !source.is_memory()) {
      // If an element's target is a register that doesn't need to be
      // synced, and the element is not in memory, then the sync state
      // of the element is irrelevant.  We clear the sync bit.
      ASSERT(source.is_valid());
      elements_[i].clear_sync();
    }
  }
}


void VirtualFrame::PrepareForCall(int spilled_args, int dropped_args) {
  ASSERT(height() >= dropped_args);
  ASSERT(height() >= spilled_args);
  ASSERT(dropped_args <= spilled_args);

  SyncRange(0, element_count() - 1);
  // Spill registers.
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    if (is_used(i)) {
      SpillElementAt(register_location(i));
    }
  }

  // Spill the arguments.
  for (int i = element_count() - spilled_args; i < element_count(); i++) {
    if (!elements_[i].is_memory()) {
      SpillElementAt(i);
    }
  }

  // Forget the frame elements that will be popped by the call.
  Forget(dropped_args);
}


// If there are any registers referenced only by the frame, spill one.
Register VirtualFrame::SpillAnyRegister() {
  // Find the leftmost (ordered by register number) register whose only
  // reference is in the frame.
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    if (is_used(i) && cgen()->allocator()->count(i) == 1) {
      SpillElementAt(register_location(i));
      ASSERT(!cgen()->allocator()->is_used(i));
      return RegisterAllocator::ToRegister(i);
    }
  }
  return no_reg;
}

} }  // namespace v8::internal
