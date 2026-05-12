#include "ir_compat.h"

#include <unordered_set>

namespace yaff::compilation {

struct IRCompatibilityChecker {
public:
    explicit IRCompatibilityChecker(std::unique_ptr<AbstractErrorHandler> errors);

    bool Check(const ir::IR& base, const ir::IR& patched) &&;

private:
    struct CheckedMessagesHash {
        inline size_t operator()(const std::pair<const ir::MessageDef*, const ir::MessageDef*>& p) const noexcept {
            return std::hash<const void*>{}(p.first) ^ std::hash<const void*>{}(p.second);
        }
    };
    using CheckedMessagesSet =
        std::unordered_set<std::pair<const ir::MessageDef*, const ir::MessageDef*>, CheckedMessagesHash>;

    static std::string ConcatNames(const std::string& base, const std::string& name);
    static std::string GetTypeDefault(const ir::TypeDef& type);
    static bool CheckBaseType(const Type base, const Type patched);

    void MarkCritical();

    void CheckMessage(const ir::MessageDef& base, const ir::MessageDef& patched);
    void CheckField(const std::string& msgName, const ir::MessageDef::FieldDef& base,
                    const ir::MessageDef::FieldDef& patched);
    void CheckTypes(const std::string& fieldName, const ir::TypeDef& base, const ir::TypeDef& patched);

    CheckedMessagesSet CheckedMessages_;
    std::unique_ptr<AbstractErrorHandler> Errors_;

    bool HasCriticalErrors_;
};

IRCompatibilityChecker::IRCompatibilityChecker(std::unique_ptr<AbstractErrorHandler> errors)
    : Errors_(std::move(errors)), HasCriticalErrors_(false) {
}

bool IRCompatibilityChecker::Check(const ir::IR& lhs, const ir::IR& rhs) && {
    for (const auto& [lhsName, lhsMessage] : lhs.Messages.Symbols) {
        const auto* rhsMessage = FindKey(rhs.Messages.Symbols, lhsName);
        if (!rhsMessage) {
            Errors_->Warning(ErrorType::COMPAT_MESSAGE_REMOVED, lhsName, "message has been removed in new schema");
            continue;
        }
        CheckMessage(lhsMessage, *rhsMessage);
    }
    return !HasCriticalErrors_;
}

void IRCompatibilityChecker::CheckMessage(const ir::MessageDef& base, const ir::MessageDef& patched) {
    auto [_, emplace] = CheckedMessages_.emplace(std::make_pair(&base, &patched));
    if (!emplace) {
        return;
    }

    if (base.Name != patched.Name) {
        Errors_->Warning(ErrorType::COMPAT_MESSAGE_NAME_CHANGED, base.Name,
                         "message name changed to '" + patched.Name + "'");
    }

    if (base.Layout != patched.Layout) {
        Errors_->Error(ErrorType::COMPAT_MESSAGE_LAYOUT_CHANGED, base.Name, "message layout changed");
        MarkCritical();
    }

    const uint64_t commonFieldCount = std::min(base.Fields.size(), patched.Fields.size());
    for (uint64_t i = 0; i < commonFieldCount; ++i) {
        CheckField(base.Name, base.Fields[i], patched.Fields[i]);
    }
}

void IRCompatibilityChecker::CheckField(const std::string& msgName, const ir::MessageDef::FieldDef& base,
                                        const ir::MessageDef::FieldDef& patched) {
    YAFF_REQUIRE(base.Id == patched.Id);

    const std::string fieldName = (!base.Deprecated ? base.Name : "Deprecated" + std::to_string(base.Id));
    const std::string fullName = ConcatNames(msgName, fieldName);

    if (base.Deprecated && !patched.Deprecated) {
        Errors_->Warning(ErrorType::COMPAT_DEPRECATED_REMOVE, fullName, "deprecated field reused as non-deprecated");
    }

    if (base.FlatOffset != patched.FlatOffset) {
        Errors_->Error(ErrorType::COMPAT_OFFSET_MISMATCH, fullName, "field offset value changed");
        MarkCritical();
    }

    CheckTypes(fullName, *base.Type, *patched.Type);
}

void IRCompatibilityChecker::CheckTypes(const std::string& fieldName, const ir::TypeDef& base,
                                        const ir::TypeDef& patched) {
    const std::string baseDefault = GetTypeDefault(base);
    const std::string patchedDefault = GetTypeDefault(patched);
    if (baseDefault != patchedDefault) {
        Errors_->Error(ErrorType::COMPAT_DEFAULT_MISMATCH, fieldName,
                       "default value changed from '" + baseDefault + "' to '" + patchedDefault + "'");
        MarkCritical();
    }

    if (!CheckBaseType(base.Type, patched.Type)) {
        Errors_->Error(ErrorType::COMPAT_BASE_TYPE_MISMATCH, fieldName,
                       "base type of field has changed from '" + base.ToString() + "' to '" + patched.ToString() + "'");
        MarkCritical();
    }

    if (base.Type == Type::TYPE_MESSAGE && base.MessageDef && patched.MessageDef &&
        base.MessageDef != patched.MessageDef) {
        CheckMessage(*base.MessageDef, *patched.MessageDef);
    }

    if (base.Type == Type::TYPE_VECTOR && base.ElementType && patched.ElementType &&
        base.ElementType != patched.ElementType) {
        CheckTypes(ConcatNames(fieldName, "vector_type"), *base.ElementType, *patched.ElementType);
    }
}

void IRCompatibilityChecker::MarkCritical() {
    HasCriticalErrors_ = true;
}

bool IRCompatibilityChecker::CheckBaseType(const Type base, const Type patched) {
    if (base == patched) {
        return true;
    }
    if (ir::IsScalar(base) && ir::IsScalar(patched)) {
        return ir::InlineSize(base) == ir::InlineSize(patched);
    }
    return false;  // Composite types can not change base type;
}

std::string IRCompatibilityChecker::GetTypeDefault(const ir::TypeDef& type) {
    const std::string* defaultPtr = FindKey(type.Modifiers, ir::DEFAULT_MODIFIER_NAME);
    return (defaultPtr ? *defaultPtr : "");
}

std::string IRCompatibilityChecker::ConcatNames(const std::string& base, const std::string& name) {
    return base + "." + name;
}

bool CheckIRCompatibility(const ir::IR& base, const ir::IR& patched,
                          std::unique_ptr<AbstractErrorHandler> errorHandler) {
    YAFF_REQUIRE(errorHandler);
    return IRCompatibilityChecker{std::move(errorHandler)}.Check(base, patched);
}

}  // namespace yaff::compilation
