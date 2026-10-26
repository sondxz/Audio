#pragma once
#include <juce_core/juce_core.h>

namespace lah::ui {
// Synchronous GUI/control-to-callback acceptance test. Uses only a synthetic
// eight-channel AudioIODevice; never initializes a physical device manager.
// destination is a directory for the CSV measurements and text report.
bool runNativePluginAudioAudit(const juce::File& destination,juce::String& error);
}
