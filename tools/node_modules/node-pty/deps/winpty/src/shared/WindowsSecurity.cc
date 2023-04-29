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

#include "WindowsSecurity.h"

#include <array>

#include "DebugClient.h"
#include "OsModule.h"
#include "OwnedHandle.h"
#include "StringBuilder.h"
#include "WindowsVersion.h"
#include "WinptyAssert.h"
#include "WinptyException.h"

namespace {

struct LocalFreer {
    void operator()(void *ptr) {
        if (ptr != nullptr) {
            LocalFree(reinterpret_cast<HLOCAL>(ptr));
        }
    }
};

typedef std::unique_ptr<void, LocalFreer> PointerLocal;

template <typename T>
SecurityItem<T> localItem(typename T::type v) {
    typedef typename T::type P;
    struct Impl : SecurityItem<T>::Impl {
        P m_v;
        Impl(P v) : m_v(v) {}
        virtual ~Impl() {
            LocalFree(reinterpret_cast<HLOCAL>(m_v));
        }
    };
    return SecurityItem<T>(v, std::unique_ptr<Impl>(new Impl { v }));
}

Sid allocatedSid(PSID v) {
    struct Impl : Sid::Impl {
        PSID m_v;
        Impl(PSID v) : m_v(v) {}
        virtual ~Impl() {
            if (m_v != nullptr) {
                FreeSid(m_v);
            }
        }
    };
    return Sid(v, std::unique_ptr<Impl>(new Impl { v }));
}

} // anonymous namespace

// Returns a handle to the thread's effective security token.  If the thread
// is impersonating another user, its token is returned, and otherwise, the
// process' security token is opened.  The handle is opened with TOKEN_QUERY.
static OwnedHandle openSecurityTokenForQuery() {
    HANDLE token = nullptr;
    // It is unclear to me whether OpenAsSelf matters for winpty, or what the
    // most appropriate value is.
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY,
                         /*OpenAsSelf=*/FALSE, &token)) {
        if (GetLastError() != ERROR_NO_TOKEN) {
            throwWindowsError(L"OpenThreadToken failed");
        }
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            throwWindowsError(L"OpenProcessToken failed");
        }
    }
    ASSERT(token != nullptr &&
        "OpenThreadToken/OpenProcessToken token is NULL");
    return OwnedHandle(token);
}

// Returns the TokenOwner of the thread's effective security token.
Sid getOwnerSid() {
    struct Impl : Sid::Impl {
        std::unique_ptr<char[]> buffer;
    };

    OwnedHandle token = openSecurityTokenForQuery();
    DWORD actual = 0;
    BOOL success;
    success = GetTokenInformation(token.get(), TokenOwner,
        nullptr, 0, &actual);
    if (success) {
        throwWinptyException(L"getOwnerSid: GetTokenInformation: "
            L"expected ERROR_INSUFFICIENT_BUFFER");
    } else if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        throwWindowsError(L"getOwnerSid: GetTokenInformation: "
            L"expected ERROR_INSUFFICIENT_BUFFER");
    }
    std::unique_ptr<Impl> impl(new Impl);
    impl->buffer = std::unique_ptr<char[]>(new char[actual]);
    success = GetTokenInformation(token.get(), TokenOwner,
                                  impl->buffer.get(), actual, &actual);
    if (!success) {
        throwWindowsError(L"getOwnerSid: GetTokenInformation");
    }
    TOKEN_OWNER tmp;
    ASSERT(actual >= sizeof(tmp));
    std::copy(
        impl->buffer.get(),
        impl->buffer.get() + sizeof(tmp),
        reinterpret_cast<char*>(&tmp));
    return Sid(tmp.Owner, std::move(impl));
}

Sid wellKnownSid(
        const wchar_t *debuggingName,
        SID_IDENTIFIER_AUTHORITY authority,
        BYTE authorityCount,
        DWORD subAuthority0/*=0*/,
        DWORD subAuthority1/*=0*/) {
    PSID psid = nullptr;
    if (!AllocateAndInitializeSid(&authority, authorityCount,
            subAuthority0,
            subAuthority1,
            0, 0, 0, 0, 0, 0,
            &psid)) {
        const auto err = GetLastError();
        const auto msg =
            std::wstring(L"wellKnownSid: error getting ") +
            debuggingName + L" SID";
        throwWindowsError(msg.c_str(), err);
    }
    return allocatedSid(psid);
}

Sid builtinAdminsSid() {
    // S-1-5-32-544
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    return wellKnownSid(L"BUILTIN\\Administrators group",
            authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,    // 32
            DOMAIN_ALIAS_RID_ADMINS);       // 544
}

Sid localSystemSid() {
    // S-1-5-18
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    return wellKnownSid(L"LocalSystem account",
            authority, 1,
            SECURITY_LOCAL_SYSTEM_RID);     // 18
}

Sid everyoneSid() {
    // S-1-1-0
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_WORLD_SID_AUTHORITY };
    return wellKnownSid(L"Everyone account",
            authority, 1,
            SECURITY_WORLD_RID);            // 0
}

static SecurityDescriptor finishSecurityDescriptor(
        size_t daclEntryCount,
        EXPLICIT_ACCESSW *daclEntries,
        Acl &outAcl) {
    {
        PACL aclRaw = nullptr;
        DWORD aclError =
            SetEntriesInAclW(daclEntryCount,
                             daclEntries,
                             nullptr, &aclRaw);
        if (aclError != ERROR_SUCCESS) {
            WStringBuilder sb(64);
            sb << L"finishSecurityDescriptor: "
               << L"SetEntriesInAcl failed: " << aclError;
            throwWinptyException(sb.c_str());
        }
        outAcl = localItem<AclTag>(aclRaw);
    }

    const PSECURITY_DESCRIPTOR sdRaw =
        reinterpret_cast<PSECURITY_DESCRIPTOR>(
            LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH));
    if (sdRaw == nullptr) {
        throwWinptyException(L"finishSecurityDescriptor: LocalAlloc failed");
    }
    SecurityDescriptor sd = localItem<SecurityDescriptorTag>(sdRaw);
    if (!InitializeSecurityDescriptor(sdRaw, SECURITY_DESCRIPTOR_REVISION)) {
        throwWindowsError(
            L"finishSecurityDescriptor: InitializeSecurityDescriptor");
    }
    if (!SetSecurityDescriptorDacl(sdRaw, TRUE, outAcl.get(), FALSE)) {
        throwWindowsError(
            L"finishSecurityDescriptor: SetSecurityDescriptorDacl");
    }

    return std::move(sd);
}

// Create a security descriptor that grants full control to the local system
// account, built-in administrators, and the owner.
SecurityDescriptor
createPipeSecurityDescriptorOwnerFullControl() {

    struct Impl : SecurityDescriptor::Impl {
        Sid localSystem;
        Sid builtinAdmins;
        Sid owner;
        std::array<EXPLICIT_ACCESSW, 3> daclEntries = {};
        Acl dacl;
        SecurityDescriptor value;
    };

    std::unique_ptr<Impl> impl(new Impl);
    impl->localSystem = localSystemSid();
    impl->builtinAdmins = builtinAdminsSid();
    impl->owner = getOwnerSid();

    for (auto &ea : impl->daclEntries) {
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    }
    impl->daclEntries[0].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->localSystem.get());
    impl->daclEntries[1].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->builtinAdmins.get());
    impl->daclEntries[2].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->owner.get());

    impl->value = finishSecurityDescriptor(
        impl->daclEntries.size(),
        impl->daclEntries.data(),
        impl->dacl);

    const auto retValue = impl->value.get();
    return SecurityDescriptor(retValue, std::move(impl));
}

SecurityDescriptor
createPipeSecurityDescriptorOwnerFullControlEveryoneWrite() {

    struct Impl : SecurityDescriptor::Impl {
        Sid localSystem;
        Sid builtinAdmins;
        Sid owner;
        Sid everyone;
        std::array<EXPLICIT_ACCESSW, 4> daclEntries = {};
        Acl dacl;
        SecurityDescriptor value;
    };

    std::unique_ptr<Impl> impl(new Impl);
    impl->localSystem = localSystemSid();
    impl->builtinAdmins = builtinAdminsSid();
    impl->owner = getOwnerSid();
    impl->everyone = everyoneSid();

    for (auto &ea : impl->daclEntries) {
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    }
    impl->daclEntries[0].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->localSystem.get());
    impl->daclEntries[1].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->builtinAdmins.get());
    impl->daclEntries[2].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->owner.get());
    impl->daclEntries[3].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->everyone.get());
    // Avoid using FILE_GENERIC_WRITE because it includes FILE_APPEND_DATA,
    // which is equal to FILE_CREATE_PIPE_INSTANCE.  Instead, include all the
    // flags that comprise FILE_GENERIC_WRITE, except for the one.
    impl->daclEntries[3].grfAccessPermissions =
        FILE_GENERIC_READ |
        FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA | FILE_WRITE_EA |
        STANDARD_RIGHTS_WRITE | SYNCHRONIZE;

    impl->value = finishSecurityDescriptor(
        impl->daclEntries.size(),
        impl->daclEntries.data(),
        impl->dacl);

    const auto retValue = impl->value.get();
    return SecurityDescriptor(retValue, std::move(impl));
}

SecurityDescriptor getObjectSecurityDescriptor(HANDLE handle) {
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    const DWORD errCode = GetSecurityInfo(handle, SE_KERNEL_OBJECT,
        OWNER_SECURITY_INFORMATION |
            GROUP_SECURITY_INFORMATION |
            DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &dacl, nullptr, &sd);
    if (errCode != ERROR_SUCCESS) {
        throwWindowsError(L"GetSecurityInfo failed");
    }
    return localItem<SecurityDescriptorTag>(sd);
}

// The (SID/SD)<->string conversion APIs are useful for testing/debugging, so
// create convenient accessor functions for them.  They're too slow for
// ordinary use.  The APIs exist in XP and up, but the MinGW headers only
// declare the SID<->string APIs, not the SD APIs.  MinGW also gets the
// prototype wrong for ConvertStringSidToSidW (LPWSTR instead of LPCWSTR) and
// requires WINVER to be defined.  MSVC and MinGW-w64 get everything right, but
// for consistency, use LoadLibrary/GetProcAddress for all four APIs.

typedef BOOL WINAPI ConvertStringSidToSidW_t(
    LPCWSTR StringSid,
    PSID *Sid);

typedef BOOL WINAPI ConvertSidToStringSidW_t(
    PSID Sid,
    LPWSTR *StringSid);

typedef BOOL WINAPI ConvertStringSecurityDescriptorToSecurityDescriptorW_t(
    LPCWSTR StringSecurityDescriptor,
    DWORD StringSDRevision,
    PSECURITY_DESCRIPTOR *SecurityDescriptor,
    PULONG SecurityDescriptorSize);

typedef BOOL WINAPI ConvertSecurityDescriptorToStringSecurityDescriptorW_t(
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    DWORD RequestedStringSDRevision,
    SECURITY_INFORMATION SecurityInformation,
    LPWSTR *StringSecurityDescriptor,
    PULONG StringSecurityDescriptorLen);

#define GET_MODULE_PROC(mod, funcName)                                      \
    const auto p##funcName =                                                \
        reinterpret_cast<funcName##_t*>(                                    \
            mod.proc(#funcName));                                           \
    if (p##funcName == nullptr) {                                           \
        throwWinptyException(                                               \
            L"" L ## #funcName L" API is missing from ADVAPI32.DLL");     \
    }

const DWORD kSDDL_REVISION_1 = 1;

std::wstring sidToString(PSID sid) {
    OsModule advapi32(L"advapi32.dll");
    GET_MODULE_PROC(advapi32, ConvertSidToStringSidW);
    wchar_t *sidString = NULL;
    BOOL success = pConvertSidToStringSidW(sid, &sidString);
    if (!success) {
        throwWindowsError(L"ConvertSidToStringSidW failed");
    }
    PointerLocal freer(sidString);
    return std::wstring(sidString);
}

Sid stringToSid(const std::wstring &str) {
    // Cast the string from const wchar_t* to LPWSTR because the function is
    // incorrectly prototyped in the MinGW sddl.h header.  The API does not
    // modify the string -- it is correctly prototyped as taking LPCWSTR in
    // MinGW-w64, MSVC, and MSDN.
    OsModule advapi32(L"advapi32.dll");
    GET_MODULE_PROC(advapi32, ConvertStringSidToSidW);
    PSID psid = nullptr;
    BOOL success = pConvertStringSidToSidW(const_cast<LPWSTR>(str.c_str()),
                                           &psid);
    if (!success) {
        const auto err = GetLastError();
        throwWindowsError(
            (std::wstring(L"ConvertStringSidToSidW failed on \"") +
                str + L'"').c_str(),
            err);
    }
    return localItem<SidTag>(psid);
}

SecurityDescriptor stringToSd(const std::wstring &str) {
    OsModule advapi32(L"advapi32.dll");
    GET_MODULE_PROC(advapi32, ConvertStringSecurityDescriptorToSecurityDescriptorW);
    PSECURITY_DESCRIPTOR desc = nullptr;
    if (!pConvertStringSecurityDescriptorToSecurityDescriptorW(
            str.c_str(), kSDDL_REVISION_1, &desc, nullptr)) {
        const auto err = GetLastError();
        throwWindowsError(
            (std::wstring(L"ConvertStringSecurityDescriptorToSecurityDescriptorW failed on \"") +
                str + L'"').c_str(),
            err);
    }
    return localItem<SecurityDescriptorTag>(desc);
}

std::wstring sdToString(PSECURITY_DESCRIPTOR sd) {
    OsModule advapi32(L"advapi32.dll");
    GET_MODULE_PROC(advapi32, ConvertSecurityDescriptorToStringSecurityDescriptorW);
    wchar_t *sdString = nullptr;
    if (!pConvertSecurityDescriptorToStringSecurityDescriptorW(
            sd,
            kSDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION |
                GROUP_SECURITY_INFORMATION |
                DACL_SECURITY_INFORMATION,
            &sdString,
            nullptr)) {
        throwWindowsError(
            L"ConvertSecurityDescriptorToStringSecurityDescriptor failed");
    }
    PointerLocal freer(sdString);
    return std::wstring(sdString);
}

// Vista added a useful flag to CreateNamedPipe, PIPE_REJECT_REMOTE_CLIENTS,
// that rejects remote connections.  Return this flag on Vista, or return 0
// otherwise.
DWORD rejectRemoteClientsPipeFlag() {
    if (isAtLeastWindowsVista()) {
        // MinGW lacks this flag; MinGW-w64 has it.
        const DWORD kPIPE_REJECT_REMOTE_CLIENTS = 8;
        return kPIPE_REJECT_REMOTE_CLIENTS;
    } else {
        trace("Omitting PIPE_REJECT_REMOTE_CLIENTS on pre-Vista OS");
        return 0;
    }
}

typedef BOOL WINAPI GetNamedPipeClientProcessId_t(
    HANDLE Pipe,
    PULONG ClientProcessId);

std::tuple<GetNamedPipeClientProcessId_Result, DWORD, DWORD>
getNamedPipeClientProcessId(HANDLE serverPipe) {
    OsModule kernel32(L"kernel32.dll");
    const auto pGetNamedPipeClientProcessId =
        reinterpret_cast<GetNamedPipeClientProcessId_t*>(
            kernel32.proc("GetNamedPipeClientProcessId"));
    if (pGetNamedPipeClientProcessId == nullptr) {
        return std::make_tuple(
            GetNamedPipeClientProcessId_Result::UnsupportedOs, 0, 0);
    }
    ULONG pid = 0;
    if (!pGetNamedPipeClientProcessId(serverPipe, &pid)) {
        return std::make_tuple(
            GetNamedPipeClientProcessId_Result::Failure, 0, GetLastError());
    }
    return std::make_tuple(
        GetNamedPipeClientProcessId_Result::Success,
        static_cast<DWORD>(pid),
        0);
}
