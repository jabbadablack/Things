#pragma once

#include <AzCore/JSON/document.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>

namespace Things
{
    //! Every blueprint of the game and its mods, as layered JSON definitions resolved on demand.
    //!
    //! A blueprint is a JSON object keyed by name, e.g.
    //! `{"Goblin": {"Inherits": ["Creature"], "Tags": {"Monster": true}, "Parts": {...}, "Children": {...}}}`.
    //! Definitions of the same name from later files are layers on top of earlier ones. All layering (mods, inheritance,
    //! recipes, child overrides) uses one rule, ApplyLayer: JSON Merge Patch, where null removes a key.
    //! Raw layers are kept separately so that a mod's null can remove a part the blueprint only inherits.
    class BlueprintLibrary
    {
    public:
        //! Key listing a blueprint's bases, applied in order; later bases win.
        static constexpr const char* InheritsKey = "Inherits";
        //! Key that is "Replace" when a definition discards the earlier layers of its name.
        static constexpr const char* LoadKey = "Load";
        //! Key of the object of parts, keyed by part id.
        static constexpr const char* PartsKey = "Parts";
        //! Key of the object of owned child Things, keyed by child id.
        static constexpr const char* ChildrenKey = "Children";
        //! Key of the object of tags, each true or false.
        static constexpr const char* TagsKey = "Tags";
        //! Key of a child's blueprint name.
        static constexpr const char* BlueprintKey = "Blueprint";
        //! Key of a part's type.
        static constexpr const char* TypeKey = "$type";
        //! Part key that is true when a layer's part replaces the part below instead of merging into it.
        static constexpr const char* ReplaceKey = "$replace";

        //! Creates an empty library.
        BlueprintLibrary() = default;

        //! Libraries own their documents and aren't copied.
        BlueprintLibrary(const BlueprintLibrary&) = delete;

        //! Libraries own their documents and aren't copied.
        BlueprintLibrary& operator=(const BlueprintLibrary&) = delete;

        //! Removes every definition.
        void Clear();

        //! Adds every member of a file's root object as a definition layer.
        //! Returns false, with a warning, when the root isn't an object.
        bool AddFile(const rapidjson::Value& root, AZStd::string_view source);

        //! Adds one definition layer for a name, on top of the existing ones.
        void AddLayer(AZStd::string_view name, const rapidjson::Value& layer, AZStd::string_view source);

        //! The fully resolved blueprint, or nullptr when no definition has the name. Valid until the next change.
        const rapidjson::Value* Resolve(AZStd::string_view name);

        //! Layers the resolved blueprints in order into out, as a recipe does. Missing names are skipped with a warning.
        //! Returns false when none of them exists.
        bool Compose(const AZStd::vector<AZStd::string>& layers, rapidjson::Document& out);

        //! Whether a definition has the name.
        bool Has(AZStd::string_view name) const;

        //! Names of all definitions, sorted.
        AZStd::vector<AZStd::string> GetNames() const;

        //! Number of definitions.
        size_t GetCount() const;

        //! The files that contributed layers to a definition, in order.
        AZStd::vector<AZStd::string> GetSources(AZStd::string_view name) const;

        //! Applies a layer onto target with JSON Merge Patch, for the blueprint called name.
        //! A part whose "$type" differs from the one below, or that has "$replace": true, replaces it instead of merging.
        static void ApplyLayer(
            rapidjson::Value& target,
            rapidjson::Document::AllocatorType& allocator,
            const rapidjson::Value& layer,
            AZStd::string_view name);

    private:
        //! One definition of a name, as read from a file.
        struct Layer
        {
            AZStd::unique_ptr<rapidjson::Document> m_json; //!< The definition.
            AZStd::string m_source; //!< The file it came from.
        };

        //! Removes reserved keys that only matter while resolving.
        static void StripReserved(rapidjson::Value& blueprint);

        AZStd::unordered_map<AZStd::string, AZStd::vector<Layer>> m_raw; //!< Definition layers by name, in load order.
        AZStd::unordered_map<AZStd::string, AZStd::unique_ptr<rapidjson::Document>> m_resolved; //!< Memoized resolved blueprints by name.
        AZStd::vector<AZStd::string> m_resolving; //!< Names being resolved, outermost first, to detect inheritance cycles.
    };
} // namespace Things
