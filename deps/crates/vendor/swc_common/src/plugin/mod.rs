pub mod diagnostics;
pub mod emit;
pub mod metadata;
#[cfg(feature = "__plugin")]
#[cfg_attr(docsrs, doc(cfg(feature = "__plugin")))]
pub mod serialized;

/**
 * Compile-time version constant for the AST struct schema's version.
 *
 * NOTE: this is for PARTIAL compatibility only, supporting if AST struct
 * adds new properties without changing / removing existing properties.
 *
 * - When adding a new properties to the AST struct:
 *  1. Create a new feature flag in cargo.toml
 *  2. Create a new schema version with new feature flag.
 *  3. Create a new AST struct with compile time feature flag with newly
 *     added properties. Previous struct should remain with existing feature
 *     flag, or add previous latest feature flag.
 *
 * - When removing, or changing existing properties in the AST struct: TBD
 */
#[cfg(feature = "plugin_transform_schema_v1")]
pub const PLUGIN_TRANSFORM_AST_SCHEMA_VERSION: u32 = 1;

// Reserved for the testing purpose.
#[cfg(all(
    feature = "plugin_transform_schema_vtest",
    not(feature = "plugin_transform_schema_v1")
))]
pub const PLUGIN_TRANSFORM_AST_SCHEMA_VERSION: u32 = u32::MAX - 1;
