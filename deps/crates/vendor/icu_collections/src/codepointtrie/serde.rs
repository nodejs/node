// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::codepointtrie::{CodePointTrie, CodePointTrieHeader, TrieValue};
use serde::{de::Error, Deserialize, Deserializer, Serialize, Serializer};
use zerofrom::ZeroFrom;
use zerovec::ZeroVec;

#[derive(Serialize, Deserialize)]
pub struct CodePointTrieSerde<'trie, T: TrieValue> {
    header: CodePointTrieHeader,
    #[serde(borrow)]
    index: ZeroVec<'trie, u16>,
    #[serde(borrow)]
    data: ZeroVec<'trie, T>,
}

impl<T: TrieValue + Serialize> Serialize for CodePointTrie<'_, T> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let ser = CodePointTrieSerde {
            header: self.header,
            index: ZeroFrom::zero_from(&self.index),
            data: ZeroFrom::zero_from(&self.data),
        };
        ser.serialize(serializer)
    }
}

impl<'de, 'trie, T: TrieValue + Deserialize<'de>> Deserialize<'de> for CodePointTrie<'trie, T>
where
    'de: 'trie,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let de = CodePointTrieSerde::deserialize(deserializer)?;
        let error_value = de.data.last().ok_or_else(|| {
            D::Error::custom("CodePointTrie vector must have at least one element")
        })?;
        Ok(CodePointTrie {
            header: de.header,
            index: de.index,
            data: de.data,
            error_value,
        })
    }
}
