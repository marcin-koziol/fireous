#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "FIReous"
#define DISTRHO_PLUGIN_NAME    "FIReous"
#define DISTRHO_PLUGIN_URI     "https://github.com/marcin-koziol/fireous"
#define DISTRHO_PLUGIN_CLAP_ID "fireous.fireous"

#define DISTRHO_PLUGIN_BRAND_ID  FirO
#define DISTRHO_PLUGIN_UNIQUE_ID FirO

// UI is added in a later stage; the DSP engine is built and tested first.
#define DISTRHO_PLUGIN_HAS_UI           0
#define DISTRHO_PLUGIN_IS_RT_SAFE       1
#define DISTRHO_PLUGIN_IS_SYNTH         0
#define DISTRHO_PLUGIN_NUM_INPUTS       4
#define DISTRHO_PLUGIN_NUM_OUTPUTS      2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  0
#define DISTRHO_PLUGIN_WANT_STATE       0
#define DISTRHO_PLUGIN_WANT_PROGRAMS    0
#define DISTRHO_PLUGIN_WANT_TIMEPOS     1
#define DISTRHO_PLUGIN_WANT_LATENCY     1

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
