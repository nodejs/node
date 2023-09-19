export const async_id_symbol: unique symbol;
export const handle_onclose_symbol: unique symbol;
export const no_message_symbol: unique symbol;
export const messaging_deserialize_symbol: unique symbol;
export const messaging_transfer_symbol: unique symbol;
export const messaging_clone_symbol: unique symbol;
export const messaging_transfer_list_symbol: unique symbol;
export const oninit_symbol: unique symbol;
export const owner_symbol: unique symbol;
export const onpskexchange_symbol: unique symbol;
export const trigger_async_id_symbol: unique symbol;

export interface SymbolsBinding {
  async_id_symbol: typeof async_id_symbol;
  handle_onclose_symbol: typeof handle_onclose_symbol;
  no_message_symbol: typeof no_message_symbol;
  messaging_deserialize_symbol: typeof messaging_deserialize_symbol;
  messaging_transfer_symbol: typeof messaging_transfer_symbol;
  messaging_clone_symbol: typeof messaging_clone_symbol;
  messaging_transfer_list_symbol: typeof messaging_transfer_list_symbol;
  oninit_symbol: typeof oninit_symbol;
  owner_symbol: typeof owner_symbol;
  onpskexchange_symbol: typeof onpskexchange_symbol;
  trigger_async_id_symbol: typeof trigger_async_id_symbol;
}
