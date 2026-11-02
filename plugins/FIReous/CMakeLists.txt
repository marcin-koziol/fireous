dpf_add_plugin(fireous
  TARGETS lv2 vst3 clap jack
  UI_TYPE cairo
  FILES_DSP
      FireousPlugin.cpp
  FILES_UI
      FireousUI.cpp)

target_include_directories(fireous PUBLIC ".")
