#pragma once

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/JSON/document.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/JsonSerializationResult.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/string/string.h>

namespace Things
{
    //! Deserializer settings that report every problem as a warning naming the context, such as a blueprint and part,
    //! and replace containers instead of appending to their defaults.
    inline AZ::JsonDeserializerSettings MakeDeserializerSettings(AZStd::string context)
    {
        using namespace AZ::JsonSerializationResult;

        AZ::JsonDeserializerSettings settings;
        settings.m_clearContainers = true;
        settings.m_reporting = [context = AZStd::move(context)](AZStd::string_view message, ResultCode result, AZStd::string_view path)
        {
            if (result.GetProcessing() != Processing::Completed || result.GetOutcome() == Outcomes::Skipped)
            {
                AZ_Warning("Things", false, "%s: %.*s (%s)", context.c_str(), AZ_STRING_ARG(message), result.ToString(path).c_str());
            }
            return result;
        };
        return settings;
    }

    //! Loads JSON into an existing reflected object, warning about every field that doesn't fit.
    //! Returns false when loading halted.
    inline bool LoadTypedJson(void* object, const AZ::TypeId& objectType, const rapidjson::Value& value, AZStd::string context)
    {
        const AZ::JsonSerializationResult::ResultCode result =
            AZ::JsonSerialization::Load(object, objectType, value, MakeDeserializerSettings(AZStd::move(context)));
        return result.GetProcessing() != AZ::JsonSerializationResult::Processing::Halted;
    }

    //! The first part of a resolved blueprint whose "$type" is the given class name, or nullptr.
    //! Lets a game read data parts (such as materials or terrain) without spawning the blueprint.
    inline const rapidjson::Value* FindPartOfType(const rapidjson::Value& blueprint, AZStd::string_view type)
    {
        if (!blueprint.IsObject())
        {
            return nullptr;
        }
        const auto parts = blueprint.FindMember("Parts");
        if (parts == blueprint.MemberEnd() || !parts->value.IsObject())
        {
            return nullptr;
        }
        for (const auto& part : parts->value.GetObject())
        {
            if (!part.value.IsObject())
            {
                continue;
            }
            const auto typeField = part.value.FindMember(AZ::JsonSerialization::TypeIdFieldIdentifier);
            if (typeField != part.value.MemberEnd() && typeField->value.IsString() &&
                AZStd::string_view(typeField->value.GetString(), typeField->value.GetStringLength()) == type)
            {
                return &part.value;
            }
        }
        return nullptr;
    }

    //! Creates an object of the class named by the value's "$type" (a class name or TypeId) and loads the value into it.
    //! The class must derive from Base. Returns nullptr, with a warning naming the context, when the type is missing,
    //! unknown or not a Base. The caller owns the result.
    template<class Base>
    Base* CreateFromTypedJson(const rapidjson::Value& value, const AZStd::string& context)
    {
        if (!value.IsObject())
        {
            AZ_Warning("Things", false, "%s: expected an object with a \"$type\".", context.c_str());
            return nullptr;
        }

        const auto typeField = value.FindMember(AZ::JsonSerialization::TypeIdFieldIdentifier);
        if (typeField == value.MemberEnd())
        {
            AZ_Warning("Things", false, "%s: has no \"$type\".", context.c_str());
            return nullptr;
        }

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);
        AZ_Assert(serializeContext, "CreateFromTypedJson needs a SerializeContext.");

        const AZ::TypeId baseType = azrtti_typeid<Base>();
        AZ::TypeId type = AZ::TypeId::CreateNull();
        AZ::JsonSerialization::LoadTypeId(type, typeField->value, &baseType, "", MakeDeserializerSettings(context));
        const AZ::SerializeContext::ClassData* classData = type.IsNull() ? nullptr : serializeContext->FindClassData(type);
        if (!classData || !classData->m_factory || !classData->m_azRtti)
        {
            AZ_Warning("Things", false, "%s: \"$type\" names no class that can be created.", context.c_str());
            return nullptr;
        }

        void* instance = classData->m_factory->Create(classData->m_name);
        Base* base = static_cast<Base*>(classData->m_azRtti->Cast(instance, baseType));
        if (!base)
        {
            AZ_Warning("Things", false, "%s: \"%s\" is not a %s.", context.c_str(), classData->m_name, AZ::AzTypeInfo<Base>::Name());
            classData->m_factory->Destroy(instance);
            return nullptr;
        }

        LoadTypedJson(instance, type, value, context);
        return base;
    }
} // namespace Things
