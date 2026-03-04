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
 */

#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"
#include "OperatorEditor.h"
#include "GlobalEditor.h"
#include "DXComponents.h"
#include "DXLookNFeel.h"
#include "CartManager.h"

//==============================================================================
/**
*/
class DexedAudioProcessorEditor  : public AudioProcessorEditor, public ComboBox::Listener, public Timer,
                                   public FileDragAndDropTarget, public KeyListener {
    MidiKeyboardComponent midiKeyboard;
    OperatorEditor operators[6];
    Colour background;
    CartManager cartManager;
    // This cover is used to disable main window when cart manager is shown
    Component cartManagerCover;

    SharedResourcePointer<DXLookNFeel> lookAndFeel;
    std::unique_ptr<juce::DialogWindow> dexedParameterDialog;
    #ifdef DEXED_EVENT_DEBUG
        FocusLogger focusLogger;
    #endif

    void resetSize();

    Component frameComponent;

    // Futuristic reskin additions
    WaveformVisualizer waveformVis;
    std::unique_ptr<TextButton> chorusOffBtn;
    std::unique_ptr<TextButton> chorusIBtn;
    std::unique_ptr<TextButton> chorusIIBtn;

    void updateChorusButtons();

public:
    DexedAudioProcessor *processor;
    GlobalEditor global;
    
    DexedAudioProcessorEditor (DexedAudioProcessor* ownerFilter);
    ~DexedAudioProcessorEditor();
    virtual void timerCallback() override;

    virtual void paint (Graphics& g) override;
    virtual void comboBoxChanged (ComboBox* comboBoxThatHasChanged) override;
    void updateUI();
    void rebuildProgramCombobox();
    void loadCart(File file);
    void saveCart();
    void initProgram();
    void storeProgram();
    void cartShow();
    void parmShow();
    void tuningShow();
    void discoverMidiCC(Ctrl *ctrl);

    static float getLargestScaleFactor();
    void resetZoomFactor();

    virtual bool isInterestedInFileDrag (const StringArray &files) override;
    virtual void filesDropped (const StringArray &files, int x, int y ) override;
    std::unique_ptr<ComponentTraverser> createFocusTraverser() override;

    bool keyPressed(const KeyPress& key, Component* originatingComponent) override;

    // Extra 52px for the waveform visualizer strip between operator rows
    static const int WINDOW_SIZE_X = 866;
    static const int WINDOW_SIZE_Y = 802;
    static const int VIS_STRIP_Y   = 219;  // y-position of the visualizer strip
    static const int VIS_STRIP_H   = 52;   // height of the visualizer strip
};


#endif  // PLUGINEDITOR_H_INCLUDED
