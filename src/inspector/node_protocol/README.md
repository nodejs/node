This directory was initialy created by the `inspector_protocol` code-gen tool.
The protocol definition is in [../node_protocol.pdl]. It was converted to JSON
and stored in [../node_protocol.json] by running:
```console
python $(INSPECTOR_PROTOCOL_PATH)/ConvertProtocolToJSON.py ../node_protocol.pdl ../node_protocol.json
```

After making changes to the protocol definition, code need to be manuly
regenerated and checked in to git, by running:
```console
python $(INSPECTOR_PROTOCOL_PATH)/CodeGenerator.py --jinja_dir $(JINJA_PATH) --output_base .. --config ../node_protocol_config.json
```
