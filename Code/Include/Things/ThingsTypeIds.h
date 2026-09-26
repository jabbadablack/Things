#pragma once

namespace Things
{
    //! TypeId of the gem's module.
    inline constexpr const char* ThingsModuleTypeId = "{C12A2C2B-22A7-4929-AA8E-2130E1EB293A}";

    //! TypeId of ThingSystemComponent, which builds and tracks Things.
    inline constexpr const char* ThingSystemComponentTypeId = "{EF80D510-5681-424F-BAD4-01C78AB3F4B2}";
    //! TypeId of ModDataSystemComponent, which finds and layers data files from the game and its mods.
    inline constexpr const char* ModDataSystemComponentTypeId = "{A87AE428-2DD1-4E87-9B8D-AA47A4084BC1}";
    //! TypeId of ThingComponent, which marks an entity as a Thing.
    inline constexpr const char* ThingComponentTypeId = "{0551773B-79AD-4A91-B15A-6F04E9C9CBED}";
    //! TypeId of ThingBodyComponent, the part that spawns a Thing's visible body.
    inline constexpr const char* ThingBodyComponentTypeId = "{17E35AE0-9A68-4FF5-83DD-498E4C4CBF3F}";
    //! TypeId of BodyLook, a named look of a Thing's body.
    inline constexpr const char* BodyLookTypeId = "{0BAD88F5-0520-48CF-8075-C1009EF2DE75}";

    //! TypeId of the ThingSystemRequests interface.
    inline constexpr const char* ThingSystemRequestsTypeId = "{611CA4A9-BE85-4828-9025-2A6902E2F1C0}";
    //! TypeId of the ThingNotifications interface.
    inline constexpr const char* ThingNotificationsTypeId = "{58BE6B30-49F3-4DA4-AB3B-89AE345E9FBA}";
    //! TypeId of the ModDataRequests interface.
    inline constexpr const char* ModDataRequestsTypeId = "{D9EC6474-A78D-4D6B-BF17-D8829B4708D1}";
    //! TypeId of ModDataConfig, the Settings Registry description of data and mod roots.
    inline constexpr const char* ModDataConfigTypeId = "{4C5F4B04-0F56-4A01-A688-3FDFD4660C31}";
} // namespace Things
