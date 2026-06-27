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

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
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
        explicit MainWindow (juce::String name)
            : DocumentWindow (name, sa::theme::colour::bg,
                              DocumentWindow::minimiseButton | DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
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
