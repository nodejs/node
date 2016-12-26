// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_constants.h"
#include "env.h"
#include "env-inl.h"

#include "uv.h"
#include "zlib.h"

#include <errno.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits>

#if HAVE_OPENSSL
# include <openssl/ec.h>
# include <openssl/ssl.h>
# ifndef OPENSSL_NO_ENGINE
#  include <openssl/engine.h>
# endif  // !OPENSSL_NO_ENGINE
#endif

namespace node {

using v8::Local;
using v8::Object;

#if HAVE_OPENSSL
const char* default_cipher_list = DEFAULT_CIPHER_LIST_CORE;
#endif

void DefineErrnoConstants(Local<Object> target) {
#ifdef E2BIG
  NODE_DEFINE_CONSTANT(target, E2BIG);
#endif

#ifdef EACCES
  NODE_DEFINE_CONSTANT(target, EACCES);
#endif

#ifdef EADDRINUSE
  NODE_DEFINE_CONSTANT(target, EADDRINUSE);
#endif

#ifdef EADDRNOTAVAIL
  NODE_DEFINE_CONSTANT(target, EADDRNOTAVAIL);
#endif

#ifdef EAFNOSUPPORT
  NODE_DEFINE_CONSTANT(target, EAFNOSUPPORT);
#endif

#ifdef EAGAIN
  NODE_DEFINE_CONSTANT(target, EAGAIN);
#endif

#ifdef EALREADY
  NODE_DEFINE_CONSTANT(target, EALREADY);
#endif

#ifdef EBADF
  NODE_DEFINE_CONSTANT(target, EBADF);
#endif

#ifdef EBADMSG
  NODE_DEFINE_CONSTANT(target, EBADMSG);
#endif

#ifdef EBUSY
  NODE_DEFINE_CONSTANT(target, EBUSY);
#endif

#ifdef ECANCELED
  NODE_DEFINE_CONSTANT(target, ECANCELED);
#endif

#ifdef ECHILD
  NODE_DEFINE_CONSTANT(target, ECHILD);
#endif

#ifdef ECONNABORTED
  NODE_DEFINE_CONSTANT(target, ECONNABORTED);
#endif

#ifdef ECONNREFUSED
  NODE_DEFINE_CONSTANT(target, ECONNREFUSED);
#endif

#ifdef ECONNRESET
  NODE_DEFINE_CONSTANT(target, ECONNRESET);
#endif

#ifdef EDEADLK
  NODE_DEFINE_CONSTANT(target, EDEADLK);
#endif

#ifdef EDESTADDRREQ
  NODE_DEFINE_CONSTANT(target, EDESTADDRREQ);
#endif

#ifdef EDOM
  NODE_DEFINE_CONSTANT(target, EDOM);
#endif

#ifdef EDQUOT
  NODE_DEFINE_CONSTANT(target, EDQUOT);
#endif

#ifdef EEXIST
  NODE_DEFINE_CONSTANT(target, EEXIST);
#endif

#ifdef EFAULT
  NODE_DEFINE_CONSTANT(target, EFAULT);
#endif

#ifdef EFBIG
  NODE_DEFINE_CONSTANT(target, EFBIG);
#endif

#ifdef EHOSTUNREACH
  NODE_DEFINE_CONSTANT(target, EHOSTUNREACH);
#endif

#ifdef EIDRM
  NODE_DEFINE_CONSTANT(target, EIDRM);
#endif

#ifdef EILSEQ
  NODE_DEFINE_CONSTANT(target, EILSEQ);
#endif

#ifdef EINPROGRESS
  NODE_DEFINE_CONSTANT(target, EINPROGRESS);
#endif

#ifdef EINTR
  NODE_DEFINE_CONSTANT(target, EINTR);
#endif

#ifdef EINVAL
  NODE_DEFINE_CONSTANT(target, EINVAL);
#endif

#ifdef EIO
  NODE_DEFINE_CONSTANT(target, EIO);
#endif

#ifdef EISCONN
  NODE_DEFINE_CONSTANT(target, EISCONN);
#endif

#ifdef EISDIR
  NODE_DEFINE_CONSTANT(target, EISDIR);
#endif

#ifdef ELOOP
  NODE_DEFINE_CONSTANT(target, ELOOP);
#endif

#ifdef EMFILE
  NODE_DEFINE_CONSTANT(target, EMFILE);
#endif

#ifdef EMLINK
  NODE_DEFINE_CONSTANT(target, EMLINK);
#endif

#ifdef EMSGSIZE
  NODE_DEFINE_CONSTANT(target, EMSGSIZE);
#endif

#ifdef EMULTIHOP
  NODE_DEFINE_CONSTANT(target, EMULTIHOP);
#endif

#ifdef ENAMETOOLONG
  NODE_DEFINE_CONSTANT(target, ENAMETOOLONG);
#endif

#ifdef ENETDOWN
  NODE_DEFINE_CONSTANT(target, ENETDOWN);
#endif

#ifdef ENETRESET
  NODE_DEFINE_CONSTANT(target, ENETRESET);
#endif

#ifdef ENETUNREACH
  NODE_DEFINE_CONSTANT(target, ENETUNREACH);
#endif

#ifdef ENFILE
  NODE_DEFINE_CONSTANT(target, ENFILE);
#endif

#ifdef ENOBUFS
  NODE_DEFINE_CONSTANT(target, ENOBUFS);
#endif

#ifdef ENODATA
  NODE_DEFINE_CONSTANT(target, ENODATA);
#endif

#ifdef ENODEV
  NODE_DEFINE_CONSTANT(target, ENODEV);
#endif

#ifdef ENOENT
  NODE_DEFINE_CONSTANT(target, ENOENT);
#endif

#ifdef ENOEXEC
  NODE_DEFINE_CONSTANT(target, ENOEXEC);
#endif

#ifdef ENOLCK
  NODE_DEFINE_CONSTANT(target, ENOLCK);
#endif

#ifdef ENOLINK
  NODE_DEFINE_CONSTANT(target, ENOLINK);
#endif

#ifdef ENOMEM
  NODE_DEFINE_CONSTANT(target, ENOMEM);
#endif

#ifdef ENOMSG
  NODE_DEFINE_CONSTANT(target, ENOMSG);
#endif

#ifdef ENOPROTOOPT
  NODE_DEFINE_CONSTANT(target, ENOPROTOOPT);
#endif

#ifdef ENOSPC
  NODE_DEFINE_CONSTANT(target, ENOSPC);
#endif

#ifdef ENOSR
  NODE_DEFINE_CONSTANT(target, ENOSR);
#endif

#ifdef ENOSTR
  NODE_DEFINE_CONSTANT(target, ENOSTR);
#endif

#ifdef ENOSYS
  NODE_DEFINE_CONSTANT(target, ENOSYS);
#endif

#ifdef ENOTCONN
  NODE_DEFINE_CONSTANT(target, ENOTCONN);
#endif

#ifdef ENOTDIR
  NODE_DEFINE_CONSTANT(target, ENOTDIR);
#endif

#ifdef ENOTEMPTY
  NODE_DEFINE_CONSTANT(target, ENOTEMPTY);
#endif

#ifdef ENOTSOCK
  NODE_DEFINE_CONSTANT(target, ENOTSOCK);
#endif

#ifdef ENOTSUP
  NODE_DEFINE_CONSTANT(target, ENOTSUP);
#endif

#ifdef ENOTTY
  NODE_DEFINE_CONSTANT(target, ENOTTY);
#endif

#ifdef ENXIO
  NODE_DEFINE_CONSTANT(target, ENXIO);
#endif

#ifdef EOPNOTSUPP
  NODE_DEFINE_CONSTANT(target, EOPNOTSUPP);
#endif

#ifdef EOVERFLOW
  NODE_DEFINE_CONSTANT(target, EOVERFLOW);
#endif

#ifdef EPERM
  NODE_DEFINE_CONSTANT(target, EPERM);
#endif

#ifdef EPIPE
  NODE_DEFINE_CONSTANT(target, EPIPE);
#endif

#ifdef EPROTO
  NODE_DEFINE_CONSTANT(target, EPROTO);
#endif

#ifdef EPROTONOSUPPORT
  NODE_DEFINE_CONSTANT(target, EPROTONOSUPPORT);
#endif

#ifdef EPROTOTYPE
  NODE_DEFINE_CONSTANT(target, EPROTOTYPE);
#endif

#ifdef ERANGE
  NODE_DEFINE_CONSTANT(target, ERANGE);
#endif

#ifdef EROFS
  NODE_DEFINE_CONSTANT(target, EROFS);
#endif

#ifdef ESPIPE
  NODE_DEFINE_CONSTANT(target, ESPIPE);
#endif

#ifdef ESRCH
  NODE_DEFINE_CONSTANT(target, ESRCH);
#endif

#ifdef ESTALE
  NODE_DEFINE_CONSTANT(target, ESTALE);
#endif

#ifdef ETIME
  NODE_DEFINE_CONSTANT(target, ETIME);
#endif

#ifdef ETIMEDOUT
  NODE_DEFINE_CONSTANT(target, ETIMEDOUT);
#endif

#ifdef ETXTBSY
  NODE_DEFINE_CONSTANT(target, ETXTBSY);
#endif

#ifdef EWOULDBLOCK
  NODE_DEFINE_CONSTANT(target, EWOULDBLOCK);
#endif

#ifdef EXDEV
  NODE_DEFINE_CONSTANT(target, EXDEV);
#endif
}

void DefineWindowsErrorConstants(Local<Object> target) {
#ifdef WSAEINTR
  NODE_DEFINE_CONSTANT(target, WSAEINTR);
#endif

#ifdef WSAEBADF
  NODE_DEFINE_CONSTANT(target, WSAEBADF);
#endif

#ifdef WSAEACCES
  NODE_DEFINE_CONSTANT(target, WSAEACCES);
#endif

#ifdef WSAEFAULT
  NODE_DEFINE_CONSTANT(target, WSAEFAULT);
#endif

#ifdef WSAEINVAL
  NODE_DEFINE_CONSTANT(target, WSAEINVAL);
#endif

#ifdef WSAEMFILE
  NODE_DEFINE_CONSTANT(target, WSAEMFILE);
#endif

#ifdef WSAEWOULDBLOCK
  NODE_DEFINE_CONSTANT(target, WSAEWOULDBLOCK);
#endif

#ifdef WSAEINPROGRESS
  NODE_DEFINE_CONSTANT(target, WSAEINPROGRESS);
#endif

#ifdef WSAEALREADY
  NODE_DEFINE_CONSTANT(target, WSAEALREADY);
#endif

#ifdef WSAENOTSOCK
  NODE_DEFINE_CONSTANT(target, WSAENOTSOCK);
#endif

#ifdef WSAEDESTADDRREQ
  NODE_DEFINE_CONSTANT(target, WSAEDESTADDRREQ);
#endif

#ifdef WSAEMSGSIZE
  NODE_DEFINE_CONSTANT(target, WSAEMSGSIZE);
#endif

#ifdef WSAEPROTOTYPE
  NODE_DEFINE_CONSTANT(target, WSAEPROTOTYPE);
#endif

#ifdef WSAENOPROTOOPT
  NODE_DEFINE_CONSTANT(target, WSAENOPROTOOPT);
#endif

#ifdef WSAEPROTONOSUPPORT
  NODE_DEFINE_CONSTANT(target, WSAEPROTONOSUPPORT);
#endif

#ifdef WSAESOCKTNOSUPPORT
  NODE_DEFINE_CONSTANT(target, WSAESOCKTNOSUPPORT);
#endif

#ifdef WSAEOPNOTSUPP
  NODE_DEFINE_CONSTANT(target, WSAEOPNOTSUPP);
#endif

#ifdef WSAEPFNOSUPPORT
  NODE_DEFINE_CONSTANT(target, WSAEPFNOSUPPORT);
#endif

#ifdef WSAEAFNOSUPPORT
  NODE_DEFINE_CONSTANT(target, WSAEAFNOSUPPORT);
#endif

#ifdef WSAEADDRINUSE
  NODE_DEFINE_CONSTANT(target, WSAEADDRINUSE);
#endif

#ifdef WSAEADDRNOTAVAIL
  NODE_DEFINE_CONSTANT(target, WSAEADDRNOTAVAIL);
#endif

#ifdef WSAENETDOWN
  NODE_DEFINE_CONSTANT(target, WSAENETDOWN);
#endif

#ifdef WSAENETUNREACH
  NODE_DEFINE_CONSTANT(target, WSAENETUNREACH);
#endif

#ifdef WSAENETRESET
  NODE_DEFINE_CONSTANT(target, WSAENETRESET);
#endif

#ifdef WSAECONNABORTED
  NODE_DEFINE_CONSTANT(target, WSAECONNABORTED);
#endif

#ifdef WSAECONNRESET
  NODE_DEFINE_CONSTANT(target, WSAECONNRESET);
#endif

#ifdef WSAENOBUFS
  NODE_DEFINE_CONSTANT(target, WSAENOBUFS);
#endif

#ifdef WSAEISCONN
  NODE_DEFINE_CONSTANT(target, WSAEISCONN);
#endif

#ifdef WSAENOTCONN
  NODE_DEFINE_CONSTANT(target, WSAENOTCONN);
#endif

#ifdef WSAESHUTDOWN
  NODE_DEFINE_CONSTANT(target, WSAESHUTDOWN);
#endif

#ifdef WSAETOOMANYREFS
  NODE_DEFINE_CONSTANT(target, WSAETOOMANYREFS);
#endif

#ifdef WSAETIMEDOUT
  NODE_DEFINE_CONSTANT(target, WSAETIMEDOUT);
#endif

#ifdef WSAECONNREFUSED
  NODE_DEFINE_CONSTANT(target, WSAECONNREFUSED);
#endif

#ifdef WSAELOOP
  NODE_DEFINE_CONSTANT(target, WSAELOOP);
#endif

#ifdef WSAENAMETOOLONG
  NODE_DEFINE_CONSTANT(target, WSAENAMETOOLONG);
#endif

#ifdef WSAEHOSTDOWN
  NODE_DEFINE_CONSTANT(target, WSAEHOSTDOWN);
#endif

#ifdef WSAEHOSTUNREACH
  NODE_DEFINE_CONSTANT(target, WSAEHOSTUNREACH);
#endif

#ifdef WSAENOTEMPTY
  NODE_DEFINE_CONSTANT(target, WSAENOTEMPTY);
#endif

#ifdef WSAEPROCLIM
  NODE_DEFINE_CONSTANT(target, WSAEPROCLIM);
#endif

#ifdef WSAEUSERS
  NODE_DEFINE_CONSTANT(target, WSAEUSERS);
#endif

#ifdef WSAEDQUOT
  NODE_DEFINE_CONSTANT(target, WSAEDQUOT);
#endif

#ifdef WSAESTALE
  NODE_DEFINE_CONSTANT(target, WSAESTALE);
#endif

#ifdef WSAEREMOTE
  NODE_DEFINE_CONSTANT(target, WSAEREMOTE);
#endif

#ifdef WSASYSNOTREADY
  NODE_DEFINE_CONSTANT(target, WSASYSNOTREADY);
#endif

#ifdef WSAVERNOTSUPPORTED
  NODE_DEFINE_CONSTANT(target, WSAVERNOTSUPPORTED);
#endif

#ifdef WSANOTINITIALISED
  NODE_DEFINE_CONSTANT(target, WSANOTINITIALISED);
#endif

#ifdef WSAEDISCON
  NODE_DEFINE_CONSTANT(target, WSAEDISCON);
#endif

#ifdef WSAENOMORE
  NODE_DEFINE_CONSTANT(target, WSAENOMORE);
#endif

#ifdef WSAECANCELLED
  NODE_DEFINE_CONSTANT(target, WSAECANCELLED);
#endif

#ifdef WSAEINVALIDPROCTABLE
  NODE_DEFINE_CONSTANT(target, WSAEINVALIDPROCTABLE);
#endif

#ifdef WSAEINVALIDPROVIDER
  NODE_DEFINE_CONSTANT(target, WSAEINVALIDPROVIDER);
#endif

#ifdef WSAEPROVIDERFAILEDINIT
  NODE_DEFINE_CONSTANT(target, WSAEPROVIDERFAILEDINIT);
#endif

#ifdef WSASYSCALLFAILURE
  NODE_DEFINE_CONSTANT(target, WSASYSCALLFAILURE);
#endif

#ifdef WSASERVICE_NOT_FOUND
  NODE_DEFINE_CONSTANT(target, WSASERVICE_NOT_FOUND);
#endif

#ifdef WSATYPE_NOT_FOUND
  NODE_DEFINE_CONSTANT(target, WSATYPE_NOT_FOUND);
#endif

#ifdef WSA_E_NO_MORE
  NODE_DEFINE_CONSTANT(target, WSA_E_NO_MORE);
#endif

#ifdef WSA_E_CANCELLED
  NODE_DEFINE_CONSTANT(target, WSA_E_CANCELLED);
#endif

#ifdef WSAEREFUSED
  NODE_DEFINE_CONSTANT(target, WSAEREFUSED);
#endif
}

void DefineSignalConstants(Local<Object> target) {
#ifdef SIGHUP
  NODE_DEFINE_CONSTANT(target, SIGHUP);
#endif

#ifdef SIGINT
  NODE_DEFINE_CONSTANT(target, SIGINT);
#endif

#ifdef SIGQUIT
  NODE_DEFINE_CONSTANT(target, SIGQUIT);
#endif

#ifdef SIGILL
  NODE_DEFINE_CONSTANT(target, SIGILL);
#endif

#ifdef SIGTRAP
  NODE_DEFINE_CONSTANT(target, SIGTRAP);
#endif

#ifdef SIGABRT
  NODE_DEFINE_CONSTANT(target, SIGABRT);
#endif

#ifdef SIGIOT
  NODE_DEFINE_CONSTANT(target, SIGIOT);
#endif

#ifdef SIGBUS
  NODE_DEFINE_CONSTANT(target, SIGBUS);
#endif

#ifdef SIGFPE
  NODE_DEFINE_CONSTANT(target, SIGFPE);
#endif

#ifdef SIGKILL
  NODE_DEFINE_CONSTANT(target, SIGKILL);
#endif

#ifdef SIGUSR1
  NODE_DEFINE_CONSTANT(target, SIGUSR1);
#endif

#ifdef SIGSEGV
  NODE_DEFINE_CONSTANT(target, SIGSEGV);
#endif

#ifdef SIGUSR2
  NODE_DEFINE_CONSTANT(target, SIGUSR2);
#endif

#ifdef SIGPIPE
  NODE_DEFINE_CONSTANT(target, SIGPIPE);
#endif

#ifdef SIGALRM
  NODE_DEFINE_CONSTANT(target, SIGALRM);
#endif

  NODE_DEFINE_CONSTANT(target, SIGTERM);

#ifdef SIGCHLD
  NODE_DEFINE_CONSTANT(target, SIGCHLD);
#endif

#ifdef SIGSTKFLT
  NODE_DEFINE_CONSTANT(target, SIGSTKFLT);
#endif


#ifdef SIGCONT
  NODE_DEFINE_CONSTANT(target, SIGCONT);
#endif

#ifdef SIGSTOP
  NODE_DEFINE_CONSTANT(target, SIGSTOP);
#endif

#ifdef SIGTSTP
  NODE_DEFINE_CONSTANT(target, SIGTSTP);
#endif

#ifdef SIGBREAK
  NODE_DEFINE_CONSTANT(target, SIGBREAK);
#endif

#ifdef SIGTTIN
  NODE_DEFINE_CONSTANT(target, SIGTTIN);
#endif

#ifdef SIGTTOU
  NODE_DEFINE_CONSTANT(target, SIGTTOU);
#endif

#ifdef SIGURG
  NODE_DEFINE_CONSTANT(target, SIGURG);
#endif

#ifdef SIGXCPU
  NODE_DEFINE_CONSTANT(target, SIGXCPU);
#endif

#ifdef SIGXFSZ
  NODE_DEFINE_CONSTANT(target, SIGXFSZ);
#endif

#ifdef SIGVTALRM
  NODE_DEFINE_CONSTANT(target, SIGVTALRM);
#endif

#ifdef SIGPROF
  NODE_DEFINE_CONSTANT(target, SIGPROF);
#endif

#ifdef SIGWINCH
  NODE_DEFINE_CONSTANT(target, SIGWINCH);
#endif

#ifdef SIGIO
  NODE_DEFINE_CONSTANT(target, SIGIO);
#endif

#ifdef SIGPOLL
  NODE_DEFINE_CONSTANT(target, SIGPOLL);
#endif

#ifdef SIGLOST
  NODE_DEFINE_CONSTANT(target, SIGLOST);
#endif

#ifdef SIGPWR
  NODE_DEFINE_CONSTANT(target, SIGPWR);
#endif

#ifdef SIGINFO
  NODE_DEFINE_CONSTANT(target, SIGINFO);
#endif

#ifdef SIGSYS
  NODE_DEFINE_CONSTANT(target, SIGSYS);
#endif

#ifdef SIGUNUSED
  NODE_DEFINE_CONSTANT(target, SIGUNUSED);
#endif
}

void DefineOpenSSLConstants(Local<Object> target) {
#ifdef SSL_OP_ALL
    NODE_DEFINE_CONSTANT(target, SSL_OP_ALL);
#endif

#ifdef SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
    NODE_DEFINE_CONSTANT(target, SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);
#endif

#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
    NODE_DEFINE_CONSTANT(target, SSL_OP_CIPHER_SERVER_PREFERENCE);
#endif

#ifdef SSL_OP_CISCO_ANYCONNECT
    NODE_DEFINE_CONSTANT(target, SSL_OP_CISCO_ANYCONNECT);
#endif

#ifdef SSL_OP_COOKIE_EXCHANGE
    NODE_DEFINE_CONSTANT(target, SSL_OP_COOKIE_EXCHANGE);
#endif

#ifdef SSL_OP_CRYPTOPRO_TLSEXT_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_CRYPTOPRO_TLSEXT_BUG);
#endif

#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
    NODE_DEFINE_CONSTANT(target, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
#endif

#ifdef SSL_OP_EPHEMERAL_RSA
    NODE_DEFINE_CONSTANT(target, SSL_OP_EPHEMERAL_RSA);
#endif

#ifdef SSL_OP_LEGACY_SERVER_CONNECT
    NODE_DEFINE_CONSTANT(target, SSL_OP_LEGACY_SERVER_CONNECT);
#endif

#ifdef SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
    NODE_DEFINE_CONSTANT(target, SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER);
#endif

#ifdef SSL_OP_MICROSOFT_SESS_ID_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_MICROSOFT_SESS_ID_BUG);
#endif

#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    NODE_DEFINE_CONSTANT(target, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif

#ifdef SSL_OP_NETSCAPE_CA_DN_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_NETSCAPE_CA_DN_BUG);
#endif

#ifdef SSL_OP_NETSCAPE_CHALLENGE_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_NETSCAPE_CHALLENGE_BUG);
#endif

#ifdef SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG);
#endif

#ifdef SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG);
#endif

#ifdef SSL_OP_NO_COMPRESSION
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_COMPRESSION);
#endif

#ifdef SSL_OP_NO_QUERY_MTU
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_QUERY_MTU);
#endif

#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#endif

#ifdef SSL_OP_NO_SSLv2
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_SSLv2);
#endif

#ifdef SSL_OP_NO_SSLv3
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_SSLv3);
#endif

#ifdef SSL_OP_NO_TICKET
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_TICKET);
#endif

#ifdef SSL_OP_NO_TLSv1
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_TLSv1);
#endif

#ifdef SSL_OP_NO_TLSv1_1
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_TLSv1_1);
#endif

#ifdef SSL_OP_NO_TLSv1_2
    NODE_DEFINE_CONSTANT(target, SSL_OP_NO_TLSv1_2);
#endif

#ifdef SSL_OP_PKCS1_CHECK_1
    NODE_DEFINE_CONSTANT(target, SSL_OP_PKCS1_CHECK_1);
#endif

#ifdef SSL_OP_PKCS1_CHECK_2
    NODE_DEFINE_CONSTANT(target, SSL_OP_PKCS1_CHECK_2);
#endif

#ifdef SSL_OP_SINGLE_DH_USE
    NODE_DEFINE_CONSTANT(target, SSL_OP_SINGLE_DH_USE);
#endif

#ifdef SSL_OP_SINGLE_ECDH_USE
    NODE_DEFINE_CONSTANT(target, SSL_OP_SINGLE_ECDH_USE);
#endif

#ifdef SSL_OP_SSLEAY_080_CLIENT_DH_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_SSLEAY_080_CLIENT_DH_BUG);
#endif

#ifdef SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG);
#endif

#ifdef SSL_OP_TLS_BLOCK_PADDING_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_TLS_BLOCK_PADDING_BUG);
#endif

#ifdef SSL_OP_TLS_D5_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_TLS_D5_BUG);
#endif

#ifdef SSL_OP_TLS_ROLLBACK_BUG
    NODE_DEFINE_CONSTANT(target, SSL_OP_TLS_ROLLBACK_BUG);
#endif

# ifndef OPENSSL_NO_ENGINE

# ifdef ENGINE_METHOD_RSA
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_RSA);
# endif

# ifdef ENGINE_METHOD_DSA
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_DSA);
# endif

# ifdef ENGINE_METHOD_DH
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_DH);
# endif

# ifdef ENGINE_METHOD_RAND
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_RAND);
# endif

# ifdef ENGINE_METHOD_ECDH
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_ECDH);
# endif

# ifdef ENGINE_METHOD_ECDSA
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_ECDSA);
# endif

# ifdef ENGINE_METHOD_CIPHERS
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_CIPHERS);
# endif

# ifdef ENGINE_METHOD_DIGESTS
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_DIGESTS);
# endif

# ifdef ENGINE_METHOD_STORE
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_STORE);
# endif

# ifdef ENGINE_METHOD_PKEY_METHS
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_PKEY_METHS);
# endif

# ifdef ENGINE_METHOD_PKEY_ASN1_METHS
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_PKEY_ASN1_METHS);
# endif

# ifdef ENGINE_METHOD_ALL
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_ALL);
# endif

# ifdef ENGINE_METHOD_NONE
    NODE_DEFINE_CONSTANT(target, ENGINE_METHOD_NONE);
# endif

# endif  // !OPENSSL_NO_ENGINE

#ifdef DH_CHECK_P_NOT_SAFE_PRIME
    NODE_DEFINE_CONSTANT(target, DH_CHECK_P_NOT_SAFE_PRIME);
#endif

#ifdef DH_CHECK_P_NOT_PRIME
    NODE_DEFINE_CONSTANT(target, DH_CHECK_P_NOT_PRIME);
#endif

#ifdef DH_UNABLE_TO_CHECK_GENERATOR
    NODE_DEFINE_CONSTANT(target, DH_UNABLE_TO_CHECK_GENERATOR);
#endif

#ifdef DH_NOT_SUITABLE_GENERATOR
    NODE_DEFINE_CONSTANT(target, DH_NOT_SUITABLE_GENERATOR);
#endif

#ifndef OPENSSL_NO_NEXTPROTONEG
#define NPN_ENABLED 1
    NODE_DEFINE_CONSTANT(target, NPN_ENABLED);
#endif

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
#define ALPN_ENABLED 1
    NODE_DEFINE_CONSTANT(target, ALPN_ENABLED);
#endif

#ifdef RSA_PKCS1_PADDING
    NODE_DEFINE_CONSTANT(target, RSA_PKCS1_PADDING);
#endif

#ifdef RSA_SSLV23_PADDING
    NODE_DEFINE_CONSTANT(target, RSA_SSLV23_PADDING);
#endif

#ifdef RSA_NO_PADDING
    NODE_DEFINE_CONSTANT(target, RSA_NO_PADDING);
#endif

#ifdef RSA_PKCS1_OAEP_PADDING
    NODE_DEFINE_CONSTANT(target, RSA_PKCS1_OAEP_PADDING);
#endif

#ifdef RSA_X931_PADDING
    NODE_DEFINE_CONSTANT(target, RSA_X931_PADDING);
#endif

#ifdef RSA_PKCS1_PSS_PADDING
    NODE_DEFINE_CONSTANT(target, RSA_PKCS1_PSS_PADDING);
#endif

#if HAVE_OPENSSL
  // NOTE: These are not defines
  NODE_DEFINE_CONSTANT(target, POINT_CONVERSION_COMPRESSED);

  NODE_DEFINE_CONSTANT(target, POINT_CONVERSION_UNCOMPRESSED);

  NODE_DEFINE_CONSTANT(target, POINT_CONVERSION_HYBRID);
#endif
}

void DefineSystemConstants(Local<Object> target) {
  // file access modes
  NODE_DEFINE_CONSTANT(target, O_RDONLY);
  NODE_DEFINE_CONSTANT(target, O_WRONLY);
  NODE_DEFINE_CONSTANT(target, O_RDWR);

  NODE_DEFINE_CONSTANT(target, S_IFMT);
  NODE_DEFINE_CONSTANT(target, S_IFREG);
  NODE_DEFINE_CONSTANT(target, S_IFDIR);
  NODE_DEFINE_CONSTANT(target, S_IFCHR);
#ifdef S_IFBLK
  NODE_DEFINE_CONSTANT(target, S_IFBLK);
#endif

#ifdef S_IFIFO
  NODE_DEFINE_CONSTANT(target, S_IFIFO);
#endif

#ifdef S_IFLNK
  NODE_DEFINE_CONSTANT(target, S_IFLNK);
#endif

#ifdef S_IFSOCK
  NODE_DEFINE_CONSTANT(target, S_IFSOCK);
#endif

#ifdef O_CREAT
  NODE_DEFINE_CONSTANT(target, O_CREAT);
#endif

#ifdef O_EXCL
  NODE_DEFINE_CONSTANT(target, O_EXCL);
#endif

#ifdef O_NOCTTY
  NODE_DEFINE_CONSTANT(target, O_NOCTTY);
#endif

#ifdef O_TRUNC
  NODE_DEFINE_CONSTANT(target, O_TRUNC);
#endif

#ifdef O_APPEND
  NODE_DEFINE_CONSTANT(target, O_APPEND);
#endif

#ifdef O_DIRECTORY
  NODE_DEFINE_CONSTANT(target, O_DIRECTORY);
#endif

#ifdef O_EXCL
  NODE_DEFINE_CONSTANT(target, O_EXCL);
#endif

#ifdef O_NOATIME
  NODE_DEFINE_CONSTANT(target, O_NOATIME);
#endif

#ifdef O_NOFOLLOW
  NODE_DEFINE_CONSTANT(target, O_NOFOLLOW);
#endif

#ifdef O_SYNC
  NODE_DEFINE_CONSTANT(target, O_SYNC);
#endif

#ifdef O_SYMLINK
  NODE_DEFINE_CONSTANT(target, O_SYMLINK);
#endif

#ifdef O_DIRECT
  NODE_DEFINE_CONSTANT(target, O_DIRECT);
#endif

#ifdef O_NONBLOCK
  NODE_DEFINE_CONSTANT(target, O_NONBLOCK);
#endif

#ifdef S_IRWXU
  NODE_DEFINE_CONSTANT(target, S_IRWXU);
#endif

#ifdef S_IRUSR
  NODE_DEFINE_CONSTANT(target, S_IRUSR);
#endif

#ifdef S_IWUSR
  NODE_DEFINE_CONSTANT(target, S_IWUSR);
#endif

#ifdef S_IXUSR
  NODE_DEFINE_CONSTANT(target, S_IXUSR);
#endif

#ifdef S_IRWXG
  NODE_DEFINE_CONSTANT(target, S_IRWXG);
#endif

#ifdef S_IRGRP
  NODE_DEFINE_CONSTANT(target, S_IRGRP);
#endif

#ifdef S_IWGRP
  NODE_DEFINE_CONSTANT(target, S_IWGRP);
#endif

#ifdef S_IXGRP
  NODE_DEFINE_CONSTANT(target, S_IXGRP);
#endif

#ifdef S_IRWXO
  NODE_DEFINE_CONSTANT(target, S_IRWXO);
#endif

#ifdef S_IROTH
  NODE_DEFINE_CONSTANT(target, S_IROTH);
#endif

#ifdef S_IWOTH
  NODE_DEFINE_CONSTANT(target, S_IWOTH);
#endif

#ifdef S_IXOTH
  NODE_DEFINE_CONSTANT(target, S_IXOTH);
#endif

#ifdef F_OK
  NODE_DEFINE_CONSTANT(target, F_OK);
#endif

#ifdef R_OK
  NODE_DEFINE_CONSTANT(target, R_OK);
#endif

#ifdef W_OK
  NODE_DEFINE_CONSTANT(target, W_OK);
#endif

#ifdef X_OK
  NODE_DEFINE_CONSTANT(target, X_OK);
#endif
}

void DefineUVConstants(Local<Object> target) {
  NODE_DEFINE_CONSTANT(target, UV_UDP_REUSEADDR);
}

void DefineCryptoConstants(Local<Object> target) {
#if HAVE_OPENSSL
  NODE_DEFINE_STRING_CONSTANT(target,
                              "defaultCoreCipherList",
                              DEFAULT_CIPHER_LIST_CORE);
  NODE_DEFINE_STRING_CONSTANT(target,
                              "defaultCipherList",
                              default_cipher_list);
#endif
}

void DefineZlibConstants(Local<Object> target) {
  NODE_DEFINE_CONSTANT(target, Z_NO_FLUSH);
  NODE_DEFINE_CONSTANT(target, Z_PARTIAL_FLUSH);
  NODE_DEFINE_CONSTANT(target, Z_SYNC_FLUSH);
  NODE_DEFINE_CONSTANT(target, Z_FULL_FLUSH);
  NODE_DEFINE_CONSTANT(target, Z_FINISH);
  NODE_DEFINE_CONSTANT(target, Z_BLOCK);

  // return/error codes
  NODE_DEFINE_CONSTANT(target, Z_OK);
  NODE_DEFINE_CONSTANT(target, Z_STREAM_END);
  NODE_DEFINE_CONSTANT(target, Z_NEED_DICT);
  NODE_DEFINE_CONSTANT(target, Z_ERRNO);
  NODE_DEFINE_CONSTANT(target, Z_STREAM_ERROR);
  NODE_DEFINE_CONSTANT(target, Z_DATA_ERROR);
  NODE_DEFINE_CONSTANT(target, Z_MEM_ERROR);
  NODE_DEFINE_CONSTANT(target, Z_BUF_ERROR);
  NODE_DEFINE_CONSTANT(target, Z_VERSION_ERROR);

  NODE_DEFINE_CONSTANT(target, Z_NO_COMPRESSION);
  NODE_DEFINE_CONSTANT(target, Z_BEST_SPEED);
  NODE_DEFINE_CONSTANT(target, Z_BEST_COMPRESSION);
  NODE_DEFINE_CONSTANT(target, Z_DEFAULT_COMPRESSION);
  NODE_DEFINE_CONSTANT(target, Z_FILTERED);
  NODE_DEFINE_CONSTANT(target, Z_HUFFMAN_ONLY);
  NODE_DEFINE_CONSTANT(target, Z_RLE);
  NODE_DEFINE_CONSTANT(target, Z_FIXED);
  NODE_DEFINE_CONSTANT(target, Z_DEFAULT_STRATEGY);
  NODE_DEFINE_CONSTANT(target, ZLIB_VERNUM);

  enum node_zlib_mode {
    NONE,
    DEFLATE,
    INFLATE,
    GZIP,
    GUNZIP,
    DEFLATERAW,
    INFLATERAW,
    UNZIP
  };

  NODE_DEFINE_CONSTANT(target, DEFLATE);
  NODE_DEFINE_CONSTANT(target, INFLATE);
  NODE_DEFINE_CONSTANT(target, GZIP);
  NODE_DEFINE_CONSTANT(target, GUNZIP);
  NODE_DEFINE_CONSTANT(target, DEFLATERAW);
  NODE_DEFINE_CONSTANT(target, INFLATERAW);
  NODE_DEFINE_CONSTANT(target, UNZIP);

#define Z_MIN_WINDOWBITS 8
#define Z_MAX_WINDOWBITS 15
#define Z_DEFAULT_WINDOWBITS 15
// Fewer than 64 bytes per chunk is not recommended.
// Technically it could work with as few as 8, but even 64 bytes
// is low.  Usually a MB or more is best.
#define Z_MIN_CHUNK 64
#define Z_MAX_CHUNK std::numeric_limits<double>::infinity()
#define Z_DEFAULT_CHUNK (16 * 1024)
#define Z_MIN_MEMLEVEL 1
#define Z_MAX_MEMLEVEL 9
#define Z_DEFAULT_MEMLEVEL 8
#define Z_MIN_LEVEL -1
#define Z_MAX_LEVEL 9
#define Z_DEFAULT_LEVEL Z_DEFAULT_COMPRESSION

  NODE_DEFINE_CONSTANT(target, Z_MIN_WINDOWBITS);
  NODE_DEFINE_CONSTANT(target, Z_MAX_WINDOWBITS);
  NODE_DEFINE_CONSTANT(target, Z_DEFAULT_WINDOWBITS);
  NODE_DEFINE_CONSTANT(target, Z_MIN_CHUNK);
  NODE_DEFINE_CONSTANT(target, Z_MAX_CHUNK);
  NODE_DEFINE_CONSTANT(target, Z_DEFAULT_CHUNK);
  NODE_DEFINE_CONSTANT(target, Z_MIN_MEMLEVEL);
  NODE_DEFINE_CONSTANT(target, Z_MAX_MEMLEVEL);
  NODE_DEFINE_CONSTANT(target, Z_DEFAULT_MEMLEVEL);
  NODE_DEFINE_CONSTANT(target, Z_MIN_LEVEL);
  NODE_DEFINE_CONSTANT(target, Z_MAX_LEVEL);
  NODE_DEFINE_CONSTANT(target, Z_DEFAULT_LEVEL);
}

void DefineConstants(v8::Isolate* isolate, Local<Object> target) {
  Environment* env = Environment::GetCurrent(isolate);

  Local<Object> os_constants = Object::New(isolate);
  CHECK(os_constants->SetPrototype(env->context(),
                                   Null(env->isolate())).FromJust());

  Local<Object> err_constants = Object::New(isolate);
  CHECK(err_constants->SetPrototype(env->context(),
                                    Null(env->isolate())).FromJust());

  Local<Object> sig_constants = Object::New(isolate);
  CHECK(sig_constants->SetPrototype(env->context(),
                                    Null(env->isolate())).FromJust());

  Local<Object> fs_constants = Object::New(isolate);
  CHECK(fs_constants->SetPrototype(env->context(),
                                   Null(env->isolate())).FromJust());

  Local<Object> crypto_constants = Object::New(isolate);
  CHECK(crypto_constants->SetPrototype(env->context(),
                                       Null(env->isolate())).FromJust());

  Local<Object> zlib_constants = Object::New(isolate);
  CHECK(zlib_constants->SetPrototype(env->context(),
                                     Null(env->isolate())).FromJust());

  DefineErrnoConstants(err_constants);
  DefineWindowsErrorConstants(err_constants);
  DefineSignalConstants(sig_constants);
  DefineUVConstants(os_constants);
  DefineSystemConstants(fs_constants);
  DefineOpenSSLConstants(crypto_constants);
  DefineCryptoConstants(crypto_constants);
  DefineZlibConstants(zlib_constants);

  os_constants->Set(OneByteString(isolate, "errno"), err_constants);
  os_constants->Set(OneByteString(isolate, "signals"), sig_constants);
  target->Set(OneByteString(isolate, "os"), os_constants);
  target->Set(OneByteString(isolate, "fs"), fs_constants);
  target->Set(OneByteString(isolate, "crypto"), crypto_constants);
  target->Set(OneByteString(isolate, "zlib"), zlib_constants);
}

}  // namespace node
