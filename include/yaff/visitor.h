#pragma once

#include <sstream>

#include "any_table.h"
#include "any_vector.h"
#include "reflect.h"
#include "util.h"
#include "vector.h"

namespace NYaFF::NReflect {

class IVisitor {
public:
    virtual ~IVisitor() = default;

    virtual void OnTableStart() = 0;
    virtual void OnTableEnd() = 0;

    virtual void OnVectorStart() = 0;
    virtual void OnVectorEnd() = 0;

    virtual void OnField(const char* name) = 0;
    virtual void OnElement(size_t i) = 0;

    virtual void OnBool(bool val) = 0;
    virtual void OnInt32(int32_t val) = 0;
    virtual void OnUint32(uint32_t val) = 0;
    virtual void OnInt64(int64_t val) = 0;
    virtual void OnUint64(uint64_t val) = 0;
    virtual void OnFloat(float val) = 0;
    virtual void OnDouble(double val) = 0;
    virtual void OnEnum(int32_t val, const char* enumName) = 0;

    virtual void OnString(std::string_view val) = 0;
};

class TVisitor {
public:
    explicit TVisitor(IVisitor* visitor) : Visitor_(visitor) {
    }

    void VisitTable(const void* ptr, const TTableDescriptor* desc) {
        if (!ptr) {
            return;
        }
        YAFF_REQUIRE(desc);

        Visitor_->OnTableStart();
        const auto resolver = MakeFieldResolverFunc(desc);
        for (uint64_t i = 0; i < desc->FieldCount; ++i) {
            const auto& field = desc->Fields[i];
            if (field.Deprecated) {
                continue;
            }

            const auto& type = *field.Type;
            if (type.Type == EType::TYPE_TABLE) {
                DispatchTableField(desc->Layout, ptr, field, resolver);
            } else if (type.Type == EType::TYPE_STRING) {
                DispatchStringField(desc->Layout, ptr, field, resolver);
            } else if (type.Type == EType::TYPE_VECTOR) {
                DispatchVectorField(desc->Layout, ptr, field, resolver);
            } else {
                DispatchScalarField(desc->Layout, ptr, field, resolver);
            }
        }
        Visitor_->OnTableEnd();
    }

private:
    static const char* ResolveEnum(int64_t scalar, const TEnumDescriptor* enm) {
        if (!enm) {
            return nullptr;
        }
        for (uint64_t i = 0; i < enm->ValueCount; ++i) {
            const auto& value = enm->Values[i];
            if (value.Value == scalar) {
                return value.Name;
            }
        }
        return nullptr;
    }

    template <typename T, typename F>
    void VisitScalarVector(const void* ptr, F&& cb) {
        const auto& vector = *ReadLayout<TAnyVector>(ptr);
        for (size_t i = 0; i < vector.Size(); ++i) {
            cb(i, vector.GetValue<T>(i));
        }
    }

    template <typename T, typename F>
    void VisitScalarField(T val, F&& cb) {
        cb(val);
    }

    void VisitVector(const void* ptr, const TTypeDescriptor* element) {
        if (!ptr) {
            return;
        }

        Visitor_->OnVectorStart();
        if (element->Type == EType::TYPE_TABLE) {
            DispatchTableVector(ptr, element);
        } else if (element->Type == EType::TYPE_STRING) {
            DispatchStringVector(ptr, element);
        } else {
            DispatchScalarVector(ptr, element);
        }
        Visitor_->OnVectorEnd();
    }

    void DispatchTableVector(const void* ptr, const TTypeDescriptor* element) {
        const auto& vector = *ReadLayout<TAnyVector>(ptr);
        const size_t inlineSize = (element->Inline ? InlineSize(*element) : 0);
        for (size_t i = 0; i < vector.Size(); ++i) {
            Visitor_->OnElement(i);
            const void* ith = inlineSize ? vector.GetLayout<char>(i, inlineSize) : vector.GetLayout<char>(i);
            VisitTable(ith, element->Descriptor.Table);
        }
    }

    void DispatchStringVector(const void* ptr, const TTypeDescriptor*) {
        const auto& vector = *ReadLayout<TAnyVector>(ptr);
        for (size_t i = 0; i < vector.Size(); ++i) {
            const auto& str = *vector.GetLayout<NYaFF::TString>(i);
            Visitor_->OnElement(i);
            Visitor_->OnString(str);
        }
    }

    void DispatchScalarVector(const void* ptr, const TTypeDescriptor* element) {
        switch (element->Type) {
            case EType::TYPE_BOOL: {
                VisitScalarVector<bool>(ptr, [&](size_t i, bool val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnBool(val);
                });
                break;
            }
            case EType::TYPE_INT32: {
                VisitScalarVector<int32_t>(ptr, [&](size_t i, int32_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnInt32(val);
                });
                break;
            }
            case EType::TYPE_UINT32: {
                VisitScalarVector<uint32_t>(ptr, [&](size_t i, uint32_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnUint32(val);
                });
                break;
            }
            case EType::TYPE_INT64: {
                VisitScalarVector<int64_t>(ptr, [&](size_t i, int64_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnInt64(val);
                });
                break;
            }
            case EType::TYPE_UINT64: {
                VisitScalarVector<uint64_t>(ptr, [&](size_t i, uint64_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnUint64(val);
                });
                break;
            }
            case EType::TYPE_FLOAT: {
                VisitScalarVector<float>(ptr, [&](size_t i, float val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnFloat(val);
                });
                break;
            }
            case EType::TYPE_DOUBLE: {
                VisitScalarVector<double>(ptr, [&](size_t i, double val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnDouble(val);
                });
                break;
            }
            case EType::TYPE_ENUM: {
                VisitScalarVector<int32_t>(ptr, [&](size_t i, int32_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnEnum(val, ResolveEnum(val, element->Descriptor.Enum));
                });
                break;
            }
            default:
                YAFF_THROW("unknown type");
        }
    }

    void DispatchTableField(const ETableLayout layout, const void* ptr, const TFieldDescriptor& field,
                            const TFieldResolverFunc& resolver) {
        const void* child = ReadLayout<TAnyTable>(ptr)->ReadLayout<char>(layout, field.Id, &resolver);
        if (child) {
            Visitor_->OnField(field.Name);
            VisitTable(child, field.Type->Descriptor.Table);
        }
    }

    void DispatchStringField(const ETableLayout layout, const void* ptr, const TFieldDescriptor& field,
                             const TFieldResolverFunc& resolver) {
        const auto* strPtr = ReadLayout<TAnyTable>(ptr)->ReadLayout<TString>(layout, field.Id, &resolver);
        if (strPtr) {
            Visitor_->OnField(field.Name);
            Visitor_->OnString(*strPtr);
        }
    }

    void DispatchVectorField(const ETableLayout layout, const void* ptr, const TFieldDescriptor& field,
                             const TFieldResolverFunc& resolver) {
        const void* vecPtr = ReadLayout<TAnyTable>(ptr)->ReadLayout<char>(layout, field.Id, &resolver);
        if (vecPtr) {
            Visitor_->OnField(field.Name);
            VisitVector(vecPtr, field.Type->Descriptor.Element);
        }
    }

    void DispatchScalarField(const ETableLayout layout, const void* ptr, const TFieldDescriptor& field,
                             const TFieldResolverFunc& resolver) {
        const auto& table = *ReadLayout<TAnyTable>(ptr);
        const auto& type = *field.Type;

        switch (type.Type) {
            case EType::TYPE_BOOL: {
                VisitScalarField<bool>(table.ReadValue<bool>(layout, field.Id, type.Default.Bool, &resolver),
                                       [&](bool v) {
                                           Visitor_->OnField(field.Name);
                                           Visitor_->OnBool(v);
                                       });
                break;
            }
            case EType::TYPE_INT32: {
                VisitScalarField<int32_t>(table.ReadValue<int32_t>(layout, field.Id, type.Default.Int32, &resolver),
                                          [&](int32_t v) {
                                              Visitor_->OnField(field.Name);
                                              Visitor_->OnInt32(v);
                                          });
                break;
            }
            case EType::TYPE_UINT32: {
                VisitScalarField<uint32_t>(table.ReadValue<uint32_t>(layout, field.Id, type.Default.Uint32, &resolver),
                                           [&](uint32_t v) {
                                               Visitor_->OnField(field.Name);
                                               Visitor_->OnUint32(v);
                                           });
                break;
            }
            case EType::TYPE_INT64: {
                VisitScalarField<int64_t>(table.ReadValue<int64_t>(layout, field.Id, type.Default.Int64, &resolver),
                                          [&](int64_t v) {
                                              Visitor_->OnField(field.Name);
                                              Visitor_->OnInt64(v);
                                          });
                break;
            }
            case EType::TYPE_UINT64: {
                VisitScalarField<uint64_t>(table.ReadValue<uint64_t>(layout, field.Id, type.Default.Uint64, &resolver),
                                           [&](uint64_t v) {
                                               Visitor_->OnField(field.Name);
                                               Visitor_->OnUint64(v);
                                           });
                break;
            }
            case EType::TYPE_FLOAT: {
                VisitScalarField<float>(table.ReadValue<float>(layout, field.Id, type.Default.Float, &resolver),
                                        [&](float v) {
                                            Visitor_->OnField(field.Name);
                                            Visitor_->OnFloat(v);
                                        });
                break;
            }
            case EType::TYPE_DOUBLE: {
                VisitScalarField<double>(table.ReadValue<double>(layout, field.Id, type.Default.Double, &resolver),
                                         [&](double v) {
                                             Visitor_->OnField(field.Name);
                                             Visitor_->OnDouble(v);
                                         });
                break;
            }
            case EType::TYPE_ENUM: {
                VisitScalarField<int32_t>(table.ReadValue<int32_t>(layout, field.Id, type.Default.Int32, &resolver),
                                          [&](int32_t v) {
                                              Visitor_->OnField(field.Name);
                                              Visitor_->OnEnum(v, ResolveEnum(v, type.Descriptor.Enum));
                                          });
                break;
            }
            default:
                YAFF_THROW("unknown type");
        }
    }

    IVisitor* Visitor_;
};

class TPrinter : public IVisitor {
public:
    TPrinter(std::ostream& out, const std::string& ident = "\t") : Writer_(out, ident) {
    }

    virtual void OnTableStart() {
        Writer_ |= "{";
        Writer_.IncrementIdentLevel();
    };

    virtual void OnTableEnd() {
        Writer_.DecrementIdentLevel();
        Writer_ |= "}";
    };

    virtual void OnVectorStart() {
        Writer_ |= "[";
        Writer_.IncrementIdentLevel();
    }

    virtual void OnVectorEnd() {
        Writer_.DecrementIdentLevel();
        Writer_ |= "]";
    }

    virtual void OnField(const char* name) {
        Writer_ |= std::string{name} + ": \\";
    }

    virtual void OnElement(size_t) {
    }

    virtual void OnString(std::string_view val) {
        Writer_ |= "\"" + std::string{val} + "\"";
    }

    virtual void OnEnum(int32_t val, const char* enumName) {
        Writer_ |= (enumName ? enumName : std::to_string(val));
    }

    virtual void OnBool(bool val) {
        Writer_ |= std::to_string(val);
    }

    virtual void OnInt32(int32_t val) {
        Writer_ |= std::to_string(val);
    }

    virtual void OnUint32(uint32_t val) {
        Writer_ |= std::to_string(val);
    }

    virtual void OnInt64(int64_t val) {
        Writer_ |= std::to_string(val);
    }

    virtual void OnUint64(uint64_t val) {
        Writer_ |= std::to_string(val);
    }

    virtual void OnFloat(float val) {
        Writer_ |= std::to_string(val);
    }

    virtual void OnDouble(double val) {
        Writer_ |= std::to_string(val);
    }

private:
    TCodeWriter Writer_;
};

inline std::string DebugString(const void* ptr, const TTableDescriptor* desc) {
    std::ostringstream out;
    TPrinter printer(out);
    TVisitor{&printer}.VisitTable(ptr, desc);
    return out.str();
}

}  // namespace NYaFF::NReflect
