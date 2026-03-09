use std::io;
use std::io::{BufReader, Read};

use bitvec::field::BitField;
use bitvec::order::Lsb0;
use bitvec::vec::BitVec;
use serde_json::Value;

use crate::errors::{Error, Result};
use crate::hermes::decode_hermes;
use crate::jsontypes::RawSourceMap;
use crate::types::{DecodedMap, RawToken, SourceMap, SourceMapIndex, SourceMapSection};
use crate::vlq::parse_vlq_segment_into;

const DATA_PREAMBLE: &str = "data:application/json;base64,";

#[derive(PartialEq, Eq)]
enum HeaderState {
    Undecided,
    Junk,
    AwaitingNewline,
    PastHeader,
}

pub struct StripHeaderReader<R: Read> {
    r: R,
    header_state: HeaderState,
}

impl<R: Read> StripHeaderReader<R> {
    pub fn new(reader: R) -> StripHeaderReader<R> {
        StripHeaderReader {
            r: reader,
            header_state: HeaderState::Undecided,
        }
    }
}

fn is_junk_json(byte: u8) -> bool {
    byte == b')' || byte == b']' || byte == b'}' || byte == b'\''
}

impl<R: Read> Read for StripHeaderReader<R> {
    #[inline(always)]
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        if self.header_state == HeaderState::PastHeader {
            return self.r.read(buf);
        }
        self.strip_head_read(buf)
    }
}

impl<R: Read> StripHeaderReader<R> {
    fn strip_head_read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let mut backing = vec![0; buf.len()];
        let local_buf: &mut [u8] = &mut backing;

        loop {
            let read = self.r.read(local_buf)?;
            if read == 0 {
                return Ok(0);
            }
            for (offset, &byte) in local_buf[0..read].iter().enumerate() {
                self.header_state = match self.header_state {
                    HeaderState::Undecided => {
                        if is_junk_json(byte) {
                            HeaderState::Junk
                        } else {
                            buf[..read].copy_from_slice(&local_buf[..read]);
                            self.header_state = HeaderState::PastHeader;
                            return Ok(read);
                        }
                    }
                    HeaderState::Junk => {
                        if byte == b'\r' {
                            HeaderState::AwaitingNewline
                        } else if byte == b'\n' {
                            HeaderState::PastHeader
                        } else {
                            HeaderState::Junk
                        }
                    }
                    HeaderState::AwaitingNewline => {
                        if byte == b'\n' {
                            HeaderState::PastHeader
                        } else {
                            Err(io::Error::new(
                                io::ErrorKind::InvalidData,
                                "expected newline",
                            ))?
                        }
                    }
                    HeaderState::PastHeader => {
                        let rem = read - offset;
                        buf[..rem].copy_from_slice(&local_buf[offset..read]);
                        return Ok(rem);
                    }
                };
            }
        }
    }
}

pub fn strip_junk_header(slice: &[u8]) -> io::Result<&[u8]> {
    if slice.is_empty() || !is_junk_json(slice[0]) {
        return Ok(slice);
    }
    let mut need_newline = false;
    for (idx, &byte) in slice.iter().enumerate() {
        if need_newline && byte != b'\n' {
            Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "expected newline",
            ))?
        } else if is_junk_json(byte) {
            continue;
        } else if byte == b'\r' {
            need_newline = true;
        } else if byte == b'\n' {
            return Ok(&slice[idx..]);
        }
    }
    Ok(&slice[slice.len()..])
}

/// Decodes range mappping bitfield string into index
pub(crate) fn decode_rmi(rmi_str: &str, val: &mut BitVec<u8, Lsb0>) -> Result<()> {
    val.clear();
    val.resize(rmi_str.len() * 6, false);

    for (idx, &byte) in rmi_str.as_bytes().iter().enumerate() {
        let byte = match byte {
            b'A'..=b'Z' => byte - b'A',
            b'a'..=b'z' => byte - b'a' + 26,
            b'0'..=b'9' => byte - b'0' + 52,
            b'+' => 62,
            b'/' => 63,
            _ => {
                return Err(Error::InvalidBase64(byte as char));
            }
        };

        val[6 * idx..6 * (idx + 1)].store_le::<u8>(byte);
    }

    Ok(())
}

pub fn decode_regular(rsm: RawSourceMap) -> Result<SourceMap> {
    let mut dst_col;

    // Source IDs, lines, columns, and names are "running" values.
    // Each token (except the first) contains the delta from the previous value.
    let mut running_src_id = 0;
    let mut running_src_line = 0;
    let mut running_src_col = 0;
    let mut running_name_id = 0;

    let names = rsm.names.unwrap_or_default();
    let sources = rsm.sources.unwrap_or_default();
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

                if running_src_id >= sources.len() as u32 {
                    return Err(Error::BadSourceReference(running_src_id));
                }

                running_src_line = (i64::from(running_src_line) + nums[2]) as u32;
                running_src_col = (i64::from(running_src_col) + nums[3]) as u32;

                current_src_id = running_src_id;
                current_src_line = running_src_line;
                current_src_col = running_src_col;

                if nums.len() > 4 {
                    running_name_id = (i64::from(running_name_id) + nums[4]) as u32;
                    if running_name_id >= names.len() as u32 {
                        return Err(Error::BadNameReference(running_name_id));
                    }
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

    let sources = sources
        .into_iter()
        .map(Option::unwrap_or_default)
        .map(Into::into)
        .collect();

    // apparently we can encounter some non string types in real world
    // sourcemaps :(
    let names = names
        .into_iter()
        .map(|val| match val {
            Value::String(s) => s.into(),
            Value::Number(num) => num.to_string().into(),
            _ => "".into(),
        })
        .collect::<Vec<_>>();

    // file sometimes is not a string for unexplicable reasons
    let file = rsm.file.map(|val| match val {
        Value::String(s) => s.into(),
        _ => "<invalid>".into(),
    });

    let source_content = rsm
        .sources_content
        .map(|x| x.into_iter().map(|v| v.map(Into::into)).collect::<Vec<_>>());

    let mut sm = SourceMap::new(file, tokens, names, sources, source_content);
    sm.set_source_root(rsm.source_root);
    // Use _debug_id_new (from "debugId" key) only if debug_id
    // from ( "debug_id" key) is unset
    sm.set_debug_id(rsm.debug_id.or(rsm._debug_id_new));
    if let Some(ignore_list) = rsm.ignore_list {
        for idx in ignore_list {
            sm.add_to_ignore_list(idx);
        }
    }

    Ok(sm)
}

fn decode_index(rsm: RawSourceMap) -> Result<SourceMapIndex> {
    let mut sections = vec![];

    for mut raw_section in rsm.sections.unwrap_or_default() {
        sections.push(SourceMapSection::new(
            (raw_section.offset.line, raw_section.offset.column),
            raw_section.url,
            match raw_section.map.take() {
                Some(map) => Some(decode_common(*map)?),
                None => None,
            },
        ));
    }

    sections.sort_by_key(SourceMapSection::get_offset);

    // file sometimes is not a string for unexplicable reasons
    let file = rsm.file.map(|val| match val {
        Value::String(s) => s.into(),
        _ => "<invalid>".into(),
    });

    Ok(SourceMapIndex::new_ram_bundle_compatible(
        file,
        sections,
        rsm.x_facebook_offsets,
        rsm.x_metro_module_paths,
    )
    .with_debug_id(rsm._debug_id_new.or(rsm.debug_id)))
}

fn decode_common(rsm: RawSourceMap) -> Result<DecodedMap> {
    Ok(if rsm.sections.is_some() {
        DecodedMap::Index(decode_index(rsm)?)
    } else if rsm.x_facebook_sources.is_some() {
        DecodedMap::Hermes(decode_hermes(rsm)?)
    } else {
        DecodedMap::Regular(decode_regular(rsm)?)
    })
}

/// Decodes a sourcemap or sourcemap index from a reader
///
/// This supports both sourcemaps and sourcemap indexes unless the
/// specialized methods on the individual types.
pub fn decode<R: Read>(rdr: R) -> Result<DecodedMap> {
    let mut rdr = StripHeaderReader::new(rdr);
    let mut rdr = BufReader::new(&mut rdr);
    let rsm: RawSourceMap = serde_json::from_reader(&mut rdr)?;
    decode_common(rsm)
}

/// Decodes a sourcemap or sourcemap index from a byte slice
///
/// This supports both sourcemaps and sourcemap indexes unless the
/// specialized methods on the individual types.
pub fn decode_slice(slice: &[u8]) -> Result<DecodedMap> {
    let content = strip_junk_header(slice)?;
    let rsm: RawSourceMap = serde_json::from_slice(content)?;
    decode_common(rsm)
}

/// Loads a sourcemap from a data URL
pub fn decode_data_url(url: &str) -> Result<DecodedMap> {
    if !url.starts_with(DATA_PREAMBLE) {
        return Err(Error::InvalidDataUrl);
    }
    let data_b64 = &url[DATA_PREAMBLE.len()..];
    let data = data_encoding::BASE64
        .decode(data_b64.as_bytes())
        .map_err(|_| Error::InvalidDataUrl)?;
    decode_slice(&data[..])
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{self, BufRead};

    #[test]
    fn test_strip_header() {
        let input: &[_] = b")]}garbage\r\n[1, 2, 3]";
        let mut reader = io::BufReader::new(StripHeaderReader::new(input));
        let mut text = String::new();
        reader.read_line(&mut text).ok();
        assert_eq!(text, "[1, 2, 3]");
    }

    #[test]
    fn test_bad_newline() {
        let input: &[_] = b")]}'\r[1, 2, 3]";
        let mut reader = io::BufReader::new(StripHeaderReader::new(input));
        let mut text = String::new();
        match reader.read_line(&mut text) {
            Err(err) => {
                assert_eq!(err.kind(), io::ErrorKind::InvalidData);
            }
            Ok(_) => {
                panic!("Expected failure");
            }
        }
    }

    #[test]
    fn test_decode_rmi() {
        fn decode(rmi_str: &str) -> Vec<usize> {
            let mut out = bitvec::bitvec![u8, Lsb0; 0; 0];
            decode_rmi(rmi_str, &mut out).expect("failed to decode");

            let mut res = vec![];
            for (idx, bit) in out.iter().enumerate() {
                if *bit {
                    res.push(idx);
                }
            }
            res
        }

        // This is 0-based index of the bits
        assert_eq!(decode("AAB"), vec![12]);
        assert_eq!(decode("g"), vec![5]);
        assert_eq!(decode("Bg"), vec![0, 11]);
    }

    #[test]
    fn test_decode_sourcemap_index_no_debug_id() {
        let raw = RawSourceMap {
            version: Some(3),
            file: Some("test.js".into()),
            sources: None,
            source_root: None,
            sources_content: None,
            sections: Some(vec![]),
            names: None,
            range_mappings: None,
            mappings: None,
            ignore_list: None,
            x_facebook_offsets: None,
            x_metro_module_paths: None,
            x_facebook_sources: None,
            debug_id: None,
            _debug_id_new: None,
        };

        let decoded = decode_common(raw).expect("should decoded");
        assert_eq!(
            decoded,
            DecodedMap::Index(SourceMapIndex::new(Some("test.js".into()), vec![]))
        );
    }

    #[test]
    fn test_decode_sourcemap_index_debug_id() {
        const DEBUG_ID: &str = "0123456789abcdef0123456789abcdef";

        let raw = RawSourceMap {
            version: Some(3),
            file: Some("test.js".into()),
            sources: None,
            source_root: None,
            sources_content: None,
            sections: Some(vec![]),
            names: None,
            range_mappings: None,
            mappings: None,
            ignore_list: None,
            x_facebook_offsets: None,
            x_metro_module_paths: None,
            x_facebook_sources: None,
            debug_id: None,
            _debug_id_new: Some(DEBUG_ID.parse().expect("valid debug id")),
        };

        let decoded = decode_common(raw).expect("should decode");
        assert_eq!(
            decoded,
            DecodedMap::Index(
                SourceMapIndex::new(Some("test.js".into()), vec![])
                    .with_debug_id(Some(DEBUG_ID.parse().expect("valid debug id")))
            )
        );
    }

    #[test]
    fn test_decode_sourcemap_index_debug_id_from_legacy_key() {
        const DEBUG_ID: &str = "0123456789abcdef0123456789abcdef";

        let raw = RawSourceMap {
            version: Some(3),
            file: Some("test.js".into()),
            sources: None,
            source_root: None,
            sources_content: None,
            sections: Some(vec![]),
            names: None,
            range_mappings: None,
            mappings: None,
            ignore_list: None,
            x_facebook_offsets: None,
            x_metro_module_paths: None,
            x_facebook_sources: None,
            debug_id: Some(DEBUG_ID.parse().expect("valid debug id")),
            _debug_id_new: None,
        };

        let decoded = decode_common(raw).expect("should decode");
        assert_eq!(
            decoded,
            DecodedMap::Index(
                SourceMapIndex::new(Some("test.js".into()), vec![])
                    .with_debug_id(Some(DEBUG_ID.parse().expect("valid debug id")))
            )
        );
    }
}
