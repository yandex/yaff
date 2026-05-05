# Examples

Each example demonstrates a different aspect of the YaFF library:

* `feed_api`: converting protobuf messages to YaFF and back with zero-copy field access and reflection;

* `doc_views`: generating multiple YaFF slices from one protobuf schema to control which fields end up in each view;

* `trading`: building YaFF buffers directly via the generated API, skipping protobuf allocation entirely.
