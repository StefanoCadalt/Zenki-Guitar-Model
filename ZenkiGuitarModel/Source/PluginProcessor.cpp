#include "PluginProcessor.h"
#include "PluginEditor.h"

// Array statico: definisce l'accordatura standard (EADGBE) come base di fallback
const int ZenkiGuitarModelAudioProcessor::defaultMidiNotes[ZenkiGuitarModelAudioProcessor::numStrings] = { 64, 59, 55, 50, 45, 40 };

//==============================================================================
// Costruttore: Configura i bus I/O audio (stereo) e aggancia l'APVTS.
// Inizializza gli stati di base per le corde e per la comunicazione con l'interfaccia.
ZenkiGuitarModelAudioProcessor::ZenkiGuitarModelAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
    , apvts(*this, nullptr, "PARAMETERS", createParameters())
{
    // Reset dello stato vettoriale per le 6 corde e degli atomic flags per la UI
    for (int i = 0; i < numStrings; ++i)
    {
        currentMidiNotes[i] = defaultMidiNotes[i];
        uiStringWasPlucked[i].store(false);
        uiPluckPosition[i].store(0.0f);
    }

#pragma region APVTS Raw Pointers
    // Binding dei raw pointer: essenziale per evitare latenze.
    // L'APVTS è thread-safe, ma usare questi puntatori evita lock pesanti durante il processBlock.
    driveParameter = apvts.getRawParameterValue("drive");
    gainParameter = apvts.getRawParameterValue("gain");
    hardnessParameter = apvts.getRawParameterValue("hardness");
    dampingParameter = apvts.getRawParameterValue("damping");
    sustainParameter = apvts.getRawParameterValue("sustain");
    revMixParameter = apvts.getRawParameterValue("revMix");
    revSizeParameter = apvts.getRawParameterValue("revSize");
    delayTimeParameter = apvts.getRawParameterValue("delayTime");
    delayFbParameter = apvts.getRawParameterValue("delayFb");
    masterVolumeParameter = apvts.getRawParameterValue("masterVolume");
    delayOnParameter = apvts.getRawParameterValue("delayOn");
    distOnParameter = apvts.getRawParameterValue("distOn");
    revOnParameter = apvts.getRawParameterValue("revOn");
    phaserRateParameter = apvts.getRawParameterValue("phaserRate");
    phaserDepthParameter = apvts.getRawParameterValue("phaserDepth");
    phaserMixParameter = apvts.getRawParameterValue("phaserMix");
    phaserOnParameter = apvts.getRawParameterValue("phaserOn");
#pragma endregion
}

ZenkiGuitarModelAudioProcessor::~ZenkiGuitarModelAudioProcessor() {}

//==============================================================================
// Definisce l'albero gerarchico dei parametri (ID univoci, nomi in chiaro, range min/max/default).
juce::AudioProcessorValueTreeState::ParameterLayout ZenkiGuitarModelAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", 1.0f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gain", "Gain", 0.1f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hardness", "Hardness", 0.01f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterInt>("damping", "Damping", 0, 100, 100));
    params.push_back(std::make_unique<juce::AudioParameterInt>("sustain", "Sustain", 0, 100, 100));
    params.push_back(std::make_unique<juce::AudioParameterInt>("revMix", "Rev Mix", 0, 100, 50));
    params.push_back(std::make_unique<juce::AudioParameterInt>("revSize", "Rev Size", 0, 100, 50));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("delayTime", "Time", 0.01f, 1.5f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("delayFb", "Feedback", 0, 95, 50));
    params.push_back(std::make_unique<juce::AudioParameterInt>("masterVolume", "Master Volume", 0, 100, 50));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("phaserRate", "Phaser Rate", 0.1f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phaserDepth", "Phaser Depth", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("phaserMix", "Phaser Mix", 0, 100, 50));

    // Parametri booleani per gestire l'attivazione (bypass) delle singole catene DSP.
    params.push_back(std::make_unique<juce::AudioParameterBool>("delayOn", "Delay On", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("distOn", "Distortion On", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("revOn", "Reverb On", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("phaserOn", "Phaser On", true));

    return { params.begin(), params.end() };
}

//==============================================================================
// Riceve l'input dalla UI (indice corda e posizione del click 0-1) e innesca la simulazione fisica 
// calcolando la frequenza target in base all'accordatura della singola corda.
void ZenkiGuitarModelAudioProcessor::pluckString(int stringIndex, float position)
{
    if (stringIndex < 0 || stringIndex >= stringSynths.size())
        return;

    const int numFrets = 12;
    int fret = juce::jlimit(0, numFrets, (int)(position * (float)numFrets));
    int midiNote = currentMidiNotes[stringIndex] + fret;

    double freqHz = juce::MidiMessage::getMidiNoteInHertz(midiNote);

    auto* synth = stringSynths.getUnchecked(stringIndex);
    synth->setFrequency(freqHz);
    synth->stringPlucked(position);
}

void ZenkiGuitarModelAudioProcessor::setStringMidiNote(int stringIndex, int newMidiNote)
{
    if (stringIndex < 0 || stringIndex >= numStrings)
        return;

    int minNote = defaultMidiNotes[stringIndex] - tuningRangeSemitones;
    int maxNote = defaultMidiNotes[stringIndex] + tuningRangeSemitones;
    currentMidiNotes[stringIndex] = juce::jlimit(minNote, maxNote, newMidiNote);

    if (stringIndex < stringSynths.size())
    {
        double freqHz = juce::MidiMessage::getMidiNoteInHertz(currentMidiNotes[stringIndex]);
        stringSynths.getUnchecked(stringIndex)->setFrequency(freqHz);
    }
}

int ZenkiGuitarModelAudioProcessor::getStringMidiNote(int stringIndex) const
{
    if (stringIndex >= 0 && stringIndex < numStrings)
        return currentMidiNotes[stringIndex];
    return 0;
}

void ZenkiGuitarModelAudioProcessor::resetTuning()
{
    for (int i = 0; i < numStrings; ++i)
        setStringMidiNote(i, defaultMidiNotes[i]);
}

//==============================================================================
const juce::String ZenkiGuitarModelAudioProcessor::getName() const { return JucePlugin_Name; }

bool ZenkiGuitarModelAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ZenkiGuitarModelAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ZenkiGuitarModelAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ZenkiGuitarModelAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int ZenkiGuitarModelAudioProcessor::getNumPrograms() { return 1; }
int ZenkiGuitarModelAudioProcessor::getCurrentProgram() { return 0; }
void ZenkiGuitarModelAudioProcessor::setCurrentProgram(int) {}
const juce::String ZenkiGuitarModelAudioProcessor::getProgramName(int) { return {}; }
void ZenkiGuitarModelAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
// Inizializza i buffer e alloca la memoria prima che il processing DSP inizi.
// Cruciale per evitare allocazioni dinamiche (malloc/new) nel processBlock, che causerebbero glitch audio.
void ZenkiGuitarModelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    stringSynths.clear();
    currentSampleRate = sampleRate;

    // Inizializzazione Ring Buffer per il Delay (Capacità statica fissa di 2 secondi per evitare riallocazioni)
    int delayBufferSize = (int)(sampleRate * 2.0);
    delayBuffer.setSize(getTotalNumOutputChannels(), delayBufferSize);
    delayBuffer.clear();
    delayWritePosition = 0;

    // Inizializzazione Motore Karplus-Strong per ogni corda indipendente
    for (int i = 0; i < numStrings; ++i)
    {
        double freq = juce::MidiMessage::getMidiNoteInHertz(currentMidiNotes[i]);
        stringSynths.add(new StringSynthesiser(sampleRate, freq, hardnessParameter->load()));
    }

    // Setup Moduli DSP JUCE (Riverbero e Phaser)
    reverb.setSampleRate(sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    phaser.prepare(spec);
}

void ZenkiGuitarModelAudioProcessor::releaseResources()
{
    stringSynths.clear();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ZenkiGuitarModelAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    return true;
#endif
}
#endif

// Cuore del processamento Real-Time. Esegue il routing audio in serie:
// 1. Parsing MIDI -> 2. Karplus-Strong -> 3. Distorsione -> 4. Phaser -> 5. Delay -> 6. Reverb -> 7. Master.
void ZenkiGuitarModelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; // Previene rallentamenti della CPU dovuti a valori in virgola mobile troppo vicini allo zero.

#pragma region Gestione MIDI
    // Intercetta i messaggi "Note On" (es. da tastiera MIDI) e li dirotta sulla corda virtuale 
    // appropriata calcolandone il fret in base all'accordatura corrente.
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            int midiNote = message.getNoteNumber();

            // Assegnazione euristica: cerca la prima corda su cui la nota rientra nei 12 tasti fisici
            for (int i = 0; i < numStrings; ++i)
            {
                int openStringNote = currentMidiNotes[i];
                int fret = midiNote - openStringNote;

                if (fret >= 0 && fret <= 12)
                {
                    float position = (float)fret / 12.0f;
                    pluckString(i, position);

                    // Push asincrono dello stato verso il thread UI per l'animazione grafica
                    uiPluckPosition[i].store(position);
                    uiStringWasPlucked[i].store(true);
                    break;
                }
            }
        }
    }
#pragma endregion

#pragma region Generazione Audio (Karplus-Strong)
    // Svuota il buffer prima di scriverci (previene la permanenza di blocchi audio precedenti)
    buffer.clear();

    // Scrive i dati di sintesi sul canale sinistro (0)
    float* channelData = buffer.getWritePointer(0);

    for (int i = 0; i < stringSynths.size(); ++i)
    {
        stringSynths.getUnchecked(i)->SetHardness(hardnessParameter->load());
        stringSynths.getUnchecked(i)->SetDamping(dampingParameter->load() / 100.0f);
        stringSynths.getUnchecked(i)->SetSustain(sustainParameter->load() / 100.0f);
        stringSynths.getUnchecked(i)->generateAndAddData(channelData, buffer.getNumSamples());
    }

    // Effettua la duplicazione statica: clona l'output mono sul canale destro (1) creando un falso stereo.
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, buffer.getNumSamples());
#pragma endregion

#pragma region DSP: Distorsione (Soft Clipping)
    // Algoritmo di saturazione (Soft-Clipping) basato su funzione Tangente Iperbolica.
    if (distOnParameter->load() >= 0.5f)
    {
        float currentDrive = driveParameter->load();
        float currentGain = gainParameter->load();
        float appliedDrive = currentDrive * currentDrive; // Risposta esponenziale del controllo drive

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* tempChannelData = buffer.getWritePointer(ch);
            for (int numSample = 0; numSample < buffer.getNumSamples(); ++numSample)
            {
                tempChannelData[numSample] = std::tanh(tempChannelData[numSample] * appliedDrive) * currentGain;
            }
        }
    }
#pragma endregion

#pragma region DSP: Phaser
    // Modulazione di fase che sfrutta i moduli ottimizzati JUCE DSP
    if (phaserOnParameter->load() >= 0.5f)
    {
        phaser.setRate(phaserRateParameter->load());
        phaser.setDepth(phaserDepthParameter->load());
        phaser.setMix(phaserMixParameter->load() / 100.0f); // Normalizzazione 0-1

        juce::dsp::AudioBlock<float> audioBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(audioBlock);
        phaser.process(context);
    }
#pragma endregion

#pragma region DSP: Delay Line
    // Implementazione custom di un Ring Buffer per l'effetto di ritardo e retroazione (Feedback)
    if (delayOnParameter->load() >= 0.5f)
    {
        float timeInSeconds = delayTimeParameter->load();
        float feedback = delayFbParameter->load() / 100.0f;

        int delayLengthInSamples = (int)(timeInSeconds * currentSampleRate);
        int delayBufferLength = delayBuffer.getNumSamples();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* tempChannelData = buffer.getWritePointer(ch);
            auto* delayData = delayBuffer.getWritePointer(ch % delayBuffer.getNumChannels());
            int localWritePosition = delayWritePosition;

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                // Determina la "playhead" di lettura andando indietro nel tempo e wrappando gli estremi
                int readPosition = localWritePosition - delayLengthInSamples;
                if (readPosition < 0) readPosition += delayBufferLength;

                float delayedSample = delayData[readPosition];

                // Scrittura nel Ring Buffer con Feedback cumulativo
                delayData[localWritePosition] = tempChannelData[i] + (delayedSample * feedback);

                // Output in uscita combinato al 50% di Dry/Wet
                tempChannelData[i] += delayedSample * 0.5f;

                localWritePosition++;
                if (localWritePosition >= delayBufferLength) localWritePosition = 0;
            }
        }
        // Aggiorna la posizione globale per il prossimo Audio Block
        delayWritePosition += buffer.getNumSamples();
        delayWritePosition %= delayBufferLength;
    }
    else
    {
        // Se l'effetto è spento, è cruciale far avanzare lo stesso la testina per mantenere la coerenza di fase nel tempo.
        delayWritePosition += buffer.getNumSamples();
        delayWritePosition %= delayBuffer.getNumSamples();
    }
#pragma endregion

#pragma region DSP: Riverbero Stereo
    // Modulo di spazializzazione JUCE. Opera direttamente sui canali stereo del buffer in mutazione.
    if (revOnParameter->load() >= 0.5f)
    {
        float mix = revMixParameter->load() / 100.0f;
        reverbParams.roomSize = revSizeParameter->load() / 100.0f;
        reverbParams.damping = 0.5f;
        reverbParams.width = 1.0f;
        reverbParams.dryLevel = 1.0f - mix;
        reverbParams.wetLevel = mix;

        reverb.setParameters(reverbParams);

        if (buffer.getNumChannels() >= 2)
        {
            float* leftChannel = buffer.getWritePointer(0);
            float* rightChannel = buffer.getWritePointer(1);
            reverb.processStereo(leftChannel, rightChannel, buffer.getNumSamples());
        }
    }
#pragma endregion

#pragma region Master Stage e Metering
    // Controllo guadagno finale ed estrazione dei valori Root Mean Square (RMS) per la barra del volume UI.
    buffer.applyGain(masterVolumeParameter->load() / 100.0f);

    float rmsLeft = juce::Decibels::gainToDecibels(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));
    masterRmsLeft.store(rmsLeft);

    if (buffer.getNumChannels() > 1)
    {
        float rmsRight = juce::Decibels::gainToDecibels(buffer.getRMSLevel(1, 0, buffer.getNumSamples()));
        masterRmsRight.store(rmsRight);
    }
#pragma endregion

#pragma region Oscilloscopio
    // Clona il segnale DSP finale in un buffer separato e lo pompa visivamente, inviandolo asincronamente all'UI.
    if (puntatoreOscilloscopio != nullptr)
    {
        visualBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            visualBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());

        // Gain compensativo visivo
        visualBuffer.applyGain(1.5f);
        puntatoreOscilloscopio->pushBuffer(visualBuffer);
    }
#pragma endregion
}

//==============================================================================
#pragma region Gestione Preset Utente (XML)
// Utility interne per serializzare l'intero stato APVTS e l'accordatura custom 
// salvandoli permanentemente in un file XML.
juce::File ZenkiGuitarModelAudioProcessor::getPresetsFolder()
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File presetsDir = appDataDir.getChildFile("ZenkiGuitarModel").getChildFile("Presets");

    if (!presetsDir.exists())
        presetsDir.createDirectory();

    return presetsDir;
}

void ZenkiGuitarModelAudioProcessor::saveUserPreset(const juce::String& presetName)
{
    juce::File presetFile = getPresetsFolder().getChildFile(presetName + ".xml");
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml != nullptr)
    {
        // Custom Data: Iniezione Array Accordatura
        auto* tuningElement = new juce::XmlElement("TUNING");
        for (int i = 0; i < 6; ++i)
        {
            tuningElement->setAttribute("string" + juce::String(i), getStringMidiNote(i));
        }
        xml->addChildElement(tuningElement);
        xml->writeTo(presetFile);
    }
}

void ZenkiGuitarModelAudioProcessor::loadUserPreset(const juce::String& presetName)
{
    juce::File presetFile = getPresetsFolder().getChildFile(presetName + ".xml");
    if (presetFile.existsAsFile())
    {
        std::unique_ptr<juce::XmlElement> xmlState = juce::XmlDocument::parse(presetFile);

        if (xmlState != nullptr)
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

            auto* tuningElement = xmlState->getChildByName("TUNING");
            if (tuningElement != nullptr)
            {
                int s0 = tuningElement->getIntAttribute("string0", 60);
                int s1 = tuningElement->getIntAttribute("string1", 55);
                int s2 = tuningElement->getIntAttribute("string2", 50);
                int s3 = tuningElement->getIntAttribute("string3", 45);
                int s4 = tuningElement->getIntAttribute("string4", 40);
                int s5 = tuningElement->getIntAttribute("string5", 47);

                setStringMidiNote(0, s0);
                setStringMidiNote(1, s1);
                setStringMidiNote(2, s2);
                setStringMidiNote(3, s3);
                setStringMidiNote(4, s4);
                setStringMidiNote(5, s5);
            }
        }
    }
}

void ZenkiGuitarModelAudioProcessor::deleteUserPreset(const juce::String& presetName)
{
    juce::File presetFile = getPresetsFolder().getChildFile(presetName + ".xml");
    if (presetFile.existsAsFile())
    {
        presetFile.deleteFile();
    }
}

juce::StringArray ZenkiGuitarModelAudioProcessor::getAvailableUserPresets()
{
    juce::StringArray presetNames;
    juce::File presetsDir = getPresetsFolder();
    juce::Array<juce::File> presetFiles = presetsDir.findChildFiles(juce::File::findFiles, false, "*.xml");

    for (auto file : presetFiles)
    {
        presetNames.add(file.getFileNameWithoutExtension());
    }

    return presetNames;
}
#pragma endregion

//==============================================================================
bool ZenkiGuitarModelAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ZenkiGuitarModelAudioProcessor::createEditor()
{
    return new ZenkiGuitarModelAudioProcessorEditor(*this);
}

void ZenkiGuitarModelAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void ZenkiGuitarModelAudioProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZenkiGuitarModelAudioProcessor();
}