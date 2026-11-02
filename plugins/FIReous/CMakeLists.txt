dpf_add_plugin(fireous
  TARGETS lv2 vst3 clap jack
  FILES_DSP
      FireousPlugin.cpp)

target_include_directories(fireous PUBLIC ".")
