{
  "targets": [
    {
      "target_name": "test_init_order",
      "sources": [ "test_init_order.cc" ]
    },
    {
      "target_name": "test_init_order_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_init_order.cc" ]
    }
  ]
}
