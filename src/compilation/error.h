#pragma once

#include <string>

namespace yaff::compilation {

enum class ErrorType : int32_t {
    COMPILE_ERROR_NONE = 0,
    COMPAT_MESSAGE_REMOVED = 1,
    COMPAT_MESSAGE_NAME_CHANGED = 2,
    COMPAT_DEPRECATED_REMOVE = 3,
    COMPAT_OFFSET_MISMATCH = 4,
    COMPAT_DEFAULT_MISMATCH = 5,
    COMPAT_BASE_TYPE_MISMATCH = 6,
    COMPAT_MESSAGE_LAYOUT_CHANGED = 7,
};

class AbstractErrorHandler {
public:
    virtual ~AbstractErrorHandler() = default;

    virtual void Error(const ErrorType error, const std::string& name, const std::string& message) = 0;
    virtual void Warning(const ErrorType warning, const std::string& name, const std::string& message) = 0;
};

class ErrorPrinter : public AbstractErrorHandler {
public:
    ErrorPrinter(std::ostream& out, const std::string& delim = "\n");

    void Error(const ErrorType error, const std::string& name, const std::string& message) override;
    void Warning(const ErrorType warning, const std::string& name, const std::string& message) override;

private:
    std::string Delim_;
    std::ostream& Out_;
};

std::string GetErrorMessage(const ErrorType type);

}  // namespace yaff::compilation
