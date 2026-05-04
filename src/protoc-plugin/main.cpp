#include <google/protobuf/compiler/plugin.h>

#include "protoc_plugin.h"

int main(int argc, char* argv[]) {
    yaff::tool::ProtobufGeneratorPlugin generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
