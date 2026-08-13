#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Costruttore dell'Editor UI. Inizializza l'aspetto visivo e stabilisce i collegamenti 
// (Attachments) tra i componenti grafici e i parametri DSP del processore.
ZenkiGuitarModelAudioProcessorEditor::ZenkiGuitarModelAudioProcessorEditor(ZenkiGuitarModelAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Setup font personalizzato per un look da cruscotto hardware
    juce::Typeface::Ptr customFont = juce::Typeface::createSystemTypefaceFor(BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface(customFont);

#pragma region Visibilita sfondo corde
    // Inizializza i componenti visivi delle 6 corde e li rende parte del layout
    for (int i = 0; i < ZenkiGuitarModelAudioProcessor::numStrings; ++i)
    {
        auto* sc = stringComponents.add(new StringComponent(stringColour(i)));
        addAndMakeVisible(sc);
    }
#pragma endregion

#pragma region Setup accordatura corde
    // Costruisce i controlli UI per l'accordatura dinamica (pulsanti +/- e label per ogni corda)
    for (int i = 0; i < ZenkiGuitarModelAudioProcessor::numStrings; ++i)
    {
        // Pulsante [−]
        auto* btnDown = tuningDownButtons.add(new juce::TextButton("-"));
        btnDown->setLookAndFeel(&stilePomello);
        btnDown->setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF20D065).withAlpha(0.7f));
        btnDown->onClick = [this, i]()
            {
                int current = audioProcessor.getStringMidiNote(i);
                audioProcessor.setStringMidiNote(i, current - 1);
                updateTuningLabel(i);
            };
        addAndMakeVisible(btnDown);

        // Label offset semitoni
        auto* lbl = tuningLabels.add(new juce::Label());
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        lbl->setColour(juce::Label::textColourId, stringColour(i));
        addAndMakeVisible(lbl);

        // Pulsante [+]
        auto* btnUp = tuningUpButtons.add(new juce::TextButton("+"));
        btnUp->setLookAndFeel(&stilePomello);
        btnUp->setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF20D065).withAlpha(0.7f));
        btnUp->onClick = [this, i]()
            {
                int current = audioProcessor.getStringMidiNote(i);
                audioProcessor.setStringMidiNote(i, current + 1);
                updateTuningLabel(i);
            };
        addAndMakeVisible(btnUp);
    }

    // Pulsante Reset per riportare istantaneamente tutte le corde all'accordatura standard (EADGBE)
    resetTuningButton.setButtonText("Reset");
    resetTuningButton.setLookAndFeel(&stilePomello);
    resetTuningButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF20D065).withAlpha(0.7f));
    resetTuningButton.onClick = [this]()
        {
            audioProcessor.resetTuning();
            updateAllTuningLabels();
        };
    addAndMakeVisible(resetTuningButton);

    updateAllTuningLabels();
#pragma endregion

#pragma region Setup UI e Preset Menu
    // Setup del display testuale che mostra in tempo reale la nota suonata
    addAndMakeVisible(notaSuonataLabel);
    notaSuonataLabel.setText("Note", juce::NotificationType::dontSendNotification);
    notaSuonataLabel.setFont(juce::FontOptions(13.0f));
    notaSuonataLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF20D065));
    notaSuonataLabel.setJustificationType(juce::Justification::centredLeft);
    notaSuonataLabel.setBorderSize(juce::BorderSize<int>(0, 0, 0, 0));

    // Inizializza il menu a tendina per la selezione dei Preset (di fabbrica e utente)
    addAndMakeVisible(presetMenu);
    aggiornaMenuPreset();
    applicaPreset(1);

    presetMenu.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    presetMenu.setColour(juce::ComboBox::textColourId, juce::Colour(0xFF20D065).withAlpha(0.7f));
    presetMenu.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF20D065).withAlpha(0.3f));
    presetMenu.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFF20D065).withAlpha(0.5f));
    presetMenu.setJustificationType(juce::Justification::centred);
    presetMenu.setSelectedId(1);
    presetMenu.onChange = [this]() { applicaPreset(presetMenu.getSelectedId()); };

    addAndMakeVisible(savePresetButton);
    addAndMakeVisible(deletePresetButton);

    savePresetButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    savePresetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF28FF5A));
    deletePresetButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    deletePresetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF28FF5A));

    // Definisce l'azione di salvataggio di un nuovo preset utente tramite popup modale
    savePresetButton.onClick = [this]()
        {
            auto* alert = new juce::AlertWindow("Salva Preset", "Inserisci il nome del nuovo preset:", juce::AlertWindow::NoIcon);
            alert->addTextEditor("presetName", "", "Nome Preset");
            alert->addButton("Salva", 1, juce::KeyPress(juce::KeyPress::returnKey));
            alert->addButton("Annulla", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int result)
                {
                    if (result == 1)
                    {
                        juce::String name = alert->getTextEditorContents("presetName");
                        if (name.isNotEmpty())
                        {
                            audioProcessor.saveUserPreset(name);
                            aggiornaMenuPreset();

                            for (int i = 0; i < presetMenu.getNumItems(); ++i) {
                                if (presetMenu.getItemText(i) == name) {
                                    presetMenu.setSelectedId(presetMenu.getItemId(i));
                                    break;
                                }
                            }
                        }
                    }
                    delete alert;
                }));
        };

    // Definisce l'azione di eliminazione del preset utente attualmente selezionato
    deletePresetButton.onClick = [this]()
        {
            int id = presetMenu.getSelectedId();
            if (id >= 100)
            {
                juce::String name = presetMenu.getText();
                audioProcessor.deleteUserPreset(name);
                aggiornaMenuPreset();
                presetMenu.setSelectedId(1);
            }
        };
#pragma endregion

#pragma region Setup Titoli Sezioni
    // Costruzione delle label per identificare le macro-aree del DSP (Delay, Reverb, ecc.)
    juce::String nomiSezioni[numSezioni] = {
        "OSCILLOSCOPE", "MASTER VOLUME", "PHYSICAL PARAMETERS", "DELAY", "DISTORTION", "PHASER", "REVERB"
    };

    for (int i = 0; i < numSezioni; ++i)
    {
        titoloSezione[i].setText(nomiSezioni[i], juce::dontSendNotification);
        titoloSezione[i].setJustificationType(juce::Justification::centredLeft);
        titoloSezione[i].setColour(juce::Label::textColourId, juce::Colour(0xFF20D065).withAlpha(0.7f));
        addAndMakeVisible(titoloSezione[i]);
    }
#pragma endregion

#pragma region Setup manopole
    // Inizializza tutti gli slider rotativi e applica formattazioni specifiche (es. unità di misura) in base al parametro
    juce::String nomiManopole[numManopole] = {
        "Time", "Feedback",
        "Drive", "Gain",
        "Hardness", "Damping", "Sustain",
        "Mix", "Size",
        "Master",
        "Rate", "Depth", "Mix"
    };

    for (int i = 0; i < numManopole; ++i)
    {
        manopolaEffetto[i].setSliderStyle(juce::Slider::Rotary);
        manopolaEffetto[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 15);
        manopolaEffetto[i].setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        manopolaEffetto[i].setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        manopolaEffetto[i].setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);

        if (nomiManopole[i] == "Time") {
            manopolaEffetto[i].setTextValueSuffix(" s");
        }
        else if (nomiManopole[i] == "Rate") {
            manopolaEffetto[i].setTextValueSuffix(" Hz");
        }
        else if (nomiManopole[i] != "Drive" && nomiManopole[i] != "Gain" &&
            nomiManopole[i] != "Hardness" && nomiManopole[i] != "Depth") {
            manopolaEffetto[i].setTextValueSuffix(" %");
        }

        manopolaEffetto[i].setLookAndFeel(&stilePomello);
        addAndMakeVisible(manopolaEffetto[i]);

        titoloManopolaEffetto[i].setText(nomiManopole[i], juce::dontSendNotification);
        titoloManopolaEffetto[i].setJustificationType(juce::Justification::centred);
        titoloManopolaEffetto[i].setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(titoloManopolaEffetto[i]);
    }
#pragma endregion

    setSize(1152, 648);
    // Avvia il loop del timer a 60 FPS per aggiornamenti di UI e Metering indipendenti dall'audio thread
    startTimerHz(60);

#pragma region Setup bottoni On / Off
    // Setup dei pulsanti di bypass per attivare/disattivare interi blocchi DSP
    juce::TextButton* bypassButtons[] = { &btnDelayOn, &btnDistOn, &btnRevOn, &btnPhaserOn };

    for (int i = 0; i < 4; ++i)
    {
        bypassButtons[i]->setClickingTogglesState(true);
        bypassButtons[i]->setLookAndFeel(&stilePomello);
        bypassButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF20D065).withAlpha(0.5f));
        bypassButtons[i]->setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF00D4FF));
        addAndMakeVisible(bypassButtons[i]);
    }
#pragma endregion

#pragma region Attachments
    // Binding bidirezionale tra GUI (Slider/Button) e DSP (APVTS). 
    // Garantisce che l'UI rifletta automazioni DAW e i controlli influenzino il DSP in modo sicuro.
    timeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "delayTime", manopolaEffetto[0]);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "delayFb", manopolaEffetto[1]);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "drive", manopolaEffetto[2]);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "gain", manopolaEffetto[3]);
    hardnessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "hardness", manopolaEffetto[4]);
    dampingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "damping", manopolaEffetto[5]);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "sustain", manopolaEffetto[6]);
    revMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "revMix", manopolaEffetto[7]);
    revSizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "revSize", manopolaEffetto[8]);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "masterVolume", manopolaEffetto[9]);
    phaserRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "phaserRate", manopolaEffetto[10]);
    phaserDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "phaserDepth", manopolaEffetto[11]);
    phaserMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "phaserMix", manopolaEffetto[12]);

    atcDelayOn = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "delayOn", btnDelayOn);
    atcDistOn = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "distOn", btnDistOn);
    atcRevOn = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "revOn", btnRevOn);
    atcPhaserOn = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "phaserOn", btnPhaserOn);
#pragma endregion
}

ZenkiGuitarModelAudioProcessorEditor::~ZenkiGuitarModelAudioProcessorEditor()
{
    // Scollega esplicitamente il puntatore prima di distruggere l'interfaccia 
    // per evitare tentativi di accesso del thread audio a memoria non valida (Dangling Pointer).
    audioProcessor.puntatoreOscilloscopio = nullptr;
    stopTimer();
}

//==============================================================================

// Chiamata periodica asincrona gestita dal thread della UI. Si occupa di aggiornamenti 
// visivi che non necessitano della priorità o precisione al sample del thread DSP.
void ZenkiGuitarModelAudioProcessorEditor::timerCallback()
{
    // Effettua un controllo atomico (senza lock) per verificare se il processBlock
    // ha segnalato una nota suonata via MIDI, permettendo all'UI di animare la corda corrispondente.
    for (int i = 0; i < ZenkiGuitarModelAudioProcessor::numStrings; ++i)
    {
        if (audioProcessor.uiStringWasPlucked[i].exchange(false))
        {
            float position = audioProcessor.uiPluckPosition[i].load();
            stringComponents.getUnchecked(i)->stringPlucked(position);

#pragma region Aggiornamento label nota suonata
            int fret = juce::jlimit(0, numFret, (int)(position * numFret));
            int midiNote = audioProcessor.getStringMidiNote(i) + fret;
            juce::String nomeNota = juce::MidiMessage::getMidiNoteName(midiNote, true, true, 3);

            notaSuonataLabel.setText("Note: " + nomeNota + "  Fret: " + juce::String(fret), juce::dontSendNotification);
#pragma endregion
        }
    }

#pragma region Aggiornamento testo bottoni On / Off
    btnDelayOn.setButtonText(btnDelayOn.getToggleState() ? "ON" : "OFF");
    btnDistOn.setButtonText(btnDistOn.getToggleState() ? "ON" : "OFF");
    btnRevOn.setButtonText(btnRevOn.getToggleState() ? "ON" : "OFF");
    btnPhaserOn.setButtonText(btnPhaserOn.getToggleState() ? "ON" : "OFF");
#pragma endregion

#pragma region Lettura Volume Meter
    // Legge i livelli RMS (calcolati dall'audio thread) in modo thread-safe e mappa
    // il range in decibel su una scala lineare visiva normalizzata 0.0 - 1.0.
    float rmsL = audioProcessor.masterRmsLeft.load();
    float rmsR = audioProcessor.masterRmsRight.load();

    float newLevelL = juce::jmap(rmsL, -60.0f, +3.0f, 0.0f, 1.0f);
    float newLevelR = juce::jmap(rmsR, -60.0f, +3.0f, 0.0f, 1.0f);

    levelLeftScaled = juce::jlimit(0.0f, 1.0f, newLevelL);
    levelRightScaled = juce::jlimit(0.0f, 1.0f, newLevelR);

    // Forza il repaint mirato solo della porzione di schermo interessata dai meter, ottimizzando le performance grafiche.
    repaint(meterLeftArea.expanded(2));
    repaint(meterRightArea.expanded(2));
#pragma endregion
}

//==============================================================================

#pragma region paint UI
// Gestisce il rendering grafico dell'intera interfaccia. Richiamato solo quando necessario 
// (es. avvio, resize, o repaint esplicito). Non esegue calcoli di layout.
void ZenkiGuitarModelAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF08080A));

    SetStrings(g);
    SetSeparationFret(g);

#pragma region Disegno suddivisione delle aree
    juce::Colour verdeNeon = juce::Colour(0xFF20D065);
    g.setColour(verdeNeon.withAlpha(0.01f));

    float cornerRadius = 2.0f;
    g.fillRoundedRectangle(areaOscilloscopio.reduced(4).toFloat(), cornerRadius);
    g.fillRoundedRectangle(areaMaster.reduced(4).toFloat(), cornerRadius);
    g.fillRoundedRectangle(areaParametriFisici.reduced(4).toFloat(), cornerRadius);
    g.fillRoundedRectangle(areaDelay.reduced(4).toFloat(), cornerRadius);
    g.fillRoundedRectangle(areaDistortion.reduced(4).toFloat(), cornerRadius);
    g.fillRoundedRectangle(areaReverb.reduced(4).toFloat(), cornerRadius);
    g.fillRoundedRectangle(areaPhaser.reduced(4).toFloat(), cornerRadius);

    g.setColour(verdeNeon.withAlpha(0.1f));
    float lineThickness = 1.0f;

    g.drawRoundedRectangle(areaOscilloscopio.reduced(4).toFloat(), cornerRadius, lineThickness);
    g.drawRoundedRectangle(areaMaster.reduced(4).toFloat(), cornerRadius, lineThickness);
    g.drawRoundedRectangle(areaParametriFisici.reduced(4).toFloat(), cornerRadius, lineThickness);
    g.drawRoundedRectangle(areaDelay.reduced(4).toFloat(), cornerRadius, lineThickness);
    g.drawRoundedRectangle(areaDistortion.reduced(4).toFloat(), cornerRadius, lineThickness);
    g.drawRoundedRectangle(areaReverb.reduced(4).toFloat(), cornerRadius, lineThickness);
    g.drawRoundedRectangle(areaPhaser.reduced(4).toFloat(), cornerRadius, lineThickness);
#pragma endregion

#pragma region Disegno Volume Meter
    // Disegna la barra fissa scura di sfondo per i meter L/R.
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(meterLeftArea.toFloat(), 2.0f);
    g.fillRoundedRectangle(meterRightArea.toFloat(), 2.0f);

    // Applica una sfumatura dinamica (verde -> giallo -> rosso) per indicare l'intensità del segnale.
    juce::ColourGradient meterGrad(
        juce::Colours::red, meterLeftArea.getX(), meterLeftArea.getY(),
        juce::Colours::limegreen, meterLeftArea.getX(), meterLeftArea.getBottom(),
        false
    );
    meterGrad.addColour(0.3, juce::Colours::yellow);
    g.setGradientFill(meterGrad);

    // Determina dinamicamente l'altezza del "riempimento" del meter rispetto ai calcoli RMS correnti.
    int heightL = (int)(meterLeftArea.getHeight() * levelLeftScaled);
    int heightR = (int)(meterRightArea.getHeight() * levelRightScaled);

    auto localMeterL = meterLeftArea;
    auto localMeterR = meterRightArea;

    // Estrae e disegna solo la porzione inferiore calcolata (il volume vero e proprio).
    auto fillL = localMeterL.removeFromBottom(heightL);
    auto fillR = localMeterR.removeFromBottom(heightR);

    g.fillRoundedRectangle(fillL.toFloat(), 2.0f);
    g.fillRoundedRectangle(fillR.toFloat(), 2.0f);
#pragma endregion
}
#pragma endregion

#pragma region resized UI
// Eseguito automaticamente alla creazione della finestra o in caso di ridimensionamento. 
// Definisce proporzioni dinamiche (Layout responsive) aggiornando i bound di tutti i componenti.
void ZenkiGuitarModelAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    float scale = (float)getWidth() / 750.0f;

    area = area.reduced(15 * scale, 10 * scale);

#pragma region Area corde scalata
    int stringH = 17 * scale;
    int gap = 4 * scale;
    int rightMargin = 10 * scale;
    int scaledTuningPanelWidth = tuningPanelWidth * scale;
    const int totalStrings = ZenkiGuitarModelAudioProcessor::numStrings;

    int stringsAreaH = totalStrings * stringH + (totalStrings - 1) * gap + (16 * scale);

    auto bottomArea = area.removeFromBottom(stringsAreaH);
    areaCordeSotto = bottomArea;
    bottomArea.removeFromTop(8 * scale);

    // Distribuisce in modo uniforme lo spazio per le stringhe vettoriali e relativi tasti di accordatura.
    for (int i = 0; i < totalStrings; ++i)
    {
        auto row = bottomArea.removeFromTop(stringH);
        bottomArea.removeFromTop(gap);

        auto tuningRow = row.removeFromLeft(scaledTuningPanelWidth);
        row.removeFromRight(4 * scale);
        stringComponents.getUnchecked(i)->setBounds(row);

        int btnW = 22 * scale;
        int lblW = 44 * scale;

        tuningRow.removeFromLeft(4 * scale);
        tuningDownButtons.getUnchecked(i)->setBounds(tuningRow.removeFromLeft(btnW).reduced(0, 1));

        tuningRow.removeFromLeft(2 * scale);
        tuningLabels.getUnchecked(i)->setBounds(tuningRow.removeFromLeft(lblW).reduced(0, 1));
        tuningLabels.getUnchecked(i)->setFont(juce::FontOptions(11.0f * scale, juce::Font::bold));

        tuningRow.removeFromLeft(4 * scale);
        tuningUpButtons.getUnchecked(i)->setBounds(tuningRow.removeFromLeft(btnW).reduced(0, 1));
    }
#pragma endregion

#pragma region Area superiore scalata
    area.removeFromTop(5 * scale);
    area.removeFromBottom(12 * scale);

    auto toolbarArea = area.removeFromBottom(22 * scale);
    area.removeFromBottom(15 * scale);

    // Layout Toolbar
    int resetW = 55 * scale;
    resetTuningButton.setBounds(toolbarArea.getX() + (4 * scale), toolbarArea.getY(), resetW, toolbarArea.getHeight());

    int presetW = 130 * scale;
    int btnPresetW = 20 * scale;
    int rightEdge = toolbarArea.getRight() - (2 * scale);

    presetMenu.setBounds(rightEdge - presetW, toolbarArea.getY(), presetW, toolbarArea.getHeight());

    int deleteX = presetMenu.getX() - btnPresetW - (8 * scale);
    deletePresetButton.setBounds(deleteX, toolbarArea.getY(), btnPresetW, toolbarArea.getHeight());

    int saveX = deletePresetButton.getX() - btnPresetW - (3 * scale);
    savePresetButton.setBounds(saveX, toolbarArea.getY(), btnPresetW, toolbarArea.getHeight());

    int noteX = toolbarArea.getX() + scaledTuningPanelWidth;
    int noteW = savePresetButton.getX() - noteX;

    notaSuonataLabel.setBounds(noteX, toolbarArea.getY(), noteW, toolbarArea.getHeight());
    notaSuonataLabel.setFont(juce::FontOptions(13.0f * scale, juce::Font::bold));

    // Suddivisione UI Macro Aree: alloca blocchi logici (Destra/Sinistra e Alto/Basso)
    // sui quali andranno incastonati manopole e componenti grafici secondari.
    auto leftArea = area.removeFromLeft(area.getWidth() / 5);
    auto rightArea = area;

    areaOscilloscopio = leftArea.removeFromTop(leftArea.getHeight() / 2);
    areaMaster = leftArea;

    auto rightTopArea = rightArea.removeFromTop(rightArea.getHeight() / 2);
    auto rightBottomArea = rightArea;

    areaParametriFisici = rightTopArea.removeFromLeft((rightTopArea.getWidth() * 2) / 3);
    areaDistortion = rightTopArea;

    areaDelay = rightBottomArea.removeFromLeft(rightBottomArea.getWidth() / 3);
    areaPhaser = rightBottomArea.removeFromLeft(rightBottomArea.getWidth() / 2);
    areaReverb = rightBottomArea;
#pragma endregion

#pragma region Griglia manopole scalata
    // Crea copie locali dei bound per preservare le coordinate originali dei quadranti usate nel paint()
    juce::Rectangle<int> celle[13];

    auto workOsc = areaOscilloscopio;
    auto workMaster = areaMaster;
    auto workPhys = areaParametriFisici;
    auto workDelay = areaDelay;
    auto workDist = areaDistortion;
    auto workRev = areaReverb;
    auto workPhaser = areaPhaser;

    int titleHeight = 35 * scale;
    int leftMargin = 15 * scale;

    titoloSezione[0].setBounds(workOsc.removeFromTop(titleHeight).withTrimmedLeft(leftMargin));
    titoloSezione[1].setBounds(workMaster.removeFromTop(titleHeight).withTrimmedLeft(leftMargin));
    titoloSezione[2].setBounds(workPhys.removeFromTop(titleHeight).withTrimmedLeft(leftMargin));
    titoloSezione[3].setBounds(workDelay.removeFromTop(titleHeight).withTrimmedLeft(leftMargin));
    titoloSezione[4].setBounds(workDist.removeFromTop(titleHeight).withTrimmedLeft(leftMargin));
    titoloSezione[5].setBounds(workPhaser.removeFromTop(titleHeight).withTrimmedLeft(leftMargin));
    titoloSezione[6].setBounds(workRev.removeFromTop(titleHeight).withTrimmedLeft(leftMargin));

    for (int i = 0; i < numSezioni; ++i)
        titoloSezione[i].setFont(juce::FontOptions(11.0f * scale, juce::Font::bold));

    auto delayArea = workDelay.reduced(5 * scale, 5 * scale);
    celle[0] = delayArea.removeFromLeft(delayArea.getWidth() / 2);
    celle[1] = delayArea;

    auto distArea = workDist.reduced(5 * scale, 5 * scale);
    celle[2] = distArea.removeFromLeft(distArea.getWidth() / 2);
    celle[3] = distArea;

    auto physArea = workPhys.reduced(5 * scale, 5 * scale);
    celle[4] = physArea.removeFromLeft(physArea.getWidth() / 3);
    celle[5] = physArea.removeFromLeft(physArea.getWidth() / 2);
    celle[6] = physArea;

    auto verbArea = workRev.reduced(5 * scale, 5 * scale);
    celle[7] = verbArea.removeFromLeft(verbArea.getWidth() / 2);
    celle[8] = verbArea;

    celle[9] = workMaster.reduced(5 * scale, 5 * scale);
    celle[9].removeFromRight(24 * scale);

    auto phasArea = workPhaser.reduced(5 * scale, 5 * scale);
    celle[10] = phasArea.removeFromLeft(phasArea.getWidth() / 3);
    celle[11] = phasArea.removeFromLeft(phasArea.getWidth() / 2);
    celle[12] = phasArea;

    int uniformKnobSize = 45 * scale;

    // Centra ogni manopola all'interno della propria cella logica, preservandone le proporzioni.
    for (int i = 0; i < numManopole; ++i)
    {
        auto bounds = celle[i].withSizeKeepingCentre(uniformKnobSize, uniformKnobSize).translated(0, -1 * scale);
        manopolaEffetto[i].setBounds(bounds);
        manopolaEffetto[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 37 * scale, 13 * scale);

        titoloManopolaEffetto[i].setBounds(celle[i].getX(), bounds.getY() - (16 * scale), celle[i].getWidth(), 20 * scale);
        titoloManopolaEffetto[i].setFont(juce::FontOptions(11.0f * scale, juce::Font::plain));
    }
#pragma endregion

#pragma region Posizionamento bottoni On/Off
    int btnW = 30 * scale;
    int btnH = 15 * scale;
    int marginX = 10 * scale;

    int delayY = titoloSezione[3].getBounds().getCentreY() - (btnH / 2);
    int distY = titoloSezione[4].getBounds().getCentreY() - (btnH / 2);
    int revY = titoloSezione[5].getBounds().getCentreY() - (btnH / 2);
    int phasY = titoloSezione[5].getBounds().getCentreY() - (btnH / 2);

    btnDelayOn.setBounds(areaDelay.getRight() - btnW - marginX, delayY, btnW, btnH);
    btnDistOn.setBounds(areaDistortion.getRight() - btnW - marginX, distY, btnW, btnH);
    btnRevOn.setBounds(areaReverb.getRight() - btnW - marginX, revY, btnW, btnH);
    btnPhaserOn.setBounds(areaPhaser.getRight() - btnW - marginX, phasY, btnW, btnH);
#pragma endregion

#pragma region Posizionamento Volume Meter
    auto masterBounds = manopolaEffetto[9].getBounds();
    auto nameBounds = titoloManopolaEffetto[9].getBounds();

    int startY = nameBounds.getY();
    int endY = masterBounds.getBottom();
    int altezzaMeter = endY - startY;

    auto meterBox = juce::Rectangle<int>(masterBounds.getRight() + (12 * scale),
        startY,
        14 * scale,
        altezzaMeter);

    meterLeftArea = meterBox.removeFromLeft(meterBox.getWidth() / 2 - 1);
    meterRightArea = meterBox.removeFromRight(meterLeftArea.getWidth());
#pragma endregion

#pragma region Sezione oscilloscopio
    // Setta le proprietà del buffer visivo dell'oscilloscopio. Maggiori sample/block garantiscono un segnale più denso.
    oscilloscopio.setBufferSize(2048);
    oscilloscopio.setSamplesPerBlock(256);
    oscilloscopio.setRepaintRate(60);

    // Passa il riferimento all'audioProcessor affinché possa iniettarvi i buffer elaborati dal DSP.
    audioProcessor.puntatoreOscilloscopio = &oscilloscopio;
    oscilloscopio.setColours(juce::Colours::transparentBlack, juce::Colour(0xFF28FF5A));
    oscilloscopio.setOpaque(false);
    addAndMakeVisible(oscilloscopio);
    oscilloscopio.setBounds(workOsc.reduced(10 * scale));
#pragma endregion
}
#pragma endregion

//==============================================================================
// Risponde agli eventi click e drag del mouse. Traduce le coordinate fisiche (X) del cursore 
// in una posizione relativa per attivare il calcolo dell'accordatura del fret corrispondente.
void ZenkiGuitarModelAudioProcessorEditor::handleMouseEvent(const juce::MouseEvent& e)
{
    for (int i = 0; i < stringComponents.size(); ++i)
    {
        auto* sc = stringComponents.getUnchecked(i);

        if (sc->getBounds().contains(e.getPosition()))
        {
            float relPos = (e.position.x - (float)sc->getX()) / (float)sc->getWidth();
            relPos = juce::jlimit(0.0f, 1.0f, relPos);

            int posFret = juce::jlimit(0, numFret, (int)(relPos * numFret));
            int baseMidi = audioProcessor.getStringMidiNote(i);
            int midiNote = baseMidi + posFret;

            if (oldPosFret != posFret || oldMidiNote != midiNote)
            {
                oldPosFret = posFret;
                oldMidiNote = midiNote;

                juce::String nomeNota = juce::MidiMessage::getMidiNoteName(midiNote, true, true, 3);
                notaSuonataLabel.setText("Note: " + nomeNota + "  Fret: " + juce::String(posFret),
                    juce::dontSendNotification);

                // Richiesta di aggiornamento visivo al componente corda
                sc->stringPlucked(relPos);
                // Trigger sonoro sul motore fisico AudioProcessor
                audioProcessor.pluckString(i, relPos);
                break;
            }
        }
    }
}

//==============================================================================
#pragma region Metodi Accordatura
// Aggiorna logicamente e visivamente le label affiancate ai bottoni di accordatura
// calcolando il distacco in semitoni (+/-) rispetto all'accordatura standard EADGBE.
void ZenkiGuitarModelAudioProcessorEditor::updateTuningLabel(int stringIndex)
{
    if (stringIndex < 0 || stringIndex >= ZenkiGuitarModelAudioProcessor::numStrings)
        return;

    int currentNote = audioProcessor.getStringMidiNote(stringIndex);
    int defaultNote = ZenkiGuitarModelAudioProcessor::defaultMidiNotes[stringIndex];
    int delta = currentNote - defaultNote;

    juce::String noteName = juce::MidiMessage::getMidiNoteName(currentNote, true, true, 3);

    juce::String deltaStr;
    if (delta > 0) deltaStr = "(+" + juce::String(delta) + ")";
    else if (delta < 0) deltaStr = "(" + juce::String(delta) + ")";

    juce::String labelText = noteName;
    if (deltaStr.isNotEmpty())
        labelText += " " + deltaStr;

    tuningLabels.getUnchecked(stringIndex)->setText(labelText, juce::dontSendNotification);
}

void ZenkiGuitarModelAudioProcessorEditor::updateAllTuningLabels()
{
    for (int i = 0; i < ZenkiGuitarModelAudioProcessor::numStrings; ++i)
        updateTuningLabel(i);
}
#pragma endregion

//==============================================================================
#pragma region Metodi disegni UI
void ZenkiGuitarModelAudioProcessorEditor::SetTitle(juce::Graphics& g)
{
    float scale = (float)getWidth() / 750.0f;
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::FontOptions(18.0f * scale, juce::Font::bold));
    g.drawText("MODELLAZIONE FISICA CHITARRA", 0, 5 * scale, getWidth(), 30 * scale, juce::Justification::centred);
}

void ZenkiGuitarModelAudioProcessorEditor::SetLineaSeparatrice(juce::Graphics& g)
{
    float lineaY = areaCordeSotto.getY() - 4.0f;
    g.setColour(juce::Colour(0xFF4D453A));
    g.drawHorizontalLine((int)lineaY, 10.0f, (float)getWidth() - 10.0f);
}

void ZenkiGuitarModelAudioProcessorEditor::SetStrings(juce::Graphics& g)
{
    for (int i = 0; i < ZenkiGuitarModelAudioProcessor::numStrings; ++i)
    {
    }
}

// Rendering dei "tasti" visivi della chitarra tramite calcolo geometrico delle frazioni.
void ZenkiGuitarModelAudioProcessorEditor::SetSeparationFret(juce::Graphics& g)
{
    auto* primaCorda = stringComponents.getUnchecked(0);
    auto* ultimaCorda = stringComponents.getUnchecked(ZenkiGuitarModelAudioProcessor::numStrings - 1);

    float startX = (float)primaCorda->getX();
    float width = (float)primaCorda->getWidth();
    float fretW = width / (float)numFret;

    int yIniziale = primaCorda->getBounds().getCentreY();
    int yFinale = ultimaCorda->getBounds().getCentreY();

    int margine = 12;
    int startY = yIniziale - margine;
    int altezzaTotale = (yFinale - yIniziale) + (margine * 2);

    g.setColour(juce::Colours::lightgrey.withAlpha(0.5f));

    for (int i = 0; i <= numFret; ++i)
    {
        float x = startX + i * fretW;
        g.fillRect((int)x, startY, 3, altezzaTotale);
    }
}
#pragma endregion

#pragma region Funzione per preset
// Snodo centrale per l'applicazione dei preset.
// Interpreta l'ID ricevuto dal menu a tendina e converte la richiesta in modifiche di stato APVTS o chiamate XML.
void ZenkiGuitarModelAudioProcessorEditor::applicaPreset(int presetId)
{
    // Funzione helper isolata (lambda): formatta il passaggio dei dati verso l'APVTS normalizzando i parametri utente a valori tra 0.0 - 1.0.
    auto setParam = [this](const juce::String& id, float veroValore)
        {
            if (auto* param = audioProcessor.apvts.getParameter(id))
            {
                param->setValueNotifyingHost(param->convertTo0to1(veroValore));
            }
        };

    // Funzione helper isolata (lambda): formatta in batch il set delle 6 intonazioni.
    auto setTuning = [this](int n0, int n1, int n2, int n3, int n4, int n5)
        {
            audioProcessor.setStringMidiNote(0, n0);
            audioProcessor.setStringMidiNote(1, n1);
            audioProcessor.setStringMidiNote(2, n2);
            audioProcessor.setStringMidiNote(3, n3);
            audioProcessor.setStringMidiNote(4, n4);
            audioProcessor.setStringMidiNote(5, n5);
            updateAllTuningLabels();
        };

    // Preset User-Generated (Caricati da XML via disco).
    if (presetId >= 100)
    {
        audioProcessor.loadUserPreset(presetMenu.getText());
        return;
    }

    // Preset Hardcoded (Preset di fabbrica veloci, senza I/O su disco).
    switch (presetId)
    {
    case 1: // Default
        setParam("drive", 1.0f); setParam("gain", 0.5f);
        setParam("delayTime", 0.4f); setParam("delayFb", 0.0f);
        setParam("revMix", 0.0f); setParam("revSize", 50.0f);
        setParam("hardness", 0.5f); setParam("damping", 100.0f); setParam("sustain", 100.0f);
        setParam("delayOn", 0.0f); setParam("distOn", 0.0f); setParam("revOn", 0.0f);
        setParam("phaserRate", 1.0f); setParam("phaserDepth", 0.5f); setParam("phaserMix", 50.0f); setParam("phaserOn", 0.0f);
        setTuning(64, 59, 55, 50, 45, 40);
        break;
    case 2: // Dream Harp
        setParam("drive", 1.0f); setParam("gain", 0.10f);
        setParam("delayTime", 0.11f); setParam("delayFb", 36.0f);
        setParam("revMix", 100.0f); setParam("revSize", 100.0f);
        setParam("hardness", 0.01f); setParam("damping", 100.0f); setParam("sustain", 100.0f);
        setParam("delayOn", 1.0f); setParam("distOn", 1.0f); setParam("revOn", 1.0f);
        setParam("phaserRate", 4.0f); setParam("phaserDepth", 0.8f); setParam("phaserMix", 40.0f); setParam("phaserOn", 1.0f);
        setTuning(60, 57, 55, 52, 50, 48);
        break;
    case 3: // Electric
        setParam("drive", 8.6f); setParam("gain", 0.90f);
        setParam("delayTime", 0.03f); setParam("delayFb", 6.0f);
        setParam("revMix", 15.0f); setParam("revSize", 18.0f);
        setParam("hardness", 0.80f); setParam("damping", 100.0f); setParam("sustain", 100.0f);
        setParam("delayOn", 1.0f); setParam("distOn", 1.0f); setParam("revOn", 1.0f);
        setParam("phaserRate", 2.62f); setParam("phaserDepth", 0.5f); setParam("phaserMix", 20.0f); setParam("phaserOn", 1.0f);
        setTuning(67, 62, 58, 53, 48, 43);
        break;
    case 4: // Bass
        setParam("drive", 1.44f); setParam("gain", 0.45f);
        setParam("delayTime", 0.03f); setParam("delayFb", 5.0f);
        setParam("revMix", 0.0f); setParam("revSize", 0.0f);
        setParam("hardness", 0.20f); setParam("damping", 90.0f); setParam("sustain", 80.0f);
        setParam("delayOn", 1.0f); setParam("distOn", 1.0f); setParam("revOn", 0.0f);
        setParam("phaserRate", 1.5f); setParam("phaserDepth", 0.5f); setParam("phaserMix", 40.0f); setParam("phaserOn", 1.0f);
        setTuning(54, 49, 43, 38, 33, 52);
        break;
    case 5: // Ghost
        setParam("drive", 1.6f); setParam("gain", 0.5f);
        setParam("delayTime", 0.03f); setParam("delayFb", 5.0f);
        setParam("revMix", 10.0f); setParam("revSize", 20.0f);
        setParam("hardness", 0.20f); setParam("damping", 100.0f); setParam("sustain", 100.0f);
        setParam("delayOn", 0.0f); setParam("distOn", 1.0f); setParam("revOn", 1.0f);
        setParam("phaserRate", 5.05f); setParam("phaserDepth", 0.6f); setParam("phaserMix", 66.0f); setParam("phaserOn", 1.0f);
        setTuning(67, 62, 58, 53, 48, 43);
        break;
    default: break;
    }
}
#pragma endregion

#pragma region Aggiornamento menu preset
// Scansiona la memoria e il disco per rimpinguare dinamicamente le voci selezionabili nel menu preset.
void ZenkiGuitarModelAudioProcessorEditor::aggiornaMenuPreset()
{
    presetMenu.clear();

    presetMenu.addItem("Default", 1);
    presetMenu.addItem("Dream Harp", 2);
    presetMenu.addItem("Electric", 3);
    presetMenu.addItem("Bass", 4);
    presetMenu.addItem("Ghost", 5);

    presetMenu.addSeparator();

    juce::StringArray userPresets = audioProcessor.getAvailableUserPresets();
    int currentId = 100;

    for (const auto& name : userPresets)
    {
        presetMenu.addItem(name, currentId);
        currentId++;
    }
}
#pragma endregion