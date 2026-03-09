//! Header: `unistd.h`
//!
//! <https://pubs.opengroup.org/onlinepubs/007904975/basedefs/unistd.h.html>

use crate::prelude::*;

pub const STDIN_FILENO: c_int = 0;
pub const STDOUT_FILENO: c_int = 1;
pub const STDERR_FILENO: c_int = 2;
