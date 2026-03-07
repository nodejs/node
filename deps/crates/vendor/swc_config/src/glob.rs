use std::sync::Arc;

use anyhow::{Context, Result};
use dashmap::DashMap;
use globset::{Glob, GlobMatcher};
use once_cell::sync::Lazy;
use rustc_hash::FxBuildHasher;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone)]
pub struct CachedGlob {
    glob: Arc<GlobMatcher>,
}

impl CachedGlob {
    pub fn new(glob_str: &str) -> Result<Self> {
        static CACHE: Lazy<DashMap<String, Arc<GlobMatcher>, FxBuildHasher>> =
            Lazy::new(Default::default);

        if let Some(cache) = CACHE.get(glob_str).as_deref().cloned() {
            return Ok(Self { glob: cache });
        }

        let glob = Glob::new(glob_str)
            .with_context(|| format!("failed to create glob for '{glob_str}'"))?
            .compile_matcher();

        let glob = Arc::new(glob);

        CACHE.insert(glob_str.to_string(), glob.clone());

        Ok(Self { glob })
    }

    pub fn is_match(&self, path: &str) -> bool {
        self.glob.is_match(path)
    }
}

impl Serialize for CachedGlob {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        String::serialize(&self.glob.glob().to_string(), serializer)
    }
}

impl<'de> Deserialize<'de> for CachedGlob {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use serde::de::Error;

        let glob = String::deserialize(deserializer)?;

        Self::new(&glob).map_err(|err| D::Error::custom(err.to_string()))
    }
}
