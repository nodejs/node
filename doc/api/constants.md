# Constants

The `constants` module provides convenient access to commonly used constants for
use with file access, crypto methods, error handling and more. No methods are
exposed by the `constants` module itself.

**Note:** Specific constants may not be available on every operating system.

## Error Constants

The following error constants are adapted directly from the POSIX `errno`
standards. See http://man7.org/linux/man-pages/man3/errno.3.html for more
details about specific error constants.

### constants.E2BIG

Indicates that the list of arguments is longer than expected.

### constants.EACCES

Indicates that the operation did not have sufficient permissions.

### constants.EADDRINUSE

Indicates that the network address is already in use.

### constants.EADDRNOTAVAIL

Indicates that the network address is currently unavailable for use.

### constants.EAFNOSUPPORT

Indicates that the network address family is not supported.

### constants.EAGAIN

Indicates that there is currently no data available and to try the operation
again later.

### constants.EALREADY

Indicates that the socket already has a pending connection in progress.

### constants.EBADF

Indicates that a file descriptor is not valid.

### constants.EBADMSG

Indicates an invalid data message.

### constants.EBUSY

Indicates that a device or resource is busy.

### constants.ECANCELED

Indicates that an operation was canceled.

### constants.ECHILD

Indicates that there are no child processes

### constants.ECONNABORTED

Indicates that the network connection has been aborted.

### constants.ECONNREFUSED

Indicates that the network connection has been refused.

### constants.ECONNRESET

Indicates that the network connection has been reset.

### constants.EDEADLK

Indicates that a resource deadlock has been avoided.

### constants.EDESTADDRREQ

Indicates that a destination address is required.

### constants.EDOM

Indicates that an argument is out of the domain of the function.

### constants.EDQUOT

Indicates that the disk quota has been exceeded.

### constants.EEXIST

Indicates that the file already exists.

### constants.EFAULT

Indicates an invalid pointer address.

### constants.EFBIG

Indicates that the file is too large.

### constants.EHOSTUNREACH

Indicates that the host is unreachable.

### constants.EIDRM

Indicates that the identifier has been removed.

### constants.EILSEQ

Indicates an illegal byte sequence.

### constants.EINPROGRESS

Indicates that an operation is already in progress.

### constants.EINTR

Indicates that a function call was interrupted.

### constants.EINVAL

Indicates that an invalid argument was provided.

### constants.EIO

Indicates an otherwise unspecified I/O error.

### constants.EISCONN

Indicates that the socket is connected.

### constants.EISDIR

Indicates that the path is a directory.

### constants.ELOOP

Indicates too many levels of symbolic links in a path.

### constants.EMFILE

Indicates that there are too many open files.

### constants.EMLINK

Indicates that there are too many links.

### constants.EMSGSIZE

Indicates that the provided message is too long.

### constants.EMULTIHOP

Indicates that a multihop was attempted.

### constants.ENAMETOOLONG

Indicates that the filename is too long.

### constants.ENETDOWN

Indicates that the network is down.

### constants.ENETRESET

Indicates that the connection has been aborted by the network.

### constants.ENETUNREACH

Indicates that the network is unreachable.

### constants.ENFILE

Indicates too many open files in the system.

### constants.ENOBUFS

Indicates that no buffer space is available.

### constants.ENODATA

Indicates that no message is available on the stream head read queue.

### constants.ENODEV

Indicates that there is no such device.

### constants.ENOENT

Indicates that there is no such file or directory.

### constants.ENOEXEC

Indicates an exec format error.

### constants.ENOLCK

Indicates that there are no locks available.

### constants.ENOLINK

Indications that a link has been severed.

### constants.ENOMEM

Indicates that there is not enough space.

### constants.ENOMSG

Indicates that there is no message of the desired type.

### constants.ENOPROTOOPT

Indicates that a given protocol is not available

### constants.ENOSPC

Indicates that there is no space available on the device.

### constants.ENOSR

Indicates that there are no stream resources available.

### constants.ENOSTR

Indicates that a given resource is not a stream.

### constants.ENOSYS

Indicates that a function has not been implemented.

### constants.ENOTCONN

Indicates that the socket is not connected.

### constants.ENOTDIR

Indicates that the path is not a directory.

### constants.ENOTEMPTY

Indicates that the directory is not empty.

### constants.ENOTSOCK

Indicates that the given item is not a socket.

### constants.ENOTSUP

Indicates that a given operation is not supported.

### constants.ENOTTY

Indicates an inappropriate I/O control operation.

### constants.ENXIO

Indicates no such device or address.

### constants.EOPNOTSUPP

Indicates that an operation is not supported on the socket.

### constants.EOVERFLOW

Indicates that a value is too large to be stored in a given data type.

### constants.EPERM

Indicates that the operation is not permitted.

### constants.EPIPE

Indicates a broken pipe.

### constants.EPROTO

Indicates a protocol error.

### constants.EPROTONOSUPPORT

Indicates that a protocol is not supported.

### constants.EPROTOTYPE

Indicates the wrong type of protocol for a socket.

### constants.ERANGE

Indicates that the results are too large.

### constants.EROFS

Indicates that the file system is read only.

### constants.ESPIPE

Indicates an invalid seek operation.

### constants.ESRCH

Indicates that there is no such process.

### constants.ESTALE

Indicates that the file handle is stale.

### constants.ETIME

Indicates an expired timer.

### constants.ETIMEDOUT

Indicates that the connection timed out.

### constants.ETXTBSY

Indicates that a text file is busy.

### constants.EWOULDBLOCK

Indicates that the operation would block.

### constants.EXDEV

Indicates an improper link.


## Windows Specific Error Constants

The following error codes are specific to the Windows operating system.

### constants.WSAEINTR

Indicates an interrupted function call.

### constants.WSAEBADF

Indicates an invalid file handle.

### constants.WSAEACCES

Indicates insufficient permissions to complete the operation.

### constants.WSAEFAULT

Indicates an invalid pointer address.

### constants.WSAEINVAL

Indicates that an invalid argument was passed.

### constants.WSAEMFILE

Indicates that there are too many open files.

### constants.WSAEWOULDBLOCK

Indicates that a resource is temporarily unavailable.

### constants.WSAEINPROGRESS

Indicates that an operation is currently in progress.

### constants.WSAEALREADY

Indicates that an operation is already in progress.

### constants.WSAENOTSOCK

Indicates that the resource is not a socket.

### constants.WSAEDESTADDRREQ

Indicates that a destination address is required.

### constants.WSAEMSGSIZE

Indicates that the message size is too long.

### constants.WSAEPROTOTYPE

Indicates the wrong protocol type for the socket.

### constants.WSAENOPROTOOPT

Indicates a bad protocol option.

### constants.WSAEPROTONOSUPPORT

Indicates that the protocol is not supported.

### constants.WSAESOCKTNOSUPPORT

Indicates that the socket type is not supported.

### constants.WSAEOPNOTSUPP

Indicates that the operation is not supported.

### constants.WSAEPFNOSUPPORT

Indicates that the protocol family is not supported.

### constants.WSAEAFNOSUPPORT

Indicates that the address family is not supported.

### constants.WSAEADDRINUSE

Indicates that the network address is already in use.

### constants.WSAEADDRNOTAVAIL

Indicates that the network address is not available.

### constants.WSAENETDOWN

Indicates that the network is down.

### constants.WSAENETUNREACH

Indicates that the network is unreachable.

### constants.WSAENETRESET

Indicates that the network connection has been reset.

### constants.WSAECONNABORTED

Indicates that the connection has been aborted.

### constants.WSAECONNRESET

Indicates that the connection has been reset by the peer.

### constants.WSAENOBUFS

Indicates that there is no buffer space available.

### constants.WSAEISCONN

Indicates that the socket is already connected.

### constants.WSAENOTCONN

Indicates that the socket is not connected.

### constants.WSAESHUTDOWN

Indicates that data cannot be sent after the socket has been shutdown.

### constants.WSAETOOMANYREFS

Indicates that there are too many references.

### constants.WSAETIMEDOUT

Indicates that the connection has timed out.

### constants.WSAECONNREFUSED

Indicates that the connection has been refused.

### constants.WSAELOOP

Indicates that a name cannot be translated.

### constants.WSAENAMETOOLONG

Indicates that a name was too long.

### constants.WSAEHOSTDOWN

Indicates that a network host is down.

### constants.WSAEHOSTUNREACH

Indicates that there is no route to a network host.

### constants.WSAENOTEMPTY

Indicates that the directory is not empty.

### constants.WSAEPROCLIM

Indicates that there are too many processes.

### constants.WSAEUSERS

Indicates that the user quota has been exceeded.

### constants.WSAEDQUOT

Indicates that the disk quota has been exceeded.

### constants.WSAESTALE

Indicates a stale file handle reference.

### constants.WSAEREMOTE

Indicates that the item is remote.

### constants.WSASYSNOTREADY

Indicates that the network subsystem is not ready.

### constants.WSAVERNOTSUPPORTED

Indicates that the winsock.dll version is out of range.

### constants.WSANOTINITIALISED

Indicates that successful WSAStartup has not yet been performed.

### constants.WSAEDISCON

Indicates that a graceful shutdown is in progress.

### constants.WSAENOMORE

Indicates that there are no more results.

### constants.WSAECANCELLED

Indicates that an operation has been canceled.

### constants.WSAEINVALIDPROCTABLE

Indicates that the procedure call table is invalid.

### constants.WSAEINVALIDPROVIDER

Indicates an invalid service provider.

### constants.WSAEPROVIDERFAILEDINIT

Indicates that the service provider failed to initialized.

### constants.WSASYSCALLFAILURE

Indicates a system call failure.

### constants.WSASERVICE_NOT_FOUND

Indicates that a service was not found.

### constants.WSATYPE_NOT_FOUND

Indicates that a class type was not found.

### constants.WSA_E_NO_MORE

Indicates that there are no more results.

### constants.WSA_E_CANCELLED

Indicates that the call was canceled.

### constants.WSAEREFUSED

Indicates that a database query was refused.


## Signal Constants

See http://man7.org/linux/man-pages/man7/signal.7.html for additional
information.

### constants.SIGHUP

Sent to indicate when a controlling terminal or process is closed.

### constants.SIGINT

Sent to indicate when a user wishes to interrupt a process.

### constants.SIGQUIT

Sent to indicate when a user wishes to terminate a process and perform a
core dump.

### constants.SIGILL

Sent to a process to notify that it has attempted to perform an illegal,
malformed, unknown or privileged instruction.

### constants.SIGTRAP

Sent to a proces when an exception has occurred.

### constants.SIGABRT

Sent to a process to request that it abort.

### constants.SIGIOT

Synonym for `constants.SIGABRT`

### constants.SIGBUS

Sent to a process to notify that it has caused a bus error.

### constants.SIGFPE

Sent to a process to notify that it has performed an illegal arithmetic operation.

### constants.SIGKILL

Sent to a process to terminate it immediately.

### constants.SIGUSR1 and constants.SIGUSR2

Sent to a process to identify user-defined conditions.

### constants.SIGSEGV

Sent to a process to notify of a segmentation fault.

### constants.SIGPIPE

Sent to a process when it has attempted to write to a disconnected pipe.

### constants.SIGALRM

Sent to a process when a system timer elapses.

### constants.SIGTERM

Sent to a process to request termination.

### constants.SIGCHLD

Sent to a process when a child process terminates.

### constants.SIGSTKFLT

Sent to a process to indicate a stack fault on a coprocessor.

### constants.SIGCONT

Sent to instruct the operating system to continue a paused process.

### constants.SIGSTOP

Sent to instruct the operating system to halt a process.

### constants.SIGTSTP

Sent to a process to request it to stop.

### constants.SIGBREAK

Sent to indicate when a user wishes to interrupt a process.

### constants.SIGTTIN

Sent to a process when it reads from the TTY while in the background.

### constants.SIGTTOU

Sent to a process when it writes to the TTY while in the background.

### constants.SIGURG

Sent to a process when a socket has urgent data to read.

### constants.SIGXCPU

Sent to a process when it has exceeded it's limit on CPI usage.

### constants.SIGXFSZ

Sent to a process when it grows a file larger than the maximum allowed.

### constants.SIGVTALRM

Sent to a process when a virtual timer has elapsed.

### constants.SIGPROF

Sent to a process when a system timer has elapsed.

### constants.SIGWINCH

Sent to a process when the controlling terminal has changed sized.

### constants.SIGIO

Sent to a process when I/O is available.

### constants.SIGPOLL

Synonym for `constants.SIGIO`

### constants.SIGLOST

Sent to a process when a file lock has been lost.

### constants.SIGPWR

Sent to a process to notify of a power failure.

### constants.SIGINFO

Synonym for `constants.SIGPWR`

### constants.SIGSYS

Sent to a process to notify of a bad argument.

### constants.SIGUNUSED

Synonym for `constants.SIGSYS`


## File Access Constants

The following flags are meant for use with [`fs.access()`][].

### constants.F_OK

Flag indicating that the file is visible to the calling process.

### constants.R_OK

Flag indicating that the file can be read by the calling process.

### constants.W_OK

Flag indicating that the file can be written by the calling process.

### constants.X_OK

Flag indicating that the file can be executed by the calling process.

## File I/O Flags

### constants.O_RDONLY

Flag indicating to open a file for read-only access.

### constants.O_WRONLY

Flag indicating to open a file for write-only access.

### constants.O_RDWR

Flag indicating to open a file for read-write access.

### constants.O_CREAT

Flag indicating to create the file if it does not already exist.

### constants.O_EXCL

Flag indicating that opening a file should fail if the `constants.O_CREAT`
flag is set and the file already exists.

### constants.O_NOCTTY

Flag indicating that if path identifies a terminal device, opening the path
shall not cause that terminal to become the controlling terminal for the 
process.

### constants.O_TRUNC

Flag indicating that if the file exists and is a regular file, and the file
is opened successfully for write access, it's length shall be truncated to
zero.

### constants.O_APPEND

Flag indicating that data will be appended to the end of the file.

### constants.O_DIRECTORY

Flag indicating that the open should fail if the path is not a directory.

### constants.O_NOFOLLOW

Flag indicating that the open should fail if the path is a symbolic link.

### constants.O_SYNC

Flag indicating that the file is opened for synchronous I/O.

### constants.O_SYMLINK

Flag indicating to open the symbolic link itself rather than the resource it
is pointing to.

### constants.O_DIRECT

When set, an attempt will be made to minimize caching effects of file I/O.

### constants.O_NONBLOCK

Flag indicating to open the file in nonblocking mode when possible.


## File Type Constants

### constants.S_IFMT

Bit mask used to extract the file type code.

### constants.S_IFREG

File type constant for a regular file.

### constants.S_IFDIR

File type constant for a directory.

### constants.S_IFCHR

File type constant for a character-oriented device file.

### constants.S_IFBLK

File type constant for a block-oriented device file.

### constants.S_IFIFO

File type constant for a FIFO/pipe.

### constants.S_IFLNK

File type constant for a symbolic link.

### constants.S_IFSOCK

File type constant for a socket.


## File Mode Constants

### constants.S_IRWXU

File mode indicating read, write and execute by owner.

### constants.S_IRUSR

File mode indicating read by owner.

### constants.S_IWUSR

File mode indicating write by owner.

### constants.S_IXUSR

File mode indicating execute by owner.

### constants.S_IRWXG

File mode indicating read, write and execute by group.

### constants.S_IRGRP

File mode indicating read by group.

### constants.S_IWGRP

File mode indicating write by group.

### constants.S_IXGRP

File mode indicating execute by group.

### constants.S_IRWXO

File mode indicating read, write and execute by others.

### constants.S_IROTH

File mode indicating read by others.

### constants.S_IWOTH

File mode indicating write by others.

### constants.S_IXOTH

File mode indicating execute by others.

## libuv Constants

### constants.UV_UDP_REUSEADDR


## Crypto Constants

The following constants apply to various uses of the `crypto`, `tls`, and
`https` modules and are generally specific to OpenSSL (with noted exceptions).

### OpenSSL Options

#### constants.SSL_OP_ALL

Applies multiple bug workarounds within OpenSSL. See https://www.openssl.org/docs/manmaster/ssl/SSL_CTX_set_options.html for detail.

#### constants.SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION

Allows legacy insecure renegotiation between OpenSSL and unpatched clients or
servers. See https://www.openssl.org/docs/manmaster/ssl/SSL_CTX_set_options.html.

#### constants.SSL_OP_CIPHER_SERVER_PREFERENCE

Uses the server's preferences instead of the clients when selecting a cipher.
See https://www.openssl.org/docs/manmaster/ssl/SSL_CTX_set_options.html.

#### constants.SSL_OP_CISCO_ANYCONNECT

Instructs OpenSSL to use Cisco's "speshul" version of DTLS_BAD_VER.

#### constants.SSL_OP_COOKIE_EXCHANGE

Instructs OpenSSL to turn on cookie exchange.

#### constants.SSL_OP_CRYPTOPRO_TLSEXT_BUG

Instructs OpenSSL to add server-hello extension from an early version of the
cryptopro draft.

#### constants.SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS

Instructs OpenSSL to disable a SSL 3.0/TLS 1.0 vulnerability workaround added
in OpenSSL 0.9.6d.

#### constants.SSL_OP_EPHEMERAL_RSA

Instructs OpenSSL to always use the tmp_rsa key when performing RSA operations.

#### constants.SSL_OP_LEGACY_SERVER_CONNECT

Allow initial connection to servers that do not support RI.

#### constants.SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
#### constants.SSL_OP_MICROSOFT_SESS_ID_BUG

#### constants.SSL_OP_MSIE_SSLV2_RSA_PADDING

Instructs OpenSSL to disable the workaround for a man-in-the-middle
protocol-version vulnerability in the SSL 2.0 server implementation.

#### constants.SSL_OP_NETSCAPE_CA_DN_BUG
#### constants.SSL_OP_NETSCAPE_CHALLENGE_BUG
#### constants.SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG
#### constants.SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG

#### constants.SSL_OP_NO_COMPRESSION

Instructs OpenSSL to disable support for SSL/TLS compression.

#### constants.SSL_OP_NO_QUERY_MTU

#### constants.SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION

Instructs OpenSSL to always start a new session when performing renegotiation.

#### constants.SSL_OP_NO_SSLv2

Instructs OpenSSL to turn off SSL v2

#### constants.SSL_OP_NO_SSLv3

Instructs OpenSSL to turn off SSL v3

#### constants.SSL_OP_NO_TICKET

Instructs OpenSSL to disable use of RFC4507bis tickets.

#### constants.SSL_OP_NO_TLSv1

Instructs OpenSSL to turn off TLS v1

#### constants.SSL_OP_NO_TLSv1_1

Instructs OpenSSL to turn off TLS v1.1

#### constants.SSL_OP_NO_TLSv1_2

Instructs OpenSSL to turn off TLS v1.2

#### constants.SSL_OP_PKCS1_CHECK_1
#### constants.SSL_OP_PKCS1_CHECK_2

#### constants.SSL_OP_SINGLE_DH_USE

Instruct OpenSSL to always create a new key when using temporary/ephemeral DH
parameters.

#### constants.SSL_OP_SINGLE_ECDH_USE

Instruct OpenSSL to always create a new key when using temporary/ephemeral ECDH
parameters.

#### constants.SSL_OP_SSLEAY_080_CLIENT_DH_BUG
#### constants.SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
#### constants.SSL_OP_TLS_BLOCK_PADDING_BUG
#### constants.SSL_OP_TLS_D5_BUG
#### constants.SSL_OP_TLS_ROLLBACK_BUG

Instructs OpenSSL to disable version rollback attack detection.

### OpenSSL Engine Constants

#### constants.ENGINE_METHOD_RSA

Limit engine usage to RSA

#### constants.ENGINE_METHOD_DSA

Limit engine usage to DSA

#### constants.ENGINE_METHOD_DH

Limit engine usage to DH

#### constants.ENGINE_METHOD_RAND

Limit engine usage to RAND

#### constants.ENGINE_METHOD_ECDH

Limit engine usage to ECDH

#### constants.ENGINE_METHOD_ECDSA

Limit engine usage to ECDSA

#### constants.ENGINE_METHOD_CIPHERS

Limit engine usage to CIPHERS

#### constants.ENGINE_METHOD_DIGESTS

Limit engine usage to DIGESTS

#### constants.ENGINE_METHOD_STORE

Limit engine usage to STORE

#### constants.ENGINE_METHOD_PKEY_METHS

Limit engine usage to PKEY_METHDS

#### constants.ENGINE_METHOD_PKEY_ASN1_METHS

Limit engine usage to PKEY_ASN1_METHS

#### constants.ENGINE_METHOD_ALL
#### constants.ENGINE_METHOD_NONE

### Other OpenSSL Constants

#### constants.DH_CHECK_P_NOT_SAFE_PRIME
#### constants.DH_CHECK_P_NOT_PRIME
#### constants.DH_UNABLE_TO_CHECK_GENERATOR
#### constants.DH_NOT_SUITABLE_GENERATOR
#### constants.NPN_ENABLED
#### constants.ALPN_ENABLED
#### constants.RSA_PKCS1_PADDING
#### constants.RSA_SSLV23_PADDING
#### constants.RSA_NO_PADDING
#### constants.RSA_PKCS1_OAEP_PADDING
#### constants.RSA_X931_PADDING
#### constants.RSA_PKCS1_PSS_PADDING
#### constants.POINT_CONVERSION_COMPRESSED
#### constants.POINT_CONVERSION_UNCOMPRESSED
#### constants.POINT_CONVERSION_HYBRID

### Node.js Crypto Constants

#### constants.defaultCoreCipherList

Specifies the built in default cipher list used by Node.js.

#### constants.defaultCipherList

Specifies the active default cipher list used by the current Node.js process.

[`fs.access()`]: fs.html#fs_fs_access_path_mode_callback
