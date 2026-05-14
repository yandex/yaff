#pragma once

#include <sstream>

#include "any_array.h"
#include "any_message.h"
#include "array.h"
#include "reflect.h"
#include "util.h"

namespace yaff::reflect {

class AbstractVisitor {
public:
    virtual ~AbstractVisitor() = default;

    virtual void OnMessageStart() = 0;
    virtual void OnMessageEnd() = 0;

    virtual void OnArrayStart() = 0;
    virtual void OnArrayEnd() = 0;

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

class Visitor {
public:
    explicit Visitor(AbstractVisitor* visitor) : Visitor_(visitor) {
    }

    void VisitMessage(const void* ptr, const MessageDescriptor* desc) {
        if (!ptr) {
            return;
        }
        YAFF_REQUIRE(desc);

        Visitor_->OnMessageStart();
        const auto resolver = MakeFieldResolverFunc(desc);
        for (uint64_t i = 0; i < desc->FieldCount; ++i) {
            const auto& field = desc->Fields[i];
            if (field.Deprecated) {
                continue;
            }

            const auto& type = *field.Type;
            if (type.Type == Type::TYPE_MESSAGE) {
                DispatchMessageField(desc->Layout, ptr, field, resolver);
            } else if (type.Type == Type::TYPE_STRING) {
                DispatchStringField(desc->Layout, ptr, field, resolver);
            } else if (type.Type == Type::TYPE_ARRAY) {
                DispatchArrayField(desc->Layout, ptr, field, resolver);
            } else {
                DispatchScalarField(desc->Layout, ptr, field, resolver);
            }
        }
        Visitor_->OnMessageEnd();
    }

private:
    static const char* ResolveEnum(int64_t scalar, const EnumDescriptor* enm) {
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
    void VisitScalarArray(const void* ptr, F&& cb) {
        const auto& array = *ReadLayout<AnyArray>(ptr);
        for (size_t i = 0; i < array.Size(); ++i) {
            cb(i, array.GetValue<T>(i));
        }
    }

    template <typename T, typename F>
    void VisitScalarField(T val, F&& cb) {
        cb(val);
    }

    void VisitArray(const void* ptr, const TypeDescriptor* element) {
        if (!ptr) {
            return;
        }

        Visitor_->OnArrayStart();
        if (element->Type == Type::TYPE_MESSAGE) {
            DispatchMessageArray(ptr, element);
        } else if (element->Type == Type::TYPE_STRING) {
            DispatchStringArray(ptr, element);
        } else {
            DispatchScalarArray(ptr, element);
        }
        Visitor_->OnArrayEnd();
    }

    void DispatchMessageArray(const void* ptr, const TypeDescriptor* element) {
        const auto& array = *ReadLayout<AnyArray>(ptr);
        const size_t inlineSize = (element->Inline ? InlineSize(*element) : 0);
        for (size_t i = 0; i < array.Size(); ++i) {
            Visitor_->OnElement(i);
            const void* ith = inlineSize ? array.GetLayout<char>(i, inlineSize) : array.GetLayout<char>(i);
            VisitMessage(ith, element->Descriptor.Message);
        }
    }

    void DispatchStringArray(const void* ptr, const TypeDescriptor*) {
        const auto& array = *ReadLayout<AnyArray>(ptr);
        for (size_t i = 0; i < array.Size(); ++i) {
            const auto& str = *array.GetLayout<yaff::String>(i);
            Visitor_->OnElement(i);
            Visitor_->OnString(str);
        }
    }

    void DispatchScalarArray(const void* ptr, const TypeDescriptor* element) {
        switch (element->Type) {
            case Type::TYPE_BOOL: {
                VisitScalarArray<bool>(ptr, [&](size_t i, bool val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnBool(val);
                });
                break;
            }
            case Type::TYPE_INT32: {
                VisitScalarArray<int32_t>(ptr, [&](size_t i, int32_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnInt32(val);
                });
                break;
            }
            case Type::TYPE_UINT32: {
                VisitScalarArray<uint32_t>(ptr, [&](size_t i, uint32_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnUint32(val);
                });
                break;
            }
            case Type::TYPE_INT64: {
                VisitScalarArray<int64_t>(ptr, [&](size_t i, int64_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnInt64(val);
                });
                break;
            }
            case Type::TYPE_UINT64: {
                VisitScalarArray<uint64_t>(ptr, [&](size_t i, uint64_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnUint64(val);
                });
                break;
            }
            case Type::TYPE_FLOAT: {
                VisitScalarArray<float>(ptr, [&](size_t i, float val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnFloat(val);
                });
                break;
            }
            case Type::TYPE_DOUBLE: {
                VisitScalarArray<double>(ptr, [&](size_t i, double val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnDouble(val);
                });
                break;
            }
            case Type::TYPE_ENUM: {
                VisitScalarArray<int32_t>(ptr, [&](size_t i, int32_t val) {
                    Visitor_->OnElement(i);
                    Visitor_->OnEnum(val, ResolveEnum(val, element->Descriptor.Enum));
                });
                break;
            }
            default:
                YAFF_THROW("unknown type");
        }
    }

    void DispatchMessageField(const MessageLayout layout, const void* ptr, const FieldDescriptor& field,
                              const FieldResolver& resolver) {
        const void* child = ReadLayout<AnyMessage>(ptr)->ReadLayout<char>(layout, field.Id, &resolver);
        if (child) {
            Visitor_->OnField(field.Name);
            VisitMessage(child, field.Type->Descriptor.Message);
        }
    }

    void DispatchStringField(const MessageLayout layout, const void* ptr, const FieldDescriptor& field,
                             const FieldResolver& resolver) {
        const auto* strPtr = ReadLayout<AnyMessage>(ptr)->ReadLayout<String>(layout, field.Id, &resolver);
        if (strPtr) {
            Visitor_->OnField(field.Name);
            Visitor_->OnString(*strPtr);
        }
    }

    void DispatchArrayField(const MessageLayout layout, const void* ptr, const FieldDescriptor& field,
                            const FieldResolver& resolver) {
        const void* vecPtr = ReadLayout<AnyMessage>(ptr)->ReadLayout<char>(layout, field.Id, &resolver);
        if (vecPtr) {
            Visitor_->OnField(field.Name);
            VisitArray(vecPtr, field.Type->Descriptor.Element);
        }
    }

    void DispatchScalarField(const MessageLayout layout, const void* ptr, const FieldDescriptor& field,
                             const FieldResolver& resolver) {
        const auto& msg = *ReadLayout<AnyMessage>(ptr);
        const auto& type = *field.Type;

        switch (type.Type) {
            case Type::TYPE_BOOL: {
                VisitScalarField<bool>(msg.ReadValue<bool>(layout, field.Id, type.Default.Bool, &resolver),
                                       [&](bool v) {
                                           Visitor_->OnField(field.Name);
                                           Visitor_->OnBool(v);
                                       });
                break;
            }
            case Type::TYPE_INT32: {
                VisitScalarField<int32_t>(msg.ReadValue<int32_t>(layout, field.Id, type.Default.Int32, &resolver),
                                          [&](int32_t v) {
                                              Visitor_->OnField(field.Name);
                                              Visitor_->OnInt32(v);
                                          });
                break;
            }
            case Type::TYPE_UINT32: {
                VisitScalarField<uint32_t>(msg.ReadValue<uint32_t>(layout, field.Id, type.Default.Uint32, &resolver),
                                           [&](uint32_t v) {
                                               Visitor_->OnField(field.Name);
                                               Visitor_->OnUint32(v);
                                           });
                break;
            }
            case Type::TYPE_INT64: {
                VisitScalarField<int64_t>(msg.ReadValue<int64_t>(layout, field.Id, type.Default.Int64, &resolver),
                                          [&](int64_t v) {
                                              Visitor_->OnField(field.Name);
                                              Visitor_->OnInt64(v);
                                          });
                break;
            }
            case Type::TYPE_UINT64: {
                VisitScalarField<uint64_t>(msg.ReadValue<uint64_t>(layout, field.Id, type.Default.Uint64, &resolver),
                                           [&](uint64_t v) {
                                               Visitor_->OnField(field.Name);
                                               Visitor_->OnUint64(v);
                                           });
                break;
            }
            case Type::TYPE_FLOAT: {
                VisitScalarField<float>(msg.ReadValue<float>(layout, field.Id, type.Default.Float, &resolver),
                                        [&](float v) {
                                            Visitor_->OnField(field.Name);
                                            Visitor_->OnFloat(v);
                                        });
                break;
            }
            case Type::TYPE_DOUBLE: {
                VisitScalarField<double>(msg.ReadValue<double>(layout, field.Id, type.Default.Double, &resolver),
                                         [&](double v) {
                                             Visitor_->OnField(field.Name);
                                             Visitor_->OnDouble(v);
                                         });
                break;
            }
            case Type::TYPE_ENUM: {
                VisitScalarField<int32_t>(msg.ReadValue<int32_t>(layout, field.Id, type.Default.Int32, &resolver),
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

    AbstractVisitor* Visitor_;
};

class Printer : public AbstractVisitor {
public:
    Printer(std::ostream& out, const std::string& ident = "\t") : Writer_(out, ident) {
    }

    virtual void OnMessageStart() {
        Writer_ |= "{";
        Writer_.IncrementIdentLevel();
    };

    virtual void OnMessageEnd() {
        Writer_.DecrementIdentLevel();
        Writer_ |= "}";
    };

    virtual void OnArrayStart() {
        Writer_ |= "[";
        Writer_.IncrementIdentLevel();
    }

    virtual void OnArrayEnd() {
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
    CodeWriter Writer_;
};

inline std::string DebugString(const void* ptr, const MessageDescriptor* desc) {
    std::ostringstream out;
    Printer printer(out);
    Visitor{&printer}.VisitMessage(ptr, desc);
    return out.str();
}

}  // namespace yaff::reflect
