#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "StringComponent.h"
#include "KnobStyle.h"

//==============================================================================
// Definisce l'interfaccia utente grafica (GUI) del plugin.
// Gestisce il rendering visivo, le animazioni e l'input dell'utente.
class ZenkiGuitarModelAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit ZenkiGuitarModelAudioProcessorEditor(ZenkiGuitarModelAudioProcessor&);
    ~ZenkiGuitarModelAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

#pragma region UI Settings
    // Mappa l'indice di ogni corda (0-5) a una gradazione specifica di verde,
    // garantendo distinzione visiva dalla più grave (scura) alla più acuta (chiara).
    static juce::Colour stringColour(int index)
    {
        const juce::Colour colours[6] = {
            juce::Colour(0xFF004A15), // E2
            juce::Colour(0xFF007520), // A2
            juce::Colour(0xFF00A830), // D3
            juce::Colour(0xFF00D23A), // G3
            juce::Colour(0xFF00FA40), // B3
            juce::Colour(0xFF28FF5A)  // E4
        };
        return colours[index % 6];
    }
#pragma endregion

private:

#pragma region Componenti Grafici
    // Contenitori in memoria per tutti gli elementi interattivi e visivi di JUCE (manopole, bottoni, menu).

    // Sezioni
    static constexpr int numSezioni = 7;
    juce::Label titoloSezione[numSezioni];

    // Manopole
    KnobStyle stilePomello;
    static constexpr int numManopole = 13;
    juce::Slider manopolaEffetto[numManopole];
    juce::Label titoloManopolaEffetto[numManopole];

    // Corde e Accordatura
    juce::OwnedArray<StringComponent> stringComponents;
    juce::OwnedArray<juce::TextButton> tuningDownButtons;
    juce::OwnedArray<juce::TextButton> tuningUpButtons;
    juce::OwnedArray<juce::Label> tuningLabels;
    juce::TextButton resetTuningButton;
    juce::Label notaSuonataLabel;

    // Oscilloscopio
    juce::AudioVisualiserComponent oscilloscopio{ 2 };

    // Bypass Effetti
    juce::TextButton btnDelayOn{ "ON" };
    juce::TextButton btnDistOn{ "ON" };
    juce::TextButton btnRevOn{ "ON" };
    juce::TextButton btnPhaserOn{ "ON" };

    // Preset Menu
    juce::ComboBox presetMenu;
    juce::TextButton savePresetButton{ "+" };
    juce::TextButton deletePresetButton{ "-" };
#pragma endregion

#pragma region APVTS Attachments
    // "Ponti" automatici che collegano gli slider e i bottoni della UI
    // ai parametri DSP dell'AudioProcessor, gestendo la sincronizzazione bidirezionale.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hardnessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> revMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> revSizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> phaserRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> phaserDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> phaserMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> atcDelayOn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> atcDistOn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> atcRevOn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> atcPhaserOn;
#pragma endregion

#pragma region Metodi e Logica UI
    // Chiamata a frequenza fissa (60Hz) per aggiornare meter, animazioni e label
    // tramite polling atomico, senza bloccare il thread audio principale.
    void timerCallback() override;

    // Routine per tradurre le interazioni del mouse in coordinate (fret) e triggerare il suono
    void mouseDown(const juce::MouseEvent& e) override { handleMouseEvent(e); }
    void mouseDrag(const juce::MouseEvent& e) override { handleMouseEvent(e); }
    void mouseUp(const juce::MouseEvent& e) override { oldPosFret = -1; oldMidiNote = -1; }
    void handleMouseEvent(const juce::MouseEvent& e);

    // Sub-routine di disegno isolate per mantenere il metodo paint() pulito e modulare
    void SetTitle(juce::Graphics&);
    void SetLineaSeparatrice(juce::Graphics&);
    void SetStrings(juce::Graphics&);
    void SetSeparationFret(juce::Graphics&);

    // Routine per calcolare e formattare il testo di offset dell'accordatura
    void updateTuningLabel(int stringIndex);
    void updateAllTuningLabels();

    // Inizializzazione, salvataggio e caricamento stati DSP da/verso menu e disco
    void applicaPreset(int presetId);
    void aggiornaMenuPreset();
#pragma endregion

#pragma region Layout e Variabili di Stato
    // Cache delle aree rettangolari (calcolate una tantum in resized) 
    // per ottimizzare massicciamente le prestazioni di ridisegno nel paint().
    juce::Rectangle<int> areaOscilloscopio;
    juce::Rectangle<int> areaMaster;
    juce::Rectangle<int> areaParametriFisici;
    juce::Rectangle<int> areaDelay;
    juce::Rectangle<int> areaDistortion;
    juce::Rectangle<int> areaReverb;
    juce::Rectangle<int> areaPhaser;
    juce::Rectangle<int> areaCordeSotto;

    juce::Rectangle<int> meterLeftArea;
    juce::Rectangle<int> meterRightArea;
    float levelLeftScaled = 0.0f;
    float levelRightScaled = 0.0f;

    const int numFret = 12;
    const int numCorde = 6;
    static constexpr int tuningPanelWidth = 110;

    // Memoria di stato per evitare retriggers indesiderati della stessa nota durante il drag del mouse
    int oldPosFret = -1;
    int oldMidiNote = -1;

    // Riferimento diretto al motore DSP per comunicazioni custom fuori dall'APVTS (es. pluckString)
    ZenkiGuitarModelAudioProcessor& audioProcessor;
#pragma endregion

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZenkiGuitarModelAudioProcessorEditor)
};