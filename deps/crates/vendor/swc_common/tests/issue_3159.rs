#![allow(unexpected_cfgs)]

use swc_common::ast_serde;

#[ast_serde]
pub enum Message {
    #[tag("Request")]
    Request(String),
    #[tag("Response")]
    Response(u8),
}
