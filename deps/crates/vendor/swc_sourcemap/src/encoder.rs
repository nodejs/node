use std::io::Write;

use bitvec::field::BitField;
use bitvec::order::Lsb0;
use bitvec::view::BitView;
use serde_json::Value;

use crate::errors::Result;
use crate::jsontypes::{RawSection, RawSectionOffset, RawSourceMap};
use crate::types::{DecodedMap, SourceMap, SourceMapIndex};
use crate::vlq::encode_vlq;

pub trait Encodable {
    fn as_raw_sourcemap(&self) -> RawSourceMap;
}

pub fn encode<M: Encodable, W: Write>(sm: &M, mut w: W) -> Result<()> {
    let ty = sm.as_raw_sourcemap();
    serde_json::to_writer(&mut w, &ty)?;
    Ok(())
}

pub(crate) fn encode_vlq_diff(out: &mut String, a: u32, b: u32) {
    encode_vlq(out, i64::from(a) - i64::from(b))
}

pub(crate) fn encode_rmi(out: &mut Vec<u8>, data: &[u8]) {
    fn encode_byte(b: u8) -> u8 {
        match b {
            0..=25 => b + b'A',
            26..=51 => b + b'a' - 26,
            52..=61 => b + b'0' - 52,
            62 => b'+',
            63 => b'/',
            _ => panic!("invalid byte"),
        }
    }

    let bits = data.view_bits::<Lsb0>();

    // trim zero at the end
    let mut last = 0;
    for (idx, bit) in bits.iter().enumerate() {
        if *bit {
            last = idx;
        }
    }
    let bits = &bits[..last + 1];

    for byte in bits.chunks(6) {
        let byte = byte.load::<u8>();

        let encoded = encode_byte(byte);

        out.push(encoded);
    }
}

fn serialize_range_mappings(sm: &SourceMap) -> Option<String> {
    let mut buf = Vec::new();
    let mut prev_line = 0;
    let mut had_rmi = false;
    let mut empty = true;

    let mut idx_of_first_in_line = 0;

    let mut rmi_data = Vec::<u8>::new();

    for (idx, token) in sm.tokens().enumerate() {
        if token.is_range() {
            had_rmi = true;
            empty = false;

            let num = idx - idx_of_first_in_line;

            rmi_data.resize(rmi_data.len() + 2, 0);

            let rmi_bits = rmi_data.view_bits_mut::<Lsb0>();
            rmi_bits.set(num, true);
        }

        while token.get_dst_line() != prev_line {
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

    for (idx, token) in sm.tokens().enumerate() {
        if token.get_dst_line() != prev_dst_line {
            prev_dst_col = 0;
            while token.get_dst_line() != prev_dst_line {
                rv.push(';');
                prev_dst_line += 1;
            }
        } else if idx > 0 {
            if Some(&token) == sm.get_token(idx - 1).as_ref() {
                continue;
            }
            rv.push(',');
        }

        encode_vlq_diff(&mut rv, token.get_dst_col(), prev_dst_col);
        prev_dst_col = token.get_dst_col();

        if token.has_source() {
            encode_vlq_diff(&mut rv, token.get_src_id(), prev_src_id);
            prev_src_id = token.get_src_id();
            encode_vlq_diff(&mut rv, token.get_src_line(), prev_src_line);
            prev_src_line = token.get_src_line();
            encode_vlq_diff(&mut rv, token.get_src_col(), prev_src_col);
            prev_src_col = token.get_src_col();
            if token.has_name() {
                encode_vlq_diff(&mut rv, token.get_name_id(), prev_name_id);
                prev_name_id = token.get_name_id();
            }
        }
    }

    rv
}

impl Encodable for SourceMap {
    fn as_raw_sourcemap(&self) -> RawSourceMap {
        let mut have_contents = false;
        let contents = self
            .source_contents()
            .map(|contents| {
                if let Some(contents) = contents {
                    have_contents = true;
                    Some(contents.to_string())
                } else {
                    None
                }
            })
            .collect();
        RawSourceMap {
            version: Some(3),
            file: self.get_file().map(|x| Value::String(x.to_string())),
            sources: Some(self.sources.iter().map(|x| Some(x.to_string())).collect()),
            source_root: self.get_source_root().map(|x| x.to_string()),
            sources_content: if have_contents { Some(contents) } else { None },
            sections: None,
            names: Some(self.names().map(|x| Value::String(x.to_string())).collect()),
            range_mappings: serialize_range_mappings(self),
            mappings: Some(serialize_mappings(self)),
            ignore_list: if self.ignore_list.is_empty() {
                None
            } else {
                Some(self.ignore_list.iter().cloned().collect())
            },
            x_facebook_offsets: None,
            x_metro_module_paths: None,
            x_facebook_sources: None,
            debug_id: self.get_debug_id(),
            _debug_id_new: None,
        }
    }
}

impl Encodable for SourceMapIndex {
    fn as_raw_sourcemap(&self) -> RawSourceMap {
        RawSourceMap {
            version: Some(3),
            file: self.get_file().map(|x| Value::String(x.to_string())),
            sources: None,
            source_root: None,
            sources_content: None,
            sections: Some(
                self.sections()
                    .map(|section| RawSection {
                        offset: RawSectionOffset {
                            line: section.get_offset_line(),
                            column: section.get_offset_col(),
                        },
                        url: section.get_url().map(str::to_owned),
                        map: section
                            .get_sourcemap()
                            .map(|sm| Box::new(sm.as_raw_sourcemap())),
                    })
                    .collect(),
            ),
            names: None,
            range_mappings: None,
            mappings: None,
            ignore_list: None,
            x_facebook_offsets: None,
            x_metro_module_paths: None,
            x_facebook_sources: None,
            debug_id: None,
            // Put the debug ID on _debug_id_new to serialize it to the debugId field.
            _debug_id_new: self.debug_id(),
        }
    }
}

impl Encodable for DecodedMap {
    fn as_raw_sourcemap(&self) -> RawSourceMap {
        match *self {
            DecodedMap::Regular(ref sm) => sm.as_raw_sourcemap(),
            DecodedMap::Index(ref smi) => smi.as_raw_sourcemap(),
            DecodedMap::Hermes(ref smh) => smh.as_raw_sourcemap(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encode_rmi() {
        fn encode(indices: &[usize]) -> String {
            let mut out = vec![];

            // Fill with zeros while testing
            let mut data = vec![0; 256];

            let bits = data.view_bits_mut::<Lsb0>();
            for &i in indices {
                bits.set(i, true);
            }

            encode_rmi(&mut out, &data);
            String::from_utf8(out).unwrap()
        }

        // This is 0-based index
        assert_eq!(encode(&[12]), "AAB");
        assert_eq!(encode(&[5]), "g");
        assert_eq!(encode(&[0, 11]), "Bg");
    }

    #[test]
    fn test_encode_sourcemap_index_no_debug_id() {
        let smi = SourceMapIndex::new(Some("test.js".into()), vec![]);
        let raw = smi.as_raw_sourcemap();

        assert_eq!(
            raw,
            RawSourceMap {
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
            }
        );
    }

    #[test]
    fn test_encode_sourcemap_index_debug_id() {
        const DEBUG_ID: &str = "0123456789abcdef0123456789abcdef";

        let smi = SourceMapIndex::new(Some("test.js".into()), vec![])
            .with_debug_id(Some(DEBUG_ID.parse().expect("valid debug id")));

        let raw = smi.as_raw_sourcemap();
        assert_eq!(
            raw,
            RawSourceMap {
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
            }
        );
    }
}
