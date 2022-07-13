/*
//MIT License
//
//Copyright (c) 2022 Aaron Anderson
*/
#include <JuceHeader.h>
#include "PluginStuff.h"
#include "PluginWindow.h"

// ====== These functions borrowed from Tracktion's examples/common/Utilities.h
//=============================================================================
namespace EngineHelpers
{
    void browseForAudioFile (tracktion_engine::Engine& engine, std::function<void (const juce::File&)> fileChosenCallback)
    {
        auto fc = std::make_shared<juce::FileChooser> ("Please select an audio file to load...",
                                                 engine.getPropertyStorage().getDefaultLoadSaveDirectory ("pitchAndTimeExample"),
                                                 engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats());

        fc->launchAsync (juce::FileBrowserComponent::openMode + juce::FileBrowserComponent::canSelectFiles,
                         [fc, &engine, callback = std::move (fileChosenCallback)] (const juce::FileChooser&)
                         {
                             const auto f = fc->getResult();

                             if (f.existsAsFile())
                                 engine.getPropertyStorage().setDefaultLoadSaveDirectory ("pitchAndTimeExample", f.getParentDirectory());

                             callback (f);
                         });
    }
    tracktion_engine::AudioTrack* getOrInsertAudioTrackAt (tracktion_engine::Edit& edit, int index)
    {
        edit.ensureNumberOfAudioTracks (index + 1);
        return tracktion_engine::getAudioTracks (edit)[index];
    }
    void removeAllClips (tracktion_engine::AudioTrack& track)
    {
        auto clips = track.getClips();

        for (int i = clips.size(); --i >= 0;)
            clips.getUnchecked (i)->removeFromParentTrack();
    }

    tracktion_engine::WaveAudioClip::Ptr loadAudioFileAsClip (tracktion_engine::Edit& edit, const File& file)
    {
        // Find the first track and delete all clips from it
        if (auto track = getOrInsertAudioTrackAt (edit, 0))
        {
            removeAllClips (*track);

            // Add a new clip to this track
            tracktion_engine::AudioFile audioFile (edit.engine, file);

            if (audioFile.isValid())
                if (auto newClip = track->insertWaveClip (file.getFileNameWithoutExtension(), file,
                                                          { { 0.0, audioFile.getLength() }, 0.0 }, false))
                    return newClip;
        }

        return {};
    }
    template<typename ClipType>
    typename ClipType::Ptr loopAroundClip (ClipType& clip)
    {
        auto& transport = clip.edit.getTransport();
        transport.setLoopRange (clip.getEditTimeRange());
        transport.looping = true;
        transport.position = 0.0;
        transport.play (false);

        return clip;
    }
    class FlaggedAsyncUpdater : public AsyncUpdater
    {
    public:
        //==============================================================================
        void markAndUpdate (bool& flag)     { flag = true; triggerAsyncUpdate(); }
        
        bool compareAndReset (bool& flag) noexcept
        {
            if (! flag)
                return false;
            
            flag = false;
            return true;
        }
    };
}
//======================================================================================
//===This class massaged from tracktion_engine/examples/PluginDemo.h====================
class TrackPluginListComponent : public juce::Component,
                                 private EngineHelpers::FlaggedAsyncUpdater,
                                 private tracktion_engine::ValueTreeAllEventListener
{
public:
    TrackPluginListComponent(tracktion_engine::Edit& e)
      :  edit(e), 
         track(EngineHelpers::getOrInsertAudioTrackAt(edit, 0))
    {

        track->state.addListener(this);
        addAndMakeVisible(&addPluginButton);
        addPluginButton.onClick = [this]()
        {
            if(auto plugin = showMenuAndCreatePlugin(edit))
            {
                auto track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);
                track->pluginList.insertPlugin(plugin, plugins.size(), nullptr);
                auto p = plugins.add(std::make_unique<PluginComponent>(plugin));
                addAndMakeVisible(p);
                resized();
            }
        };
        rebuildPluginButtons();
    }
    ~TrackPluginListComponent() override 
    {
        track->state.removeListener(this);
    }

    void resized() override 
    {
        int spacer = 2;
        auto b = getBounds();
        for(auto p : plugins)
        {
            p->setBounds(b.removeFromTop(20).withWidth(40));
            b.removeFromTop(spacer);
        }
        addPluginButton.setBounds(b.removeFromTop(20).withWidth(40));
    }
private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c) override
    {
        if(c.hasType(tracktion_engine::IDs::PLUGIN))
            markAndUpdate(needsUpdate);
    }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int) override
    {
        if(c.hasType(tracktion_engine::IDs::PLUGIN))
            markAndUpdate(needsUpdate);
    }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override
    {
        markAndUpdate(needsUpdate);
    }

    void handleAsyncUpdate() override
    {
        if(compareAndReset(needsUpdate))
            rebuildPluginButtons();
    }

    void rebuildPluginButtons()
    {
        plugins.clear();
        for(auto p : track->pluginList)
        {
            auto button = plugins.add(std::make_unique<PluginComponent>(p));
            addAndMakeVisible(button);
        }
        resized();
    }

    tracktion_engine::Edit& edit;
    tracktion_engine::Track::Ptr track;
    juce::TextButton addPluginButton {"+"};
    juce::OwnedArray<PluginComponent> plugins;

    bool needsUpdate = false; //async update flag
};
//==========================================================================================
class MainComponent : public juce::Component, 
                      private juce::ChangeListener
{
public:
    MainComponent()
    {
        addAndMakeVisible(&playStopButton);
        playStopButton.onClick = [this](){togglePlay(edit);};
        addAndMakeVisible(&sfLoadButton);
        sfLoadButton.onClick   = [this](){loadSoundFile();};
        addAndMakeVisible(&pluginAddButton);
        pluginAddButton.onClick = [this](){launchPluginList();};
        pluginAddButton.setHelpText("Scan Plugins for KnownPluginList");

        pluginList = std::make_unique<TrackPluginListComponent>(edit);
        addAndMakeVisible(pluginList.get());

        edit.getTransport().addChangeListener(this);
        
    }

    void resized() override
    {
        playStopButton.setBounds(20, 20, 50, 50);
        sfLoadButton.setBounds(80, 20, 50, 50);
        //pluginAddButton.setBounds(140, 20, 50, 50);
        pluginList->setBounds(20, 72, 80, 300);
    }
private:
    tracktion_engine::Engine engine { ProjectInfo::projectName, std::make_unique<ExtendedUIBehaviour>(), nullptr };
    tracktion_engine::Edit   edit   { tracktion_engine::Edit::Options { engine, 
                                                                        tracktion_engine::createEmptyEdit(engine), 
                                                                        tracktion_engine::ProjectItemID::createNewID(0)}};


    juce::TextButton playStopButton {"Play"}, sfLoadButton {"Load SF"}, pluginAddButton {"Load Plugin"}, addPluginButton {"+"};
    std::unique_ptr<TrackPluginListComponent> pluginList;

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        playStopButton.setButtonText(edit.getTransport().isPlaying() ? "Pause" : "Play");
    }

    void togglePlay (tracktion_engine::Edit& edit)
    {
        auto& transport = edit.getTransport();

        if (transport.isPlaying())
            transport.stop (false, false);
        else
            transport.play (false);
    }

    void loadSoundFile()
    {
        //Loads audio file to clip, and adds it to the first track 
        //(creating the track if it doesn't exist)
        auto loadFileToTrack = [this](const juce::File& file)
                               {    
                                    if(file != juce::File())
                                    {
                                        auto clip = EngineHelpers::loadAudioFileAsClip(edit, file);
                                        EngineHelpers::loopAroundClip (*clip);
                                    }
                               };
        EngineHelpers::browseForAudioFile(engine, loadFileToTrack);
    }
    void launchPluginList()
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle                   = TRANS("Plugins");
        o.dialogBackgroundColour        = Colours::black;
        o.escapeKeyTriggersCloseButton  = true;
        o.useNativeTitleBar             = true;
        o.resizable                     = true;
        o.useBottomRightCornerResizer   = true;
        auto v = new juce::PluginListComponent (engine.getPluginManager().pluginFormatManager,
                                                engine.getPluginManager().knownPluginList,
                                                engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                                tracktion_engine::getApplicationSettings());

        v->setSize (800, 600);
        o.content.setOwned (v);
        o.launchAsync();
    }
};

// Boiler Plate JUCE APP stuff ================================================
// ============================================================================
class Application    : public juce::JUCEApplication
{
public:
    //==============================================================================
    Application() = default;

    const juce::String getApplicationName() override       { return "PluginHostingDemo"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow ("Plugin Host", new MainComponent, *this));
    }

    void shutdown() override                         { mainWindow = nullptr; }

private:
    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, juce::Component* c, JUCEApplication& a)
            : DocumentWindow (name, juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                                .findColour (ResizableWindow::backgroundColourId),
                              juce::DocumentWindow::allButtons),
              app (a)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (c, true);

            setResizable (true, false);
            setResizeLimits (800, 600, 10000, 10000);
            centreWithSize (getWidth(), getHeight());

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            app.systemRequestedQuit();
        }

    private:
        juce::JUCEApplication& app;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (Application)
