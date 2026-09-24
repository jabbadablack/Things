#include <Blueprints/BlueprintLibrary.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

namespace Things
{
    namespace
    {
        //! The string view of a JSON string value.
        AZStd::string_view ToView(const rapidjson::Value& value)
        {
            return AZStd::string_view(value.GetString(), value.GetStringLength());
        }

        //! The member's value when it is an object, otherwise nullptr.
        rapidjson::Value* FindObject(rapidjson::Value& value, const char* key)
        {
            if (!value.IsObject())
            {
                return nullptr;
            }
            const auto member = value.FindMember(key);
            return member != value.MemberEnd() && member->value.IsObject() ? &member->value : nullptr;
        }

        //! The member's value when it is an object, otherwise nullptr.
        const rapidjson::Value* FindObject(const rapidjson::Value& value, const char* key)
        {
            return FindObject(const_cast<rapidjson::Value&>(value), key);
        }

        //! The part's "$type", or an empty view when it has none.
        AZStd::string_view GetPartType(const rapidjson::Value& part)
        {
            const auto type = part.FindMember(BlueprintLibrary::TypeKey);
            return type != part.MemberEnd() && type->value.IsString() ? ToView(type->value) : AZStd::string_view();
        }
    } // namespace

    void BlueprintLibrary::Clear()
    {
        m_raw.clear();
        m_resolved.clear();
    }

    bool BlueprintLibrary::AddFile(const rapidjson::Value& root, AZStd::string_view source)
    {
        if (!root.IsObject())
        {
            AZ_Warning("Things", false, "Blueprint file '%.*s' must hold an object of blueprints by name; it is skipped.", AZ_STRING_ARG(source));
            return false;
        }

        for (const auto& member : root.GetObject())
        {
            AddLayer(ToView(member.name), member.value, source);
        }
        return true;
    }

    void BlueprintLibrary::AddLayer(AZStd::string_view name, const rapidjson::Value& layer, AZStd::string_view source)
    {
        if (!layer.IsObject())
        {
            AZ_Warning("Things", false, "Blueprint '%.*s' in '%.*s' is not an object; it is skipped.", AZ_STRING_ARG(name), AZ_STRING_ARG(source));
            return;
        }

        m_resolved.clear();
        AZStd::vector<Layer>& layers = m_raw[AZStd::string(name)];

        const auto load = layer.FindMember(LoadKey);
        if (load != layer.MemberEnd())
        {
            if (load->value.IsString() && ToView(load->value) == "Replace")
            {
                layers.clear();
            }
            else if (!load->value.IsString() || ToView(load->value) != "Merge")
            {
                AZ_Warning(
                    "Things", false, "Blueprint '%.*s' in '%.*s': \"Load\" must be \"Merge\" or \"Replace\"; it merges.",
                    AZ_STRING_ARG(name), AZ_STRING_ARG(source));
            }
        }

        Layer& added = layers.emplace_back();
        added.m_json = AZStd::make_unique<rapidjson::Document>();
        added.m_json->CopyFrom(layer, added.m_json->GetAllocator(), true);
        added.m_source = source;
    }

    const rapidjson::Value* BlueprintLibrary::Resolve(AZStd::string_view name)
    {
        const AZStd::string key(name);
        if (const auto memo = m_resolved.find(key); memo != m_resolved.end())
        {
            return memo->second.get();
        }

        const auto raw = m_raw.find(key);
        if (raw == m_raw.end())
        {
            return nullptr;
        }

        rapidjson::Document own(rapidjson::kObjectType);
        for (const Layer& layer : raw->second)
        {
            ApplyLayer(own, own.GetAllocator(), *layer.m_json, name);
        }

        auto result = AZStd::make_unique<rapidjson::Document>(rapidjson::kObjectType);
        m_resolving.push_back(key);

        const auto inherits = own.FindMember(InheritsKey);
        if (inherits != own.MemberEnd() && !inherits->value.IsArray())
        {
            AZ_Warning("Things", false, "Blueprint '%s': \"Inherits\" must be an array of names; it is ignored.", key.c_str());
        }
        else if (inherits != own.MemberEnd())
        {
            for (const rapidjson::Value& baseName : inherits->value.GetArray())
            {
                if (!baseName.IsString())
                {
                    AZ_Warning("Things", false, "Blueprint '%s': \"Inherits\" holds something that isn't a name.", key.c_str());
                    continue;
                }

                const AZStd::string base(ToView(baseName));
                if (AZStd::find(m_resolving.begin(), m_resolving.end(), base) != m_resolving.end())
                {
                    AZStd::string cycle;
                    for (const AZStd::string& step : m_resolving)
                    {
                        cycle += step + " -> ";
                    }
                    AZ_Warning("Things", false, "Blueprint inheritance cycle %s%s; base '%s' of '%s' is skipped.", cycle.c_str(), base.c_str(), base.c_str(), key.c_str());
                    continue;
                }

                const rapidjson::Value* resolvedBase = Resolve(base);
                if (!resolvedBase)
                {
                    AZ_Warning("Things", false, "Blueprint '%s' inherits from '%s', which does not exist; the base is skipped.", key.c_str(), base.c_str());
                    continue;
                }
                ApplyLayer(*result, result->GetAllocator(), *resolvedBase, name);
            }
        }

        m_resolving.pop_back();

        for (const Layer& layer : raw->second)
        {
            ApplyLayer(*result, result->GetAllocator(), *layer.m_json, name);
        }
        StripReserved(*result);

        const rapidjson::Value* resolved = result.get();
        m_resolved.emplace(key, AZStd::move(result));
        return resolved;
    }

    bool BlueprintLibrary::Compose(const AZStd::vector<AZStd::string>& layers, rapidjson::Document& out)
    {
        out.SetObject();
        bool anyFound = false;
        for (const AZStd::string& name : layers)
        {
            const rapidjson::Value* resolved = Resolve(name);
            if (!resolved)
            {
                AZ_Warning("Things", false, "Composition layer '%s' does not exist; it is skipped.", name.c_str());
                continue;
            }
            ApplyLayer(out, out.GetAllocator(), *resolved, name);
            anyFound = true;
        }
        return anyFound;
    }

    bool BlueprintLibrary::Has(AZStd::string_view name) const
    {
        return m_raw.find(AZStd::string(name)) != m_raw.end();
    }

    AZStd::vector<AZStd::string> BlueprintLibrary::GetNames() const
    {
        AZStd::vector<AZStd::string> names;
        names.reserve(m_raw.size());
        for (const auto& [name, layers] : m_raw)
        {
            names.push_back(name);
        }
        AZStd::sort(names.begin(), names.end());
        return names;
    }

    size_t BlueprintLibrary::GetCount() const
    {
        return m_raw.size();
    }

    AZStd::vector<AZStd::string> BlueprintLibrary::GetSources(AZStd::string_view name) const
    {
        AZStd::vector<AZStd::string> sources;
        if (const auto raw = m_raw.find(AZStd::string(name)); raw != m_raw.end())
        {
            for (const Layer& layer : raw->second)
            {
                sources.push_back(layer.m_source);
            }
        }
        return sources;
    }

    void BlueprintLibrary::ApplyLayer(
        rapidjson::Value& target, rapidjson::Document::AllocatorType& allocator, const rapidjson::Value& layer, AZStd::string_view name)
    {
        const rapidjson::Value* layerParts = FindObject(layer, PartsKey);
        rapidjson::Value* targetParts = FindObject(target, PartsKey);
        if (layerParts && targetParts)
        {
            for (const auto& part : layerParts->GetObject())
            {
                const auto below = targetParts->FindMember(part.name);
                if (!part.value.IsObject() || below == targetParts->MemberEnd() || !below->value.IsObject())
                {
                    continue;
                }

                const auto replace = part.value.FindMember(ReplaceKey);
                const bool replaceRequested = replace != part.value.MemberEnd() && replace->value.IsTrue();
                const AZStd::string_view newType = GetPartType(part.value);
                const AZStd::string_view oldType = GetPartType(below->value);
                const bool typeChanges = !newType.empty() && newType != oldType;
                AZ_Warning(
                    "Things", replaceRequested || !typeChanges,
                    "Blueprint '%.*s': part '%s' changes type from '%.*s' to '%.*s'; the part is replaced, not merged.",
                    AZ_STRING_ARG(name), part.name.GetString(), AZ_STRING_ARG(oldType), AZ_STRING_ARG(newType));

                if (replaceRequested || typeChanges)
                {
                    targetParts->EraseMember(below);
                }
            }
        }

        AZ::JsonSerialization::ApplyPatch(target, allocator, layer, AZ::JsonMergeApproach::JsonMergePatch);

        if (rapidjson::Value* parts = FindObject(target, PartsKey))
        {
            for (auto& part : parts->GetObject())
            {
                if (part.value.IsObject())
                {
                    part.value.EraseMember(ReplaceKey);
                }
            }
        }
    }

    void BlueprintLibrary::StripReserved(rapidjson::Value& blueprint)
    {
        blueprint.EraseMember(InheritsKey);
        blueprint.EraseMember(LoadKey);
    }
} // namespace Things
