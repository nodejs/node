//! Header: `stdio.h`

use super::*;
use crate::prelude::*;

// Buffer size
pub const BUFSIZ: c_uint = 1024;
pub const FILENAME_MAX: c_uint = 260;

// End of file
pub const EOF: c_int = -1;

extern "C" {
    // File operations
    pub fn fopen(filename: *const c_char, mode: *const c_char) -> *mut FILE;
    pub fn freopen(filename: *const c_char, mode: *const c_char, stream: *mut FILE) -> *mut FILE;
    pub fn fclose(stream: *mut FILE) -> c_int;
    pub fn fflush(stream: *mut FILE) -> c_int;
    pub fn fread(ptr: *mut c_void, size: size_t, nmemb: size_t, stream: *mut FILE) -> size_t;
    pub fn fwrite(ptr: *const c_void, size: size_t, nmemb: size_t, stream: *mut FILE) -> size_t;

    // Character I/O
    pub fn fgetc(stream: *mut FILE) -> c_int;
    pub fn fputc(c: c_int, stream: *mut FILE) -> c_int;
    pub fn getchar() -> c_int;
    pub fn putchar(c: c_int) -> c_int;
    pub fn ungetc(c: c_int, stream: *mut FILE) -> c_int;

    // Line I/O
    pub fn fgets(s: *mut c_char, size: c_int, stream: *mut FILE) -> *mut c_char;
    pub fn fputs(s: *const c_char, stream: *mut FILE) -> c_int;
    pub fn gets(s: *mut c_char) -> *mut c_char;
    pub fn puts(s: *const c_char) -> c_int;

    // Formatted I/O
    pub fn printf(format: *const c_char, ...) -> c_int;
    pub fn fprintf(stream: *mut FILE, format: *const c_char, ...) -> c_int;
    pub fn sprintf(s: *mut c_char, format: *const c_char, ...) -> c_int;
    pub fn snprintf(s: *mut c_char, n: size_t, format: *const c_char, ...) -> c_int;
    pub fn vprintf(format: *const c_char, ap: crate::va_list) -> c_int;
    pub fn vfprintf(stream: *mut FILE, format: *const c_char, ap: crate::va_list) -> c_int;
    pub fn vsprintf(s: *mut c_char, format: *const c_char, ap: crate::va_list) -> c_int;
    pub fn vsnprintf(s: *mut c_char, n: size_t, format: *const c_char, ap: crate::va_list)
        -> c_int;

    // Input formatted functions
    pub fn scanf(format: *const c_char, ...) -> c_int;
    pub fn fscanf(stream: *mut FILE, format: *const c_char, ...) -> c_int;
    pub fn sscanf(s: *const c_char, format: *const c_char, ...) -> c_int;

    // File positioning
    pub fn fseek(stream: *mut FILE, offset: c_long, whence: c_int) -> c_int;
    pub fn ftell(stream: *mut FILE) -> c_long;
    pub fn rewind(stream: *mut FILE);
    pub fn fgetpos(stream: *mut FILE, pos: *mut fpos_t) -> c_int;
    pub fn fsetpos(stream: *mut FILE, pos: *const fpos_t) -> c_int;

    // Error handling
    pub fn clearerr(stream: *mut FILE);
    pub fn feof(stream: *mut FILE) -> c_int;
    pub fn ferror(stream: *mut FILE) -> c_int;
    pub fn perror(s: *const c_char);

    // File management
    pub fn remove(filename: *const c_char) -> c_int;
    pub fn rename(old: *const c_char, new: *const c_char) -> c_int;
    pub fn tmpfile() -> *mut FILE;
    pub fn tmpnam(s: *mut c_char) -> *mut c_char;

    // Buffer control
    pub fn setvbuf(stream: *mut FILE, buffer: *mut c_char, mode: c_int, size: size_t) -> c_int;
    pub fn setbuf(stream: *mut FILE, buffer: *mut c_char);
}
