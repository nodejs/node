use crate::decode::decoded_length;
use crate::encode::encoded_length_unchecked;
use crate::{AppendBase64Decode, AppendBase64Encode};
use crate::{Base64, Error};
use crate::{FromBase64Decode, FromBase64Encode};

use vsimd::tools::{alloc_uninit_bytes, assume_init, boxed_str, slice_parts};

use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;

#[inline]
fn encode_to_boxed_str(base64: &Base64, data: &[u8]) -> Box<str> {
    if data.is_empty() {
        return Box::from("");
    }

    unsafe {
        let m = encoded_length_unchecked(data.len(), base64.config);
        assert!(m <= usize::MAX / 2);

        let mut buf = alloc_uninit_bytes(m);

        {
            let (src, len) = slice_parts(data);
            let dst: *mut u8 = buf.as_mut_ptr().cast();
            crate::multiversion::encode::auto(src, len, dst, base64.config);
        }

        boxed_str(assume_init(buf))
    }
}

#[inline]
fn encode_append_vec(base64: &Base64, src: &[u8], buf: &mut Vec<u8>) {
    if src.is_empty() {
        return;
    }

    unsafe {
        let m = encoded_length_unchecked(src.len(), base64.config);
        assert!(m <= usize::MAX / 2);

        buf.reserve_exact(m);
        let prev_len = buf.len();

        {
            let (src, len) = slice_parts(src);
            let dst = buf.as_mut_ptr().add(prev_len);
            crate::multiversion::encode::auto(src, len, dst, base64.config);
        }

        buf.set_len(prev_len + m);
    }
}

#[inline]
fn decode_to_boxed_bytes(base64: &Base64, data: &[u8]) -> Result<Box<[u8]>, Error> {
    if data.is_empty() {
        return Ok(Box::from([]));
    }

    unsafe {
        let (n, m) = decoded_length(data, base64.config)?;

        // safety: 0 < m < isize::MAX
        let mut buf = alloc_uninit_bytes(m);

        {
            let dst = buf.as_mut_ptr().cast();
            let src = data.as_ptr();
            crate::multiversion::decode::auto(src, dst, n, base64.config)?;
        }

        Ok(assume_init(buf))
    }
}

#[inline]
fn decode_append_vec(base64: &Base64, src: &[u8], buf: &mut Vec<u8>) -> Result<(), Error> {
    if src.is_empty() {
        return Ok(());
    }

    unsafe {
        let (n, m) = decoded_length(src, base64.config)?;

        buf.reserve_exact(m);
        let prev_len = buf.len();

        let dst = buf.as_mut_ptr().add(prev_len);
        let src = src.as_ptr();
        crate::multiversion::decode::auto(src, dst, n, base64.config)?;

        buf.set_len(prev_len + m);
        Ok(())
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl FromBase64Decode for Box<[u8]> {
    #[inline]
    fn from_base64_decode(base64: &Base64, data: &[u8]) -> Result<Self, Error> {
        decode_to_boxed_bytes(base64, data)
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl FromBase64Decode for Vec<u8> {
    #[inline]
    fn from_base64_decode(base64: &Base64, data: &[u8]) -> Result<Self, crate::Error> {
        let ans = decode_to_boxed_bytes(base64, data)?;
        Ok(Vec::from(ans))
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl FromBase64Encode for Box<[u8]> {
    #[inline]
    fn from_base64_encode(base64: &Base64, data: &[u8]) -> Self {
        let ans = encode_to_boxed_str(base64, data);
        ans.into_boxed_bytes()
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl FromBase64Encode for Box<str> {
    #[inline]
    fn from_base64_encode(base64: &Base64, data: &[u8]) -> Self {
        encode_to_boxed_str(base64, data)
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl FromBase64Encode for Vec<u8> {
    #[inline]
    fn from_base64_encode(base64: &Base64, data: &[u8]) -> Self {
        let ans = encode_to_boxed_str(base64, data);
        Vec::from(ans.into_boxed_bytes())
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl FromBase64Encode for String {
    #[inline]
    fn from_base64_encode(base64: &Base64, data: &[u8]) -> Self {
        let ans = encode_to_boxed_str(base64, data);
        String::from(ans)
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl AppendBase64Encode for Vec<u8> {
    #[inline]
    fn append_base64_encode(base64: &Base64, src: &[u8], dst: &mut Self) {
        encode_append_vec(base64, src, dst);
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl AppendBase64Encode for String {
    #[inline]
    fn append_base64_encode(base64: &Base64, src: &[u8], dst: &mut Self) {
        unsafe { encode_append_vec(base64, src, dst.as_mut_vec()) };
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
impl AppendBase64Decode for Vec<u8> {
    #[inline]
    fn append_base64_decode(base64: &Base64, src: &[u8], dst: &mut Self) -> Result<(), Error> {
        decode_append_vec(base64, src, dst)
    }
}
