/**
 *
 * Copyright (c) 2013-2018 Pascal Gauthier.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GlobalEditor.h"
#include "ParamDialog.h"
#include "SysexComm.h"
#include "TuningShow.h"
#include "Dexed.h"
#include "math.h"
#include <fstream>
#include <memory>

#include "msfa/fm_op_kernel.h"

//==============================================================================
DexedAudioProcessorEditor::DexedAudioProcessorEditor (DexedAudioProcessor* ownerFilter)
    : AudioProcessorEditor (ownerFilter),
      midiKeyboard (ownerFilter->keyboardState, MidiKeyboardComponent::horizontalKeyboard),
      cartManager(this)
{
    processor = ownerFilter;

    // We have to set size at startup because the keyboard doesnt show up before being added
    resetSize();
    setExplicitFocusOrder(1);


    frameComponent.setBounds(0,0, WINDOW_SIZE_X, (processor->showKeyboard ? WINDOW_SIZE_Y : WINDOW_SIZE_Y - 94));
    addAndMakeVisible(&frameComponent);
    lookAndFeel->setDefaultLookAndFeel(lookAndFeel);
    background = lookAndFeel->background;

    // OPERATORS - Row 1 (unchanged positions)
    frameComponent.addAndMakeVisible(&(operators[0]));
    operators[0].setBounds(2, 1, 287, 218);
    operators[0].bind(processor, 0);

    frameComponent.addAndMakeVisible(&(operators[1]));
    operators[1].setBounds(290, 1, 287, 218);
    operators[1].bind(processor, 1);

    frameComponent.addAndMakeVisible(&(operators[2]));
    operators[2].setBounds(578, 1, 287, 218);
    operators[2].bind(processor, 2);

    // Waveform visualizer strip (between the two operator rows)
    frameComponent.addAndMakeVisible(&waveformVis);
    waveformVis.setBounds(0, VIS_STRIP_Y, 700, VIS_STRIP_H);

    // Juno-style chorus mode buttons (right side of visualizer strip)
    chorusOffBtn.reset(new TextButton("CHORUS OFF"));
    chorusOffBtn->setButtonText("OFF");
    chorusOffBtn->onClick = [this] {
        processor->fxChorusMode->publishValue(0.0f);
        updateChorusButtons();
    };
    frameComponent.addAndMakeVisible(chorusOffBtn.get());
    chorusOffBtn->setBounds(706, VIS_STRIP_Y + 4, 46, VIS_STRIP_H - 8);

    chorusIBtn.reset(new TextButton("CHORUS I"));
    chorusIBtn->setButtonText("CHR I");
    chorusIBtn->onClick = [this] {
        processor->fxChorusMode->publishValue(0.5f);
        updateChorusButtons();
    };
    frameComponent.addAndMakeVisible(chorusIBtn.get());
    chorusIBtn->setBounds(755, VIS_STRIP_Y + 4, 46, VIS_STRIP_H - 8);

    chorusIIBtn.reset(new TextButton("CHORUS II"));
    chorusIIBtn->setButtonText("CHR II");
    chorusIIBtn->onClick = [this] {
        processor->fxChorusMode->publishValue(1.0f);
        updateChorusButtons();
    };
    frameComponent.addAndMakeVisible(chorusIIBtn.get());
    chorusIIBtn->setBounds(804, VIS_STRIP_Y + 4, 58, VIS_STRIP_H - 8);

    // OPERATORS - Row 2 (shifted down by VIS_STRIP_H)
    frameComponent.addAndMakeVisible(&(operators[3]));
    operators[3].setBounds(2, VIS_STRIP_Y + VIS_STRIP_H, 287, 218);
    operators[3].bind(processor, 3);

    frameComponent.addAndMakeVisible(&(operators[4]));
    operators[4].setBounds(290, VIS_STRIP_Y + VIS_STRIP_H, 287, 218);
    operators[4].bind(processor, 4);

    frameComponent.addAndMakeVisible(&(operators[5]));
    operators[5].setBounds(578, VIS_STRIP_Y + VIS_STRIP_H, 287, 218);
    operators[5].bind(processor, 5);

    // add the midi keyboard component..
    frameComponent.addAndMakeVisible (&midiKeyboard);

    // The DX7 is a badass on the bass, keep it that way
    midiKeyboard.setLowestVisibleKey(24);
    midiKeyboard.setBounds(4, VIS_STRIP_Y + VIS_STRIP_H + 218 + 220 + 4, getWidth() - 8, 90);
    midiKeyboard.setTitle("Keyboard keys");

    frameComponent.addAndMakeVisible(&global);
    global.setBounds(2, VIS_STRIP_Y + VIS_STRIP_H + 218, 864, 220);
    global.bind(this);

    global.setMonoState(processor->isMonoMode());

    rebuildProgramCombobox();
    global.programs->addListener(this);

    frameComponent.addChildComponent(&cartManagerCover);
    cartManagerCover.addChildComponent(&cartManager);
    cartManager.setVisible(true);

    AffineTransform scale = AffineTransform::scale(processor->getZoomFactor());
    frameComponent.setTransform(scale);
    resetSize();
    addKeyListener(this);
    updateUI();
    updateChorusButtons();
    startTimer(100);
}

DexedAudioProcessorEditor::~DexedAudioProcessorEditor() {
    stopTimer();
    processor->unbindUI();
    setLookAndFeel(nullptr);
}

void DexedAudioProcessorEditor::updateChorusButtons() {
    float mode = processor->fxChorusMode->getValueHost();
    // Highlight the active button with teal-green, others default
    auto highlight = Colour(0xFF00E5A0);
    auto normal    = Colour(0xFF00B8D4);
    chorusOffBtn->setColour(TextButton::buttonColourId, mode <= 0.25f ? highlight : normal);
    chorusOffBtn->setColour(TextButton::textColourOffId, mode <= 0.25f ? Colour(0xFF0A0E14) : Colours::white);
    chorusIBtn->setColour(TextButton::buttonColourId,   (mode > 0.25f && mode <= 0.75f) ? highlight : normal);
    chorusIBtn->setColour(TextButton::textColourOffId,  (mode > 0.25f && mode <= 0.75f) ? Colour(0xFF0A0E14) : Colours::white);
    chorusIIBtn->setColour(TextButton::buttonColourId,  mode > 0.75f ? highlight : normal);
    chorusIIBtn->setColour(TextButton::textColourOffId, mode > 0.75f ? Colour(0xFF0A0E14) : Colours::white);
}

//==============================================================================
void DexedAudioProcessorEditor::paint (Graphics& g) {
    g.setColour(background);
    g.fillRoundedRectangle(0.0f, 0.0f, (float) getWidth(), (float) getHeight(), 0);

    // The paint() here is on the outer (scaled) container.
    // The waveformVis strip background is painted by the component itself.
    // Draw a yellow accent label for the CHORUS section header.
    float factor = processor->getZoomFactor();
    float stripY  = VIS_STRIP_Y  * factor;
    float stripH  = VIS_STRIP_H  * factor;

    // Horizontal separator lines bordering the visualizer strip
    g.setColour(Colour(0xFF00B8D4).withAlpha(0.3f));
    g.drawHorizontalLine((int)stripY,        0.0f, (float)getWidth());
    g.drawHorizontalLine((int)(stripY + stripH), 0.0f, (float)getWidth());

    // "CHORUS" label above the buttons
    g.setColour(Colour(0xFF00E5A0).withAlpha(0.8f));
    g.setFont(Font(9.5f, Font::bold));
    g.drawText("JUNO CHORUS", (int)(706 * factor), (int)stripY, (int)(160 * factor), 14, Justification::centred, false);
}

void DexedAudioProcessorEditor::cartShow() {
    stopTimer();
    cartManager.resetActiveSysex();
    cartManagerCover.setBounds(0, 0, WINDOW_SIZE_X , WINDOW_SIZE_Y - 94);
    cartManager.setBounds(16, 16, cartManagerCover.getWidth() - 32, cartManagerCover.getHeight() - 32);
    cartManager.updateCartFilename();
    cartManagerCover.setVisible(true);
    cartManager.initialFocus();
}

std::unique_ptr<ComponentTraverser> DexedAudioProcessorEditor::createFocusTraverser() {
    return std::make_unique<FocusTraverser>();
}

void DexedAudioProcessorEditor::loadCart(File file) {
    Cartridge cart;

    int rc = cart.load(file);

    if ( rc < 0 ) {
        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon,
                                          "Error",
                                          "Unable to open: " + file.getFullPathName());
        return;
    }

    if ( rc != 0 ) {
        rc = AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Unable to find DX7 sysex cartridge in file",
                                          "This sysex file is not for the DX7 or it is corrupted. "
                                          "Do you still want to load this file as random data ?");
        if ( rc == 0 )
            return;
    }

    processor->loadCartridge(cart);
    rebuildProgramCombobox();
    processor->setCurrentProgram(0);
    global.programs->setSelectedId(processor->getCurrentProgram()+1, dontSendNotification);
    processor->updateHostDisplay();

    processor->activeFileCartridge = file;
}

void DexedAudioProcessorEditor::saveCart() {
    File startFileName = processor->activeFileCartridge.exists() ? processor->activeFileCartridge : processor->dexedCartDir;

    FileChooser fc ("Export DX sysex...", processor->dexedCartDir, "*.syx;*.SYX", 1);
    if ( fc.browseForFileToSave(true) ) {
        if ( ! processor->currentCart.saveVoice(fc.getResults().getReference(0)) ) {
            AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon,
                                              "Error",
                                              "Unable to write: " + fc.getResults().getReference(0).getFullPathName());
        }
    }
}

void DexedAudioProcessorEditor::tuningShow() {
    auto te = new TuningShow();
    te->setTuning( processor->synthTuningState->getTuning() );

    DialogWindow::LaunchOptions options;
    options.content.setOwned(te);
    options.dialogTitle = "Current Tuning";
    options.dialogBackgroundColour = Colour(0xFF323E44);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;

    auto dialogwindow = options.launchAsync();
}

// I don't know why closeButtonPressed is not implemented in the standard DialogWindow version.
class DexedDialogWindow : public juce::DialogWindow {
public:
    DexedDialogWindow(const juce::String& title, juce::Colour backgroundColour)
        : juce::DialogWindow(title, backgroundColour, true, false) {

    }
    void closeButtonPressed() override {
        setVisible(false);
    }
};

void DexedAudioProcessorEditor::parmShow() {
    int tp = processor->getEngineType();
    auto param = new ParamDialog();
    param->setColour(AlertWindow::backgroundColourId, Colour(0xFF323E44));
    param->setDialogValues(processor->controllers, processor->sysexComm, tp, processor->showKeyboard, processor->getZoomFactor());
    param->setIsStandardTuning(processor->synthTuningState->is_standard_tuning() );
    param->setTuningCallback([this](ParamDialog *p, ParamDialog::TuningAction which) {
                                switch(which)
                                {
                                case ParamDialog::LOAD_SCL:
                                    this->processor->applySCLTuning();
                                    break;
                                case ParamDialog::LOAD_KBM:
                                    this->processor->applyKBMMapping();
                                    break;
                                case ParamDialog::RESET_TUNING:
                                    this->processor->retuneToStandard();
                                    break;
                                case ParamDialog::SHOW_TUNING:
                                    // consider https://forum.juce.com/t/closing-a-modal-dialog-window/2961
                                    this->tuningShow();
                                    break;
                                }
                                p->setIsStandardTuning(this->processor->synthTuningState->is_standard_tuning() );
                            } );

    auto generalCallback = [this](ParamDialog *param)
                               {
                                   int tpo;
                                   float scale = this->processor->getZoomFactor();
                                   bool ret = param->getDialogValues(this->processor->controllers, this->processor->sysexComm, &tpo, &this->processor->showKeyboard, &scale);
                                   this->processor->setZoomFactor(scale);
                                   this->processor->setEngineType(tpo);
                                   this->processor->savePreference();

                                   AffineTransform scaleAffine = AffineTransform::scale(this->processor->getZoomFactor());
                                   frameComponent.setTransform(scaleAffine);
                                   resetSize();

                                   if ( ret == false ) {
                                       AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Midi Interface", "Error opening midi ports");
                                   }
                               };
    param->setGeneralCallback(generalCallback);

    dexedParameterDialog = std::make_unique<DexedDialogWindow>("TapSynth Parameters", Colour(0xFF323E44));
    dexedParameterDialog->setContentOwned(param, true);
    frameComponent.addAndMakeVisible(dexedParameterDialog.get());
    dexedParameterDialog->centreAroundComponent(&frameComponent, param->getWidth(), param->getHeight() + dexedParameterDialog->getTitleBarHeight());
    dexedParameterDialog->enterModalState(true,{});
}

void DexedAudioProcessorEditor::initProgram() {
    processor->resetToInitVoice();
}

void DexedAudioProcessorEditor::comboBoxChanged (ComboBox* comboBoxThatHasChanged) {
    processor->setCurrentProgram(global.programs->getSelectedId()-1);
    processor->updateHostDisplay();
}

void DexedAudioProcessorEditor::timerCallback() {
    if ( processor->forceRefreshUI ) {
        processor->forceRefreshUI = false;
        updateUI();
    }

    if ( ! processor->peekVoiceStatus() )
        return;

    for(int i=0;i<6;i++) {
        //operators[i].updateGain(sqrt(processor->voiceStatus.amp[5 - i]) / 8196);        // TODO: FUGLY !!!! change this sqrt nonsense

        const int amp_min = 1036152;
        const float one_per_amp_diff = (float)(1.0 / (259037922 - amp_min));
        // Note, that minimum and maximum values of ``amp`` are different and depend on the engines. 
        // These two constants were determined from the results produced by the OPL engine,
        // because its minimum is smaller, its maximum is higher than in the other cases. 

        int amp = processor->voiceStatus.amp[5 - i] - amp_min;
        if (amp <= 0) amp = 0;
        operators[i].updateGain(amp * one_per_amp_diff);


        operators[i].updateEnvPos(processor->voiceStatus.ampStep[5 - i]);
    }
    global.updatePitchPos(processor->voiceStatus.pitchStep);
    global.updateVu(processor->vuSignal);

    // Update waveform visualizer
    waveformVis.updateFromRingBuffer(
        processor->waveformCapture,
        processor->waveformCapturePos.load(std::memory_order_acquire),
        DexedAudioProcessor::WAVEFORM_CAPTURE_SIZE);
    waveformVis.repaint();
}

void DexedAudioProcessorEditor::updateUI() {
    for(int i=0;i<processor->ctrl.size();i++) {
        processor->ctrl[i]->updateComponent();
    }
    for(int i=0;i<6;i++) {
        operators[i].updateDisplay();
    }
    rebuildProgramCombobox();
    global.updateDisplay();
    cartManager.updateCartFilename();
    updateChorusButtons();
}

void DexedAudioProcessorEditor::rebuildProgramCombobox() {
    global.programs->clear(dontSendNotification);

    processor->currentCart.getProgramNames(processor->programNames);

    for(int i=0;i<processor->getNumPrograms();i++) {
        String id;
        id << (i+1) << ". " << processor->getProgramName(i);
        global.programs->addItem(id, i+1);
    }

    global.programs->setSelectedId(processor->getCurrentProgram()+1, dontSendNotification);

    String name = Cartridge::normalizePgmName((const char *) processor->data+145);
    cartManager.setActiveProgram(processor->getCurrentProgram(), name);
    if ( name != processor->getProgramName(processor->getCurrentProgram()) )
        global.programs->setText("**. " + name, dontSendNotification);

    cartManager.resetActiveSysex();
}

void DexedAudioProcessorEditor::storeProgram() {
    String currentName = Cartridge::normalizePgmName((const char *) processor->data+145);
    Cartridge destSysex = processor->currentCart;
    File *externalFile = NULL;

    bool activeCartridgeFound = processor->activeFileCartridge.exists();

    while (true) {
        String msg;

        if ( externalFile == NULL ) {
            if ( activeCartridgeFound )
                msg = "Store program to current (" + processor->activeFileCartridge.getFileName() + ") / new cartridge";
            else
                msg = "Store program to current / new cartridge";
        } else {
            msg = "Store program to " + externalFile->getFileName();
        }

        AlertWindow dialog("Store Program", msg, AlertWindow::NoIcon, this);
        dialog.addTextEditor("Name", currentName, String("Name"), false);
        // TODO: fix the name length to 10

        StringArray programs;
        destSysex.getProgramNames(programs);
        dialog.addComboBox("Dest", programs, "Program Destination");

        if ( externalFile == NULL ) {
            StringArray saveAction;
            saveAction.add("Store program to DAW plugin song state");
            saveAction.add("Store program and create a new copy of the .syx cartridge");
            if ( activeCartridgeFound )
                saveAction.add("Store program and overwrite current .syx cartridge");

            dialog.addComboBox("SaveAction", saveAction, "Store Action");
        }

        dialog.addButton("OK", 0, KeyPress(KeyPress::returnKey));
        dialog.addButton("CANCEL", 1, KeyPress(KeyPress::escapeKey));
        dialog.addButton("EXTERNAL FILE", 2, KeyPress());
        int response = dialog.runModalLoop();

        if ( response == 2 ) {
            FileChooser fc("Destination Sysex", processor->dexedCartDir, "*.syx;*.SYX;*.*", 1);

            if ( fc.browseForFileToOpen() ) {
                if ( externalFile != NULL ) 
                    delete externalFile;

                externalFile = new File(fc.getResults().getReference(0));
                if ( destSysex.load(*externalFile) == 0 )
                    continue;
                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Read error", "Unable to read file");
            }
        }

        if ( response == 0 ) {
            TextEditor *name = dialog.getTextEditor("Name");
            ComboBox *dest = dialog.getComboBoxComponent("Dest");

            int programNum = dest->getSelectedItemIndex();
            String programName(name->getText());
            if ( programName.length() > 10 ) {
                int toStrip = programName.length() - 10;
                programName = programName.dropLastCharacters(toStrip);
            }

            if ( externalFile == NULL ) {
                processor->currentCart.packProgram((uint8_t *) processor->data, programNum, programName, processor->controllers.opSwitch);
                rebuildProgramCombobox();
                processor->setCurrentProgram(programNum);
                processor->updateHostDisplay();

                int action = dialog.getComboBoxComponent("SaveAction")->getSelectedItemIndex();
                if ( action > 0 ) {
                    File destination = processor->activeFileCartridge;
                    if ( action == 1 ) {
                        FileChooser fc("Destination Sysex", processor->dexedCartDir, "*.syx;*.SYX", 1);
                        if ( ! fc.browseForFileToSave(true) )
                            break;
                        destination = fc.getResult();
                    }

                    processor->currentCart.saveVoice(destination);
                    processor->activeFileCartridge = destination;
                }
            } else {
                destSysex.packProgram((uint8_t *) processor->data, programNum, programName, processor->controllers.opSwitch);
                if ( ! destSysex.saveVoice(*externalFile)) {
                    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Write error", "Unable to write file");
                }
            }
        }
        break;
    }

    if ( externalFile != NULL )
        delete externalFile;
    cartManager.resetActiveSysex();
}

class MidiCCListener: public AlertWindow, Value::Listener {
    DexedAudioProcessorEditor *editor;
    Ctrl *target;
public :
    MidiCCListener(DexedAudioProcessorEditor *editor, Ctrl *target) : AlertWindow("","", AlertWindow::InfoIcon, editor) {
        this->editor = editor;
        this->target = target;
        setMessage("Mapping: " + String(target->label) + ", waiting for midi controller change (CC) message...");
        addButton("CANCEL", -1);
        editor->processor->lastCCUsed.setValue(-1);
        editor->processor->lastCCUsed.addListener(this);
    }

    ~MidiCCListener() {
        editor->processor->lastCCUsed.removeListener(this);
    }

    void valueChanged(Value &value) {
        int cc = value.getValue();
        editor->processor->mappedMidiCC.remove(cc);
        editor->processor->mappedMidiCC.set(cc, target);
        editor->processor->savePreference();
        exitModalState(0);
    }
};

void DexedAudioProcessorEditor::discoverMidiCC(Ctrl *ctrl) {
    MidiCCListener ccListener(this, ctrl);
    ccListener.runModalLoop();
}

float DexedAudioProcessorEditor::getLargestScaleFactor() {
    // constexpr float TESTING_SCALE_FACTOR[] = { 4.0f, 3.0f, 2.0f, 1.5f, 1.0f };
    //
    // for (float factor: TESTING_SCALE_FACTOR) {
    //     const juce::Rectangle<int> rect(WINDOW_SIZE_X * factor, WINDOW_SIZE_Y * factor);
    //
    //     // validate if there is really a display that can show the complete plugin size
    //     for (auto& display : Desktop::getInstance().getDisplays().displays) {
    //         int height = display.userArea.getHeight();
    //         int width = display.userArea.getWidth();
    //
    //         TRACE("Testing size %d x %d < Dexed Window %d x %d", height, width, rect.getWidth(), rect.getHeight() );
    //         if ( height > rect.getHeight() && width > rect.getWidth() ) {
    //             TRACE("Found factor %f for display %s with size %d x %d", factor, display.userArea.toString().toRawUTF8(), height, width );
    //             return factor;
    //         }
    //     }
    // }
    //
    // TRACE("No suitable display found, returning default scale factor 1.0");

    // For now, always return 4.0f as the maximum scale factor.
    return 4.0f;
}

void DexedAudioProcessorEditor::resetZoomFactor() {
    TRACE("Resetting zoom factor to 1.0");
    processor->setZoomFactor(1.0);
    processor->savePreference();
    AffineTransform scale = AffineTransform::scale(processor->getZoomFactor());
    frameComponent.setTransform(scale);
    resetSize();
}

bool DexedAudioProcessorEditor::isInterestedInFileDrag (const StringArray &files)
{
    if( files.size() != 1 ) return false;

    for( auto i = files.begin(); i != files.end(); ++i )
    {
        if( i->endsWithIgnoreCase( ".scl" ) || i->endsWithIgnoreCase( ".kbm" ) )
            return true;
    }
    return false;
}

void DexedAudioProcessorEditor::filesDropped (const StringArray &files, int x, int y )
{
    if( files.size() != 1 ) return;
    auto fn = files[0];
    try {
        std::ifstream in(fn.toStdString(), std::ifstream::ate | std::ifstream::binary);
        std::ifstream::pos_type filesize = in.tellg();
        if (fn.endsWithIgnoreCase(".scl"))
        {
            if (filesize == 0) {
                AlertWindow::showMessageBox(
                    AlertWindow::WarningIcon,
                    "File size error!",
                    "File \'" + fn.toStdString() + "\' is empty."
                );
            }
            else if (filesize > MAX_SCL_KBM_FILE_SIZE) {
                AlertWindow::showMessageBox(
                    AlertWindow::WarningIcon,
                    "File size error!",
                    "File \'" + fn.toStdString() + "\' has " + std::to_string(filesize) + " bytes, exceeding the maximum limit ("+std::to_string(MAX_SCL_KBM_FILE_SIZE)+")."
                );
            }
            else {
                processor->applySCLTuning(File(fn));
            }
        }
        if (fn.endsWithIgnoreCase(".kbm"))
        {
            if (filesize == 0) {
                AlertWindow::showMessageBox(
                    AlertWindow::WarningIcon,
                    "File size error!",
                    "File \'" + fn.toStdString() + "\' is empty."
                );
            }
            else if (filesize > MAX_SCL_KBM_FILE_SIZE) {
                AlertWindow::showMessageBox(
                    AlertWindow::WarningIcon,
                    "File size error!",
                    "File \'" + fn.toStdString() + "\' has " + std::to_string(filesize) + " bytes, exceeding the maximum limit (" + std::to_string(MAX_SCL_KBM_FILE_SIZE) + ")."
                );
            }
            else {
                processor->applyKBMMapping(File(fn));
            }
        }
    }
    catch (const std::ios_base::failure& ex) {
        AlertWindow::showMessageBox(
            AlertWindow::WarningIcon, 
            "I/O error!", 
            "Related to file \'" + fn.toStdString() + "\', an exception (std::ios_base::failure) occured: " + ex.what()
        );
    }
    catch (std::bad_alloc& ex) {
        AlertWindow::showMessageBox(
            AlertWindow::WarningIcon, 
            "I/O error!", 
            "Related to file \'" + fn.toStdString() + "\', an exception (std::bad_alloc) occured: " + ex.what());
    }
    catch (...) {
        AlertWindow::showMessageBox(
            AlertWindow::WarningIcon, 
            "I/O error!", 
            "Related to file \'"+fn.toStdString()+"\', an unknown exception occured.");
    };
}

bool DexedAudioProcessorEditor::keyPressed(const KeyPress& key, Component* originatingComponent) {
    int keycode = key.getKeyCode();
    ModifierKeys mods = key.getModifiers();

    #ifdef DEXED_EVENT_DEBUG
        TRACE("key pressed: %d\n", keycode);
    #endif

    if ( (keycode >= '1' && keycode <= '6') && mods.isCtrlDown() ) {
        int op = keycode - '1';

        if ( mods.isShiftDown() ) {
            operators[op].toggleOpSwitch();
            return true;
        }

        operators[op].grabKeyboardFocus();
        return true;
    }

    if ( keycode == 'G' && mods.isCtrlDown() ) {
        global.grabKeyboardFocus();
        return true;
    }

    if ( keycode == 'L' && mods.isCtrlDown() ) {
        cartShow();
        return true;
    }

    if ( keycode == 'P' && mods.isCtrlDown() ) {
        parmShow();
        return true;
    }

    if ( key.getKeyCode() == KeyPress::escapeKey ) {
        cartManager.hideCartridgeManager();
        return true;
    }

    return false;
}

void DexedAudioProcessorEditor::resetSize() {
    float factor = processor->getZoomFactor();
    setSize(WINDOW_SIZE_X * factor, (processor->showKeyboard ? WINDOW_SIZE_Y : WINDOW_SIZE_Y - 94) * factor);
}