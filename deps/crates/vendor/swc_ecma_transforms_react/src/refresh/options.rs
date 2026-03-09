use serde::{Deserialize, Deserializer, Serialize};
use swc_atoms::{atom, Atom};
#[derive(Debug, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "camelCase")]
#[serde(deny_unknown_fields)]
pub struct RefreshOptions {
    #[serde(default = "default_refresh_reg")]
    pub refresh_reg: Atom,
    #[serde(default = "default_refresh_sig")]
    pub refresh_sig: Atom,
    #[serde(default = "default_emit_full_signatures")]
    pub emit_full_signatures: bool,
}

fn default_refresh_reg() -> Atom {
    atom!("$RefreshReg$")
}
fn default_refresh_sig() -> Atom {
    atom!("$RefreshSig$")
}
fn default_emit_full_signatures() -> bool {
    // Prefer to hash when we can
    // This makes it deterministically compact, even if there's
    // e.g. a useState initializer with some code inside.
    false
}

impl Default for RefreshOptions {
    fn default() -> Self {
        RefreshOptions {
            refresh_reg: default_refresh_reg(),
            refresh_sig: default_refresh_sig(),
            emit_full_signatures: default_emit_full_signatures(),
        }
    }
}

#[derive(Deserialize)]
#[serde(untagged)]
enum BoolOrRefresh {
    Bool(bool),
    Refresh(RefreshOptions),
}
pub fn deserialize_refresh<'de, D>(deserializer: D) -> Result<Option<RefreshOptions>, D::Error>
where
    D: Deserializer<'de>,
{
    match BoolOrRefresh::deserialize(deserializer)? {
        BoolOrRefresh::Refresh(refresh) => Ok(Some(refresh)),
        BoolOrRefresh::Bool(true) => Ok(Some(Default::default())),
        BoolOrRefresh::Bool(false) => Ok(None),
    }
}
