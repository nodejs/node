//! This is _lazy_ because we skip deserializing all the fields that we don't need. (unlike the original crate)

use crate::{
    decoder::{decode_rmi, strip_junk_header},
    encoder::{encode_rmi, encode_vlq_diff},
    types::adjust_mappings,
    vlq::parse_vlq_segment_into,
    Error, RawToken, Result,
};
use std::{
    borrow::Cow,
    collections::{BTreeSet, HashMap},
};

use bitvec::{order::Lsb0, vec::BitVec, view::BitView};
use bytes_str::BytesStr;
use serde::{Deserialize, Deserializer, Serialize};
use serde_json::value::RawValue;

#[derive(Serialize, Deserialize, Debug)]
#[serde(rename_all = "camelCase")]
pub struct RawSourceMap<'a> {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub(crate) version: Option<u32>,
    #[serde(default, borrow, skip_serializing_if = "Option::is_none")]
    pub(crate) file: Option<MaybeRawValue<'a, Str>>,
    #[serde(borrow)]
    pub(crate) sources: MaybeRawValue<'a, Vec<StrValue<'a>>>,
    #[serde(default, borrow, skip_serializing_if = "Option::is_none")]
    pub(crate) source_root: Option<StrValue<'a>>,
    #[serde(default, borrow, skip_serializing_if = "MaybeRawValue::is_empty")]
    pub(crate) sources_content: MaybeRawValue<'a, Vec<Option<StrValue<'a>>>>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub(crate) sections: Option<Vec<RawSection<'a>>>,
    #[serde(default, borrow)]
    pub(crate) names: MaybeRawValue<'a, Vec<StrValue<'a>>>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub(crate) range_mappings: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub(crate) mappings: Option<String>,
    #[serde(default, borrow, skip_serializing_if = "Option::is_none")]
    pub(crate) ignore_list: Option<MaybeRawValue<'a, BTreeSet<u32>>>,
}

#[derive(Serialize, Deserialize, PartialEq, Debug, Clone, Copy)]
pub struct RawSectionOffset {
    pub line: u32,
    pub column: u32,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct RawSection<'a> {
    pub offset: RawSectionOffset,
    #[serde(borrow)]
    pub url: Option<&'a RawValue>,
    #[serde(borrow)]
    pub map: Option<&'a RawValue>,
}

#[derive(Debug)]
pub enum DecodedMap<'a> {
    Regular(SourceMap<'a>),
    Index(SourceMapIndex<'a>),
}

impl<'a> DecodedMap<'a> {
    pub fn into_source_map(self) -> Result<SourceMap<'a>> {
        match self {
            DecodedMap::Regular(source_map) => Ok(source_map),
            DecodedMap::Index(source_map_index) => source_map_index.flatten(),
        }
    }
}

#[derive(Debug, Clone, Copy, Serialize)]
#[serde(untagged)]
pub(crate) enum MaybeRawValue<'a, T> {
    RawValue(#[serde(borrow)] &'a RawValue),
    Data(T),
}

impl<T> MaybeRawValue<'_, Vec<T>> {
    pub fn is_empty(&self) -> bool {
        match self {
            MaybeRawValue::Data(vec) => vec.is_empty(),
            MaybeRawValue::RawValue(_) => false,
        }
    }
}

impl<'a, 'de, T> Deserialize<'de> for MaybeRawValue<'a, T>
where
    'de: 'a,
    T: Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> std::result::Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let raw: &'de RawValue = Deserialize::deserialize(deserializer)?;
        Ok(MaybeRawValue::RawValue(raw))
    }
}

impl<'a, T> MaybeRawValue<'a, T>
where
    T: Deserialize<'a>,
{
    pub fn into_data(self) -> T {
        match self {
            MaybeRawValue::RawValue(s) => {
                serde_json::from_str(s.get()).expect("Failed to convert RawValue to Data")
            }
            MaybeRawValue::Data(data) => data,
        }
    }

    fn assert_raw_value(self) -> &'a RawValue {
        match self {
            MaybeRawValue::RawValue(s) => s,
            MaybeRawValue::Data(_) => unreachable!("Expected RawValue, got Data"),
        }
    }
}

impl<T> Default for MaybeRawValue<'_, T>
where
    T: Default,
{
    fn default() -> Self {
        MaybeRawValue::Data(T::default())
    }
}

impl<'a, T> MaybeRawValue<'a, T>
where
    T: Deserialize<'a>,
    T: Default,
{
    pub fn as_data(&mut self) -> &mut T {
        match self {
            MaybeRawValue::RawValue(s) => {
                *self = MaybeRawValue::Data(
                    serde_json::from_str(s.get()).expect("Failed to convert RawValue to Data"),
                );
                if let MaybeRawValue::Data(data) = self {
                    data
                } else {
                    unreachable!()
                }
            }
            MaybeRawValue::Data(data) => data,
        }
    }
}

type Str = BytesStr;

type StrValue<'a> = MaybeRawValue<'a, Str>;

#[derive(Debug)]
pub struct SourceMap<'a> {
    pub(crate) file: Option<StrValue<'a>>,
    pub(crate) tokens: Vec<RawToken>,
    pub(crate) names: MaybeRawValue<'a, Vec<StrValue<'a>>>,
    pub(crate) source_root: Option<StrValue<'a>>,
    pub(crate) sources: MaybeRawValue<'a, Vec<StrValue<'a>>>,
    pub(crate) sources_content: MaybeRawValue<'a, Vec<Option<StrValue<'a>>>>,
    pub(crate) ignore_list: Option<MaybeRawValue<'a, BTreeSet<u32>>>,
}

#[derive(Debug)]
pub(crate) struct SourceMapBuilder<'a> {
    file: Option<StrValue<'a>>,
    name_map: HashMap<&'a str, u32>,
    names: Vec<StrValue<'a>>,
    tokens: Vec<RawToken>,
    source_map: HashMap<&'a str, u32>,
    sources: Vec<StrValue<'a>>,
    source_contents: Vec<Option<StrValue<'a>>>,
    source_root: Option<StrValue<'a>>,
    ignore_list: Option<BTreeSet<u32>>,
}

impl<'a> SourceMapBuilder<'a> {
    pub fn new(file: Option<StrValue<'a>>) -> Self {
        SourceMapBuilder {
            file,
            name_map: HashMap::new(),
            names: Vec::new(),
            tokens: Vec::new(),
            source_map: HashMap::new(),
            sources: Vec::new(),
            source_contents: Vec::new(),
            source_root: None,
            ignore_list: None,
        }
    }

    pub fn add_source(&mut self, src_raw: &'a RawValue) -> u32 {
        let src_str = src_raw.get(); // RawValue provides get() -> &str
        let count = self.sources.len() as u32;
        let id = *self.source_map.entry(src_str).or_insert(count);
        if id == count {
            // New source
            self.sources.push(MaybeRawValue::RawValue(src_raw));
            // Ensure source_contents has a corresponding entry, defaulting to None.
            // This logic ensures source_contents is always same length as sources if new one added.
            self.source_contents.resize(self.sources.len(), None);
        }
        id
    }

    pub fn add_name(&mut self, name_raw: &'a RawValue) -> u32 {
        let name_str = name_raw.get();
        let count = self.names.len() as u32;
        let id = *self.name_map.entry(name_str).or_insert(count);
        if id == count {
            // New name
            self.names.push(MaybeRawValue::RawValue(name_raw));
        }
        id
    }

    pub fn set_source_contents(&mut self, src_id: u32, contents: Option<&'a RawValue>) {
        // Ensure source_contents is large enough. src_id is 0-indexed.
        if (src_id as usize) >= self.source_contents.len() {
            self.source_contents.resize(src_id as usize + 1, None);
        }
        self.source_contents[src_id as usize] = contents.map(MaybeRawValue::RawValue);
    }

    pub fn add_to_ignore_list(&mut self, src_id: u32) {
        self.ignore_list
            .get_or_insert_with(BTreeSet::new)
            .insert(src_id);
    }

    pub fn into_sourcemap(self) -> SourceMap<'a> {
        SourceMap {
            file: self.file,
            tokens: self.tokens,
            names: MaybeRawValue::Data(self.names),
            source_root: self.source_root,
            sources: MaybeRawValue::Data(self.sources),
            sources_content: MaybeRawValue::Data(self.source_contents),
            ignore_list: self.ignore_list.map(MaybeRawValue::Data),
        }
    }

    /// Adds a new mapping to the builder.
    #[allow(clippy::too_many_arguments)]
    pub fn add_raw(
        &mut self,
        dst_line: u32,
        dst_col: u32,
        src_line: u32,
        src_col: u32,
        source: Option<u32>,
        name: Option<u32>,
        is_range: bool,
    ) -> RawToken {
        let src_id = source.unwrap_or(!0);
        let name_id = name.unwrap_or(!0);
        let raw = RawToken {
            dst_line,
            dst_col,
            src_line,
            src_col,
            src_id,
            name_id,
            is_range,
        };
        self.tokens.push(raw);
        raw
    }
}

#[derive(Debug)]
pub(crate) struct SourceMapSection<'a> {
    offset: (u32, u32),
    url: Option<MaybeRawValue<'a, String>>,
    map: Option<Box<MaybeRawValue<'a, RawSourceMap<'a>>>>,
}

impl<'a> SourceMapSection<'a> {
    /// Create a new sourcemap index section
    ///
    /// - `offset`: offset as line and column
    /// - `url`: optional URL of where the sourcemap is located
    /// - `map`: an optional already resolved internal sourcemap
    pub fn new(
        offset: (u32, u32),
        url: Option<MaybeRawValue<'a, String>>,
        map: Option<MaybeRawValue<'a, RawSourceMap<'a>>>,
    ) -> SourceMapSection<'a> {
        SourceMapSection {
            offset,
            url,
            map: map.map(Box::new),
        }
    }

    /// Returns the offset as tuple
    pub fn get_offset(&self) -> (u32, u32) {
        self.offset
    }
}

#[derive(Debug)]
pub struct SourceMapIndex<'a> {
    file: Option<MaybeRawValue<'a, Str>>,
    sections: Vec<SourceMapSection<'a>>,
}

pub fn decode(slice: &[u8]) -> Result<DecodedMap<'_>> {
    let content = strip_junk_header(slice)?;
    let rsm: RawSourceMap = serde_json::from_slice(content)?;

    decode_common(rsm)
}

fn decode_common(rsm: RawSourceMap) -> Result<DecodedMap> {
    if rsm.sections.is_some() {
        decode_index(rsm).map(DecodedMap::Index)
    } else {
        decode_regular(rsm).map(DecodedMap::Regular)
    }
}

fn decode_index(rsm: RawSourceMap) -> Result<SourceMapIndex> {
    let mut sections = vec![];

    for raw_section in rsm.sections.unwrap_or_default() {
        sections.push(SourceMapSection::new(
            (raw_section.offset.line, raw_section.offset.column),
            raw_section.url.map(MaybeRawValue::RawValue),
            raw_section.map.map(MaybeRawValue::RawValue),
        ));
    }

    sections.sort_by_key(SourceMapSection::get_offset);

    // file sometimes is not a string for unexplicable reasons
    let file = rsm.file;

    Ok(SourceMapIndex { file, sections })
}

pub fn decode_regular(rsm: RawSourceMap) -> Result<SourceMap> {
    let mut dst_col;

    // Source IDs, lines, columns, and names are "running" values.
    // Each token (except the first) contains the delta from the previous value.
    let mut running_src_id = 0;
    let mut running_src_line = 0;
    let mut running_src_col = 0;
    let mut running_name_id = 0;

    let range_mappings = rsm.range_mappings.unwrap_or_default();
    let mappings = rsm.mappings.unwrap_or_default();
    let allocation_size = mappings.matches(&[',', ';'][..]).count() + 10;
    let mut tokens = Vec::with_capacity(allocation_size);

    let mut nums = Vec::with_capacity(6);
    let mut rmi = BitVec::new();

    for (dst_line, (line, rmi_str)) in mappings
        .split(';')
        .zip(range_mappings.split(';').chain(std::iter::repeat("")))
        .enumerate()
    {
        if line.is_empty() {
            continue;
        }

        dst_col = 0;

        decode_rmi(rmi_str, &mut rmi)?;

        for (line_index, segment) in line.split(',').enumerate() {
            if segment.is_empty() {
                continue;
            }

            nums.clear();
            parse_vlq_segment_into(segment, &mut nums)?;
            match nums.len() {
                1 | 4 | 5 => {}
                _ => return Err(Error::BadSegmentSize(nums.len() as u32)),
            }

            dst_col = (i64::from(dst_col) + nums[0]) as u32;

            // The source file , source line, source column, and name
            // may not be present in the current token. We use `u32::MAX`
            // as the placeholder for missing values.
            let mut current_src_id = !0;
            let mut current_src_line = !0;
            let mut current_src_col = !0;
            let mut current_name_id = !0;

            if nums.len() > 1 {
                running_src_id = (i64::from(running_src_id) + nums[1]) as u32;

                running_src_line = (i64::from(running_src_line) + nums[2]) as u32;
                running_src_col = (i64::from(running_src_col) + nums[3]) as u32;

                current_src_id = running_src_id;
                current_src_line = running_src_line;
                current_src_col = running_src_col;

                if nums.len() > 4 {
                    running_name_id = (i64::from(running_name_id) + nums[4]) as u32;
                    current_name_id = running_name_id;
                }
            }

            let is_range = rmi.get(line_index).map(|v| *v).unwrap_or_default();

            tokens.push(RawToken {
                dst_line: dst_line as u32,
                dst_col,
                src_line: current_src_line,
                src_col: current_src_col,
                src_id: current_src_id,
                name_id: current_name_id,
                is_range,
            });
        }
    }

    let sm = SourceMap {
        file: rsm.file,
        tokens,
        names: rsm.names,
        source_root: rsm.source_root,
        sources: rsm.sources,
        sources_content: rsm.sources_content,
        ignore_list: rsm.ignore_list,
    };

    Ok(sm)
}

impl<'a> SourceMap<'a> {
    /// Refer to [crate::SourceMap::adjust_mappings] for more details.
    pub fn adjust_mappings(&mut self, adjustment: crate::SourceMap) {
        self.tokens = adjust_mappings(
            std::mem::take(&mut self.tokens),
            Cow::Owned(adjustment.tokens),
        );
    }

    pub fn into_raw_sourcemap(self) -> RawSourceMap<'a> {
        RawSourceMap {
            version: Some(3),
            range_mappings: serialize_range_mappings(&self),
            mappings: Some(serialize_mappings(&self)),
            file: self.file,
            sources: self.sources,
            source_root: self.source_root,
            sources_content: self.sources_content,
            sections: None,
            names: self.names,
            ignore_list: self.ignore_list,
        }
    }

    pub fn file(&mut self) -> Option<&BytesStr> {
        self.file.as_mut().map(|f| &*f.as_data())
    }

    pub fn sources(&mut self) -> impl Iterator<Item = &'_ BytesStr> + use<'_, 'a> {
        self.sources.as_data().iter_mut().map(|d| &*d.as_data())
    }

    pub fn get_source(&mut self, src_id: u32) -> Option<&BytesStr> {
        self.sources
            .as_data()
            .get_mut(src_id as usize)
            .map(|d| &*d.as_data())
    }

    pub fn get_name(&mut self, src_id: u32) -> Option<&BytesStr> {
        self.names
            .as_data()
            .get_mut(src_id as usize)
            .map(|d| &*d.as_data())
    }

    pub fn get_source_contents(&mut self, src_id: u32) -> Option<&BytesStr> {
        self.sources_content
            .as_data()
            .get_mut(src_id as usize)
            .and_then(|d| d.as_mut().map(|d| &*d.as_data()))
    }
}

impl<'a> SourceMapIndex<'a> {
    pub fn flatten(self) -> Result<SourceMap<'a>> {
        let mut builder = SourceMapBuilder::new(self.file);

        for section in self.sections {
            let (off_line, off_col) = section.get_offset();

            let map = match section.map {
                Some(map) => match decode_common(map.into_data())? {
                    DecodedMap::Regular(sm) => sm,
                    DecodedMap::Index(idx) => idx.flatten()?,
                },
                None => {
                    return Err(Error::CannotFlatten(format!(
                        "Section has an unresolved sourcemap: {}",
                        section
                            .url
                            .map(|v| v.into_data())
                            .as_deref()
                            .unwrap_or("<unknown url>")
                    )));
                }
            };

            let sources = map.sources.into_data();
            let source_contents = map.sources_content.into_data();
            let ignore_list = map.ignore_list.unwrap_or_default().into_data();

            let mut src_id_map = Vec::<u32>::with_capacity(sources.len());

            for (original_id, (source, contents)) in
                sources.into_iter().zip(source_contents).enumerate()
            {
                debug_assert_eq!(original_id, src_id_map.len());
                let src_id = builder.add_source(source.assert_raw_value());

                src_id_map.push(src_id);

                if let Some(contents) = contents {
                    builder.set_source_contents(src_id, Some(contents.assert_raw_value()));
                }
            }

            let names = map.names.into_data();
            let mut name_id_map = Vec::<u32>::with_capacity(names.len());

            for (original_id, name) in names.into_iter().enumerate() {
                debug_assert_eq!(original_id, name_id_map.len());
                let name_id = builder.add_name(name.assert_raw_value());
                name_id_map.push(name_id);
            }

            for token in map.tokens {
                let dst_col = if token.dst_line == 0 {
                    token.dst_col + off_col
                } else {
                    token.dst_col
                };

                // Use u32 -> u32 map instead of using the hash map in SourceMapBuilder for better
                // performance
                let original_src_id = token.src_id;
                let src_id = if original_src_id == !0 {
                    None
                } else {
                    src_id_map.get(original_src_id as usize).copied()
                };

                let original_name_id = token.name_id;
                let name_id = if original_name_id == !0 {
                    None
                } else {
                    name_id_map.get(original_name_id as usize).copied()
                };

                let raw = builder.add_raw(
                    token.dst_line + off_line,
                    dst_col,
                    token.src_line,
                    token.src_col,
                    src_id,
                    name_id,
                    token.is_range,
                );

                if ignore_list.contains(&token.src_id) {
                    builder.add_to_ignore_list(raw.src_id);
                }
            }
        }

        Ok(builder.into_sourcemap())
    }
}

fn serialize_range_mappings(sm: &SourceMap) -> Option<String> {
    let mut buf = Vec::new();
    let mut prev_line = 0;
    let mut had_rmi = false;
    let mut empty = true;

    let mut idx_of_first_in_line = 0;

    let mut rmi_data = Vec::<u8>::new();

    for (idx, token) in sm.tokens.iter().enumerate() {
        if token.is_range {
            had_rmi = true;
            empty = false;

            let num = idx - idx_of_first_in_line;

            rmi_data.resize(rmi_data.len() + 2, 0);

            let rmi_bits = rmi_data.view_bits_mut::<Lsb0>();
            rmi_bits.set(num, true);
        }

        while token.dst_line != prev_line {
            if had_rmi {
                encode_rmi(&mut buf, &rmi_data);
                rmi_data.clear();
            }

            buf.push(b';');
            prev_line += 1;
            had_rmi = false;
            idx_of_first_in_line = idx;
        }
    }
    if empty {
        return None;
    }

    if had_rmi {
        encode_rmi(&mut buf, &rmi_data);
    }

    Some(String::from_utf8(buf).expect("invalid utf8"))
}

fn serialize_mappings(sm: &SourceMap) -> String {
    let mut rv = String::new();
    // dst == minified == generated
    let mut prev_dst_line = 0;
    let mut prev_dst_col = 0;
    let mut prev_src_line = 0;
    let mut prev_src_col = 0;
    let mut prev_name_id = 0;
    let mut prev_src_id = 0;

    for (idx, token) in sm.tokens.iter().enumerate() {
        if token.dst_line != prev_dst_line {
            prev_dst_col = 0;
            while token.dst_line != prev_dst_line {
                rv.push(';');
                prev_dst_line += 1;
            }
        } else if idx > 0 {
            if Some(&token) == sm.tokens.get(idx - 1).as_ref() {
                continue;
            }
            rv.push(',');
        }

        encode_vlq_diff(&mut rv, token.dst_col, prev_dst_col);
        prev_dst_col = token.dst_col;

        if token.src_id != !0 {
            encode_vlq_diff(&mut rv, token.src_id, prev_src_id);
            prev_src_id = token.src_id;
            encode_vlq_diff(&mut rv, token.src_line, prev_src_line);
            prev_src_line = token.src_line;
            encode_vlq_diff(&mut rv, token.src_col, prev_src_col);
            prev_src_col = token.src_col;
            if token.name_id != !0 {
                encode_vlq_diff(&mut rv, token.name_id, prev_name_id);
                prev_name_id = token.name_id;
            }
        }
    }

    rv
}
