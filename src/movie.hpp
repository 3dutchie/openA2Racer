#pragma once
#include "assets.hpp"
#include <memory>

namespace a2 {
// Original Smacker video with the first active audio track decoded to mono PCM.
// The Image and Sound remain valid until this Movie is destroyed.
class Movie {
public:
  Movie(const GameData &data, const std::string &filename);
  ~Movie();
  Movie(const Movie &) = delete;
  Movie &operator=(const Movie &) = delete;
  const Image &image() const;
  const Sound &audio() const;
  // Seconds of decoded audio accumulated before each source video frame.
  const std::vector<double> &audioFrameOffsets() const;
  bool advance(float seconds);
  void rewind();
  double duration() const;
  double fps() const;
  size_t frameCount() const;
  size_t frameIndex() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace a2
