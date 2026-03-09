use rustc_hash::FxHashMap;
use swc_atoms::Atom;
use swc_ecma_ast::Id;

#[derive(Debug, Default)]
pub(crate) struct ReverseMap<'a> {
    prev: Option<&'a ReverseMap<'a>>,

    inner: FxHashMap<Atom, Vec<Id>>,
}

impl ReverseMap<'_> {
    pub fn push_entry(&mut self, key: Atom, id: Id) {
        self.inner.entry(key).or_default().push(id);
    }

    fn iter(&self) -> Iter {
        Iter { cur: Some(self) }
    }

    pub fn get<'a>(&'a self, key: &'a Atom) -> impl Iterator<Item = &'a Id> + 'a {
        self.iter()
            .filter_map(|v| v.inner.get(key))
            .flat_map(|v| v.iter())
    }

    pub fn next(&self) -> ReverseMap {
        ReverseMap {
            prev: Some(self),
            ..Default::default()
        }
    }
}

pub(crate) struct Iter<'a> {
    cur: Option<&'a ReverseMap<'a>>,
}

impl<'a> Iterator for Iter<'a> {
    type Item = &'a ReverseMap<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        let cur = self.cur.take()?;
        self.cur = cur.prev;
        Some(cur)
    }
}
