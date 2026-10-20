use crate::{QUOTED_PAIR_TABLE, QUOTED_STRING_TABLE, TOKEN_TABLE, URL_TABLE};

pub enum MatchResult {
  Continue,
  Suspend,
  Stop,
}

pub enum HeaderLineScanResult {
  Cr(usize),
  Invalid(usize),
  Incomplete,
}

#[inline(always)]
pub fn ensure_valid_line(data: &[u8], cr: usize, available: usize) -> MatchResult {
  if cr + 1 == available {
    MatchResult::Suspend
  } else if data[cr + 1] != b'\n' {
    MatchResult::Stop
  } else {
    MatchResult::Continue
  }
}

#[inline(always)]
pub fn find_cr(data: &[u8], available: usize) -> Option<usize> {
  if available == 0 {
    None
  } else {
    find_char(data, 0, available - 1, b'\r')
  }
}

#[inline(always)]
pub fn find_char(buf: &[u8], start: usize, end: usize, needle: u8) -> Option<usize> {
  if start > end || end >= buf.len() {
    return None;
  }

  memchr::memchr(needle, &buf[start..=end]).map(|i| start + i)
}

#[inline(always)]
pub fn find_char2(buf: &[u8], start: usize, end: usize, needle1: u8, needle2: u8) -> Option<usize> {
  if start > end || end >= buf.len() {
    return None;
  }

  memchr::memchr2(needle1, needle2, &buf[start..=end]).map(|i| start + i)
}

#[inline(always)]
pub fn is_digit(byte: u8) -> bool { byte.wrapping_sub(b'0') <= 9 }

#[inline(always)]
pub fn is_ws(byte: u8) -> bool { byte == b' ' || byte == b'\t' }

#[inline(always)]
pub fn validate_token(data: &[u8], start: usize, end: usize) -> bool {
  if start == end {
    return false;
  }

  let mut i = start;
  while i < end {
    if !TOKEN_TABLE[data[i] as usize] {
      return false;
    }

    i += 1;
  }

  true
}

#[inline(always)]
pub fn validate_quoted_string(data: &[u8], start: usize, end: usize) -> bool {
  let mut i = start;
  while i < end {
    let byte = data[i];
    if byte == b'\\' {
      i += 1;
      if i == end || !QUOTED_PAIR_TABLE[data[i] as usize] {
        return false;
      }
    } else if !QUOTED_STRING_TABLE[byte as usize] {
      return false;
    }

    i += 1;
  }

  true
}

#[inline(always)]
pub fn validate_url(data: &[u8], start: usize, end: usize) -> bool {
  if start == end {
    return false;
  }

  let mut i = start;
  while i < end {
    if !URL_TABLE[data[i] as usize] {
      return false;
    }

    i += 1;
  }

  true
}

#[inline(always)]
pub fn strip_ows_fast(data: &[u8], start_ref: &mut usize, end_ref: &mut usize, allow_empty: bool) -> bool {
  let start = *start_ref;
  let end = *end_ref;

  // The single-space shortcut is valid only when no further leading OWS follows.
  if start + 1 < end && data[start] == b' ' && !is_ws(data[start + 1]) && !is_ws(data[end - 1]) {
    *start_ref = start + 1;
    return true;
  }

  strip_ows(data, start_ref, end_ref, allow_empty)
}

#[inline(always)]
pub fn strip_ows(data: &[u8], start_ref: &mut usize, end_ref: &mut usize, allow_empty: bool) -> bool {
  let mut start = *start_ref;
  let mut end = *end_ref;

  while start < end && is_ws(data[start]) {
    start += 1;
  }

  while end > start && is_ws(data[end - 1]) {
    end -= 1;
  }

  if start == end && !allow_empty {
    return false;
  }

  *start_ref = start;
  *end_ref = end;
  true
}

#[inline(always)]
fn scan_header_line_scalar(ptr: *const u8, mut i: usize, end: usize) -> HeaderLineScanResult {
  while i < end {
    let byte = unsafe { *ptr.add(i) };

    if byte == b'\r' {
      return HeaderLineScanResult::Cr(i);
    }

    if (byte < 0x20 && byte != b'\t') || byte == 0x7f {
      return HeaderLineScanResult::Invalid(i);
    }

    i += 1;
  }

  HeaderLineScanResult::Incomplete
}

#[cfg(target_arch = "aarch64")]
#[inline(always)]
pub fn find_header_line_end(ptr: *const u8, len: usize) -> HeaderLineScanResult {
  use core::arch::aarch64::*;

  if len < 16 {
    return scan_header_line_scalar(ptr, 0, len);
  }

  let mut i = 0;
  let v_20 = unsafe { vdupq_n_u8(0x20) };
  let v_7f = unsafe { vdupq_n_u8(0x7f) };
  let v_tab = unsafe { vdupq_n_u8(b'\t') };
  let v_cr = unsafe { vdupq_n_u8(b'\r') };

  while i + 16 <= len {
    let x = unsafe { vld1q_u8(ptr.add(i)) };
    let lt_20 = unsafe { vcltq_u8(x, v_20) };
    let eq_tab = unsafe { vceqq_u8(x, v_tab) };
    let eq_cr = unsafe { vceqq_u8(x, v_cr) };
    let eq_7f = unsafe { vceqq_u8(x, v_7f) };

    // Header lines stop at CR; other control bytes are invalid except HTAB.
    let ctrl = unsafe { vbicq_u8(lt_20, eq_tab) };
    let invalid = unsafe { vbicq_u8(vorrq_u8(ctrl, eq_7f), eq_cr) };
    let found = unsafe { vorrq_u8(eq_cr, invalid) };

    if unsafe { vmaxvq_u8(found) } != 0 {
      return scan_header_line_scalar(ptr, i, i + 16);
    }

    i += 16;
  }

  scan_header_line_scalar(ptr, i, len)
}

#[cfg(target_arch = "x86_64")]
#[inline(always)]
pub fn find_header_line_end(ptr: *const u8, len: usize) -> HeaderLineScanResult {
  use core::arch::x86_64::*;

  if len < 16 {
    return scan_header_line_scalar(ptr, 0, len);
  }

  let mut i = 0;
  let v_20 = unsafe { _mm_set1_epi8(0x20u8 as i8) };
  let v_7f = unsafe { _mm_set1_epi8(0x7fu8 as i8) };
  let v_tab = unsafe { _mm_set1_epi8(b'\t' as i8) };
  let v_cr = unsafe { _mm_set1_epi8(b'\r' as i8) };
  let v_zero = unsafe { _mm_setzero_si128() };
  let v_ones = unsafe { _mm_cmpeq_epi8(v_zero, v_zero) };

  while i + 32 <= len {
    let x = unsafe { _mm_loadu_si128(ptr.add(i) as *const __m128i) };
    let lt_20 = unsafe { _mm_andnot_si128(_mm_cmpeq_epi8(_mm_subs_epu8(v_20, x), v_zero), v_ones) };
    let eq_tab = unsafe { _mm_cmpeq_epi8(x, v_tab) };
    let eq_cr = unsafe { _mm_cmpeq_epi8(x, v_cr) };
    let eq_7f = unsafe { _mm_cmpeq_epi8(x, v_7f) };

    // Header lines stop at CR; other control bytes are invalid except HTAB.
    let ctrl = unsafe { _mm_andnot_si128(eq_tab, lt_20) };
    let invalid = unsafe { _mm_andnot_si128(eq_cr, _mm_or_si128(ctrl, eq_7f)) };
    let cr_mask = unsafe { _mm_movemask_epi8(eq_cr) };
    let invalid_mask = unsafe { _mm_movemask_epi8(invalid) };
    let found_mask = cr_mask | invalid_mask;

    if found_mask != 0 {
      let index = i + found_mask.trailing_zeros() as usize;
      return if cr_mask & (1 << (index - i)) != 0 {
        HeaderLineScanResult::Cr(index)
      } else {
        HeaderLineScanResult::Invalid(index)
      };
    }

    let x = unsafe { _mm_loadu_si128(ptr.add(i + 16) as *const __m128i) };
    let lt_20 = unsafe { _mm_andnot_si128(_mm_cmpeq_epi8(_mm_subs_epu8(v_20, x), v_zero), v_ones) };
    let eq_tab = unsafe { _mm_cmpeq_epi8(x, v_tab) };
    let eq_cr = unsafe { _mm_cmpeq_epi8(x, v_cr) };
    let eq_7f = unsafe { _mm_cmpeq_epi8(x, v_7f) };
    let ctrl = unsafe { _mm_andnot_si128(eq_tab, lt_20) };
    let invalid = unsafe { _mm_andnot_si128(eq_cr, _mm_or_si128(ctrl, eq_7f)) };
    let cr_mask = unsafe { _mm_movemask_epi8(eq_cr) };
    let invalid_mask = unsafe { _mm_movemask_epi8(invalid) };
    let found_mask = cr_mask | invalid_mask;

    if found_mask != 0 {
      let lane = found_mask.trailing_zeros() as usize;
      let index = i + 16 + lane;
      return if cr_mask & (1 << lane) != 0 {
        HeaderLineScanResult::Cr(index)
      } else {
        HeaderLineScanResult::Invalid(index)
      };
    }

    i += 32;
  }

  while i + 16 <= len {
    let x = unsafe { _mm_loadu_si128(ptr.add(i) as *const __m128i) };
    let lt_20 = unsafe { _mm_andnot_si128(_mm_cmpeq_epi8(_mm_subs_epu8(v_20, x), v_zero), v_ones) };
    let eq_tab = unsafe { _mm_cmpeq_epi8(x, v_tab) };
    let eq_cr = unsafe { _mm_cmpeq_epi8(x, v_cr) };
    let eq_7f = unsafe { _mm_cmpeq_epi8(x, v_7f) };
    let ctrl = unsafe { _mm_andnot_si128(eq_tab, lt_20) };
    let invalid = unsafe { _mm_andnot_si128(eq_cr, _mm_or_si128(ctrl, eq_7f)) };
    let cr_mask = unsafe { _mm_movemask_epi8(eq_cr) };
    let invalid_mask = unsafe { _mm_movemask_epi8(invalid) };
    let found_mask = cr_mask | invalid_mask;

    if found_mask != 0 {
      let lane = found_mask.trailing_zeros() as usize;
      let index = i + lane;
      return if cr_mask & (1 << lane) != 0 {
        HeaderLineScanResult::Cr(index)
      } else {
        HeaderLineScanResult::Invalid(index)
      };
    }

    i += 16;
  }

  scan_header_line_scalar(ptr, i, len)
}

#[cfg(all(target_arch = "wasm32", target_feature = "simd128"))]
#[target_feature(enable = "simd128")]
pub fn find_header_line_end(ptr: *const u8, len: usize) -> HeaderLineScanResult {
  use core::arch::wasm32::*;

  if len < 16 {
    return scan_header_line_scalar(ptr, 0, len);
  }

  let mut i = 0;
  let v_20 = u8x16_splat(0x20);
  let v_7f = u8x16_splat(0x7f);
  let v_tab = u8x16_splat(b'\t');
  let v_cr = u8x16_splat(b'\r');

  while i + 16 <= len {
    let x = unsafe { v128_load(ptr.add(i) as *const v128) };
    let lt_20 = u8x16_lt(x, v_20);
    let eq_tab = u8x16_eq(x, v_tab);
    let eq_cr = u8x16_eq(x, v_cr);
    let eq_7f = u8x16_eq(x, v_7f);

    // Header lines stop at CR; other control bytes are invalid except HTAB.
    // WASM andnot(a, b) computes a & !b, unlike the x86 intrinsic.
    let ctrl = v128_andnot(lt_20, eq_tab);
    let invalid = v128_andnot(v128_or(ctrl, eq_7f), eq_cr);
    let found = v128_or(eq_cr, invalid);

    if v128_any_true(found) {
      return scan_header_line_scalar(ptr, i, i + 16);
    }

    i += 16;
  }

  scan_header_line_scalar(ptr, i, len)
}

#[cfg(not(any(
  target_arch = "aarch64",
  target_arch = "x86_64",
  all(target_arch = "wasm32", target_feature = "simd128")
)))]
#[inline(always)]
pub fn find_header_line_end(ptr: *const u8, len: usize) -> HeaderLineScanResult { scan_header_line_scalar(ptr, 0, len) }

#[cfg(target_arch = "aarch64")]
#[target_feature(enable = "neon")]
pub fn validate_token_value(ptr: *const u8, len: usize) -> bool {
  use core::arch::aarch64::*;

  let mut i = 0;

  let v_20 = vdupq_n_u8(0x20);
  let v_7f = vdupq_n_u8(0x7f);
  let v_tab = vdupq_n_u8(9);

  while i + 16 <= len {
    let x = unsafe { vld1q_u8(ptr.add(i)) };
    let lt_20 = vcltq_u8(x, v_20);
    let eq_tab = vceqq_u8(x, v_tab);
    let eq_7f = vceqq_u8(x, v_7f);

    // Field values allow HTAB but reject the remaining C0 controls and DEL.
    let ctrl = vbicq_u8(lt_20, eq_tab); // lt_20 & !eq_tab
    let invalid = vorrq_u8(ctrl, eq_7f);

    if vmaxvq_u8(invalid) != 0 {
      return false;
    }

    i += 16;
  }

  while i < len {
    let b = unsafe { *ptr.add(i) };

    if (b < 0x20 && b != b'\t') || b == 0x7f {
      return false;
    }

    i += 1;
  }

  true
}

#[cfg(target_arch = "x86_64")]
#[target_feature(enable = "sse2")]
pub fn validate_token_value(ptr: *const u8, len: usize) -> bool {
  use core::arch::x86_64::*;

  let mut i = 0;
  let v_20 = _mm_set1_epi8(0x20u8 as i8);
  let v_7f = _mm_set1_epi8(0x7fu8 as i8);
  let v_tab = _mm_set1_epi8(b'\t' as i8);
  let v_zero = _mm_setzero_si128();
  let v_ones = _mm_cmpeq_epi8(v_zero, v_zero);

  while i + 16 <= len {
    let x = unsafe { _mm_loadu_si128(ptr.add(i) as *const __m128i) };
    let lt_20 = _mm_andnot_si128(_mm_cmpeq_epi8(_mm_subs_epu8(v_20, x), v_zero), v_ones);
    let eq_tab = _mm_cmpeq_epi8(x, v_tab);
    let eq_7f = _mm_cmpeq_epi8(x, v_7f);

    // Field values allow HTAB but reject the remaining C0 controls and DEL.
    let ctrl = _mm_andnot_si128(eq_tab, lt_20);
    let invalid = _mm_or_si128(ctrl, eq_7f);

    if _mm_movemask_epi8(invalid) != 0 {
      return false;
    }

    i += 16;
  }

  while i < len {
    let b = unsafe { *ptr.add(i) };

    if (b < 0x20 && b != b'\t') || b == 0x7f {
      return false;
    }

    i += 1;
  }

  true
}

#[cfg(all(target_arch = "wasm32", target_feature = "simd128"))]
#[target_feature(enable = "simd128")]
pub fn validate_token_value(ptr: *const u8, len: usize) -> bool {
  use core::arch::wasm32::*;

  let mut i = 0;
  let v_20 = u8x16_splat(0x20);
  let v_7f = u8x16_splat(0x7f);
  let v_tab = u8x16_splat(b'\t');

  while i + 16 <= len {
    let x = unsafe { v128_load(ptr.add(i) as *const v128) };
    let lt_20 = u8x16_lt(x, v_20);
    let eq_tab = u8x16_eq(x, v_tab);
    let eq_7f = u8x16_eq(x, v_7f);

    // Field values allow HTAB but reject the remaining C0 controls and DEL.
    // WASM andnot(a, b) computes a & !b, unlike the x86 intrinsic.
    let ctrl = v128_andnot(lt_20, eq_tab);
    let invalid = v128_or(ctrl, eq_7f);

    if v128_any_true(invalid) {
      return false;
    }

    i += 16;
  }

  while i < len {
    let b = unsafe { *ptr.add(i) };

    if (b < 0x20 && b != b'\t') || b == 0x7f {
      return false;
    }

    i += 1;
  }

  true
}

#[cfg(not(any(
  target_arch = "aarch64",
  target_arch = "x86_64",
  all(target_arch = "wasm32", target_feature = "simd128")
)))]
#[inline(always)]
pub fn validate_token_value(ptr: *const u8, len: usize) -> bool {
  let mut i = 0;

  while i < len {
    let b = unsafe { *ptr.add(i) };

    if (b < 0x20 && b != b'\t') || b == 0x7f {
      return false;
    }

    i += 1;
  }

  true
}
