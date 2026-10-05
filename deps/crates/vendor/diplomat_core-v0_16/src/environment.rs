use crate::ast::*;
use std::collections::BTreeMap;
use std::ops::Index;

/// The type resolution environment
///
/// Also contains the entire module structure
#[derive(Default, Clone)]
pub struct Env {
    pub(crate) env: BTreeMap<Path, ModuleEnv>,
}

/// The type resolution environment within a specific module
#[derive(Clone)]
pub struct ModuleEnv {
    pub(crate) module: BTreeMap<Ident, ModSymbol>,
    #[cfg_attr(not(feature = "hir"), allow(unused))]
    pub(crate) attrs: Attrs,
}

impl Env {
    pub(crate) fn insert(&mut self, path: Path, module: ModuleEnv) {
        self.env.insert(path, module);
    }

    /// Given a path to a module and a name, get the item, if any
    pub fn get(&self, path: &Path, name: &str) -> Option<&ModSymbol> {
        self.env.get(path).and_then(|m| m.module.get(name))
    }

    /// Iterate over all items in the environment
    ///
    /// This will occur in a stable lexically sorted order by path and then name
    pub fn iter_items(&self) -> impl Iterator<Item = (&Path, &Ident, &ModSymbol)> + '_ {
        self.env
            .iter()
            .flat_map(|(k, v)| v.module.iter().map(move |v2| (k, v2.0, v2.1)))
    }

    /// Iterate over all modules
    ///
    /// This will occur in a stable lexically sorted order by path
    pub fn iter_modules(&self) -> impl Iterator<Item = (&Path, &ModuleEnv)> + '_ {
        self.env.iter()
    }
}

impl ModuleEnv {
    pub(crate) fn new(attrs: Attrs) -> Self {
        Self {
            module: Default::default(),
            attrs,
        }
    }
    pub(crate) fn insert(&mut self, name: Ident, symbol: ModSymbol) -> Option<ModSymbol> {
        self.module.insert(name, symbol)
    }

    /// Given an item name, fetch it
    pub fn get(&self, name: &str) -> Option<&ModSymbol> {
        self.module.get(name)
    }

    /// Iterate over all name-item pairs in this module
    pub fn iter(&self) -> impl Iterator<Item = (&Ident, &ModSymbol)> + '_ {
        self.module.iter()
    }

    /// Iterate over all names in this module
    ///
    /// This will occur in a stable lexically sorted order by name
    pub fn names(&self) -> impl Iterator<Item = &Ident> + '_ {
        self.module.keys()
    }

    /// Iterate over all items in this module
    ///
    /// This will occur in a stable lexically sorted order by name
    pub fn items(&self) -> impl Iterator<Item = &ModSymbol> + '_ {
        self.module.values()
    }
}

impl Index<&Path> for Env {
    type Output = ModuleEnv;
    fn index(&self, i: &Path) -> &ModuleEnv {
        &self.env[i]
    }
}

impl Index<&str> for ModuleEnv {
    type Output = ModSymbol;
    fn index(&self, i: &str) -> &ModSymbol {
        &self.module[i]
    }
}
