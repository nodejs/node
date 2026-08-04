// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "src/zone/zone-containers.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

template <class T>
class LiveSet {
 public:
  void Add(const T* new_entry) {
    CHECK(!Contains(new_entry));
    set_.insert(new_entry);
  }

  void Remove(const T* old_entry) {
    CHECK(Contains(old_entry));
    set_.erase(old_entry);
  }

  void CheckContainsAll(ZoneVector<T>& vector) {
    CHECK_EQ(vector.size(), set_.size());
    for (const T* m = vector.begin(); m != vector.end(); m++) {
      CHECK(Contains(m));
    }
  }

  void CheckEmpty() { CHECK_EQ(0, set_.size()); }

 private:
  bool Contains(const T* entry) {
    // std::set::contains is a C++20 extension.
    return set_.find(entry) != set_.end();
  }

  std::set<const T*> set_;
};

template <typename T>
LiveSet<T>& live_set() {
  static LiveSet<T> static_live_set;
  return static_live_set;
}

class Trivial {
 public:
  Trivial() : id_(0) {}
  explicit Trivial(int id) : id_(id) {}

  int id() const { return id_; }

 private:
  int id_;
};

static_assert(std::is_trivially_copyable_v<Trivial>);

template <>
class LiveSet<Trivial> {
 public:
  void Add(const Trivial* new_entry) { UNREACHABLE(); }
  void Remove(const Trivial* old_entry) { UNREACHABLE(); }
  void CheckContainsAll(ZoneVector<Trivial>&) {}
  void CheckEmpty() {}
};

class CopyAssignable {
 public:
  CopyAssignable() : id_(0) { live_set<CopyAssignable>().Add(this); }
  explicit CopyAssignable(int id) : id_(id) {
    live_set<CopyAssignable>().Add(this);
  }
  CopyAssignable(const CopyAssignable& other) V8_NOEXCEPT : id_(other.id_) {
    live_set<CopyAssignable>().Add(this);
  }
  ~CopyAssignable() { live_set<CopyAssignable>().Remove(this); }
  CopyAssignable& operator=(const CopyAssignable& other) V8_NOEXCEPT = default;

  CopyAssignable(CopyAssignable&& other) = delete;
  CopyAssignable& operator=(CopyAssignable&& other) = delete;

  int id() const { return id_; }

 private:
  int id_;
};

static_assert(!std::is_trivially_copyable_v<CopyAssignable>);
static_assert(std::is_copy_assignable_v<CopyAssignable>);
static_assert(!std::is_move_assignable_v<CopyAssignable>);

class MoveAssignable {
 public:
  MoveAssignable() : id_(0) { live_set<MoveAssignable>().Add(this); }
  explicit MoveAssignable(int id) : id_(id) {
    live_set<MoveAssignable>().Add(this);
  }
  MoveAssignable(const MoveAssignable& other) V8_NOEXCEPT : id_(other.id_) {
    live_set<MoveAssignable>().Add(this);
  }
  MoveAssignable(MoveAssignable&& other) V8_NOEXCEPT : id_(other.id_) {
    live_set<MoveAssignable>().Add(this);
  }
  MoveAssignable& operator=(const MoveAssignable& other) = delete;
  MoveAssignable& operator=(MoveAssignable&& other) V8_NOEXCEPT {
    id_ = other.id_;
    return *this;
  }
  ~MoveAssignable() { live_set<MoveAssignable>().Remove(this); }

  int id() const { return id_; }

 private:
  int id_;
};

static_assert(!std::is_trivially_copyable_v<MoveAssignable>);
static_assert(std::is_move_assignable_v<MoveAssignable>);
static_assert(!std::is_copy_assignable_v<MoveAssignable>);

class NotAssignable {
 public:
  NotAssignable() : id_(0) { live_set<NotAssignable>().Add(this); }
  explicit NotAssignable(int id) : id_(id) {
    live_set<NotAssignable>().Add(this);
  }
  NotAssignable(const NotAssignable& other) V8_NOEXCEPT : id_(other.id_) {
    live_set<NotAssignable>().Add(this);
  }
  NotAssignable& operator=(const NotAssignable& other) = delete;
  ~NotAssignable() { live_set<NotAssignable>().Remove(this); }

  NotAssignable(NotAssignable&& other) = delete;
  NotAssignable& operator=(NotAssignable&& other) = delete;

  int id() const { return id_; }

 private:
  int id_;
};

static_assert(!std::is_trivially_copyable_v<NotAssignable>);
static_assert(!std::is_copy_assignable_v<NotAssignable>);
static_assert(!std::is_move_assignable_v<NotAssignable>);

class ZoneVectorTest : public TestWithZone {
 public:
  template <class T>
  void CheckConsistency(ZoneVector<T>& vector, std::initializer_list<int> ids) {
    live_set<T>().CheckContainsAll(vector);
    CHECK_EQ(vector.size(), ids.size());
    auto it = ids.begin();
    for (size_t i = 0; i < ids.size(); i++) {
      CHECK_EQ(*it++, vector[i].id());
    }
  }

  template <class T>
  void Basic() {
    {
      // Constructor with definition.
      ZoneVector<T> v(1, T(1), zone());
      CheckConsistency(v, {1});
    }
    live_set<T>().CheckEmpty();

    {
      // Constructor with initializer list.
      ZoneVector<T> v({T(1), T(2), T(3)}, zone());
      CheckConsistency(v, {1, 2, 3});
    }
    live_set<T>().CheckEmpty();

    {
      std::optional<ZoneVector<T>> v1;
      v1.emplace({T(1), T(2), T(3)}, zone());
      CheckConsistency(v1.value(), {1, 2, 3});
      {
        // Copy assignment with growth.
        ZoneVector<T> v2 = v1.value();
        v1.reset();
        CheckConsistency(v2, {1, 2, 3});
      }
      v1.emplace({T(1), T(2), T(3)}, zone());
      CheckConsistency(v1.value(), {1, 2, 3});

      // Copy assignment without growth.
      ZoneVector<T> v3({T(4), T(5), T(6)}, zone());
      v3 = v1.value();
      v1.reset();
      CheckConsistency(v3, {1, 2, 3});

      // Move assignment.
      {
        ZoneVector<T> v4(std::move(v3));
        CheckConsistency(v4, {1, 2, 3});
      }
      CheckConsistency(v3, {});
    }
    live_set<T>().CheckEmpty();
  }

  template <class T>
  void Assign() {
    {
      // Assign with sufficient capacity.
      ZoneVector<T> v({T(1), T(2), T(3)}, zone());
      v.assign(2, T(4));
      CheckConsistency(v, {4, 4});
      // This time, capacity > size.
      v.assign(3, T(5));
      CheckConsistency(v, {5, 5, 5});
    }

    {
      // Assign with capacity growth.
      ZoneVector<T> v({T(1)}, zone());
      v.assign(2, T(4));
      CheckConsistency(v, {4, 4});
    }

    live_set<T>().CheckEmpty();
  }

  template <class T>
  void Insert() {
    // Check that we can insert (by iterator) in the right positions.
    {
      ZoneVector<T> v({T(2), T(4)}, zone());
      {
        T src1[] = {T(1)};
        T src3[] = {T(3)};
        T src5[] = {T(5)};
        v.insert(&v.at(0), src1, std::end(src1));
        v.insert(&v.at(2), src3, std::end(src3));
        v.insert(v.end(), src5, std::end(src5));
      }
      CheckConsistency(v, {1, 2, 3, 4, 5});
    }

    // Check that we can insert (by count) in the right positions.
    {
      ZoneVector<T> v({T(2), T(4)}, zone());
      v.insert(&v.at(0), 1, T(1));
      v.insert(&v.at(2), 1, T(3));
      v.insert(v.end(), 1, T(5));
      CheckConsistency(v, {1, 2, 3, 4, 5});
    }

    // Test the "insufficient capacity" case in PrepareForInsertion.
    {
      ZoneVector<T> v(zone());
      CHECK_EQ(0, v.capacity());
      v.insert(v.begin(), 1, T(5));
      CheckConsistency(v, {5});
      {
        T src[] = {T(1), T(2), T(3), T(4)};
        v.insert(v.begin(), src, std::end(src));
      }
      CheckConsistency(v, {1, 2, 3, 4, 5});
    }

    // Test "case 1" of sufficient capacity in PrepareForInsertion.
    {
      ZoneVector<T> v({T(1), T(2), T(3), T(4), T(5)}, zone());
      v.reserve(10);
      CHECK_EQ(10, v.capacity());
      CheckConsistency(v, {1, 2, 3, 4, 5});
      {
        T src[] = {T(11), T(12), T(13), T(14)};
        v.insert(&v.at(3), src, std::end(src));
      }
      CheckConsistency(v, {1, 2, 3, 11, 12, 13, 14, 4, 5});
    }

    // Test "case 2" of sufficient capacity in PrepareForInsertion.
    {
      ZoneVector<T> v({T(1), T(2), T(3), T(4), T(5)}, zone());
      v.reserve(10);
      {
        T src[] = {T(11), T(12)};
        v.insert(&v.at(2), src, std::end(src));
      }
      CheckConsistency(v, {1, 2, 11, 12, 3, 4, 5});
    }
    live_set<T>().CheckEmpty();

    // For good measure, test the edge case where we're inserting exactly
    // as many elements as we're moving.
    {
      ZoneVector<T> v({T(1), T(2), T(3), T(4)}, zone());
      v.reserve(10);
      {
        T src[] = {T(11), T(12)};
        v.insert(&v.at(2), src, std::end(src));
      }
    }
  }

  template <class T>
  void Erase() {
    // Erase one element.
    {
      ZoneVector<T> v({T(1), T(2), T(3)}, zone());
      v.erase(&v.at(1));
      CheckConsistency(v, {1, 3});
    }
    // Erase a range.
    {
      ZoneVector<T> v({T(1), T(2), T(3), T(4)}, zone());
      v.erase(&v.at(1), &v.at(3));
      CheckConsistency(v, {1, 4});
    }
    // Erase first element.
    {
      ZoneVector<T> v({T(1), T(2), T(3)}, zone());
      v.erase(v.begin());
      CheckConsistency(v, {2, 3});
    }
    // Erase last element.
    {
      ZoneVector<T> v({T(1), T(2), T(3)}, zone());
      v.erase(&v.at(2));
      CheckConsistency(v, {1, 2});
    }
    // Erase nothing (empty range).
    {
      ZoneVector<T> v({T(1), T(2), T(3)}, zone());
      v.erase(v.begin(), v.begin());
      CheckConsistency(v, {1, 2, 3});
      v.erase(&v.at(1), &v.at(1));
      CheckConsistency(v, {1, 2, 3});
      v.erase(v.end(), v.end());
      CheckConsistency(v, {1, 2, 3});
    }
    live_set<T>().CheckEmpty();
  }
};

TEST_F(ZoneVectorTest, Basic) {
  Basic<Trivial>();
  Basic<CopyAssignable>();
  Basic<MoveAssignable>();
  Basic<NotAssignable>();
}

TEST_F(ZoneVectorTest, Assign) {
  Assign<Trivial>();
  Assign<CopyAssignable>();
  Assign<MoveAssignable>();
  Assign<NotAssignable>();
}

TEST_F(ZoneVectorTest, Insert) {
  Insert<Trivial>();
  Insert<CopyAssignable>();
  Insert<MoveAssignable>();
  Insert<NotAssignable>();
}

TEST_F(ZoneVectorTest, Erase) {
  Erase<Trivial>();
  Erase<CopyAssignable>();
  Erase<MoveAssignable>();
  Erase<NotAssignable>();
}

}  // namespace v8::internal
