#pragma once

#include <JuceHeader.h>
#include "StringSynthesiser.h"
#include <atomic>

//==============================================================================
// Classe principale del processore audio (AudioProcessor).
// Gestisce il ciclo di vita del plugin, l'elaborazione del DSP, la gestione MIDI, 
// l'albero dei parametri (APVTS) e il salvataggio dei preset su disco.
class ZenkiGuitarModelAudioProcessor : public juce::AudioProcessor
{
public:
    static const int numStrings = 6;
    static const int defaultMidiNotes[numStrings];
    static const int tuningRangeSemitones = 12;

#pragma region Interfaccia Audio/UI
    // Variabili atomiche thread-safe per la comunicazione asincrona tra Audio Thread e UI Thread.

    // Volume meter RMS (dB)
    std::atomic<float> masterRmsLeft{ -60.0f };
    std::atomic<float> masterRmsRight{ -60.0f };

    // Flag per l'aggiornamento asincrono della UI
    std::atomic<bool> uiStringWasPlucked[numStrings];
    std::atomic<float> uiPluckPosition[numStrings];

    // Riferimento all'oscilloscopio
    juce::AudioVisualiserComponent* puntatoreOscilloscopio = nullptr;
#pragma endregion

#pragma region Gestione Parametri (APVTS)
    // AudioProcessorValueTreeState: gestisce centralmente lo stato e l'automazione dei parametri in modo thread-safe.
    juce::AudioProcessorValueTreeState apvts;
#pragma endregion

    ZenkiGuitarModelAudioProcessor();
    ~ZenkiGuitarModelAudioProcessor() override;

    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    // Core dell'elaborazione DSP in tempo reale (chiamato ciclicamente dalla DAW).
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi()  const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
#pragma region Interazione Corde e Accordatura
    // Metodi di controllo per eccitare le corde virtuali e modificarne l'accordatura dinamica.
    void pluckString(int stringIndex, float position);
    void setStringMidiNote(int stringIndex, int newMidiNote);
    int getStringMidiNote(int stringIndex) const;
    void resetTuning();
#pragma endregion

    //==========================================================================
#pragma region Gestione Preset Utente
    // Routine di I/O su file system locale per la persistenza dei preset utente in formato XML.
    juce::File getPresetsFolder();
    void saveUserPreset(const juce::String& presetName);
    void loadUserPreset(const juce::String& presetName);
    void deleteUserPreset(const juce::String& presetName);
    juce::StringArray getAvailableUserPresets();
#pragma endregion

private:

#pragma region Motore Audio e DSP
    // Istanziazione delle strutture di sintesi fisica (Karplus-Strong) e dei moduli effetti DSP.
    int currentMidiNotes[numStrings];
    juce::OwnedArray<StringSynthesiser> stringSynths;

    // Delay
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition = 0;
    double currentSampleRate = 44100.0;

    // Riverbero
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;

    // Phaser
    juce::dsp::Phaser<float> phaser;

    // Buffer secondario per isolare l'elaborazione grafica
    juce::AudioBuffer<float> visualBuffer;
#pragma endregion

#pragma region Parametri Interni (Puntatori Raw)
    // Inizializza il layout dei parametri dell'APVTS e mantiene puntatori raw lock-free 
    // per un accesso ultra-rapido e sicuro all'interno del processBlock.
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    std::atomic<float>* driveParameter = nullptr;
    std::atomic<float>* gainParameter = nullptr;
    std::atomic<float>* hardnessParameter = nullptr;
    std::atomic<float>* dampingParameter = nullptr;
    std::atomic<float>* sustainParameter = nullptr;
    std::atomic<float>* revMixParameter = nullptr;
    std::atomic<float>* revSizeParameter = nullptr;
    std::atomic<float>* delayTimeParameter = nullptr;
    std::atomic<float>* delayFbParameter = nullptr;
    std::atomic<float>* masterVolumeParameter = nullptr;

    std::atomic<float>* phaserRateParameter = nullptr;
    std::atomic<float>* phaserDepthParameter = nullptr;
    std::atomic<float>* phaserMixParameter = nullptr;

    // Switch bypass (0.0f = Off, >= 0.5f = On)
    std::atomic<float>* phaserOnParameter = nullptr;
    std::atomic<float>* distOnParameter = nullptr;
    std::atomic<float>* delayOnParameter = nullptr;
    std::atomic<float>* revOnParameter = nullptr;
#pragma endregion

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZenkiGuitarModelAudioProcessor)
};