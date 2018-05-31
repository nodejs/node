/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlrcpp {

std::string join(std::vector<std::string> strings,
                 const std::string& separator);
std::map<std::string, size_t> toMap(const std::vector<std::string>& keys);
std::string escapeWhitespace(std::string str, bool escapeSpaces);
std::string toHexString(const int t);
std::string arrayToString(const std::vector<std::string>& data);
std::string replaceString(const std::string& s, const std::string& from,
                          const std::string& to);
std::vector<std::string> split(const std::string& s, const std::string& sep,
                               int count);
std::string indent(const std::string& s, const std::string& indentation,
                   bool includingFirst = true);

// Using RAII + a lambda to implement a "finally" replacement.
struct FinalAction {
  FinalAction(std::function<void()> f) : _cleanUp{f} {}
  FinalAction(FinalAction&& other)
      : _cleanUp(std::move(other._cleanUp)), _enabled(other._enabled) {
    other._enabled =
        false;  // Don't trigger the lambda after ownership has moved.
  }
  ~FinalAction() {
    if (_enabled) _cleanUp();
  }

  void disable() { _enabled = false; }

 private:
  std::function<void()> _cleanUp;
  bool _enabled{true};
};

ANTLR4CPP_PUBLIC FinalAction finally(std::function<void()> f);

// Convenience functions to avoid lengthy dynamic_cast() != nullptr checks in
// many places.
template <typename T1, typename T2>
inline bool is(T2* obj) {  // For pointer types.
  return dynamic_cast<typename std::add_const<T1>::type>(obj) != nullptr;
}

template <typename T1, typename T2>
inline bool is(Ref<T2> const& obj) {  // For shared pointers.
  return dynamic_cast<T1*>(obj.get()) != nullptr;
}

template <typename T>
std::string toString(const T& o) {
  std::stringstream ss;
  // typeid gives the mangled class name, but that's all what's possible
  // in a portable way.
  ss << typeid(o).name() << "@" << std::hex << reinterpret_cast<uintptr_t>(&o);
  return ss.str();
}

// Get the error text from an exception pointer or the current exception.
std::string what(std::exception_ptr eptr = std::current_exception());

class SingleWriteMultipleReadLock {
 public:
  void readLock();
  void readUnlock();
  void writeLock();
  void writeUnlock();

 private:
  std::condition_variable _readerGate;
  std::condition_variable _writerGate;

  std::mutex _mutex;
  size_t _activeReaders = 0;
  size_t _waitingWriters = 0;
  size_t _activeWriters = 0;
};

}  // namespace antlrcpp
