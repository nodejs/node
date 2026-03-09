use bytes_str::BytesStr;

use crate::decoder::{decode, decode_regular, decode_slice};
use crate::encoder::{encode, Encodable};
use crate::errors::{Error, Result};
use crate::jsontypes::{FacebookScopeMapping, FacebookSources, RawSourceMap};
use crate::types::{DecodedMap, RewriteOptions, SourceMap};
use crate::utils::greatest_lower_bound;
use crate::vlq::parse_vlq_segment_into;
use crate::Token;
use std::io::{Read, Write};
use std::ops::{Deref, DerefMut};

/// These are starting locations of scopes.
/// The `name_index` represents the index into the `HermesFunctionMap.names` vec,
/// which represents the function names/scopes.
#[derive(Debug, Clone, PartialEq)]
pub struct HermesScopeOffset {
    line: u32,
    column: u32,
    name_index: u32,
}

#[derive(Debug, Clone, PartialEq)]
pub struct HermesFunctionMap {
    names: Vec<BytesStr>,
    mappings: Vec<HermesScopeOffset>,
}

/// Represents a `react-native`-style SourceMap, which has additional scope
/// information embedded.
#[derive(Debug, Clone, PartialEq)]
pub struct SourceMapHermes {
    pub(crate) sm: SourceMap,
    // There should be one `HermesFunctionMap` per each `sources` entry in the main SourceMap.
    function_maps: Vec<Option<HermesFunctionMap>>,
    // XXX: right now, I am too lazy to actually serialize the above `function_maps`
    // back into json types, so just keep the original json. Might be a bit inefficient, but meh.
    raw_facebook_sources: FacebookSources,
}

impl Deref for SourceMapHermes {
    type Target = SourceMap;

    fn deref(&self) -> &Self::Target {
        &self.sm
    }
}

impl DerefMut for SourceMapHermes {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.sm
    }
}

impl Encodable for SourceMapHermes {
    fn as_raw_sourcemap(&self) -> RawSourceMap {
        // TODO: need to serialize the `HermesFunctionMap` mappings
        let mut rsm = self.sm.as_raw_sourcemap();
        rsm.x_facebook_sources
            .clone_from(&self.raw_facebook_sources);
        rsm
    }
}

impl SourceMapHermes {
    /// Creates a sourcemap from a reader over a JSON stream in UTF-8
    /// format.
    ///
    /// See [`SourceMap::from_reader`](struct.SourceMap.html#method.from_reader)
    pub fn from_reader<R: Read>(rdr: R) -> Result<Self> {
        match decode(rdr)? {
            DecodedMap::Hermes(sm) => Ok(sm),
            _ => Err(Error::IncompatibleSourceMap),
        }
    }

    /// Creates a sourcemap from a reader over a JSON byte slice in UTF-8
    /// format.
    ///
    /// See [`SourceMap::from_slice`](struct.SourceMap.html#method.from_slice)
    pub fn from_slice(slice: &[u8]) -> Result<Self> {
        match decode_slice(slice)? {
            DecodedMap::Hermes(sm) => Ok(sm),
            _ => Err(Error::IncompatibleSourceMap),
        }
    }

    /// Writes a sourcemap into a writer.
    ///
    /// See [`SourceMap::to_writer`](struct.SourceMap.html#method.to_writer)
    pub fn to_writer<W: Write>(&self, w: W) -> Result<()> {
        encode(self, w)
    }

    /// Given a bytecode offset, this will find the enclosing scopes function
    /// name.
    pub fn get_original_function_name(&self, bytecode_offset: u32) -> Option<&BytesStr> {
        let token = self.sm.lookup_token(0, bytecode_offset)?;

        self.get_scope_for_token(token)
    }

    /// Resolves the name of the enclosing function for the given [`Token`].
    pub fn get_scope_for_token(&self, token: Token) -> Option<&BytesStr> {
        let function_map = self
            .function_maps
            .get(token.get_src_id() as usize)?
            .as_ref()?;

        // Find the closest mapping, just like here:
        // https://github.com/facebook/metro/blob/63b523eb20e7bdf62018aeaf195bb5a3a1a67f36/packages/metro-symbolicate/src/SourceMetadataMapConsumer.js#L204-L231
        // Mappings use 1-based index for lines, and 0-based index for cols, as seen here:
        // https://github.com/facebook/metro/blob/f2d80cebe66d3c64742f67259f41da26e83a0d8d/packages/metro/src/Server/symbolicate.js#L58-L60
        let (_mapping_idx, mapping) = greatest_lower_bound(
            &function_map.mappings,
            &(token.get_src_line() + 1, token.get_src_col()),
            |o| (o.line, o.column),
        )?;
        function_map.names.get(mapping.name_index as usize)
    }

    /// This rewrites the sourcemap according to the provided rewrite
    /// options.
    ///
    /// See [`SourceMap::rewrite`](struct.SourceMap.html#method.rewrite)
    pub fn rewrite(self, options: &RewriteOptions<'_>) -> Result<Self> {
        let Self {
            sm,
            mut function_maps,
            mut raw_facebook_sources,
        } = self;

        let (sm, mapping) = sm.rewrite_with_mapping(options)?;

        if function_maps.len() >= mapping.len() {
            function_maps = mapping
                .iter()
                .map(|idx| function_maps[*idx as usize].take())
                .collect();
            raw_facebook_sources = raw_facebook_sources.map(|mut sources| {
                mapping
                    .into_iter()
                    .map(|idx| sources[idx as usize].take())
                    .collect()
            });
        }

        Ok(Self {
            sm,
            function_maps,
            raw_facebook_sources,
        })
    }
}

pub fn decode_hermes(mut rsm: RawSourceMap) -> Result<SourceMapHermes> {
    let x_facebook_sources = rsm
        .x_facebook_sources
        .take()
        .ok_or(Error::IncompatibleSourceMap)?;

    // This is basically the logic from here:
    // https://github.com/facebook/metro/blob/63b523eb20e7bdf62018aeaf195bb5a3a1a67f36/packages/metro-symbolicate/src/SourceMetadataMapConsumer.js#L182-L202

    let mut nums = Vec::with_capacity(4);

    let function_maps = x_facebook_sources
        .iter()
        .map(|v| {
            let FacebookScopeMapping {
                names,
                mappings: raw_mappings,
            } = v.as_ref()?.iter().next()?;

            let mut mappings = vec![];
            let mut line = 1;
            let mut name_index = 0;

            for line_mapping in raw_mappings.split(';') {
                if line_mapping.is_empty() {
                    continue;
                }

                let mut column = 0;

                for mapping in line_mapping.split(',') {
                    if mapping.is_empty() {
                        continue;
                    }

                    nums.clear();
                    parse_vlq_segment_into(mapping, &mut nums).ok()?;
                    let mut nums = nums.iter().copied();

                    column = (i64::from(column) + nums.next()?) as u32;
                    name_index = (i64::from(name_index) + nums.next().unwrap_or(0)) as u32;
                    line = (i64::from(line) + nums.next().unwrap_or(0)) as u32;
                    mappings.push(HermesScopeOffset {
                        column,
                        line,
                        name_index,
                    });
                }
            }
            Some(HermesFunctionMap {
                names: names.clone(),
                mappings,
            })
        })
        .collect();

    let sm = decode_regular(rsm)?;
    Ok(SourceMapHermes {
        sm,
        function_maps,
        raw_facebook_sources: Some(x_facebook_sources),
    })
}
