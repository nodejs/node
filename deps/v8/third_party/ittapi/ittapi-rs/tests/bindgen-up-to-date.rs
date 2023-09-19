// This is a smoke test for pre-generated `src/ittapi-bindings.rs` and
// `src/jitprofiling-bindings.rs` files to see that they don't need to be
// updated. We check in a generated version so downstream consumers don't
// have to get `bindgen` working themselves.
//
// If bindgen or ittapi.h or jitprofiling.h change you can run tests with
// `BLESS=1` (inpired by a similiar pach for binaryen) to regenerate the
// source files, otherwise this can test on CI that the file doesn't need
// to be regenerated.

#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]
#![allow(unused)]

#[test]
fn test_ittnotify_bindings_up_to_date() {
    let expected = bindgen::Builder::default()
        .rustfmt_bindings(true)
        .header("./include/ittnotify.h")
        .generate()
        .expect("Unable to generate ittnotify bindings.")
        .to_string();

    if std::env::var("BLESS").is_ok() {
        std::fs::write("src/ittnotify_bindings.rs", expected).unwrap();
    } else {
        let actual = include_str!("../src/ittnotify_bindings.rs");
        if expected == actual {
            return;
        }

        for diff in diff::lines(&expected, &actual) {
            match diff {
                diff::Result::Both(_, s) => println!(" {}", s),
                diff::Result::Left(s) => println!("-{}", s),
                diff::Result::Right(s) => println!("+{}", s),
            }
        }
        panic!("differences found, need to regenerate ittnotify bindings");
    }
}

#[test]
fn test_jitprofiling_bindings_up_to_date() {
    let expected = bindgen::Builder::default()
        .rustfmt_bindings(true)
        .header("./include/jitprofiling.h")
        .generate()
        .expect("Unable to generate jitprofiling bindings")
        .to_string();

    if std::env::var("BLESS").is_ok() {
        std::fs::write("src/jitprofiling_bindings.rs", expected).unwrap();
    } else {
        let actual = include_str!("../src/jitprofiling_bindings.rs");
        if expected == actual {
            return;
        }

        for diff in diff::lines(&expected, &actual) {
            match diff {
                diff::Result::Both(_, s) => println!(" {}", s),
                diff::Result::Left(s) => println!("-{}", s),
                diff::Result::Right(s) => println!("+{}", s),
            }
        }
        panic!("differences found, need to regenerate jitprofiling bindings");
    }
}
