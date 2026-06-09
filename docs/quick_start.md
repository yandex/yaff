# Quick Start

This page walks through the basic workflow of working with data in YaFF: define a schema (or reuse an existing one), generate code, serialize data, and read it back.

## 0. Installation

YaFF is built and installed with CMake (Conan is also supported). A minimal install:

```bash
git clone https://github.com/yandex/yaff.git
cd yaff

cmake -S . -B build
cmake --build build
cmake --install build --prefix /usr/local
```

This makes YaFF discoverable by `find_package(YaFF)`. For Conan and detailed options, see the [Integration Guide](integration.md).

## 1. Schema

YaFF needs a schema to describe your data model. You can use an existing `.proto` file or write a new one from scratch.

```proto
syntax = "proto3";

package feed;

enum ContentType {
    CONTENT_TYPE_UNKNOWN = 0;
    CONTENT_TYPE_ARTICLE = 1;
    CONTENT_TYPE_VIDEO = 2;
}

message Author {
    uint64 id = 1;
    optional string name = 3;

    // 'verified' (field 2) was removed.
    reserved 2; reserved "verified";
}

message FeedItem {
    uint64 item_id = 1;
    optional ContentType content_type = 2;
    optional string title = 3;
    optional Author author = 4;
    repeated string tags = 5;
}

message FeedResponse {
    uint64 request_id = 1;
    repeated FeedItem items = 2;
}
```

You can also annotate only part of an existing schema, producing several YaFF *slices* from a single `.proto` file. Beyond slices, schema annotations let you control per-message layouts and other advanced settings. See [Schema Overview](schema/overview.md#advanced-usage) for details.

## 2. Code Generation

With a CMake build, add the `.proto` files to a target and call `protobuf_generate()` followed by `yaff_generate()`.

```cmake
find_package(Protobuf CONFIG REQUIRED)
find_package(YaFF CONFIG REQUIRED)

add_executable(my_app main.cpp proto/feed.proto)

protobuf_generate(
    TARGET my_app
    LANGUAGE cpp
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
)

yaff_generate(
    TARGET my_app
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(my_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(my_app PRIVATE yaff::core yaff::proto)
```

Alongside the `.pb.h` files produced by `protoc`, this generates `.yaff.h` files for the YaFF representation. Generated YaFF types live in the `protoyaff::<package>` namespace (e.g. `protoyaff::feed::FeedResponse`).

The snippet above is a minimal setup to get generation working. For more advanced usage, including consuming YaFF via `find_package` or Conan and the full `yaff_generate()` options, see the [Integration Guide](integration.md).

## 3. Serialize Data

The generated code serializes objects into YaFF. The most common path is to convert an existing Protobuf message:

```cpp
#include "feed.pb.h"
#include "feed.yaff.h"

// In practice the message may be loaded from a database or received from a service.
feed::FeedResponse proto = LoadFeedResponse();

// Build the YaFF buffer from the Protobuf representation.
const auto buffer = yaff::Serialize<protoyaff::feed::FeedResponse>(proto);
```

The resulting buffer is a single contiguous blob holding the data — it can be sent over the network, written to a file, or memory-mapped:

```cpp
std::ofstream{"out.bin", std::ios::binary}.write(
    reinterpret_cast<const char*>(buffer.Data()), buffer.Size());
```

You can also build the buffer directly with YaFF's serialization API, without constructing a Protobuf object at all. This avoids Protobuf's heap allocations, string copies, and the intermediate serialization step. See the [API Reference](reference/cpp/generated.md#serializing-messages-directly).

## 4. Read Data

Reading code is generated alongside the serialization code. The API mirrors Protobuf: you work with const references into the buffer, which keeps owning the memory, so it must stay alive as long as you read from it. There is no parsing step.

```cpp
#include "feed.yaff.h"

// `blob` holds a YaFF buffer obtained from some external source.
const auto& response = yaff::ReadMessage<protoyaff::feed::FeedResponse>(blob.Data());

const uint64_t requestId = response.request_id();
for (const auto& item : response.items()) {
    // Nested messages come back as const references; a missing one yields a
    // default instance, so access chains never dereference a null pointer.
    std::string_view title  = item.title();
    std::string_view author = item.author().name();  // empty if author is unset

    // Fields with explicit presence expose has_* accessors:
    // use them to tell an unset field from one set to its default value.
    if (item.has_content_type()) {
        auto type = item.content_type();
    }
}
```

To hand the data off elsewhere, you can transform a YaFF representation back into a Protobuf message:

```cpp
#include "feed.pb.h"

feed::FeedResponse restored;
response.ParseTo(restored);
```

Of course, the schema you read with doesn't have to be the exact version the data was written with: YaFF follows the same evolution rules as Protobuf, so compatible changes (adding fields, reserving removed ones, and so on) keep old and new data interoperable. See [Schema Evolution](schema/evolution.md) for the details.

## More Examples

For complete, runnable examples covering direct buffer building, multiple slices from one schema, and other features, see the [examples](https://github.com/yandex/yaff/tree/main/examples) directory.
