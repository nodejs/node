// Copyright (c) 2016 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef WINPTY_WINDOWS_SECURITY_H
#define WINPTY_WINDOWS_SECURITY_H

#include <windows.h>
#include <aclapi.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

// PSID and PSECURITY_DESCRIPTOR are both pointers to void, but we want
// Sid and SecurityDescriptor to be different types.
struct SidTag                   {   typedef PSID type;                  };
struct AclTag                   {   typedef PACL type;                  };
struct SecurityDescriptorTag    {   typedef PSECURITY_DESCRIPTOR type;  };

template <typename T>
class SecurityItem {
public:
    struct Impl {
        virtual ~Impl() {}
    };

private:
    typedef typename T::type P;
    P m_v;
    std::unique_ptr<Impl> m_pimpl;

public:
    P get() const { return m_v; }
    operator bool() const { return m_v != nullptr; }

    SecurityItem() : m_v(nullptr) {}
    SecurityItem(P v, std::unique_ptr<Impl> &&pimpl) :
            m_v(v), m_pimpl(std::move(pimpl)) {}
    SecurityItem(SecurityItem &&other) :
            m_v(other.m_v), m_pimpl(std::move(other.m_pimpl)) {
        other.m_v = nullptr;
    }
    SecurityItem &operator=(SecurityItem &&other) {
        m_v = other.m_v;
        other.m_v = nullptr;
        m_pimpl = std::move(other.m_pimpl);
        return *this;
    }
};

typedef SecurityItem<SidTag> Sid;
typedef SecurityItem<AclTag> Acl;
typedef SecurityItem<SecurityDescriptorTag> SecurityDescriptor;

Sid getOwnerSid();
Sid wellKnownSid(
    const wchar_t *debuggingName,
    SID_IDENTIFIER_AUTHORITY authority,
    BYTE authorityCount,
    DWORD subAuthority0=0,
    DWORD subAuthority1=0);
Sid builtinAdminsSid();
Sid localSystemSid();
Sid everyoneSid();

SecurityDescriptor createPipeSecurityDescriptorOwnerFullControl();
SecurityDescriptor createPipeSecurityDescriptorOwnerFullControlEveryoneWrite();
SecurityDescriptor getObjectSecurityDescriptor(HANDLE handle);

std::wstring sidToString(PSID sid);
Sid stringToSid(const std::wstring &str);
SecurityDescriptor stringToSd(const std::wstring &str);
std::wstring sdToString(PSECURITY_DESCRIPTOR sd);

DWORD rejectRemoteClientsPipeFlag();

enum class GetNamedPipeClientProcessId_Result {
    Success,
    Failure,
    UnsupportedOs,
};

std::tuple<GetNamedPipeClientProcessId_Result, DWORD, DWORD>
getNamedPipeClientProcessId(HANDLE serverPipe);

#endif // WINPTY_WINDOWS_SECURITY_H
