# YaFF — Integration Guide

YaFF can be added to a project in two ways:

- **CMake** — build and install YaFF, then use `find_package`
- **Conan** — build YaFF into the local Conan cache, then declare it as a dependency

Both methods expose the same set of CMake targets and the `yaff_generate()` helper function.

---

## 1. Integration via CMake

### Step 1 — Build and install YaFF

```bash
git clone https://github.com/yandex/yaff.git
cd yaff

cmake -S . -B build          # Release build type is used by default
cmake --build build
cmake --install build --prefix /usr/local
```

This installs:

- headers → `<prefix>/include/yaff/`
- `options.proto` → `<prefix>/include/yaff/proto/options.proto`
- libraries → `<prefix>/lib/`
- `protoc-gen-yaff` binary → `<prefix>/bin/`
- CMake config files → `<prefix>/lib/cmake/YaFF/`

> **`options.proto`** is the YaFF Protobuf options file. Your `.proto` schemas
> can import it as `import "yaff/proto/options.proto"`. After `find_package(YaFF)`
> the variable `YAFF_PROTO_IMPORT_DIR` is set to `<prefix>/include` and
> `yaff_generate()` automatically passes it as a `-I` flag to `protoc`, so no
> extra configuration is needed.

### Step 2 — Use in your project

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(YaFF CONFIG REQUIRED)

# yaff_generate(), YAFF_PROTO_IMPORT_DIR, and all yaff::* targets are now available.

# ... define your targets, call protobuf_generate() and yaff_generate() ...

target_link_libraries(my_app
    PRIVATE
        yaff::core
        yaff::proto
)
```

Configure and build:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local
cmake --build build
```

> If YaFF was installed to a non-standard prefix, pass it via
> `-DCMAKE_PREFIX_PATH=<prefix>` so that `find_package` can locate
> `YaFFConfig.cmake`.

For complete usage examples of `yaff_generate()` see the [examples/](../examples/) directory.

---

## 2. Integration via Conan

YaFF is not published to ConanCenter. Before using it as a dependency you need
to build it into your local Conan cache once.

### Step 1 — Build YaFF into the local Conan cache

```bash
git clone https://github.com/yandex/yaff.git
cd yaff
conan create . --build=missing -s build_type=Release
```

This places `yaff/<version>` into the local cache (`~/.conan2/p/`) and makes it
available to any project on the same machine.

### Step 2 — Declare the dependency

**`conanfile.py`:**

```python
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class MyAppConan(ConanFile):
    name     = "my_app"
    version  = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("yaff/<version>")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeToolchain(self).generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
```

**`conanfile.txt`** (simpler alternative without build logic):

```ini
[requires]
yaff/<version>

[generators]
CMakeDeps
CMakeToolchain

[layout]
cmake_layout
```

### Step 3 — Install dependencies

```bash
conan install . -s build_type=Release
```

Conan generates a `CMakeUserPresets.json` that wires up the toolchain and
all dependency paths automatically.

### Step 4 — Configure and build

```bash
cmake --preset conan-release
cmake --build --preset conan-release
```

### Step 5 — Consumer `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(YaFF CONFIG REQUIRED)

# After find_package:
# - yaff_generate() is available (injected via cmake_build_modules)
# - yaff::protoc_plugin is defined as an imported executable
# - YAFF_PROTO_IMPORT_DIR points to the installed include directory,
#   so "import yaff/proto/options.proto" works in your .proto files
#   without any extra -I flags.

# ... define your targets, call protobuf_generate() and yaff_generate() ...

target_link_libraries(my_app
    PRIVATE
        yaff::core
        yaff::proto
)
```

For complete usage examples of `yaff_generate()` see the [examples/](../examples/) directory.

---

## 3. `yaff_generate()` Reference

```cmake
yaff_generate(
    [TARGET      <target>]      # CMake target whose .proto SOURCES are processed
    [PROTOS      <file> ...]    # Explicit list of .proto files (alternative to TARGET)
    [OUT_DIR     <dir>]         # Output directory (default: CMAKE_CURRENT_BINARY_DIR)
    [OUT_VAR     <var>]         # Variable to receive the list of generated files
    [TAG         <string>]      # Suffix appended to generated file names
    [NAMESPACE   <string>]      # C++ namespace injected into generated code
    [IMPORT_DIRS <dir> ...]     # Additional proto import search paths
)
```

| Parameter | Required | Description |
|---|---|---|
| `TARGET` | one of `TARGET`/`PROTOS` | Target whose `.proto` sources are discovered; generated files are attached via `target_sources()` |
| `PROTOS` | one of `TARGET`/`PROTOS` | Explicit `.proto` file list; can be combined with `TARGET` |
| `OUT_DIR` | No | Directory where `.yaff.h` / `.yaff.cpp` are written. Defaults to `CMAKE_CURRENT_BINARY_DIR` |
| `OUT_VAR` | No | CMake variable that receives the list of all generated files |
| `TAG` | No | String appended to generated file names: `<name><TAG>.yaff.h`. Useful when generating multiple views from one schema |
| `NAMESPACE` | No | C++ namespace for the generated accessor types |
| `IMPORT_DIRS` | No | Extra directories passed as `-I` to `protoc`. `CMAKE_CURRENT_SOURCE_DIR` and `YAFF_PROTO_IMPORT_DIR` are always included automatically |

For complete usage examples see the [examples/](../examples/) directory.
