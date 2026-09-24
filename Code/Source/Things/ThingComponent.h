#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <Things/ThingsTypeIds.h>

namespace Things
{
    class ThingRegistry;

    //! Marks an entity as a Thing: remembers its blueprint, tags, parts and ownership.
    //!
    //! ThingSystemComponent builds these; don't add one by hand. Ownership changes go through the ThingSystemRequests.
    class ThingComponent : public AZ::Component
    {
        friend class ThingRegistry;

    public:
        AZ_COMPONENT(ThingComponent, ThingComponentTypeId);

        //! Reflects the component's state for serialization.
        static void Reflect(AZ::ReflectContext* context);

        //! Provides the Thing service.
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        //! An entity is one Thing at most.
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        //! The blueprint the Thing was built from.
        const AZStd::string& GetBlueprint() const;

        //! Sets the blueprint name; only while the Thing is being built.
        void SetBlueprint(AZStd::string blueprint);

        //! The owning Thing, or an invalid id.
        AZ::EntityId GetOwner() const;

        //! Sets the owner the Thing is built for; only before activation, which links it to the owner.
        void SetOwner(AZ::EntityId owner);

        //! The Things this one owns, in order.
        const AZStd::vector<AZ::EntityId>& GetOwned() const;

        //! Whether the Thing has the tag.
        bool HasTag(AZStd::string_view tag) const;

        //! The tags, sorted.
        const AZStd::vector<AZStd::string>& GetTags() const;

        //! Sets the tags; only while the Thing is being built.
        void SetTags(AZStd::vector<AZStd::string> tags);

        //! Records which component a blueprint part key became.
        void AddPart(AZStd::string key, AZ::ComponentId component);

        //! Blueprint part keys and the components they became.
        const AZStd::unordered_map<AZStd::string, AZ::ComponentId>& GetParts() const;

    protected:
        //! Registers the Thing, which links it to its owner.
        void Activate() override;

        //! Unregisters the Thing, which unlinks it from its owner.
        void Deactivate() override;

    private:
        AZStd::string m_blueprint; //!< Blueprint name, or layers joined with '+'.
        AZ::EntityId m_owner; //!< The owning Thing, or invalid when top-level.
        AZStd::vector<AZ::EntityId> m_owned; //!< Owned Things, in order.
        AZStd::vector<AZStd::string> m_tags; //!< Tags from the blueprint, sorted.
        AZStd::unordered_map<AZStd::string, AZ::ComponentId> m_parts; //!< Blueprint part keys to components.
    };
} // namespace Things
