#pragma once

#include <string>

namespace NYaFF::NCompile {

enum class EErrorType : int32_t {
    COMPILE_ERROR_NONE = 0,
    COMPAT_MESSAGE_REMOVED = 1,
    COMPAT_MESSAGE_NAME_CHANGED = 2,
    COMPAT_DEPRECATED_REMOVE = 3,
    COMPAT_OFFSET_MISMATCH = 4,
    COMPAT_DEFAULT_MISMATCH = 5,
    COMPAT_BASE_TYPE_MISMATCH = 6,
    COMPAT_MESSAGE_LAYOUT_CHANGED = 7,
};

class IErrorHandler {
public:
    virtual ~IErrorHandler() = default;

    virtual void Error(const EErrorType error, const std::string& name, const std::string& message) = 0;
    virtual void Warning(const EErrorType warning, const std::string& name, const std::string& message) = 0;
};

class TErrorPrinter : public IErrorHandler {
public:
    TErrorPrinter(std::ostream& out, const std::string& delim = "\n");

    void Error(const EErrorType error, const std::string& name, const std::string& message) override;
    void Warning(const EErrorType warning, const std::string& name, const std::string& message) override;

private:
    std::string Delim_;
    std::ostream& Out_;
};

std::string GetErrorMessage(const EErrorType type);

}  // namespace NYaFF::NCompile
