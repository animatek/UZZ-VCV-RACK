#include "plugin.hpp"

Plugin* pluginInstance = nullptr;


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here

		p->addModel(modelUZZ);    // UZZ step sequencer
		p->addModel(modelOxiCv);  // OXI-CV (6HP MIDI-to-CV for Oxi One)
		p->addModel(modelOxiCvExp); // OXI-CV EXPANSOR
		p->addModel(modelApc40Ctrl); // APC40 controller CV bridge


	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
