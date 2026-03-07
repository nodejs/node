#[derive(Debug, Clone, PartialEq, Eq)]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct PluginEmitOutput {
    pub key: String,
    pub value: String,
}
