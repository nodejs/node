// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Maybe_h
#define Maybe_h

#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"

#include <memory>

namespace blink {
namespace protocol {

class String16;

template<typename T>
class Maybe {
public:
    Maybe() : m_value() { }
    Maybe(std::unique_ptr<T> value) : m_value(std::move(value)) { }
    void operator=(std::unique_ptr<T> value) { m_value = std::move(value); }
    T* fromJust() const { DCHECK(m_value); return m_value.get(); }
    T* fromMaybe(T* defaultValue) const { return m_value ? m_value.get() : defaultValue; }
    bool isJust() const { return !!m_value; }
    std::unique_ptr<T> takeJust() { DCHECK(m_value); return m_value.release(); }
private:
    std::unique_ptr<T> m_value;
};

template<typename T>
class MaybeBase {
public:
    MaybeBase() : m_isJust(false) { }
    MaybeBase(T value) : m_isJust(true), m_value(value) { }
    void operator=(T value) { m_value = value; m_isJust = true; }
    T fromJust() const { DCHECK(m_isJust); return m_value; }
    T fromMaybe(const T& defaultValue) const { return m_isJust ? m_value : defaultValue; }
    bool isJust() const { return m_isJust; }
    T takeJust() { DCHECK(m_isJust); return m_value; }

protected:
    bool m_isJust;
    T m_value;
};

template<>
class Maybe<bool> : public MaybeBase<bool> {
public:
    Maybe() { }
    Maybe(bool value) : MaybeBase(value) { }
    using MaybeBase::operator=;
};

template<>
class Maybe<int> : public MaybeBase<int> {
public:
    Maybe() { }
    Maybe(int value) : MaybeBase(value) { }
    using MaybeBase::operator=;
};

template<>
class Maybe<double> : public MaybeBase<double> {
public:
    Maybe() { }
    Maybe(double value) : MaybeBase(value) { }
    using MaybeBase::operator=;
};

template<>
class Maybe<String> : public MaybeBase<String> {
public:
    Maybe() { }
    Maybe(const String& value) : MaybeBase(value) { }
    using MaybeBase::operator=;
};

template<>
class Maybe<String16> : public MaybeBase<String16> {
public:
    Maybe() { }
    Maybe(const String16& value) : MaybeBase(value) { }
    using MaybeBase::operator=;
};

} // namespace platform
} // namespace blink

#endif // !defined(Maybe_h)
