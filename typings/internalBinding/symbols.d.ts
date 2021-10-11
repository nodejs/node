declare namespace InternalSymbolsBinding {
  const async_id_symbol: unique symbol;
  const handle_onclose_symbol: unique symbol;
  const no_message_symbol: unique symbol;
  const messaging_deserialize_symbol: unique symbol;
  const messaging_transfer_symbol: unique symbol;
  const messaging_clone_symbol: unique symbol;
  const messaging_transfer_list_symbol: unique symbol;
  const oninit_symbol: unique symbol;
  const owner_symbol: unique symbol;
  const onpskexchange_symbol: unique symbol;
  const trigger_async_id_symbol: unique symbol;
}

declare function InternalBinding(binding: 'symbols'): {
  async_id_symbol: typeof InternalSymbolsBinding.async_id_symbol;
  handle_onclose_symbol: typeof InternalSymbolsBinding.handle_onclose_symbol;
  no_message_symbol: typeof InternalSymbolsBinding.no_message_symbol;
  messaging_deserialize_symbol: typeof InternalSymbolsBinding.messaging_deserialize_symbol;
  messaging_transfer_symbol: typeof InternalSymbolsBinding.messaging_transfer_symbol;
  messaging_clone_symbol: typeof InternalSymbolsBinding.messaging_clone_symbol;
  messaging_transfer_list_symbol: typeof InternalSymbolsBinding.messaging_transfer_list_symbol;
  oninit_symbol: typeof InternalSymbolsBinding.oninit_symbol;
  owner_symbol: typeof InternalSymbolsBinding.owner_symbol;
  onpskexchange_symbol: typeof InternalSymbolsBinding.onpskexchange_symbol;
  trigger_async_id_symbol: typeof InternalSymbolsBinding.trigger_async_id_symbol;
};
