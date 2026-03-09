use crate::ascii::*;
use crate::STANDARD_FORGIVING;
use crate::{Error, Out};

use vsimd::tools::slice_mut;

use core::ptr::copy_nonoverlapping;

#[cfg(feature = "alloc")]
use alloc::vec::Vec;

/// Forgiving decodes a base64 string to bytes and writes inplace.
///
/// This function uses the standard charset.
///
/// See <https://infra.spec.whatwg.org/#forgiving-base64>
///
/// # Errors
/// This function returns `Err` if the content of `data` is invalid.
#[inline]
pub fn forgiving_decode_inplace(data: &mut [u8]) -> Result<&mut [u8], Error> {
    let data = remove_ascii_whitespace_inplace(data);
    STANDARD_FORGIVING.decode_inplace(data)
}

/// Forgiving decodes a base64 string to bytes.
///
/// This function uses the standard charset.
///
/// See <https://infra.spec.whatwg.org/#forgiving-base64>
///
/// # Errors
/// This function returns `Err` if the content of `src` is invalid.
///
/// # Panics
/// This function asserts that `src.len() <= dst.len()`
#[inline]
pub fn forgiving_decode<'d>(src: &[u8], mut dst: Out<'d, [u8]>) -> Result<&'d mut [u8], Error> {
    assert!(src.len() <= dst.len());

    let pos = find_non_ascii_whitespace(src);
    debug_assert!(pos <= src.len());

    if pos == src.len() {
        return STANDARD_FORGIVING.decode(src, dst);
    }

    unsafe {
        let len = src.len();
        let src = src.as_ptr();
        let dst = dst.as_mut_ptr();

        copy_nonoverlapping(src, dst, pos);

        let rem = remove_ascii_whitespace_fallback(src.add(pos), len - pos, dst.add(pos));
        debug_assert!(rem <= len - pos);

        let data = slice_mut(dst, pos + rem);
        STANDARD_FORGIVING.decode_inplace(data)
    }
}

/// Forgiving decodes a base64 string to bytes and returns a new [`Vec<u8>`](Vec).
///
/// This function uses the standard charset.
///
/// See <https://infra.spec.whatwg.org/#forgiving-base64>
///
/// # Errors
/// This function returns `Err` if the content of `data` is invalid.
#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
#[cfg(feature = "alloc")]
#[inline]
pub fn forgiving_decode_to_vec(data: &[u8]) -> Result<Vec<u8>, Error> {
    let pos = find_non_ascii_whitespace(data);
    debug_assert!(pos <= data.len());

    if pos == data.len() {
        return STANDARD_FORGIVING.decode_type::<Vec<u8>>(data);
    }

    let mut vec = Vec::with_capacity(data.len());

    unsafe {
        let len = data.len();
        let src = data.as_ptr();
        let dst = vec.as_mut_ptr();

        copy_nonoverlapping(src, dst, pos);

        let rem = remove_ascii_whitespace_fallback(src.add(pos), len - pos, dst.add(pos));
        debug_assert!(rem <= len - pos);

        let data = slice_mut(dst, pos + rem);
        let ans_len = STANDARD_FORGIVING.decode_inplace(data)?.len();

        vec.set_len(ans_len);
    };

    Ok(vec)
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::AsOut;

    #[cfg_attr(not(target_arch = "wasm32"), test)]
    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test::wasm_bindgen_test)]
    fn test_forgiving() {
        use const_str::hex;

        let mut inputs: Vec<&str> = Vec::new();
        let mut outputs: Vec<&[u8]> = Vec::new();

        {
            let mut i = |i| inputs.push(i);
            let mut o = |o| outputs.push(o);

            i("ab");
            o(&[0x69]);

            i("abc");
            o(&[0x69, 0xB7]);

            i("abcd");
            o(&[0x69, 0xB7, 0x1D]);

            i("helloworld");
            o(&hex!("85 E9 65 A3 0A 2B 95"));

            i(" h e l l o w o r\nl\rd\t");
            o(&hex!("85 E9 65 A3 0A 2B 95"));
        }

        for i in 0..inputs.len() {
            let (src, expected) = (inputs[i], outputs[i]);

            let mut buf = src.to_owned().into_bytes();

            let ans = forgiving_decode_inplace(&mut buf).unwrap();
            assert_eq!(ans, expected);

            let ans = crate::forgiving_decode(src.as_bytes(), buf.as_out()).unwrap();
            assert_eq!(ans, expected);

            #[cfg(feature = "alloc")]
            {
                let ans = crate::forgiving_decode_to_vec(src.as_bytes()).unwrap();
                assert_eq!(ans, expected);
            }
        }
    }
}
