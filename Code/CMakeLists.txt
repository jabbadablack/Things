o3de_pal_dir(pal_dir ${CMAKE_CURRENT_LIST_DIR}/Platform/${PAL_PLATFORM_NAME} "${gem_restricted_path}" "${gem_path}" "${gem_parent_relative_path}")
include(${pal_dir}/PAL_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)

if(NOT PAL_TRAIT_THINGS_SUPPORTED)
    return()
endif()

# Public headers. Game code links this target only and talks to the gem through its buses and interfaces.
ly_add_target(
    NAME ${gem_name}.API INTERFACE
    NAMESPACE Gem
    FILES_CMAKE
        things_api_files.cmake
    INCLUDE_DIRECTORIES
        INTERFACE
            Include
    BUILD_DEPENDENCIES
        INTERFACE
           AZ::AzCore
           AZ::AzFramework
)

# All of the gem's code. Linked by the module and by test targets that need the components themselves.
ly_add_target(
    NAME ${gem_name}.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        things_static_files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            Include
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            AZ::AzCore
            AZ::AzFramework
)

ly_add_target(
    NAME ${gem_name} ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
    NAMESPACE Gem
    FILES_CMAKE
        things_shared_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            Gem::${gem_name}.API
        PRIVATE
            Gem::${gem_name}.Static
)

ly_add_source_properties(
SOURCES
    Source/ThingsModule.cpp
PROPERTY COMPILE_DEFINITIONS
    VALUES
        O3DE_GEM_NAME=${gem_name}
        O3DE_GEM_VERSION=${gem_version})

ly_create_alias(NAME ${gem_name}.Clients NAMESPACE Gem TARGETS Gem::${gem_name})
ly_create_alias(NAME ${gem_name}.Servers NAMESPACE Gem TARGETS Gem::${gem_name})
ly_create_alias(NAME ${gem_name}.Unified NAMESPACE Gem TARGETS Gem::${gem_name})
ly_create_alias(NAME ${gem_name}.Clients.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
ly_create_alias(NAME ${gem_name}.Servers.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
ly_create_alias(NAME ${gem_name}.Unified.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
o3de_add_variant_dependencies_for_gem_dependencies(GEM_NAME ${gem_name} VARIANTS Clients Servers Unified)

if(PAL_TRAIT_BUILD_HOST_TOOLS)
    ly_create_alias(NAME ${gem_name}.Tools NAMESPACE Gem TARGETS Gem::${gem_name})
    ly_create_alias(NAME ${gem_name}.Builders NAMESPACE Gem TARGETS Gem::${gem_name})
    ly_create_alias(NAME ${gem_name}.Tools.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
    ly_create_alias(NAME ${gem_name}.Builders.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
    o3de_add_variant_dependencies_for_gem_dependencies(GEM_NAME ${gem_name} VARIANTS Tools Builders)
endif()

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND PAL_TRAIT_THINGS_TEST_SUPPORTED)
    # Test application and helpers, shared with the tests of games that use the gem.
    ly_add_target(
        NAME ${gem_name}.TestSupport STATIC
        NAMESPACE Gem
        FILES_CMAKE
            things_testsupport_files.cmake
        INCLUDE_DIRECTORIES
            PUBLIC
                Tests/Support
            PRIVATE
                Source
        BUILD_DEPENDENCIES
            PUBLIC
                AZ::AzTest
                AZ::AzFramework
                Gem::${gem_name}.Static
    )

    ly_add_target(
        NAME ${gem_name}.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        FILES_CMAKE
            things_tests_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Tests
                Source
        COMPILE_DEFINITIONS
            PRIVATE
                THINGS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Data"
        BUILD_DEPENDENCIES
            PRIVATE
                AZ::AzTest
                Gem::${gem_name}.Static
                Gem::${gem_name}.TestSupport
    )

    ly_add_googletest(
        NAME Gem::${gem_name}.Tests
    )
endif()
