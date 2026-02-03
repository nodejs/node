use serde::{Deserialize, Serialize};
use std::fmt;

use super::Ident;

#[derive(Hash, Eq, PartialEq, Deserialize, Serialize, Clone, Debug, Ord, PartialOrd)]
#[non_exhaustive]
pub struct Path {
    pub elements: Vec<Ident>,
}

impl Path {
    pub fn get_super(&self) -> Path {
        let mut new_elements = self.elements.clone();
        new_elements.remove(new_elements.len() - 1);
        Path {
            elements: new_elements,
        }
    }

    pub fn sub_path(&self, ident: Ident) -> Path {
        let mut new_elements = self.elements.clone();
        new_elements.push(ident);
        Path {
            elements: new_elements,
        }
    }

    pub fn to_syn(&self) -> syn::Path {
        syn::Path {
            leading_colon: None,
            segments: self
                .elements
                .iter()
                .map(|s| syn::PathSegment {
                    ident: s.to_syn(),
                    arguments: syn::PathArguments::None,
                })
                .collect(),
        }
    }

    pub fn from_syn(path: &syn::Path) -> Path {
        Path {
            elements: path
                .segments
                .iter()
                .map(|seg| (&seg.ident).into())
                .collect(),
        }
    }

    pub fn empty() -> Path {
        Path { elements: vec![] }
    }
}

impl fmt::Display for Path {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if let Some((head, tail)) = self.elements.split_first() {
            head.fmt(f)?;
            for seg in tail {
                "::".fmt(f)?;
                seg.fmt(f)?;
            }
        }
        Ok(())
    }
}

impl FromIterator<Ident> for Path {
    fn from_iter<T: IntoIterator<Item = Ident>>(iter: T) -> Self {
        Path {
            elements: iter.into_iter().collect(),
        }
    }
}
