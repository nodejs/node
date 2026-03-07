// Whenever the lib.rs changes, the SCHEMA_FILE_HASH environment variable will change and the
// schema_version test below will fail.
// Proceed as follows:
//
// If the schema in this library has changed then:
//  1. Change this APPROVED_SCHEMA_FILE_HASH to the new hash.
//
// If the schema in this library has changed then:
//  1. Bump the version in `crates/shared/Cargo.toml`
//  2. Change the `SCHEMA_VERSION` in this library to this new Cargo.toml version
const APPROVED_SCHEMA_FILE_HASH: &str = "4279711543728568852";

#[test]
fn schema_version() {
    assert_eq!(env!("SCHEMA_FILE_HASH"), APPROVED_SCHEMA_FILE_HASH)
}
