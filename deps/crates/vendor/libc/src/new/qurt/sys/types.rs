//! Header: `sys/types.h`
//!
//! Note: Basic types are defined at the crate root level for qurt.
//! This module provides sys/types.h specific functions and constants.

use super::super::*;
use crate::prelude::*;

pub type cpu_set_t = c_uint;
