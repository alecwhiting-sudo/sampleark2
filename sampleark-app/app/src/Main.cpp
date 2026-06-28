#include <juce_gui_extra/juce_gui_extra.h>

#include "MainComponent.h"
#include "SampleArkLookAndFeel.h"
#include "Theme.h"
#include "sampleark/Version.h"

namespace sa
{
class SampleArkApplication : public juce::JUCEApplication
{
public:
    SampleArkApplication() = default;

    const juce::String getApplicationName() override    { return "SampleArk"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), commandLine);
        juce::Logger::writeToLog ("SampleArk core v" + juce::String (sampleark::coreVersionString()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, const juce::String& commandLine)
            : DocumentWindow (name, sa::theme::colour::bg,
                              DocumentWindow::minimiseButton | DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            auto* mc = new MainComponent();
            setContentOwned (mc, true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);

            // Auto-load a command-line path (e.g. "open with"); drag-drop is handled by
            // MainComponent (a FileDragAndDropTarget).
            auto path = commandLine.unquoted().trim();
            if (path.isNotEmpty())
            {
                juce::File f (path);
                if (f.existsAsFile())
                    mc->loadFile (f);
            }
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    SampleArkLookAndFeel lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};
}

START_JUCE_APPLICATION (sa::SampleArkApplication)
