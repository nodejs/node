// Copyright 2013 the V8 project authors. All rights reserved.
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

#ifndef V8_EFFECTS_H_
#define V8_EFFECTS_H_

#include "v8.h"

#include "types.h"

namespace v8 {
namespace internal {


// A simple struct to represent (write) effects. A write is represented as a
// modification of type bounds (e.g. of a variable).
//
// An effect can either be definite, if the write is known to have taken place,
// or 'possible', if it was optional. The difference is relevant when composing
// effects.
//
// There are two ways to compose effects: sequentially (they happen one after
// the other) or alternatively (either one or the other happens). A definite
// effect cancels out any previous effect upon sequencing. A possible effect
// merges into a previous effect, i.e., type bounds are merged. Alternative
// composition always merges bounds. It yields a possible effect if at least
// one was only possible.
struct Effect {
  enum Modality { POSSIBLE, DEFINITE };

  Modality modality;
  Bounds bounds;

  Effect() : modality(DEFINITE) {}
  Effect(Bounds b, Modality m = DEFINITE) : modality(m), bounds(b) {}

  // The unknown effect.
  static Effect Unknown(Zone* zone) {
    return Effect(Bounds::Unbounded(zone), POSSIBLE);
  }

  static Effect Forget(Zone* zone) {
    return Effect(Bounds::Unbounded(zone), DEFINITE);
  }

  // Sequential composition, as in 'e1; e2'.
  static Effect Seq(Effect e1, Effect e2, Zone* zone) {
    if (e2.modality == DEFINITE) return e2;
    return Effect(Bounds::Either(e1.bounds, e2.bounds, zone), e1.modality);
  }

  // Alternative composition, as in 'cond ? e1 : e2'.
  static Effect Alt(Effect e1, Effect e2, Zone* zone) {
    return Effect(
        Bounds::Either(e1.bounds, e2.bounds, zone),
        e1.modality == POSSIBLE ? POSSIBLE : e2.modality);
  }
};


// Classes encapsulating sets of effects on variables.
//
// Effects maps variables to effects and supports sequential and alternative
// composition.
//
// NestedEffects is an incremental representation that supports persistence
// through functional extension. It represents the map as an adjoin of a list
// of maps, whose tail can be shared.
//
// Both classes provide similar interfaces, implemented in parts through the
// EffectsMixin below (using sandwich style, to work around the style guide's
// MI restriction).
//
// We also (ab)use Effects/NestedEffects as a representation for abstract
// store typings. In that case, only definite effects are of interest.

template<class Var, class Base, class Effects>
class EffectsMixin: public Base {
 public:
  explicit EffectsMixin(Zone* zone) : Base(zone) {}

  Effect Lookup(Var var) {
    Locator locator;
    return this->Find(var, &locator)
        ? locator.value() : Effect::Unknown(Base::zone());
  }

  Bounds LookupBounds(Var var) {
    Effect effect = Lookup(var);
    return effect.modality == Effect::DEFINITE
        ? effect.bounds : Bounds::Unbounded(Base::zone());
  }

  // Sequential composition.
  void Seq(Var var, Effect effect) {
    Locator locator;
    if (!this->Insert(var, &locator)) {
      effect = Effect::Seq(locator.value(), effect, Base::zone());
    }
    locator.set_value(effect);
  }

  void Seq(Effects that) {
    SeqMerger<EffectsMixin> merge = { *this };
    that.ForEach(&merge);
  }

  // Alternative composition.
  void Alt(Var var, Effect effect) {
    Locator locator;
    if (!this->Insert(var, &locator)) {
      effect = Effect::Alt(locator.value(), effect, Base::zone());
    }
    locator.set_value(effect);
  }

  void Alt(Effects that) {
    AltWeakener<EffectsMixin> weaken = { *this, that };
    this->ForEach(&weaken);
    AltMerger<EffectsMixin> merge = { *this };
    that.ForEach(&merge);
  }

  // Invalidation.
  void Forget() {
    Overrider override = {
        Effect::Forget(Base::zone()), Effects(Base::zone()) };
    this->ForEach(&override);
    Seq(override.effects);
  }

 protected:
  typedef typename Base::Locator Locator;

  template<class Self>
  struct SeqMerger {
    void Call(Var var, Effect effect) { self.Seq(var, effect); }
    Self self;
  };

  template<class Self>
  struct AltMerger {
    void Call(Var var, Effect effect) { self.Alt(var, effect); }
    Self self;
  };

  template<class Self>
  struct AltWeakener {
    void Call(Var var, Effect effect) {
      if (effect.modality == Effect::DEFINITE && !other.Contains(var)) {
        effect.modality = Effect::POSSIBLE;
        Locator locator;
        self.Insert(var, &locator);
        locator.set_value(effect);
      }
    }
    Self self;
    Effects other;
  };

  struct Overrider {
    void Call(Var var, Effect effect) { effects.Seq(var, new_effect); }
    Effect new_effect;
    Effects effects;
  };
};


template<class Var, Var kNoVar> class Effects;
template<class Var, Var kNoVar> class NestedEffectsBase;

template<class Var, Var kNoVar>
class EffectsBase {
 public:
  explicit EffectsBase(Zone* zone) : map_(new(zone) Mapping(zone)) {}

  bool IsEmpty() { return map_->is_empty(); }

 protected:
  friend class NestedEffectsBase<Var, kNoVar>;
  friend class
      EffectsMixin<Var, NestedEffectsBase<Var, kNoVar>, Effects<Var, kNoVar> >;

  Zone* zone() { return map_->allocator().zone(); }

  struct SplayTreeConfig {
    typedef Var Key;
    typedef Effect Value;
    static const Var kNoKey = kNoVar;
    static Effect NoValue() { return Effect(); }
    static int Compare(int x, int y) { return y - x; }
  };
  typedef ZoneSplayTree<SplayTreeConfig> Mapping;
  typedef typename Mapping::Locator Locator;

  bool Contains(Var var) {
    ASSERT(var != kNoVar);
    return map_->Contains(var);
  }
  bool Find(Var var, Locator* locator) {
    ASSERT(var != kNoVar);
    return map_->Find(var, locator);
  }
  bool Insert(Var var, Locator* locator) {
    ASSERT(var != kNoVar);
    return map_->Insert(var, locator);
  }

  template<class Callback>
  void ForEach(Callback* callback) {
    return map_->ForEach(callback);
  }

 private:
  Mapping* map_;
};

template<class Var, Var kNoVar>
const Var EffectsBase<Var, kNoVar>::SplayTreeConfig::kNoKey;

template<class Var, Var kNoVar>
class Effects: public
    EffectsMixin<Var, EffectsBase<Var, kNoVar>, Effects<Var, kNoVar> > {
 public:
  explicit Effects(Zone* zone)
      : EffectsMixin<Var, EffectsBase<Var, kNoVar>, Effects<Var, kNoVar> >(zone)
          {}
};


template<class Var, Var kNoVar>
class NestedEffectsBase {
 public:
  explicit NestedEffectsBase(Zone* zone) : node_(new(zone) Node(zone)) {}

  template<class Callback>
  void ForEach(Callback* callback) {
    if (node_->previous) NestedEffectsBase(node_->previous).ForEach(callback);
    node_->effects.ForEach(callback);
  }

  Effects<Var, kNoVar> Top() { return node_->effects; }

  bool IsEmpty() {
    for (Node* node = node_; node != NULL; node = node->previous) {
      if (!node->effects.IsEmpty()) return false;
    }
    return true;
  }

 protected:
  typedef typename EffectsBase<Var, kNoVar>::Locator Locator;

  Zone* zone() { return node_->zone; }

  void push() { node_ = new(node_->zone) Node(node_->zone, node_); }
  void pop() { node_ = node_->previous; }
  bool is_empty() { return node_ == NULL; }

  bool Contains(Var var) {
    ASSERT(var != kNoVar);
    for (Node* node = node_; node != NULL; node = node->previous) {
      if (node->effects.Contains(var)) return true;
    }
    return false;
  }

  bool Find(Var var, Locator* locator) {
    ASSERT(var != kNoVar);
    for (Node* node = node_; node != NULL; node = node->previous) {
      if (node->effects.Find(var, locator)) return true;
    }
    return false;
  }

  bool Insert(Var var, Locator* locator);

 private:
  struct Node: ZoneObject {
    Zone* zone;
    Effects<Var, kNoVar> effects;
    Node* previous;
    explicit Node(Zone* zone, Node* previous = NULL)
        : zone(zone), effects(zone), previous(previous) {}
  };

  explicit NestedEffectsBase(Node* node) : node_(node) {}

  Node* node_;
};


template<class Var, Var kNoVar>
bool NestedEffectsBase<Var, kNoVar>::Insert(Var var, Locator* locator) {
  ASSERT(var != kNoVar);
  if (!node_->effects.Insert(var, locator)) return false;
  Locator shadowed;
  for (Node* node = node_->previous; node != NULL; node = node->previous) {
    if (node->effects.Find(var, &shadowed)) {
      // Initialize with shadowed entry.
      locator->set_value(shadowed.value());
      return false;
    }
  }
  return true;
}


template<class Var, Var kNoVar>
class NestedEffects: public
    EffectsMixin<Var, NestedEffectsBase<Var, kNoVar>, Effects<Var, kNoVar> > {
 public:
  explicit NestedEffects(Zone* zone) :
      EffectsMixin<Var, NestedEffectsBase<Var, kNoVar>, Effects<Var, kNoVar> >(
          zone) {}

  // Create an extension of the current effect set. The current set should not
  // be modified while the extension is in use.
  NestedEffects Push() {
    NestedEffects result = *this;
    result.push();
    return result;
  }

  NestedEffects Pop() {
    NestedEffects result = *this;
    result.pop();
    ASSERT(!this->is_empty());
    return result;
  }
};

} }  // namespace v8::internal

#endif  // V8_EFFECTS_H_
