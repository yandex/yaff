#include <google/protobuf/compiler/plugin.h>

#include "protoc_plugin.h"

int main(int argc, char* argv[]) {
    NYaFF::NTool::TProtobufGeneratorPlugin generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
