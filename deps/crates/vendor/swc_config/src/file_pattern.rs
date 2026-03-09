use serde::{Deserialize, Serialize};

use crate::{glob::CachedGlob, regex::CachedRegex};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(untagged)]
pub enum FilePattern {
    Regex(CachedRegex),
    Glob { glob: CachedGlob },
}

impl FilePattern {
    pub fn is_match(&self, path: &str) -> bool {
        match self {
            FilePattern::Regex(regex) => regex.is_match(path),
            FilePattern::Glob { glob } => glob.is_match(path),
        }
    }
}
