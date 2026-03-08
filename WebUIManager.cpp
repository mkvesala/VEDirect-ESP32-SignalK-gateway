#include "WebUIManager.h"

WebUIManager::WebUIManager(
    VEDProcessor&   processorRef,
    VEDPreferences& prefsRef,
    SignalKBroker&  signalkRef,
    DisplayManager& displayRef
)
    : _processor(processorRef)
    , _prefs(prefsRef)
    , _signalk(signalkRef)
    , _display(displayRef)
{}
