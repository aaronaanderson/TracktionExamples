
//====================Borrowed From Components.cpp in Tracktion's Examples/common
//
// The following is used to provide a menu form which to select a plugin. It's just a pop-up
// menu with plugins organized by manufacturer. It certainly feels like a lot of code to accomplish
// this task - it can be safely ignored; the TL;DR is that this provides a list of plugins that are 
// available, and selecting one provides a tracktion_engine::Plugin::Ptr.

class PluginTreeBase
{
public:
    virtual ~PluginTreeBase() = default;
    virtual String getUniqueName() const = 0;
    
    void addSubItem (PluginTreeBase* itm)   { subitems.add (itm);       }
    int getNumSubItems()                    { return subitems.size();   }
    PluginTreeBase* getSubItem (int idx)    { return subitems[idx];     }
    
private:
    OwnedArray<PluginTreeBase> subitems;
};

static inline const char* getInternalPluginFormatName()     { return "TracktionInternal"; }
class PluginTreeItem : public PluginTreeBase
{
public:
    PluginTreeItem (const PluginDescription&);
    PluginTreeItem (const String& uniqueId, const String& name, const String& xmlType, bool isSynth, bool isPlugin);

    tracktion_engine::Plugin::Ptr create (tracktion_engine::Edit&);
    
    String getUniqueName() const override
    {
        if (desc.fileOrIdentifier.startsWith (tracktion_engine::RackType::getRackPresetPrefix()))
            return desc.fileOrIdentifier;

        return desc.createIdentifierString();
    }

    PluginDescription desc;
    String xmlType;
    bool isPlugin = true;

    JUCE_LEAK_DETECTOR (PluginTreeItem)
};
PluginTreeItem::PluginTreeItem (const PluginDescription& d)
    : desc (d), xmlType (tracktion_engine::ExternalPlugin::xmlTypeName), isPlugin (true)
{
    jassert (xmlType.isNotEmpty());
}

PluginTreeItem::PluginTreeItem (const String& uniqueId, const String& name,
                                const String& xmlType_, bool isSynth, bool isPlugin_)
    : xmlType (xmlType_), isPlugin (isPlugin_)
{
    jassert (xmlType.isNotEmpty());
    desc.name = name;
    desc.fileOrIdentifier = uniqueId;
    desc.pluginFormatName = (uniqueId.endsWith ("_trkbuiltin") || xmlType == tracktion_engine::RackInstance::xmlTypeName)
                                ? getInternalPluginFormatName() : String();
    desc.category = xmlType;
    desc.isInstrument = isSynth;
}

tracktion_engine::Plugin::Ptr PluginTreeItem::create (tracktion_engine::Edit& ed)
{
    return ed.getPluginCache().createNewPlugin (xmlType, desc);
}

class PluginTreeGroup : public PluginTreeBase
{
public:
    PluginTreeGroup (tracktion_engine::Edit&, juce::KnownPluginList::PluginTree&, tracktion_engine::Plugin::Type);
    PluginTreeGroup (const String&);
    
    String getUniqueName() const override           { return name; }

    String name;

private:
    void populateFrom (KnownPluginList::PluginTree&);
    void createBuiltInItems (int& num, tracktion_engine::Plugin::Type);

    JUCE_LEAK_DETECTOR (PluginTreeGroup)
};

PluginTreeGroup::PluginTreeGroup (tracktion_engine::Edit& edit, KnownPluginList::PluginTree& tree, tracktion_engine::Plugin::Type types)
    : name ("Plugins")
{
    {
        int num = 1;

        auto builtinFolder = new PluginTreeGroup (TRANS("Builtin Plugins"));
        addSubItem (builtinFolder);
        builtinFolder->createBuiltInItems (num, types);
    }

    {
        auto racksFolder = new PluginTreeGroup (TRANS("Plugin Racks"));
        addSubItem (racksFolder);

        racksFolder->addSubItem (new PluginTreeItem (String (tracktion_engine::RackType::getRackPresetPrefix()) + "-1",
                                                     TRANS("Create New Empty Rack"),
                                                     tracktion_engine::RackInstance::xmlTypeName, false, false));

        int i = 0;
        for (auto rf : edit.getRackList().getTypes())
            racksFolder->addSubItem (new PluginTreeItem ("RACK__" + String (i++), rf->rackName,
                                                         tracktion_engine::RackInstance::xmlTypeName, false, false));
    }

    populateFrom (tree);
}

PluginTreeGroup::PluginTreeGroup (const String& s)  : name (s)
{
    jassert (name.isNotEmpty());
}

void PluginTreeGroup::populateFrom (KnownPluginList::PluginTree& tree)
{
    for (auto subTree : tree.subFolders)
    {
        if (subTree->plugins.size() > 0 || subTree->subFolders.size() > 0)
        {
            auto fs = new PluginTreeGroup (subTree->folder);
            addSubItem (fs);

            fs->populateFrom (*subTree);
        }
    }

    for (const auto& pd : tree.plugins)
        addSubItem (new PluginTreeItem (pd));
}


template<class FilterClass>
void addInternalPlugin (PluginTreeBase& item, int& num, bool synth = false)
{
    item.addSubItem (new PluginTreeItem (String (num++) + "_trkbuiltin",
                                         TRANS (FilterClass::getPluginName()),
                                         FilterClass::xmlTypeName, synth, false));
}

void PluginTreeGroup::createBuiltInItems (int& num, tracktion_engine::Plugin::Type types)
{
    addInternalPlugin<tracktion_engine::VolumeAndPanPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::LevelMeterPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::EqualiserPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::ReverbPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::DelayPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::ChorusPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::PhaserPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::CompressorPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::PitchShiftPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::LowPassPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::MidiModifierPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::MidiPatchBayPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::PatchBayPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::AuxSendPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::AuxReturnPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::TextPlugin> (*this, num);
    addInternalPlugin<tracktion_engine::FreezePointPlugin> (*this, num);

   #if TRACKTION_ENABLE_REWIRE
    addInternalPlugin<tracktion_engine::ReWirePlugin> (*this, num, true);
   #endif

    if (types == tracktion_engine::Plugin::Type::allPlugins)
    {
        addInternalPlugin<tracktion_engine::SamplerPlugin> (*this, num, true);
        addInternalPlugin<tracktion_engine::FourOscPlugin> (*this, num, true);
    }

    addInternalPlugin<tracktion_engine::InsertPlugin> (*this, num);

   #if ENABLE_INTERNAL_PLUGINS
    for (auto& d : PluginTypeBase::getAllPluginDescriptions())
        if (isPluginAuthorised (d))
            addSubItem (new PluginTreeItem (d));
   #endif
}

inline std::unique_ptr<juce::KnownPluginList::PluginTree> createPluginTree (tracktion_engine::Engine& engine)
{
    auto& list = engine.getPluginManager().knownPluginList;
    if (auto tree = list.createTree (list.getTypes(), KnownPluginList::sortByManufacturer))
        return tree;

    return {};
}
class PluginMenu : public juce::PopupMenu
{
public:
    PluginMenu() = default;

    PluginMenu (PluginTreeGroup& node)
    {
        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subNode = dynamic_cast<PluginTreeGroup*> (node.getSubItem (i)))
                addSubMenu (subNode->name, PluginMenu (*subNode), true);

        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subType = dynamic_cast<PluginTreeItem*> (node.getSubItem (i)))
                addItem (subType->getUniqueName().hashCode(), subType->desc.name, true, false);
    }

    static PluginTreeItem* findType (PluginTreeGroup& node, int hash)
    {
        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subNode = dynamic_cast<PluginTreeGroup*> (node.getSubItem (i)))
                if (auto* t = findType (*subNode, hash))
                    return t;

        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto t = dynamic_cast<PluginTreeItem*> (node.getSubItem (i)))
                if (t->getUniqueName().hashCode() == hash)
                    return t;

        return nullptr;
    }

    PluginTreeItem* runMenu (PluginTreeGroup& node)
    {
        int res = show();

        if (res == 0)
            return nullptr;

        return findType (node, res);
    }
};

tracktion_engine::Plugin::Ptr showMenuAndCreatePlugin (tracktion_engine::Edit& edit)
{
    if (auto tree = createPluginTree (edit.engine))
    {
        PluginTreeGroup root (edit, *tree, tracktion_engine::Plugin::Type::allPlugins);
        PluginMenu m (root);

        if (auto type = m.runMenu (root))
            return type->create (edit);
    }
    
    return {};
}

// PluginComponent is a slightly modified version of what lives in examples/common/Components.h/cpp
// It's just a text button that allows removal of the plugin via right click, or showing the plugin
// window via left click
class PluginComponent : public juce::TextButton
{
public:
    PluginComponent::PluginComponent (tracktion_engine::Plugin::Ptr p)
    : plugin (p)
    {
        setButtonText (plugin->getName().substring (0, 5));
    }
    ~PluginComponent() override {}
    
    using TextButton::clicked;
    void clicked (const ModifierKeys& modifiers) override
    {
        if (modifiers.isPopupMenu())
        {
            PopupMenu m;
            m.addItem ("Delete", [this] { plugin->deleteFromParent(); });
            m.showAt (this);
        }
        else
        {
            plugin->showWindowExplicitly();
        }
    }
    
private:
    tracktion_engine::Plugin::Ptr plugin;
};


