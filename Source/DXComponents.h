/**
 *
 * Copyright (c) 2014-2024 Pascal Gauthier.
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

#ifndef DXCOMPONENTS_H_INCLUDED
#define DXCOMPONENTS_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include <stdint.h>

//==============================================================================
// THIS IS A TEMPORY FIX FOR COMBOBOX ACCESSIBILITY
// SEE: https://forum.juce.com/t/bug-combo-box-accessibility-bug/62501/2
// WILL BE REMOVED ONCE WE UPDATE TO JUCE 8
class ComboBoxAccessibilityHandlerFix final : public AccessibilityHandler
{
public:
    explicit ComboBoxAccessibilityHandlerFix (ComboBox& comboBoxToWrap)
        : AccessibilityHandler (comboBoxToWrap,
                                AccessibilityRole::comboBox,
                                getAccessibilityActions (comboBoxToWrap),
                                { std::make_unique<ComboBoxValueInterface> (comboBoxToWrap) }),
          comboBox (comboBoxToWrap)
    {
    }

    AccessibleState getCurrentState() const override
    {
        auto state = AccessibilityHandler::getCurrentState().withExpandable();

        return comboBox.isPopupActive() ? state.withExpanded() : state.withCollapsed();
    }

    String getTitle() const override  { return comboBox.getTitle(); }  // THE FIX IS RIGHT HERE
    String getHelp() const override   { return comboBox.getTooltip(); }

private:
    class ComboBoxValueInterface final : public AccessibilityTextValueInterface
    {
    public:
        explicit ComboBoxValueInterface (ComboBox& comboBoxToWrap)
            : comboBox (comboBoxToWrap)
        {
        }

        bool isReadOnly() const override                 { return true; }
        String getCurrentValueAsString() const override  { return comboBox.getText(); }
        void setValueAsString (const String&) override   {}

    private:
        ComboBox& comboBox;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComboBoxValueInterface)
    };

    static AccessibilityActions getAccessibilityActions (ComboBox& comboBox)
    {
        return AccessibilityActions().addAction (AccessibilityActionType::press,    [&comboBox] { comboBox.showPopup(); })
                                     .addAction (AccessibilityActionType::showMenu, [&comboBox] { comboBox.showPopup(); });
    }

    ComboBox& comboBox;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComboBoxAccessibilityHandlerFix)
};
//==============================================================================

class EnvDisplay : public Component {
public:
    EnvDisplay();
    uint8_t *pvalues;
    char vPos;    
    void paint(Graphics &g);
};

class PitchEnvDisplay : public Component {
    char rvalues[8];
public:
    PitchEnvDisplay();
    uint8_t *pvalues;
    char vPos;
    void paint(Graphics &g);
};

class LcdDisplay : public Component {
public:
    LcdDisplay();
    void setSystemMsg(String msg);
    String paramMsg;
    void paint(Graphics &g);    
};

class ComboBoxImage : public ComboBox {
    Image items;
    int itemHeight;
    PopupMenu popup;
    int itemPos[4];
public:
    ComboBoxImage();

    virtual void paint(Graphics &g) override;
    virtual void showPopup() override;
    void setImage(Image image);
    void setImage(Image image, int pos[]);

    std::unique_ptr<AccessibilityHandler> createAccessibilityHandler() override {
        return std::make_unique<ComboBoxAccessibilityHandlerFix> (*this);
    }

};

class ProgramSelector : public ComboBox {
    float accum_wheel;
public:
    ProgramSelector() {
        setWantsKeyboardFocus(true);
        setTitle("Program selector");
    }
    virtual void mouseDown(const MouseEvent &event) override;
    virtual void mouseEnter(const MouseEvent &event) override { accum_wheel = 0; }
    virtual void mouseWheelMove(const MouseEvent &event, const MouseWheelDetails &wheel) override;
    virtual void paint(Graphics &g) override;

    std::unique_ptr<AccessibilityHandler> createAccessibilityHandler() override {
        return std::make_unique<ComboBoxAccessibilityHandlerFix> (*this);
    }
};

// Simple Slider to make 10 % jumps when shift is pressed on keypress, but more precise with mouse wheel.
class DXSlider : public Slider {
public:
    DXSlider(const String& componentName) : Slider(componentName) {
        setWantsKeyboardFocus(true);
    }

    bool keyPressed(const KeyPress &key) override {
        if ( key.getModifiers().isShiftDown() ) {
            float len = getRange().getLength() * 0.10;
            if (key.getKeyCode() == key.upKey) {
                setValue(getValue() + len);
                return true;
            }
            if (key.getKeyCode() == key.downKey) {
                setValue(getValue() - len);
                return true;
            }
        } else {
            return Slider::keyPressed(key);
        }
        return false;
    }

    void mouseWheelMove(const MouseEvent &event, const MouseWheelDetails &wheel) override {
        auto newWheel = wheel;
        const auto speed = event.mods.isShiftDown() ? 0.05 : 1.0;
        newWheel.deltaY *= speed;
        Slider::mouseWheelMove(event, newWheel);
    }
};

class FocusLogger final : public juce::FocusChangeListener
{
public:
    FocusLogger ()
    {
        juce::Desktop::getInstance ().addFocusChangeListener (this);
    }

    ~FocusLogger () override
    {
        juce::Desktop::getInstance ().removeFocusChangeListener (this);
    }

    void globalFocusChanged (juce::Component * focusedComponent) override
    {
        if (focusedComponent == nullptr)
            return;

        DBG ("Component title: " << focusedComponent->getTitle ());
        DBG ("Component type: " << typeid (*focusedComponent).name ());
        DBG ("---");
    }
};

//==============================================================================
/**
 * Futuristic oscilloscope-style waveform visualizer.
 * Displays a glowing blue waveform in the center of the UI.
 * Call updateFromRingBuffer() from the editor's timer callback to update.
 */
class WaveformVisualizer : public Component {
public:
    static const int BUFFER_SIZE = 512;

    WaveformVisualizer() {
        memset(displaySamples, 0, sizeof(displaySamples));
    }

    /** Copy the most recent BUFFER_SIZE samples from a ring buffer.
     *  @param ringBuf   The source ring buffer
     *  @param writePos  The current write position in the ring buffer
     *  @param ringSize  Total size of the ring buffer (must equal BUFFER_SIZE or be a multiple)
     */
    void updateFromRingBuffer(const float *ringBuf, int writePos, int ringSize) {
        // ringSize is expected to equal BUFFER_SIZE (both 512),
        // but may also be a multiple. Use modulo for general wrapping.
        for (int i = 0; i < BUFFER_SIZE; i++) {
            int idx = (writePos - BUFFER_SIZE + i + ringSize) % ringSize;
            displaySamples[i] = ringBuf[idx];
        }
    }

    void paint(Graphics &g) override {
        const int w = getWidth();
        const int h = getHeight();

        // Deep dark background with rounded rect
        g.setColour(Colour(0xFF030810));
        g.fillRoundedRectangle(0.0f, 0.0f, (float)w, (float)h, 3.0f);

        // Subtle border
        g.setColour(Colour(0xFF00B8D4).withAlpha(0.1f));
        g.drawRoundedRectangle(0.0f, 0.0f, (float)w, (float)h, 3.0f, 0.5f);

        // Horizontal center grid line
        g.setColour(Colour(0xFF00B8D4).withAlpha(0.12f));
        g.drawHorizontalLine(h / 2, 2.0f, (float)(w - 2));

        // Vertical grid lines
        for (int x = 0; x < w; x += w / 8) {
            g.setColour(Colour(0xFF00B8D4).withAlpha(0.06f));
            g.drawVerticalLine(x, 2.0f, (float)(h - 2));
        }

        // Section label
        g.setColour(Colour(0xFF00E5A0).withAlpha(0.5f));
        g.setFont(Font(9.0f, Font::bold));
        g.drawText("WAVEFORM", 6, 3, 70, 12, Justification::left, false);

        // Waveform path
        const float xScale  = (float)w / (float)(BUFFER_SIZE - 1);
        const float yCenter = h * 0.5f;
        const float yScale  = h * 0.42f;

        Path wPath;
        bool started = false;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            float px = i * xScale;
            float py = yCenter - displaySamples[i] * yScale;
            py = jlimit(2.0f, (float)(h - 2), py);
            if (!started) { wPath.startNewSubPath(px, py); started = true; }
            else           wPath.lineTo(px, py);
        }

        // Filled area under waveform with gradient
        {
            Path fillPath(wPath);
            fillPath.lineTo((float)w, yCenter);
            fillPath.lineTo(0.0f, yCenter);
            fillPath.closeSubPath();

            ColourGradient fillGrad(Colour(0xFF00B8D4).withAlpha(0.12f), 0.0f, 0.0f,
                                    Colour(0xFF00B8D4).withAlpha(0.0f), 0.0f, (float)h, false);
            g.setGradientFill(fillGrad);
            g.fillPath(fillPath);
        }

        // Outer glow (thicker, more transparent)
        g.setColour(Colour(0xFF00E5A0).withAlpha(0.1f));
        g.strokePath(wPath, PathStrokeType(4.0f));

        // Mid glow
        g.setColour(Colour(0xFF00B8D4).withAlpha(0.2f));
        g.strokePath(wPath, PathStrokeType(2.0f));

        // Inner bright line
        g.setColour(Colour(0xFF00FFD0));
        g.strokePath(wPath, PathStrokeType(1.0f));
    }

private:
    float displaySamples[BUFFER_SIZE];
};

#endif  // DXCOMPONENTS_H_INCLUDED
