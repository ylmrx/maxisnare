#pragma once
/*
 *  File: synth.h
 *
 *  Simple Synth using Maximilian
 *
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <algorithm>

#include <arm_neon.h>

#include "unit.h"  // Note: Include common definitions for all units

#include "maximilian.h"
#include "libs/maxiPolyBLEP.h"

// Uncomment this line to use maxiEnvGen class
// #define USE_MAXIENVGEN

// Use this instead of mtof() in maxmilian to allow note number in float
inline double note2freq(float note) {
    return (440.f / 32) * powf(2, (note - 9.0) / 12);
}

enum Params {
    Note = 0,
    AmpDecay,
    Wave,
    Balance,

    // Wave,
    Decay,
    EnvPitchInt,
    Shape,
    Gain,

    EqGain,
    EqCutoff,
    EqReso,
    EqShape
};

class Synth {
 public:
  /*===========================================================================*/
  /* Public Data Structures/Types. */
  /*===========================================================================*/

  /*===========================================================================*/
  /* Lifecycle Methods. */
  /*===========================================================================*/

  Synth(void) {}
  ~Synth(void) {}

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    // Check compatibility of samplerate with unit, for drumlogue should be 48000
    if (desc->samplerate != 48000)
      return k_unit_err_samplerate;

    // Check compatibility of frame geometry
    if (desc->output_channels != 2)  // should be stereo output
      return k_unit_err_geometry;

    maxiSettings::sampleRate = 48000;
    // pitchEnvelope_.setupAR()    
    // Note: if need to allocate some memory can do it here and return k_unit_err_memory if getting allocation errors

    return k_unit_err_none;
  }

  inline void Teardown() {
    // Note: cleanup and release resources if any
  }

  inline void Reset() {
    // Note: Reset synth state. I.e.: Clear filter memory, reset oscillator
    // phase etc.
    // osc.setWaveform(maxiPolyBLEP::Waveform::SINE);
    // osc2_.setWaveform(maxiPolyBLEP::Waveform::SAWTOOTH);
    gate_ = 0;
  }

  inline void Resume() {
    // Note: Synth will resume and exit suspend state. Usually means the synth
    // was selected and the render callback will be called again
  }

  inline void Suspend() {
    // Note: Synth will enter suspend state. Usually means another synth was
    // selected and thus the render callback will not be called
  }

  /*===========================================================================*/
  /* Other Public Methods. */
  /*===========================================================================*/

  fast_inline void Render(float * out, size_t frames) {
    float * __restrict out_p = out;
    const float * out_e = out_p + (frames << 1);  // assuming stereo output

    const int trigger = (gate_ > 0);

    for (; out_p != out_e; out_p += 2) {
      // Envelope
      float env = pitchEnvelope_.adsr(1.0, trigger);
      float ampEnv = ampEnvelope_.adsr(1.0, trigger);
      float sig;
      // Oscillator
      float pitch1 = note2freq(note_ + egPitch_ * env);
      if (wave_ == 3) {
        sig = (1.f - balance_) * osc.saw(pitch1) + balance_ * noise.noise();
      } else if (wave_ == 2) {
        sig = (1.f - balance_) * osc.square(pitch1) + balance_ * noise.noise();
      } else if (wave_ == 1) {
        sig = (1.f - balance_) * osc.triangle(pitch1) + balance_ * noise.noise();
      } else {
        sig = (1.f - balance_) * osc.sinewave(pitch1) + balance_ * noise.noise();
      }
      sig = maxiConvert::dbsToAmp(gain_) * sig;

      if (shape_ == 2) {
        sig = sat.fastatan(sig);
      } else if (shape_ == 1) {
        sig = sat.softclip(sig);
      } else {
        sig = sat.hardclip(sig);
      }
      // Filter
      if (eqShape_ == 2) {
        eq.set(maxiBiquad::HIGHSHELF, note2freq(eqCutoff_), eqResonance_, eqGain_);
      } else if (eqShape_ == 1) {
        eq.set(maxiBiquad::LOWSHELF, note2freq(eqCutoff_), eqResonance_, eqGain_);
      } else {
        eq.set(maxiBiquad::PEAK, note2freq(eqCutoff_), eqResonance_, eqGain_);
      }
      sig = eq.play(sig * ampEnv);
      // Note: should take advantage of NEON ArmV7 instructions
      vst1_f32(out_p, vdup_n_f32(sig));
    }
  }

  inline void setParameter(uint8_t index, int32_t value) {
    p_[index] = value;
    switch (index) {
      case Note:
        note_ = value;
        break;
      case Decay:
        pitchEnvelope_.setAttackMS(1.0f);
        pitchEnvelope_.setSustain(0.0f);
        pitchEnvelope_.setRelease(1.0f);
        pitchEnvelope_.setDecay(value + 1);
        break;
      case AmpDecay:
        ampEnvelope_.setAttackMS(1.0f);
        ampEnvelope_.setSustain(0.0f);
        ampEnvelope_.setRelease(1.0f);
        ampEnvelope_.setDecay(value + 1);
        break;
      case EnvPitchInt:
        egPitch_ = 0.48f * value; // 24 semitones / 100%
        break;
      case Balance:
        balance_ = 0.01f * value;
        break;
      case Gain:
        gain_ = value;
        break;
      case Shape:
        shape_ = value;
        break;
      case Wave:
        wave_ = value;
        break;
      case EqShape:
        eqShape_ = value;
        break;
      case EqCutoff:
        eqCutoff_ = 1.27f * value; // -63.5 .. +63.5
        break;
      case EqReso:
        eqResonance_ = powf(2, 1.f / (1<<5) * value); // 2^(-4) .. 2^4
        break;
      case EqGain:
        eqGain_ = value;
        break;
      default:
        break;
    }
  }

  inline int32_t getParameterValue(uint8_t index) const {
    return p_[index];
  }

  inline const char * getParameterStrValue(uint8_t index, int32_t value) const {
    (void)value;
    switch (index) {
      case Shape:
        if (value < 3) {
          return ShapeStr[value];
        } else {
          return nullptr;
        }
        break;
      case Wave:
        if (value < 4) {
          return WaveStr[value];
        } else {
          return nullptr;
        }
        break;
      case EqShape:
        if (value < 3) {
          return EqShapeStr[value];
        } else {
          return nullptr;
        }
        break;
      // Note: String memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the string
      //       before the next call to getParameterStrValue
      default:
        break;
    }
    return nullptr;
  }

  inline const uint8_t * getParameterBmpValue(uint8_t index,
                                              int32_t value) const {
    (void)value;
    switch (index) {
      // Note: Bitmap memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the bitmap
      //       before the next call to getParameterBmpValue
      // Note: Not yet implemented upstream
      default:
        break;
    }
    return nullptr;
  }

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    note_ = note;
    GateOn(velocity);
  }

  inline void NoteOff(uint8_t note) {
    (void)note;
    GateOff();
  }

  inline void GateOn(uint8_t velocity) {
    amp_ = 1. / 127 * velocity;
    gate_ += 1;
    osc.phaseReset(0.0f);
  }

  inline void GateOff() {
    if (gate_ > 0 ) {
      gate_ -= 1;
    }
  }

  inline void AllNoteOff() {}

  inline void PitchBend(uint16_t bend) { (void)bend; }

  inline void ChannelPressure(uint8_t pressure) { (void)pressure; }

  inline void Aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    (void)aftertouch;
  }

  inline void LoadPreset(uint8_t idx) { (void)idx; }

  inline uint8_t getPresetIndex() const { return 0; }

  /*===========================================================================*/
  /* Static Members. */
  /*===========================================================================*/

  static inline const char * getPresetName(uint8_t idx) {
    (void)idx;
    // Note: String memory must be accessible even after function returned.
    //       It can be assumed that caller will have copied or used the string
    //       before the next call to getPresetName
    return nullptr;
  }

 private:
  /*===========================================================================*/
  /* Private Member Variables. */
  /*===========================================================================*/

  std::atomic_uint_fast32_t flags_;

  int32_t p_[24];
  maxiEnv pitchEnvelope_;
  maxiEnv ampEnvelope_;
  // maxiEnvGen pitchEnvelope_;
  // maxiEnvGen ampEnvelope_;
  maxiOsc osc;
  
  // saturation
  maxiBiquad eq;
  maxiNonlinearity sat;
  float gain_;
  int wave_;
  int shape_;
  int eqShape_;
  float eqGain_;

  double eqCutoff_;
  float eqResonance_;

  // maxiPolyBLEP osc;
  maxiOsc noise;

  int32_t note_;
  float amp_;
  uint32_t gate_;

  float egPitch_;

  float balance_; // 0 = osc1 only, 1= osc2 only
  /*===========================================================================*/
  /* Private Methods. */
  /*===========================================================================*/

  /*===========================================================================*/
  /* Constants. */
  /*===========================================================================*/
  const char *WaveStr[4] = {
    "Sine",
    "Tri",
    "Square",
    "Saw"
  };
  const char *EqShapeStr[3] = {
    "Peak",
    "LowSh",
    "HighSh"
  };
  const char *ShapeStr[3] = {
    "Soft",
    "Hard",
    "Atan",
  };

};
