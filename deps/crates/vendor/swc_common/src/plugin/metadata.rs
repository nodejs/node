use std::env;

use once_cell::sync::Lazy;
use rustc_hash::FxHashMap;

/// Indexable key to the metadata context for a transform plugin.
///
/// Avoiding serialization & allocation to the host by using incremental number.
/// TransformPluginMetadataContext does not implement Index trait, instead
/// host does manual matching to corresponding value.
#[derive(Copy, Clone)]
pub enum TransformPluginMetadataContextKind {
    // This value always should increase, even if some keys are removed in the
    // future.
    Filename = 1,
    Env = 2,
    Cwd = 3,
}

impl From<u32> for TransformPluginMetadataContextKind {
    fn from(key: u32) -> TransformPluginMetadataContextKind {
        match key {
            1 => TransformPluginMetadataContextKind::Filename,
            2 => TransformPluginMetadataContextKind::Env,
            3 => TransformPluginMetadataContextKind::Cwd,
            _ => panic!("Invalid TransformPluginMetadataContextKind key"),
        }
    }
}

/// Host side metadata context plugin may need to access.
/// This is a global context - any plugin in single transform will have same
/// values.
pub struct TransformPluginMetadataContext {
    /// The path of the file being processed. This includes all of the path as
    /// much as possible.
    pub filename: Option<String>,
    /// The current environment resolved as process.env.SWC_ENV ||
    /// process.env.NODE_ENV || "development"
    pub env: String,
    /// The current working directory.
    pub cwd: Option<String>,
    pub experimental: FxHashMap<String, String>,
}

impl TransformPluginMetadataContext {
    pub fn new(
        filename: Option<String>,
        env: String,
        experimental: Option<FxHashMap<String, String>>,
    ) -> Self {
        static CWD: Lazy<Option<String>> = Lazy::new(|| {
            env::current_dir()
                .map(|cwd| cwd.as_path().to_string_lossy().to_string())
                .ok()
        });

        TransformPluginMetadataContext {
            filename,
            env,
            cwd: CWD.clone(),
            experimental: experimental.unwrap_or_default(),
        }
    }

    pub fn get(&self, key: &TransformPluginMetadataContextKind) -> Option<String> {
        match key {
            TransformPluginMetadataContextKind::Filename => self.filename.clone(),
            TransformPluginMetadataContextKind::Env => Some(self.env.clone()),
            TransformPluginMetadataContextKind::Cwd => self.cwd.clone(),
        }
    }
}

pub struct Context(pub FxHashMap<String, String>);

#[cfg(feature = "encoding-impl")]
impl cbor4ii::core::enc::Encode for Context {
    #[inline]
    fn encode<W: cbor4ii::core::enc::Write>(
        &self,
        writer: &mut W,
    ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
        use cbor4ii::core::types;

        types::Map::bounded(self.0.len(), writer)?;
        for (k, v) in self.0.iter() {
            k.encode(writer)?;
            v.encode(writer)?;
        }

        Ok(())
    }
}

#[cfg(feature = "encoding-impl")]
impl<'de> cbor4ii::core::dec::Decode<'de> for Context {
    #[inline]
    fn decode<R: cbor4ii::core::dec::Read<'de>>(
        reader: &mut R,
    ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
        use cbor4ii::core::types;

        let len = types::Map::len(reader)?.ok_or_else(|| cbor4ii::core::dec::Error::Mismatch {
            name: &"Context",
            found: 0,
        })?;
        let len = std::cmp::min(len, 4096);
        let mut map = FxHashMap::with_capacity_and_hasher(len, Default::default());
        for _ in 0..len {
            let k = String::decode(reader)?;
            let v = String::decode(reader)?;
            map.insert(k, v);
        }

        Ok(Context(map))
    }
}
