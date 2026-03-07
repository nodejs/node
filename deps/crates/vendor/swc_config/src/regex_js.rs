//! Regex cache

use std::{ops::Deref, sync::Arc};

pub use anyhow::Error;
use anyhow::{Context, Result};
use dashmap::DashMap;
use once_cell::sync::Lazy;
use regress::Regex;
use rustc_hash::FxBuildHasher;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone)]
pub struct CachedJsRegex {
    regex: Arc<Regex>,
    source: String,
}

impl Deref for CachedJsRegex {
    type Target = Regex;

    fn deref(&self) -> &Self::Target {
        &self.regex
    }
}

impl CachedJsRegex {
    /// Get or create a cached regex. This will return the previous instance if
    /// it's already cached.
    pub fn new(source: String) -> Result<Self> {
        static CACHE: Lazy<DashMap<String, Arc<Regex>, FxBuildHasher>> =
            Lazy::new(Default::default);

        if let Some(cache) = CACHE.get(&source).as_deref().cloned() {
            return Ok(Self {
                regex: cache,
                source,
            });
        }

        let regex =
            Regex::new(&source).with_context(|| format!("failed to parse `{source}` as regex"))?;
        let regex = Arc::new(regex);

        CACHE.insert(source.to_owned(), regex.clone());

        Ok(CachedJsRegex { regex, source })
    }

    pub fn source(&self) -> &str {
        &self.source
    }
}

impl<'de> Deserialize<'de> for CachedJsRegex {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use serde::de::Error;

        let s = String::deserialize(deserializer)?;

        Self::new(s).map_err(|err| D::Error::custom(err.to_string()))
    }
}

impl Serialize for CachedJsRegex {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        let s = self.source();
        serializer.serialize_str(s)
    }
}
