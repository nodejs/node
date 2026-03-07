/// Operation not permitted
pub const EPERM: i32 = 1;

/// No such file or directory
pub const ENOENT: i32 = 2;

/// No such process
pub const ESRCH: i32 = 3;

/// Interrupted system call
pub const EINTR: i32 = 4;

/// I/O error
pub const EIO: i32 = 5;

/// No such device or address
pub const ENXIO: i32 = 6;

/// Argument list too long
pub const E2BIG: i32 = 7;

/// Exec format error
pub const ENOEXEC: i32 = 8;

/// Bad file number
pub const EBADF: i32 = 9;

/// No child processes
pub const ECHILD: i32 = 10;

/// Try again
pub const EAGAIN: i32 = 11;

/// Out of memory
pub const ENOMEM: i32 = 12;

/// Permission denied
pub const EACCES: i32 = 13;

/// Bad address
pub const EFAULT: i32 = 14;

/// Block device required
pub const ENOTBLK: i32 = 15;

/// Device or resource busy
pub const EBUSY: i32 = 16;

/// File exists
pub const EEXIST: i32 = 17;

/// Cross-device link
pub const EXDEV: i32 = 18;

/// No such device
pub const ENODEV: i32 = 19;

/// Not a directory
pub const ENOTDIR: i32 = 20;

/// Is a directory
pub const EISDIR: i32 = 21;

/// Invalid argument
pub const EINVAL: i32 = 22;

/// File table overflow
pub const ENFILE: i32 = 23;

/// Too many open files
pub const EMFILE: i32 = 24;

/// Not a typewriter
pub const ENOTTY: i32 = 25;

/// Text file busy
pub const ETXTBSY: i32 = 26;

/// File too large
pub const EFBIG: i32 = 27;

/// No space left on device
pub const ENOSPC: i32 = 28;

/// Illegal seek
pub const ESPIPE: i32 = 29;

/// Read-only file system
pub const EROFS: i32 = 30;

/// Too many links
pub const EMLINK: i32 = 31;

/// Broken pipe
pub const EPIPE: i32 = 32;

/// Math argument out of domain of func
pub const EDOM: i32 = 33;

/// Math result not representable
pub const ERANGE: i32 = 34;

/// Resource deadlock would occur
pub const EDEADLK: i32 = 35;

/// File name too long
pub const ENAMETOOLONG: i32 = 36;

/// No record locks available
pub const ENOLCK: i32 = 37;

/// Function not implemented
pub const ENOSYS: i32 = 38;

/// Directory not empty
pub const ENOTEMPTY: i32 = 39;

/// Too many symbolic links encountered
pub const ELOOP: i32 = 40;

/// Operation would block
pub const EWOULDBLOCK: i32 = EAGAIN;

/// No message of desired type
pub const ENOMSG: i32 = 42;

/// Identifier removed
pub const EIDRM: i32 = 43;

/// Channel number out of range
pub const ECHRNG: i32 = 44;

/// Level 2 not synchronized
pub const EL2NSYNC: i32 = 45;

/// Level 3 halted
pub const EL3HLT: i32 = 46;

/// Level 3 reset
pub const EL3RST: i32 = 47;

/// Link number out of range
pub const ELNRNG: i32 = 48;

/// Protocol driver not attached
pub const EUNATCH: i32 = 49;

/// No CSI structure available
pub const ENOCSI: i32 = 50;

/// Level 2 halted
pub const EL2HLT: i32 = 51;

/// Invalid exchange
pub const EBADE: i32 = 52;

/// Invalid request descriptor
pub const EBADR: i32 = 53;

/// Exchange full
pub const EXFULL: i32 = 54;

/// No anode
pub const ENOANO: i32 = 55;

/// Invalid request code
pub const EBADRQC: i32 = 56;

/// Invalid slot
pub const EBADSLT: i32 = 57;

pub const EDEADLOCK: i32 = EDEADLK;

/// Bad font file format
pub const EBFONT: i32 = 59;

/// Device not a stream
pub const ENOSTR: i32 = 60;

/// No data available
pub const ENODATA: i32 = 61;

/// Timer expired
pub const ETIME: i32 = 62;

/// Out of streams resources
pub const ENOSR: i32 = 63;

/// Machine is not on the network
pub const ENONET: i32 = 64;

/// Package not installed
pub const ENOPKG: i32 = 65;

/// Object is remote
pub const EREMOTE: i32 = 66;

/// Link has been severed
pub const ENOLINK: i32 = 67;

/// Advertise error
pub const EADV: i32 = 68;

/// Srmount error
pub const ESRMNT: i32 = 69;

/// Communication error on send
pub const ECOMM: i32 = 70;

/// Protocol error
pub const EPROTO: i32 = 71;

/// Multihop attempted
pub const EMULTIHOP: i32 = 72;

/// RFS specific error
pub const EDOTDOT: i32 = 73;

/// Not a data message
pub const EBADMSG: i32 = 74;

/// Value too large for defined data type
pub const EOVERFLOW: i32 = 75;

/// Name not unique on network
pub const ENOTUNIQ: i32 = 76;

/// File descriptor in bad state
pub const EBADFD: i32 = 77;

/// Remote address changed
pub const EREMCHG: i32 = 78;

/// Can not access a needed shared library
pub const ELIBACC: i32 = 79;

/// Accessing a corrupted shared library
pub const ELIBBAD: i32 = 80;

/// .lib section in a.out corrupted
pub const ELIBSCN: i32 = 81;

/// Attempting to link in too many shared libraries
pub const ELIBMAX: i32 = 82;

/// Cannot exec a shared library directly
pub const ELIBEXEC: i32 = 83;

/// Illegal byte sequence
pub const EILSEQ: i32 = 84;

/// Interrupted system call should be restarted
pub const ERESTART: i32 = 85;

/// Streams pipe error
pub const ESTRPIPE: i32 = 86;

/// Too many users
pub const EUSERS: i32 = 87;

/// Socket operation on non-socket
pub const ENOTSOCK: i32 = 88;

/// Destination address required
pub const EDESTADDRREQ: i32 = 89;

/// Message too long
pub const EMSGSIZE: i32 = 90;

/// Protocol wrong type for socket
pub const EPROTOTYPE: i32 = 91;

/// Protocol not available
pub const ENOPROTOOPT: i32 = 92;

/// Protocol not supported
pub const EPROTONOSUPPORT: i32 = 93;

/// Socket type not supported
pub const ESOCKTNOSUPPORT: i32 = 94;

/// Operation not supported on transport endpoint
pub const EOPNOTSUPP: i32 = 95;

/// Protocol family not supported
pub const EPFNOSUPPORT: i32 = 96;

/// Address family not supported by protocol
pub const EAFNOSUPPORT: i32 = 97;

/// Address already in use
pub const EADDRINUSE: i32 = 98;

/// Cannot assign requested address
pub const EADDRNOTAVAIL: i32 = 99;

/// Network is down
pub const ENETDOWN: i32 = 100;

/// Network is unreachable
pub const ENETUNREACH: i32 = 101;

/// Network dropped connection because of reset
pub const ENETRESET: i32 = 102;

/// Software caused connection abort
pub const ECONNABORTED: i32 = 103;

/// Connection reset by peer
pub const ECONNRESET: i32 = 104;

/// No buffer space available
pub const ENOBUFS: i32 = 105;

/// Transport endpoint is already connected
pub const EISCONN: i32 = 106;

/// Transport endpoint is not connected
pub const ENOTCONN: i32 = 107;

/// Cannot send after transport endpoint shutdown
pub const ESHUTDOWN: i32 = 108;

/// Too many references: cannot splice
pub const ETOOMANYREFS: i32 = 109;

/// Connection timed out
pub const ETIMEDOUT: i32 = 110;

/// Connection refused
pub const ECONNREFUSED: i32 = 111;

/// Host is down
pub const EHOSTDOWN: i32 = 112;

/// No route to host
pub const EHOSTUNREACH: i32 = 113;

/// Operation already in progress
pub const EALREADY: i32 = 114;

/// Operation now in progress
pub const EINPROGRESS: i32 = 115;

/// Stale file handle
pub const ESTALE: i32 = 116;

/// Structure needs cleaning
pub const EUCLEAN: i32 = 117;

/// Not a XENIX named type file
pub const ENOTNAM: i32 = 118;

/// No XENIX semaphores available
pub const ENAVAIL: i32 = 119;

/// Is a named type file
pub const EISNAM: i32 = 120;

/// Remote I/O error
pub const EREMOTEIO: i32 = 121;

/// Quota exceeded
pub const EDQUOT: i32 = 122;

/// No medium found
pub const ENOMEDIUM: i32 = 123;

/// Wrong medium type
pub const EMEDIUMTYPE: i32 = 124;

/// Operation Canceled
pub const ECANCELED: i32 = 125;

/// Required key not available
pub const ENOKEY: i32 = 126;

/// Key has expired
pub const EKEYEXPIRED: i32 = 127;

/// Key has been revoked
pub const EKEYREVOKED: i32 = 128;

/// Key was rejected by service
pub const EKEYREJECTED: i32 = 129;

/// Robust mutexes: Owner died
pub const EOWNERDEAD: i32 = 130;

/// Robust mutexes: State not recoverable
pub const ENOTRECOVERABLE: i32 = 131;

/// Robust mutexes: Operation not possible due to RF-kill
pub const ERFKILL: i32 = 132;

/// Robust mutexes: Memory page has hardware error
pub const EHWPOISON: i32 = 133;

/// Converts an error number to a corresponding error string
pub fn error_string(errno: i32) -> &'static str {
	match errno {
		0 => "Operation successful",
		EPERM => "Operation not permitted",
		ENOENT => "No such file or directory",
		ESRCH => "No such process",
		EINTR => "Interrupted system call",
		EIO => "I/O error",
		ENXIO => "No such device or address",
		E2BIG => "Argument list too long",
		ENOEXEC => "Exec format error",
		EBADF => "Bad file number",
		ECHILD => "No child processes",
		EAGAIN => "Try again",
		ENOMEM => "Out of memory",
		EACCES => "Permission denied",
		EFAULT => "Bad address",
		ENOTBLK => "Block device required",
		EBUSY => "Device or resource busy",
		EEXIST => "File exists",
		EXDEV => "Cross-device link",
		ENODEV => "No such device",
		ENOTDIR => "Not a directory",
		EISDIR => "Is a directory",
		EINVAL => "Invalid argument",
		ENFILE => "File table overflow",
		EMFILE => "Too many open files",
		ENOTTY => "Not a typewriter",
		ETXTBSY => "Text file busy",
		EFBIG => "File too large",
		ENOSPC => "No space left on device",
		ESPIPE => "Illegal seek",
		EROFS => "Read-only file system",
		EMLINK => "Too many links",
		EPIPE => "Broken pipe",
		EDOM => "Math argument out of domain of func",
		ERANGE => "Math result not representable",
		EDEADLK => "Resource deadlock would occur",
		ENAMETOOLONG => "File name too long",
		ENOLCK => "No record locks available",
		ENOSYS => "Function not implemented",
		ENOTEMPTY => "Directory not empty",
		ELOOP => "Too many symbolic links encountered",
		ENOMSG => "No message of desired type",
		EIDRM => "Identifier removed",
		ECHRNG => "Channel number out of range",
		EL2NSYNC => "Level 2 not synchronized",
		EL3HLT => "Level 3 halted",
		EL3RST => "Level 3 reset",
		ELNRNG => "Link number out of range",
		EUNATCH => "Protocol driver not attached",
		ENOCSI => "No CSI structure available",
		EL2HLT => "Level 2 halted",
		EBADE => "Invalid exchange",
		EBADR => "Invalid request descriptor",
		EXFULL => "Exchange full",
		ENOANO => "No anode",
		EBADRQC => "Invalid request code",
		EBADSLT => "Invalid slot",
		EBFONT => "Bad font file format",
		ENOSTR => "Device not a stream",
		ENODATA => "No data available",
		ETIME => "Timer expired",
		ENOSR => "Out of streams resources",
		ENONET => "Machine is not on the network",
		ENOPKG => "Package not installed",
		EREMOTE => "Object is remote",
		ENOLINK => "Link has been severed",
		EADV => "Advertise error",
		ESRMNT => "Srmount error",
		ECOMM => "Communication error on send",
		EPROTO => "Protocol error",
		EMULTIHOP => "Multihop attempted",
		EDOTDOT => "RFS specific error",
		EBADMSG => "Not a data message",
		EOVERFLOW => "Value too large for defined data type",
		ENOTUNIQ => "Name not unique on network",
		EBADFD => "File descriptor in bad state",
		EREMCHG => "Remote address changed",
		ELIBACC => "Can not access a needed shared library",
		ELIBBAD => "Accessing a corrupted shared library",
		ELIBSCN => "Lib section in a.out corrupted",
		ELIBMAX => "Attempting to link in too many shared libraries",
		ELIBEXEC => "Cannot exec a shared library directly",
		EILSEQ => "Illegal byte sequence",
		ERESTART => "Interrupted system call should be restarted",
		ESTRPIPE => "Streams pipe error",
		EUSERS => "Too many users",
		ENOTSOCK => "Socket operation on non-socket",
		EDESTADDRREQ => "Destination address required",
		EMSGSIZE => "Message too long",
		EPROTOTYPE => "Protocol wrong type for socket",
		ENOPROTOOPT => "Protocol not available",
		EPROTONOSUPPORT => "Protocol not supported",
		ESOCKTNOSUPPORT => "Socket type not supported",
		EOPNOTSUPP => "Operation not supported on transport endpoint",
		EPFNOSUPPORT => "Protocol family not supported",
		EAFNOSUPPORT => "Address family not supported by protocol",
		EADDRINUSE => "Address already in use",
		EADDRNOTAVAIL => "Cannot assign requested address",
		ENETDOWN => "Network is down",
		ENETUNREACH => "Network is unreachable",
		ENETRESET => "Network dropped connection because of reset",
		ECONNABORTED => "Software caused connection abort",
		ECONNRESET => "Connection reset by peer",
		ENOBUFS => "No buffer space available",
		EISCONN => "Transport endpoint is already connected",
		ENOTCONN => "Transport endpoint is not connected",
		ESHUTDOWN => "Cannot send after transport endpoint shutdown",
		ETOOMANYREFS => "Too many references: cannot splice",
		ETIMEDOUT => "Connection timed out",
		ECONNREFUSED => "Connection refused",
		EHOSTDOWN => "Host is down",
		EHOSTUNREACH => "No route to host",
		EALREADY => "Operation already in progress",
		EINPROGRESS => "Operation now in progress",
		ESTALE => "Stale file handle",
		EUCLEAN => "Structure needs cleaning",
		EDQUOT => "Quota exceeded",
		ENOMEDIUM => "No medium found",
		EMEDIUMTYPE => "Wrong medium type",
		ECANCELED => "Operation Canceled",
		ENOKEY => "Required key not available",
		EKEYEXPIRED => "Key has expired",
		EKEYREVOKED => "Key has been revoked",
		EKEYREJECTED => "Key was rejected by service",
		EOWNERDEAD => "Robust mutexes: Owner died",
		ENOTRECOVERABLE => "Robust mutexes: State not recoverable",
		ERFKILL => "Robust mutexes: Operation not possible due to RF-kill",
		EHWPOISON => "Robust mutexes: Memory page has hardware error",
		_ => "Unknown error",
	}
}
