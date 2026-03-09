use base64_simd::{AsOut, Base64};
use base64_simd::{STANDARD, STANDARD_NO_PAD, URL_SAFE, URL_SAFE_NO_PAD};

fn rand_bytes(n: usize) -> Vec<u8> {
    use rand::RngCore;
    let mut bytes = vec![0u8; n];
    rand::thread_rng().fill_bytes(&mut bytes);
    bytes
}

#[cfg(miri)]
use std::io::Write as _;

macro_rules! dbgmsg {
    ($($fmt:tt)*) => {
        // println!($($fmt)*);
        // #[cfg(miri)]
        // std::io::stdout().flush().unwrap();
    };
}

#[cfg_attr(not(target_arch = "wasm32"), test)]
#[cfg_attr(target_arch = "wasm32", wasm_bindgen_test::wasm_bindgen_test)]
fn basic() {
    let cases: &[(Base64, &str, &str)] = &[
        (STANDARD, "", ""),
        (STANDARD, "f", "Zg=="),
        (STANDARD, "fo", "Zm8="),
        (STANDARD, "foo", "Zm9v"),
        (STANDARD, "foob", "Zm9vYg=="),
        (STANDARD, "fooba", "Zm9vYmE="),
        (STANDARD, "foobar", "Zm9vYmFy"),
    ];

    let mut buf: Vec<u8> = Vec::new();
    for &(ref base64, input, output) in cases {
        buf.clear();
        buf.resize(base64.encoded_length(input.len()), 0);

        let ans = base64.encode_as_str(input.as_bytes(), buf.as_out());
        assert_eq!(ans, output);

        buf.clear();
        buf.resize(base64.decoded_length(output.as_bytes()).unwrap(), 0);

        let ans = base64.decode(output.as_bytes(), buf.as_out()).unwrap();
        assert_eq!(ans, input.as_bytes());
    }
}

#[cfg(feature = "alloc")]
#[cfg_attr(not(target_arch = "wasm32"), test)]
#[cfg_attr(target_arch = "wasm32", wasm_bindgen_test::wasm_bindgen_test)]
fn allocation() {
    let src = "helloworld";
    let prefix = "data:;base64,";

    let mut encode_buf = prefix.to_owned();
    STANDARD.encode_append(src, &mut encode_buf);

    assert_eq!(encode_buf, format!("{prefix}aGVsbG93b3JsZA=="));

    let mut decode_buf = b"123".to_vec();
    let src = &encode_buf[prefix.len()..];
    STANDARD.decode_append(src, &mut decode_buf).unwrap();

    assert_eq!(decode_buf, b"123helloworld");
}

#[cfg_attr(not(target_arch = "wasm32"), test)]
#[cfg_attr(target_arch = "wasm32", wasm_bindgen_test::wasm_bindgen_test)]
fn random() {
    dbgmsg!();
    for n in 0..128 {
        dbgmsg!("n = {}", n);
        let bytes = rand_bytes(n);

        let test_config = [
            STANDARD,        //
            URL_SAFE,        //
            STANDARD_NO_PAD, //
            URL_SAFE_NO_PAD, //
        ];

        let base_config = {
            use base64::alphabet::*;
            use base64::engine::fast_portable::{FastPortable, NO_PAD, PAD};

            let f = FastPortable::from;

            [
                f(&STANDARD, PAD),
                f(&URL_SAFE, PAD),
                f(&STANDARD, NO_PAD),
                f(&URL_SAFE, NO_PAD),
            ]
        };

        for (base64, config) in test_config.into_iter().zip(base_config.into_iter()) {
            dbgmsg!("base64 = {:?}", base64);

            let encoded = base64::encode_engine(&bytes, &config);
            let encoded = encoded.as_bytes();
            assert!(base64.check(encoded).is_ok());

            {
                let mut buf = vec![0u8; base64.encoded_length(n)];
                let ans = base64.encode(&bytes, buf.as_out());
                assert_eq!(ans, encoded);
                assert!(base64.check(ans).is_ok());
                dbgmsg!("encoding ... ok");
            }

            {
                let mut buf = encoded.to_owned();
                let ans = base64.decode_inplace(&mut buf).unwrap();
                assert_eq!(ans, bytes);
                dbgmsg!("decoding inplace ... ok");
            }

            {
                let mut buf = vec![0u8; n];
                let ans = base64.decode(encoded, buf.as_out()).unwrap();
                assert_eq!(ans, bytes);
                dbgmsg!("decoding ... ok");
            }
        }
    }
}

/// <https://eprint.iacr.org/2022/361>
#[cfg_attr(not(target_arch = "wasm32"), test)]
#[cfg_attr(target_arch = "wasm32", wasm_bindgen_test::wasm_bindgen_test)]
fn canonicity() {
    let test_vectors = [
        ("SGVsbG8=", Some("Hello")),
        ("SGVsbG9=", None),
        ("SGVsbG9", None),
        ("SGVsbA==", Some("Hell")),
        ("SGVsbA=", None),
        ("SGVsbA", None),
        ("SGVsbA====", None),
    ];

    let mut buf = [0u8; 64];

    for (encoded, expected) in test_vectors {
        let base64 = STANDARD;

        let is_valid = base64.check(encoded.as_bytes()).is_ok();
        let result: _ = base64.decode(encoded.as_bytes(), buf.as_mut_slice().as_out());

        assert_eq!(is_valid, result.is_ok());
        match expected {
            Some(expected) => assert_eq!(result.unwrap(), expected.as_bytes()),
            None => assert!(result.is_err()),
        }
    }
}
