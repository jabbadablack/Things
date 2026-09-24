#include <AzCore/Module/Module.h>
#include <Mods/ModDataSystemComponent.h>
#include <Things/ThingBodyComponent.h>
#include <Things/ThingComponent.h>
#include <Things/ThingSystemComponent.h>
#include <Things/ThingsTypeIds.h>

namespace Things
{
    //! Registers the Things components and requires the system components.
    class ThingsModule : public AZ::Module
    {
    public:
        AZ_RTTI(ThingsModule, ThingsModuleTypeId, AZ::Module);
        AZ_CLASS_ALLOCATOR(ThingsModule, AZ::SystemAllocator);

        //! Registers the component descriptors.
        ThingsModule()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                {
                    ModDataSystemComponent::CreateDescriptor(),
                    ThingSystemComponent::CreateDescriptor(),
                    ThingComponent::CreateDescriptor(),
                    ThingBodyComponent::CreateDescriptor(),
                });
        }

        //! The system components every application needs.
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<ModDataSystemComponent>(),
                azrtti_typeid<ThingSystemComponent>(),
            };
        }
    };
} // namespace Things

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), Things::ThingsModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Things, Things::ThingsModule)
#endif
