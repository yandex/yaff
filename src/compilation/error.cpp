#include "error.h"

#include <yaff/base.h>

#include <iostream>

namespace yaff::compilation {

ErrorPrinter::ErrorPrinter(std::ostream& out, const std::string& delim) : Delim_(delim), Out_(out) {
}

void ErrorPrinter::Error(const ErrorType error, const std::string& name, const std::string& message) {
    Out_ << "ERROR [" << GetErrorMessage(error) << "]: " << name << " incompatible changes: " << message << Delim_;
}

void ErrorPrinter::Warning(const ErrorType warning, const std::string& name, const std::string& message) {
    Out_ << "WARNING [" << GetErrorMessage(warning) << "]: " << name << " dangerous changes: " << message << Delim_;
}

std::string GetErrorMessage(const ErrorType type) {
    switch (type) {
        case ErrorType::COMPILE_ERROR_NONE:
            return "unknown compile error";
        case ErrorType::COMPAT_MESSAGE_REMOVED:
            return "message removed";
        case ErrorType::COMPAT_MESSAGE_NAME_CHANGED:
            return "message name has changed";
        case ErrorType::COMPAT_DEPRECATED_REMOVE:
            return "deprecated field has been removed";
        case ErrorType::COMPAT_OFFSET_MISMATCH:
            return "field offset mismatch";
        case ErrorType::COMPAT_DEFAULT_MISMATCH:
            return "field default value mismatch";
        case ErrorType::COMPAT_BASE_TYPE_MISMATCH:
            return "field base type mismatch";
        case ErrorType::COMPAT_MESSAGE_LAYOUT_CHANGED:
            return "message layout has changed";
    }
    YAFF_THROW("unknown error type");
}

}  // namespace yaff::compilation
