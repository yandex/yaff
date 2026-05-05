#include "error.h"

#include <yaff/base.h>

#include <iostream>

namespace NYaFF::NCompile {

TErrorPrinter::TErrorPrinter(std::ostream& out, const std::string& delim) : Delim_(delim), Out_(out) {
}

void TErrorPrinter::Error(const EErrorType error, const std::string& name, const std::string& message) {
    Out_ << "ERROR [" << GetErrorMessage(error) << "]: " << name << " incompatible changes: " << message << Delim_;
}

void TErrorPrinter::Warning(const EErrorType warning, const std::string& name, const std::string& message) {
    Out_ << "WARNING [" << GetErrorMessage(warning) << "]: " << name << " dangerous changes: " << message << Delim_;
}

std::string GetErrorMessage(const EErrorType type) {
    switch (type) {
        case EErrorType::COMPILE_ERROR_NONE:
            return "unknown compile error";
        case EErrorType::COMPAT_MESSAGE_REMOVED:
            return "message removed";
        case EErrorType::COMPAT_MESSAGE_NAME_CHANGED:
            return "message name has changed";
        case EErrorType::COMPAT_DEPRECATED_REMOVE:
            return "deprecated field has been removed";
        case EErrorType::COMPAT_OFFSET_MISMATCH:
            return "field offset mismatch";
        case EErrorType::COMPAT_DEFAULT_MISMATCH:
            return "field default value mismatch";
        case EErrorType::COMPAT_BASE_TYPE_MISMATCH:
            return "field base type mismatch";
        case EErrorType::COMPAT_MESSAGE_LAYOUT_CHANGED:
            return "message layout has changed";
    }
    YAFF_THROW("unknown error type");
}

}  // namespace NYaFF::NCompile
