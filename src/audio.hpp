#pragma once
#include "assets.hpp"
#include <SDL.h>
#include <array>

namespace a2 {
class Audio {
public:
  explicit Audio(Assets &assets);
  ~Audio();
  Audio(const Audio &) = delete;
  Audio &operator=(const Audio &) = delete;
  void play(const std::string &name, float volume = 0.5f);
  void music(int stage, bool enabled);
  void engine(float speed, bool active, bool siren);
  void muted(bool value);
  void paused(bool value);
  void levels(float music, float effects);
  // Borrowed sample storage remains valid until movie(nullptr) is called.
  void movie(const Sound *sound);
  bool available() const { return device_ != 0; }

private:
  struct Voice {
    const Sound *sound = nullptr;
    double cursor = 0, step = 1;
    float volume = 0;
    bool loop = false;
  };
  Assets &assets_;
  SDL_AudioDeviceID device_ = 0;
  std::array<Voice, 24> voices_{};
  size_t effect_ = 3;
  bool muted_ = false, paused_ = false;
  bool movie_ = false;
  float musicLevel_ = 1, effectsLevel_ = 1;
  int musicStage_ = -1;
  static void callback(void *, Uint8 *, int);
  void mix(float *, size_t);
  void setVoice(size_t, const std::string &, float, bool);
};
} // namespace a2
