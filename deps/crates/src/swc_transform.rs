// C FFI bindings for swc_ts_fast_strip – exposes a simple
// swc_transform(source, filename, mode) → {code, error} API
// that tools/js2c.cc uses to transpile .ts files at build time.

use std::ffi::{c_char, c_int};
use std::ptr;

use swc_common::errors::Handler;
use swc_common::sync::Lrc;
use swc_common::{SourceMap, GLOBALS};

/// Result of a transform operation.  Exactly one of `code` / `error` will be
/// non-null.  The caller **must** call `swc_transform_free_result()` to
/// release memory.
#[repr(C)]
pub struct SwcTransformResult {
    pub code: *mut c_char,
    pub error: *mut c_char,
    pub code_len: usize,
    pub error_len: usize,
}

fn make_ok(output: String) -> SwcTransformResult {
    let len = output.len();
    let ptr = output.into_bytes();
    let mut boxed = ptr.into_boxed_slice();
    let raw = boxed.as_mut_ptr() as *mut c_char;
    std::mem::forget(boxed);
    SwcTransformResult {
        code: raw,
        error: ptr::null_mut(),
        code_len: len,
        error_len: 0,
    }
}

fn make_err(msg: String) -> SwcTransformResult {
    let len = msg.len();
    let ptr = msg.into_bytes();
    let mut boxed = ptr.into_boxed_slice();
    let raw = boxed.as_mut_ptr() as *mut c_char;
    std::mem::forget(boxed);
    SwcTransformResult {
        code: ptr::null_mut(),
        error: raw,
        code_len: 0,
        error_len: len,
    }
}

/// Transpile TypeScript → JavaScript using SWC type-stripping.
///
/// # Safety
/// `source` and `filename` must be valid UTF-8 pointers of the given lengths.
#[no_mangle]
pub unsafe extern "C" fn swc_transform(
    source: *const c_char,
    source_len: usize,
    filename: *const c_char,
    filename_len: usize,
    mode: c_int,
) -> SwcTransformResult {
    let src = match std::str::from_utf8(std::slice::from_raw_parts(
        source as *const u8,
        source_len,
    )) {
        Ok(s) => s.to_owned(),
        Err(e) => return make_err(format!("invalid UTF-8 source: {e}")),
    };

    let fname = if filename.is_null() || filename_len == 0 {
        None
    } else {
        match std::str::from_utf8(std::slice::from_raw_parts(
            filename as *const u8,
            filename_len,
        )) {
            Ok(s) => Some(s.to_owned()),
            Err(_) => None,
        }
    };

    let swc_mode = match mode {
        1 => swc_ts_fast_strip::Mode::Transform,
        _ => swc_ts_fast_strip::Mode::StripOnly,
    };

    let options = swc_ts_fast_strip::Options {
        filename: fname,
        mode: swc_mode,
        source_map: false,
        ..Default::default()
    };

    GLOBALS.set(&swc_common::Globals::new(), || {
        let cm: Lrc<SourceMap> = Lrc::default();
        // Send diagnostics to sink – we rely on the Result for error info.
        let handler = Handler::with_emitter_writer(
            Box::new(std::io::sink()),
            None,
        );

        let result = swc_common::errors::HANDLER.set(&handler, || {
            swc_ts_fast_strip::operate(&cm, &handler, src, options)
        });

        match result {
            Ok(output) => make_ok(output.code),
            Err(e) => make_err(format!("{e:?}")),
        }
    })
}

/// Free the buffers inside a `SwcTransformResult`.
///
/// # Safety
/// Must be called exactly once per result returned by `swc_transform()`.
#[no_mangle]
pub unsafe extern "C" fn swc_transform_free_result(result: *mut SwcTransformResult) {
    if result.is_null() {
        return;
    }
    let r = &mut *result;
    if !r.code.is_null() {
        drop(Vec::from_raw_parts(
            r.code as *mut u8,
            r.code_len,
            r.code_len,
        ));
        r.code = ptr::null_mut();
    }
    if !r.error.is_null() {
        drop(Vec::from_raw_parts(
            r.error as *mut u8,
            r.error_len,
            r.error_len,
        ));
        r.error = ptr::null_mut();
    }
}
