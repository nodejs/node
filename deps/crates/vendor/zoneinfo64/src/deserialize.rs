// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::*;
use core::marker::PhantomData;
use resb::binary::BinaryDeserializerError;
use serde::de::*;
use serde::Deserialize;

/// Deserialize a ZoneInfo64
pub(crate) fn deserialize<'a>(resb: &'a [u32]) -> Result<ZoneInfo64<'a>, BinaryDeserializerError> {
    let ZoneInfo64Raw {
        zones,
        names,
        rules,
        regions,
    } = resb::binary::from_words::<ZoneInfo64Raw>(resb)?;

    if zones.len() != names.len() || names.len() != regions.len() {
        return Err(BinaryDeserializerError::unknown(
            "zones, names, regions need to have matching lengths",
        ));
    }

    // Translate rule string keys to indices
    let rules_lookup =
        |name: &PotentialUtf16| rules.iter().position(|&(n, _)| name.chars().eq(n.chars()));

    let raw_zones = zones;
    let mut zones = Vec::with_capacity(raw_zones.len());

    if zones.capacity() == 0 {
        return Err(BinaryDeserializerError::unknown(
            "at least one zone is required",
        ));
    }

    for zone in &raw_zones {
        match *zone {
            TzZoneRaw::Int(i) => {
                let Some(alias) = raw_zones.get(i as usize) else {
                    return Err(BinaryDeserializerError::unknown("invalid alias idx"));
                };
                if let TzZoneRaw::Int(_) = alias {
                    return Err(BinaryDeserializerError::unknown("multi-step alias"));
                }
                zones.push(TzZone::Int(i))
            }
            TzZoneRaw::Table(TzZoneDataRaw {
                type_offsets,
                trans,
                trans_pre32,
                trans_post32,
                type_map,
                final_rule,
                final_raw,
                final_year,
                links,
            }) => {
                let trans = trans.unwrap_or_default();
                let trans_pre32 = trans_pre32.unwrap_or_default();
                let trans_post32 = trans_post32.unwrap_or_default();
                let type_map = type_map.unwrap_or_default();

                let links = links.unwrap_or_default();

                if trans.len() + trans_post32.len() + trans_pre32.len() != type_map.len()
                    || type_offsets.is_empty()
                {
                    return Err(BinaryDeserializerError::unknown("inconsistent offset data"));
                }

                for &idx in type_map {
                    if idx as usize >= type_offsets.len() {
                        return Err(BinaryDeserializerError::unknown("invalid offset map"));
                    }
                }

                let final_rule_offset_year = match (final_rule, final_raw, final_year) {
                    (Some(name), Some(offset), Some(year)) => {
                        let Some(idx) = rules_lookup(name) else {
                            return Err(BinaryDeserializerError::unknown("invalid rule id"));
                        };
                        Some((idx as u32, offset, year))
                    }
                    (None, None, None) => None,
                    _ => {
                        return Err(BinaryDeserializerError::unknown(
                            "inconsisent finalRule, finalRaw, finalYear",
                        ))
                    }
                };

                zones.push(TzZone::Table(Box::new(TzZoneData {
                    type_offsets,
                    trans,
                    trans_pre32,
                    trans_post32,
                    type_map,
                    final_rule_offset_year,
                    links,
                })))
            }
        }
    }

    let rules = rules.into_iter().map(|(_name, rule)| rule).collect();

    Ok(ZoneInfo64 {
        zones,
        names,
        rules,
        regions,
    })
}

#[derive(Debug, Deserialize)]
#[serde(rename = "zoneinfo64")]
#[serde(rename_all = "PascalCase")]
struct ZoneInfo64Raw<'a> {
    #[serde(borrow)]
    zones: Vec<TzZoneRaw<'a>>,
    #[serde(borrow, deserialize_with = "resb::binary::helpers::vec_utf_16")]
    names: Vec<&'a PotentialUtf16>,
    #[serde(borrow, deserialize_with = "rules")]
    rules: Vec<(&'a str, TzRule)>,
    #[serde(deserialize_with = "regions")]
    regions: Vec<Region>,
}

#[derive(Debug)]
pub enum TzZoneRaw<'a> {
    Table(TzZoneDataRaw<'a>),
    Int(u32),
}

impl<'de: 'a, 'a> Deserialize<'de> for TzZoneRaw<'a> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        struct TzDataRuleEnumVisitor<'a> {
            phantom: PhantomData<TzZoneRaw<'a>>,
        }

        impl<'de: 'a, 'a> Visitor<'de> for TzDataRuleEnumVisitor<'a> {
            type Value = TzZoneRaw<'a>;

            fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                formatter.write_str("an unsigned 32-bit integer or a table of rule data")
            }

            fn visit_u32<E>(self, v: u32) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(TzZoneRaw::Int(v))
            }

            fn visit_map<A>(self, map: A) -> Result<Self::Value, A::Error>
            where
                A: MapAccess<'de>,
            {
                let value = TzZoneDataRaw::deserialize(value::MapAccessDeserializer::new(map))?;

                Ok(TzZoneRaw::Table(value))
            }
        }

        deserializer.deserialize_any(TzDataRuleEnumVisitor {
            phantom: PhantomData,
        })
    }
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct TzZoneDataRaw<'a> {
    #[serde(borrow, deserialize_with = "resb::binary::helpers::i32_tuple")]
    type_offsets: &'a [(i32, i32)],
    #[serde(
        borrow,
        default,
        deserialize_with = "resb::binary::helpers::option_i32"
    )]
    trans: Option<&'a [i32]>,
    #[serde(
        borrow,
        default,
        deserialize_with = "resb::binary::helpers::option_i32_tuple"
    )]
    trans_pre32: Option<&'a [(i32, i32)]>,
    #[serde(
        borrow,
        default,
        deserialize_with = "resb::binary::helpers::option_i32_tuple"
    )]
    trans_post32: Option<&'a [(i32, i32)]>,
    type_map: Option<&'a [u8]>,
    #[serde(
        borrow,
        default,
        deserialize_with = "resb::binary::helpers::option_utf_16"
    )]
    final_rule: Option<&'a PotentialUtf16>,
    final_raw: Option<i32>,
    final_year: Option<i32>,
    #[allow(dead_code)]
    #[serde(
        borrow,
        default,
        deserialize_with = "resb::binary::helpers::option_u32"
    )]
    links: Option<&'a [u32]>,
}

fn rules<'de, D: Deserializer<'de>>(deserializer: D) -> Result<Vec<(&'de str, TzRule)>, D::Error> {
    struct RulesVisitor;

    impl<'de> Visitor<'de> for RulesVisitor {
        type Value = Vec<(&'de str, TzRule)>;

        fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
            write!(formatter, "a sequence of UTF-16 slices")
        }

        fn visit_map<A>(self, mut map: A) -> Result<Self::Value, A::Error>
        where
            A: MapAccess<'de>,
        {
            let mut vec = vec![];
            while let Some((key, value)) = map.next_entry::<&str, &[u8]>()? {
                if value
                    .as_ptr()
                    .align_offset(core::mem::align_of::<[i32; 11]>())
                    != 0
                    || value.len() != core::mem::size_of::<[i32; 11]>()
                {
                    return Err(A::Error::custom("Wrong length or align"));
                }
                let value = unsafe { &*(value.as_ptr() as *const [i32; 11]) };

                vec.push((key, TzRule::from_raw(value)));
            }
            Ok(vec)
        }
    }

    deserializer.deserialize_map(RulesVisitor)
}

fn regions<'de, D: Deserializer<'de>>(deserializer: D) -> Result<Vec<Region>, D::Error> {
    struct RegionsVisitor;

    impl<'de> Visitor<'de> for RegionsVisitor {
        type Value = Vec<Region>;

        fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
            write!(formatter, "a sequence of UTF-16 slices")
        }

        fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
        where
            A: SeqAccess<'de>,
        {
            let mut vec = vec![];
            while let Some(bytes) = seq.next_element::<&[u8]>()? {
                let utf16 = potential_utf::PotentialUtf16::from_slice(
                    // Safety: all byte representations are valid u16s
                    unsafe { resb::binary::helpers::cast_bytes_to_slice::<_, A::Error>(bytes)? },
                );

                let mut utf16 = utf16.chars();

                let Ok(region) = Region::try_from_raw([
                    utf16.next().filter(char::is_ascii).unwrap_or_default() as u8,
                    utf16.next().filter(char::is_ascii).unwrap_or_default() as u8,
                    utf16.next().filter(char::is_ascii).unwrap_or_default() as u8,
                ]) else {
                    return Err(A::Error::custom("Invalid region code"));
                };

                vec.push(region);
            }
            Ok(vec)
        }
    }

    deserializer.deserialize_seq(RegionsVisitor)
}
