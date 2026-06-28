#include "AudioEngine.h"

namespace
{
// Plays a one-shot from an in-memory AudioBuffer. Positionable so the transport can
// seek/start it instantly (no disk streaming). Mono buffers play on all output channels.
class BufferSource : public juce::PositionableAudioSource
{
public:
    explicit BufferSource (const juce::AudioBuffer<float>& b) : buffer (b) {}

    void setNextReadPosition (juce::int64 p) override { pos = p; }
    juce::int64 getNextReadPosition() const override  { return pos; }
    juce::int64 getTotalLength() const override        { return buffer.getNumSamples(); }
    bool isLooping() const override                    { return false; }

    void prepareToPlay (int, double) override {}
    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        const auto total = buffer.getNumSamples();
        const auto srcChans = buffer.getNumChannels();
        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            const int srcCh = juce::jmin (ch, srcChans - 1);
            auto* dst = info.buffer->getWritePointer (ch, info.startSample);
            for (int i = 0; i < info.numSamples; ++i)
            {
                const auto sp = pos + i;
                dst[i] = (srcCh >= 0 && sp >= 0 && sp < total) ? buffer.getSample (srcCh, (int) sp) : 0.0f;
            }
        }
        pos += info.numSamples;
    }

private:
    const juce::AudioBuffer<float>& buffer;
    juce::int64 pos = 0;
};
}

namespace sa
{
AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();               // WAV + AIFF
    deviceManager.initialiseWithDefaultDevices (0, 2);  // no inputs, stereo out
    sourcePlayer.setSource (&transport);
    deviceManager.addAudioCallback (&sourcePlayer);
    transport.addChangeListener (this);
}

AudioEngine::~AudioEngine()
{
    transport.setSource (nullptr);
    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
    transport.removeChangeListener (this);
}

bool AudioEngine::loadFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    transport.stop();
    transport.setSource (nullptr);   // detach before mutating the buffer

    const auto numCh = (int) juce::jmax ((unsigned) 1, reader->numChannels);
    const auto numSamps = (int) reader->lengthInSamples;
    sampleBuffer.setSize (numCh, numSamps);
    reader->read (&sampleBuffer, 0, numSamps, 0, true, true);

    fileSampleRate = reader->sampleRate;
    fileBits       = (int) reader->bitsPerSample;
    fileFrames     = reader->lengthInSamples;
    loadedName     = file.getFileName();

    playSource = std::make_unique<BufferSource> (sampleBuffer);
    transport.setSource (playSource.get(), 0, nullptr, fileSampleRate);

    thumb.setSource (new juce::FileInputSource (file));

    if (onChange) onChange();
    return true;
}

void AudioEngine::play()
{
    if (! hasFile()) return;
    transport.setPosition (0.0);
    transport.start();
}

void AudioEngine::stop()
{
    transport.stop();
    transport.setPosition (0.0);
}

void AudioEngine::togglePlay()
{
    if (transport.isPlaying()) stop();
    else                       play();
}

bool AudioEngine::isPlaying() const
{
    return transport.isPlaying();
}

void AudioEngine::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (onChange) onChange();
}
}
