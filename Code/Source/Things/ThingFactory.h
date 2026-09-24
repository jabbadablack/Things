#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/JSON/document.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace Things
{
    class BlueprintLibrary;

    //! Builds Things from resolved blueprints: the entity, its ThingComponent, its parts and its owned children.
    class ThingFactory
    {
    public:
        //! How deep owned children may nest, so a blueprint that owns itself can't build forever.
        static constexpr AZ::u32 MaxChildDepth = 8;

        //! Creates a factory that looks up child blueprints in the library.
        explicit ThingFactory(BlueprintLibrary& library);

        //! Builds and activates a Thing called name, owned by owner (or top-level when invalid), and everything its
        //! blueprint gives it, then sends OnThingBuilt to each of them. Returns an invalid id, with a warning, on failure.
        AZ::EntityId Build(const rapidjson::Value& resolved, const AZStd::string& name, const AZ::Transform& transform, AZ::EntityId owner);

    private:
        //! Builds one Thing at a nesting depth and its children, recording every built Thing.
        AZ::EntityId BuildAt(
            const rapidjson::Value& resolved,
            const AZStd::string& name,
            const AZ::Transform& transform,
            AZ::EntityId owner,
            AZ::u32 depth,
            AZStd::vector<AZ::EntityId>& built);

        //! Builds the owned children listed in the blueprint.
        void BuildChildren(
            const rapidjson::Value& resolved, const AZStd::string& name, AZ::EntityId owner, AZ::u32 depth, AZStd::vector<AZ::EntityId>& built);

        BlueprintLibrary& m_library; //!< Where child blueprints come from.
    };
} // namespace Things
