#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <atomic>

namespace sa
{
// One-shot prep parameters (M2). Normalised where noted; mapped to real units by the UI.
struct PrepParams
{
    double startFrac = 0.0;   // region start as fraction of source
    double endFrac   = 1.0;   // region end
    double fadeInMs  = 2.0;   // boundary click protection
    double fadeOutMs = 4.0;
    double gainDb    = 0.0;
    bool   normalize = false;
};

// M1 audible spine + M2 one-shot prep. Decodes a sample fully into RAM, renders the
// prepped one-shot (trim + fades + gain + normalize) into a play buffer, triggers it with
// zero latency, draws it, and exports it to WAV.
class AudioEngine : private juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    bool loadFile (const juce::File& file);
    void play();
    void stop();
    void togglePlay();
    bool isPlaying() const;

    void setLoop (bool shouldLoop);
    bool isLoopOn() const { return loopOn.load(); }
    double tempo() const  { return tempoBpm; }   // fixed 120 until M6 wires tempo

    // --- prep (M2) ---
    void setTrim (double startFrac, double endFrac);
    void setFadeInMs (double ms);
    void setFadeOutMs (double ms);
    void setGainDb (double db);
    void setNormalize (bool on);
    const PrepParams& prep() const            { return prepParams; }

    bool exportPreppedTo (const juce::File& file, int bitDepth);

    bool hasFile() const                      { return sampleBuffer.getNumSamples() > 0; }
    juce::AudioThumbnail& thumbnail()         { return thumb; }

    juce::String  fileName() const            { return loadedName; }
    double        sampleRate() const          { return fileSampleRate; }
    int           bitDepth() const            { return fileBits; }
    juce::int64   lengthFrames() const        { return fileFrames; }
    double        lengthSeconds() const       { return fileSampleRate > 0.0 ? regionLen.load() / fileSampleRate : 0.0; }
    double        positionSeconds() const     { return transport.getCurrentPosition(); }

    std::function<void()> onChange;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void renderPrep();

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::AudioTransportSource transport;

    juce::AudioBuffer<float> sampleBuffer;                       // decoded source, in RAM
    juce::AudioBuffer<float> playBuffer;                         // prepped one-shot
    std::unique_ptr<juce::PositionableAudioSource> playSource;   // plays playBuffer

    PrepParams prepParams;

    juce::AudioThumbnailCache thumbCache { 2 };
    juce::AudioThumbnail thumb { 512, formatManager, thumbCache };

    std::atomic<bool> loopOn { false };
    std::atomic<int> regionLen { 0 };   // prepped region length (samples); play/loop bound
    juce::SpinLock playLock;            // guards playBuffer swaps vs the audio thread
    double tempoBpm = 120.0;

    juce::String loadedName;
    double  fileSampleRate = 0.0;
    int     fileBits = 0;
    juce::int64 fileFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
}
