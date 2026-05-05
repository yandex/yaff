#include "ir_compat.h"

#include <unordered_set>

namespace NYaFF::NCompile {

struct TIRCompatibilityChecker {
public:
    explicit TIRCompatibilityChecker(std::unique_ptr<IErrorHandler> errors);

    bool Check(const NIR::TIR& base, const NIR::TIR& patched) &&;

private:
    struct TCheckedMessagesHash {
        inline size_t operator()(const std::pair<const NIR::TMessageDef*, const NIR::TMessageDef*>& p) const noexcept {
            return std::hash<const void*>{}(p.first) ^ std::hash<const void*>{}(p.second);
        }
    };
    using TCheckedMessagesSet =
        std::unordered_set<std::pair<const NIR::TMessageDef*, const NIR::TMessageDef*>, TCheckedMessagesHash>;

    static std::string ConcatNames(const std::string& base, const std::string& name);
    static std::string GetTypeDefault(const NIR::TType& type);
    static bool CheckBaseType(const EType base, const EType patched);

    void MarkCritical();

    void CheckMessage(const NIR::TMessageDef& base, const NIR::TMessageDef& patched);
    void CheckField(const std::string& msgName, const NIR::TMessageDef::TFieldDef& base,
                    const NIR::TMessageDef::TFieldDef& patched);
    void CheckTypes(const std::string& fieldName, const NIR::TType& base, const NIR::TType& patched);

    TCheckedMessagesSet CheckedMessages_;
    std::unique_ptr<IErrorHandler> Errors_;

    bool HasCriticalErrors_;
};

TIRCompatibilityChecker::TIRCompatibilityChecker(std::unique_ptr<IErrorHandler> errors)
    : Errors_(std::move(errors)), HasCriticalErrors_(false) {
}

bool TIRCompatibilityChecker::Check(const NIR::TIR& lhs, const NIR::TIR& rhs) && {
    for (const auto& [lhsName, lhsMessage] : lhs.Messages.Symbols) {
        const auto* rhsMessage = FindKey(rhs.Messages.Symbols, lhsName);
        if (!rhsMessage) {
            Errors_->Warning(EErrorType::COMPAT_MESSAGE_REMOVED, lhsName, "message has been removed in new schema");
            continue;
        }
        CheckMessage(lhsMessage, *rhsMessage);
    }
    return !HasCriticalErrors_;
}

void TIRCompatibilityChecker::CheckMessage(const NIR::TMessageDef& base, const NIR::TMessageDef& patched) {
    auto [_, emplace] = CheckedMessages_.emplace(std::make_pair(&base, &patched));
    if (!emplace) {
        return;
    }

    if (base.Name != patched.Name) {
        Errors_->Warning(EErrorType::COMPAT_MESSAGE_NAME_CHANGED, base.Name,
                         "message name changed to '" + patched.Name + "'");
    }

    if (base.Layout != patched.Layout) {
        Errors_->Error(EErrorType::COMPAT_MESSAGE_LAYOUT_CHANGED, base.Name, "message layout changed");
        MarkCritical();
    }

    const uint64_t commonFieldCount = std::min(base.Fields.size(), patched.Fields.size());
    for (uint64_t i = 0; i < commonFieldCount; ++i) {
        CheckField(base.Name, base.Fields[i], patched.Fields[i]);
    }
}

void TIRCompatibilityChecker::CheckField(const std::string& msgName, const NIR::TMessageDef::TFieldDef& base,
                                         const NIR::TMessageDef::TFieldDef& patched) {
    YAFF_REQUIRE(base.Id == patched.Id);

    const std::string fieldName = (!base.Deprecated ? base.Name : "Deprecated" + std::to_string(base.Id));
    const std::string fullName = ConcatNames(msgName, fieldName);

    if (base.Deprecated && !patched.Deprecated) {
        Errors_->Warning(EErrorType::COMPAT_DEPRECATED_REMOVE, fullName, "deprecated field reused as non-deprecated");
    }

    if (base.FlatOffset != patched.FlatOffset) {
        Errors_->Error(EErrorType::COMPAT_OFFSET_MISMATCH, fullName, "field offset value changed");
        MarkCritical();
    }

    CheckTypes(fullName, *base.Type, *patched.Type);
}

void TIRCompatibilityChecker::CheckTypes(const std::string& fieldName, const NIR::TType& base,
                                         const NIR::TType& patched) {
    const std::string baseDefault = GetTypeDefault(base);
    const std::string patchedDefault = GetTypeDefault(patched);
    if (baseDefault != patchedDefault) {
        Errors_->Error(EErrorType::COMPAT_DEFAULT_MISMATCH, fieldName,
                       "default value changed from '" + baseDefault + "' to '" + patchedDefault + "'");
        MarkCritical();
    }

    if (!CheckBaseType(base.Type, patched.Type)) {
        Errors_->Error(EErrorType::COMPAT_BASE_TYPE_MISMATCH, fieldName,
                       "base type of field has changed from '" + base.ToString() + "' to '" + patched.ToString() + "'");
        MarkCritical();
    }

    if (base.Type == EType::TYPE_MESSAGE && base.MessageDef && patched.MessageDef &&
        base.MessageDef != patched.MessageDef) {
        CheckMessage(*base.MessageDef, *patched.MessageDef);
    }

    if (base.Type == EType::TYPE_VECTOR && base.ElementType && patched.ElementType &&
        base.ElementType != patched.ElementType) {
        CheckTypes(ConcatNames(fieldName, "vector_type"), *base.ElementType, *patched.ElementType);
    }
}

void TIRCompatibilityChecker::MarkCritical() {
    HasCriticalErrors_ = true;
}

bool TIRCompatibilityChecker::CheckBaseType(const EType base, const EType patched) {
    if (base == patched) {
        return true;
    }
    if (NIR::IsScalar(base) && NIR::IsScalar(patched)) {
        return NIR::InlineSize(base) == NIR::InlineSize(patched);
    }
    return false;  // Composite types can not change base type;
}

std::string TIRCompatibilityChecker::GetTypeDefault(const NIR::TType& type) {
    const std::string* defaultPtr = FindKey(type.Modifiers, NIR::DEFAULT_MODIFIER_NAME);
    return (defaultPtr ? *defaultPtr : "");
}

std::string TIRCompatibilityChecker::ConcatNames(const std::string& base, const std::string& name) {
    return base + "." + name;
}

bool CheckIRCompatibility(const NIR::TIR& base, const NIR::TIR& patched, std::unique_ptr<IErrorHandler> errorHandler) {
    YAFF_REQUIRE(errorHandler);
    return TIRCompatibilityChecker{std::move(errorHandler)}.Check(base, patched);
}

}  // namespace NYaFF::NCompile
