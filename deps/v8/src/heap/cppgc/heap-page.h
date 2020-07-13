// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_PAGE_H_
#define V8_HEAP_CPPGC_HEAP_PAGE_H_

#include "src/base/iterator.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/object-start-bitmap.h"

namespace cppgc {
namespace internal {

class BaseSpace;
class NormalPageSpace;
class LargePageSpace;
class Heap;
class PageBackend;

class V8_EXPORT_PRIVATE BasePage {
 public:
  static BasePage* FromPayload(void*);
  static const BasePage* FromPayload(const void*);

  BasePage(const BasePage&) = delete;
  BasePage& operator=(const BasePage&) = delete;

  Heap* heap() { return heap_; }
  const Heap* heap() const { return heap_; }

  BaseSpace* space() { return space_; }
  const BaseSpace* space() const { return space_; }
  void set_space(BaseSpace* space) { space_ = space; }

  bool is_large() const { return type_ == PageType::kLarge; }

  // |address| must refer to real object.
  HeapObjectHeader* ObjectHeaderFromInnerAddress(void* address);
  const HeapObjectHeader* ObjectHeaderFromInnerAddress(const void* address);

 protected:
  enum class PageType { kNormal, kLarge };
  BasePage(Heap*, BaseSpace*, PageType);

 private:
  Heap* heap_;
  BaseSpace* space_;
  PageType type_;
};

class V8_EXPORT_PRIVATE NormalPage final : public BasePage {
  template <typename T>
  class IteratorImpl : v8::base::iterator<std::forward_iterator_tag, T> {
   public:
    explicit IteratorImpl(T* p, ConstAddress lab_start = nullptr,
                          size_t lab_size = 0)
        : p_(p), lab_start_(lab_start), lab_size_(lab_size) {
      DCHECK(p);
      DCHECK_EQ(0, (lab_size & (sizeof(T) - 1)));
      if (reinterpret_cast<ConstAddress>(p_) == lab_start_) {
        p_ += (lab_size_ / sizeof(T));
      }
    }

    T& operator*() { return *p_; }
    const T& operator*() const { return *p_; }

    bool operator==(IteratorImpl other) const { return p_ == other.p_; }
    bool operator!=(IteratorImpl other) const { return !(*this == other); }

    IteratorImpl& operator++() {
      const size_t size = p_->GetSize();
      DCHECK_EQ(0, (size & (sizeof(T) - 1)));
      p_ += (size / sizeof(T));
      if (reinterpret_cast<ConstAddress>(p_) == lab_start_) {
        p_ += (lab_size_ / sizeof(T));
      }
      return *this;
    }
    IteratorImpl operator++(int) {
      IteratorImpl temp(*this);
      ++(*this);
      return temp;
    }

    T* base() const { return p_; }

   private:
    T* p_;
    ConstAddress lab_start_;
    size_t lab_size_;
  };

 public:
  using iterator = IteratorImpl<HeapObjectHeader>;
  using const_iterator = IteratorImpl<const HeapObjectHeader>;

  // Allocates a new page.
  static NormalPage* Create(NormalPageSpace*);
  // Destroys and frees the page. The page must be detached from the
  // corresponding space (i.e. be swept when called).
  static void Destroy(NormalPage*);

  static NormalPage* From(BasePage* page) {
    DCHECK(!page->is_large());
    return static_cast<NormalPage*>(page);
  }
  static const NormalPage* From(const BasePage* page) {
    return From(const_cast<BasePage*>(page));
  }

  iterator begin();
  const_iterator begin() const;

  iterator end() {
    return iterator(reinterpret_cast<HeapObjectHeader*>(PayloadEnd()));
  }
  const_iterator end() const {
    return const_iterator(
        reinterpret_cast<const HeapObjectHeader*>(PayloadEnd()));
  }

  Address PayloadStart();
  ConstAddress PayloadStart() const;
  Address PayloadEnd();
  ConstAddress PayloadEnd() const;

  static size_t PayloadSize();

  ObjectStartBitmap& object_start_bitmap() { return object_start_bitmap_; }
  const ObjectStartBitmap& object_start_bitmap() const {
    return object_start_bitmap_;
  }

 private:
  NormalPage(Heap* heap, BaseSpace* space);
  ~NormalPage();

  ObjectStartBitmap object_start_bitmap_;
};

class V8_EXPORT_PRIVATE LargePage final : public BasePage {
 public:
  // Allocates a new page.
  static LargePage* Create(LargePageSpace*, size_t);
  // Destroys and frees the page. The page must be detached from the
  // corresponding space (i.e. be swept when called).
  static void Destroy(LargePage*);

  static LargePage* From(BasePage* page) {
    DCHECK(page->is_large());
    return static_cast<LargePage*>(page);
  }
  static const LargePage* From(const BasePage* page) {
    return From(const_cast<BasePage*>(page));
  }

  HeapObjectHeader* ObjectHeader();
  const HeapObjectHeader* ObjectHeader() const;

  Address PayloadStart();
  ConstAddress PayloadStart() const;
  Address PayloadEnd();
  ConstAddress PayloadEnd() const;

  size_t PayloadSize() const { return payload_size_; }

 private:
  LargePage(Heap* heap, BaseSpace* space, size_t);
  ~LargePage();

  size_t payload_size_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_PAGE_H_
