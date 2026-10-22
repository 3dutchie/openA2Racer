#include "audio.hpp"
#include <iostream>

namespace a2 {
Audio::Audio(Assets &a) : assets_(a) {
  // Resource decoding may throw; finish it before acquiring an SDL device.
  assets_.sound("motor.wav");
  assets_.sound("sirene.wav");
  SDL_AudioSpec want{}, got{};
  want.freq = 44100;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = callback;
  want.userdata = this;
  device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
  if (!device_) {
    std::cerr << "Audio unavailable: " << SDL_GetError() << "\n";
    return;
  }
  // Decode before exposing these samples to the callback thread.
  SDL_PauseAudioDevice(device_, 0);
}
Audio::~Audio() {
  if (device_)
    SDL_CloseAudioDevice(device_);
}
void Audio::setVoice(size_t index, const std::string &name, float volume, bool loop) {
  if (!device_ || !assets_.has(name))
    return;
  const Sound *sound = &assets_.sound(name);
  SDL_LockAudioDevice(device_);
  voices_[index] = {sound, 0, double(sound->sampleRate) / 44100.0, volume, loop};
  SDL_UnlockAudioDevice(device_);
}
void Audio::play(const std::string &name, float volume) {
  if (name.empty())
    return;
  setVoice(effect_, name, volume, false);
  effect_ = 3 + (effect_ - 3 + 1) % (voices_.size() - 3);
}
void Audio::music(int stage, bool enabled) {
  if (!device_)
    return;
  if (!enabled) {
    SDL_LockAudioDevice(device_);
    voices_[0] = {};
    musicStage_ = -1;
    SDL_UnlockAudioDevice(device_);
    return;
  }
  stage = (stage % 6 + 6) % 6;
  if (stage == musicStage_)
    return;
  static const char *songs[] = {"speed.wav", "energy.wav", "jammin.wav",
                                "moody.wav", "disco.wav",  "rock.wav"};
  setVoice(0, songs[stage], 0.22f, true);
  musicStage_ = stage;
}
void Audio::engine(float speed, bool active, bool siren) {
  if (!device_)
    return;
  auto &motor = assets_.sound("motor.wav");
  auto &police = assets_.sound("sirene.wav");
  SDL_LockAudioDevice(device_);
  if (active) {
    if (!voices_[1].sound)
      voices_[1] = {&motor, 0, 1, 0.3f, true};
    voices_[1].step = double(motor.sampleRate) / 44100.0 * (0.6 + speed / 110.0);
    voices_[1].volume = 0.18f + std::min(speed, 300.0f) * 0.0003f;
  } else
    voices_[1] = {};
  if (siren) {
    if (!voices_[2].sound)
      voices_[2] = {&police, 0, double(police.sampleRate) / 44100.0, 0.2f, true};
  } else
    voices_[2] = {};
  SDL_UnlockAudioDevice(device_);
}
void Audio::muted(bool value) {
  if (!device_)
    return;
  SDL_LockAudioDevice(device_);
  muted_ = value;
  SDL_UnlockAudioDevice(device_);
}
void Audio::paused(bool value) {
  if (!device_)
    return;
  SDL_LockAudioDevice(device_);
  paused_ = value;
  SDL_UnlockAudioDevice(device_);
}
void Audio::movie(const Sound *sound) {
  if (!device_)
    return;
  SDL_LockAudioDevice(device_);
  voices_.fill(Voice{});
  musicStage_ = -1;
  paused_ = false;
  movie_ = sound != nullptr;
  if (sound)
    voices_[0] = {sound, 0, double(sound->sampleRate) / 44100.0, 0.8f, false};
  SDL_UnlockAudioDevice(device_);
}
void Audio::levels(float music, float effects) {
  if (!device_)
    return;
  SDL_LockAudioDevice(device_);
  musicLevel_ = std::clamp(music, 0.f, 1.f);
  effectsLevel_ = std::clamp(effects, 0.f, 1.f);
  SDL_UnlockAudioDevice(device_);
}
void Audio::callback(void *context, Uint8 *buffer, int bytes) {
  static_cast<Audio *>(context)->mix(reinterpret_cast<float *>(buffer),
                                     size_t(bytes) / sizeof(float) / 2);
}
void Audio::mix(float *output, size_t frames) {
  std::fill(output, output + frames * 2, 0);
  if (paused_)
    return;
  for (size_t voice = 0; voice < voices_.size(); ++voice) {
    auto &v = voices_[voice];
    if (!v.sound || v.sound->samples.empty())
      continue;
    const auto &samples = v.sound->samples;
    for (size_t i = 0; i < frames; ++i) {
      if (v.cursor >= double(samples.size())) {
        if (!v.loop) {
          v.sound = nullptr;
          break;
        }
        v.cursor = std::fmod(v.cursor, double(samples.size()));
      }
      size_t j = size_t(v.cursor), k = j + 1 < samples.size() ? j + 1 : (v.loop ? 0 : j);
      float level = voice == 0 && !movie_ ? musicLevel_ : effectsLevel_;
      float sample = lerp(samples[j], samples[k], float(v.cursor - double(j))) * v.volume * level;
      output[2 * i] += sample;
      output[2 * i + 1] += sample;
      v.cursor += v.step;
    }
  }
  for (size_t i = 0; i < frames * 2; ++i)
    output[i] = muted_ ? 0 : std::clamp(output[i], -0.95f, 0.95f);
}
} // namespace a2
