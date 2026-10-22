#include "movie.hpp"
#include "smacker.h"
#include <limits>
#include <stdexcept>

namespace a2 {
namespace {
int status(char value) {
  return int(static_cast<signed char>(value));
}
struct Decoder {
  smk value = nullptr;
  explicit Decoder(const Bytes &bytes)
      : value(smk_open_memory(bytes.data(), static_cast<unsigned long>(bytes.size()))) {
    if (!value)
      throw std::runtime_error("Could not open original Smacker movie");
  }
  ~Decoder() {
    if (value)
      smk_close(value);
  }
  Decoder(const Decoder &) = delete;
  Decoder &operator=(const Decoder &) = delete;
};
uint32_t word(const Bytes &b, size_t p) {
  if (p > b.size() || b.size() - p < 4)
    throw std::runtime_error("Truncated Smacker header");
  return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) |
         (uint32_t(b[p + 3]) << 24);
}
void validateHeader(const Bytes &b) {
  if (b.size() < 104 || b.size() > 64 * 1024 * 1024 || b[0] != 'S' || b[1] != 'M' || b[2] != 'K' ||
      (b[3] != '2' && b[3] != '4'))
    throw std::runtime_error("Invalid or unsupported Smacker movie");
  const auto w = word(b, 4), h = word(b, 8), frames = word(b, 12);
  if (w == 0 || h == 0 || w > 2048 || h > 2048 || w % 4 || h % 4 || frames == 0 || frames > 100000)
    throw std::runtime_error("Smacker dimensions or frame count exceed supported limits");
  // These bounds also keep allocations and the total PCM budget predictable.
  const auto tree = word(b, 52);
  const uint64_t tableFrames = uint64_t(frames) + (word(b, 20) & 1u);
  if (tree == 0 || tree > b.size() || tableFrames * 5 + 104 + tree > b.size())
    throw std::runtime_error("Truncated Smacker frame table");
  for (size_t i = 0; i < 7; ++i)
    if (word(b, 24 + i * 4) > 8 * 1024 * 1024)
      throw std::runtime_error("Smacker audio buffer exceeds supported limits");
  for (size_t i = 0; i < 4; ++i)
    if (word(b, 56 + i * 4) > 8 * 1024 * 1024)
      throw std::runtime_error("Smacker Huffman tree exceeds supported limits");
  size_t cursor = size_t(104 + tableFrames * 5 + tree);
  for (size_t frame = 0; frame < tableFrames; ++frame) {
    const size_t chunk = word(b, 104 + frame * 4) & 0xfffffffcu;
    if (!chunk || cursor > b.size() || chunk > b.size() - cursor)
      throw std::runtime_error("Smacker frame exceeds input");
    const size_t end = cursor + chunk;
    size_t p = cursor;
    const uint8_t flags = b[size_t(104 + tableFrames * 4) + frame];
    if (flags & 1) {
      size_t palette = size_t(b[p]) * 4;
      if (!palette || palette > end - p)
        throw std::runtime_error("Invalid Smacker palette chunk");
      p += palette;
    }
    for (unsigned track = 0; track < 7; ++track)
      if (flags & (2u << track)) {
        if (end - p < 4)
          throw std::runtime_error("Truncated Smacker audio chunk");
        const size_t audio = word(b, p);
        if (audio < 4 || audio > end - p)
          throw std::runtime_error("Invalid Smacker audio chunk");
        p += audio;
      }
    cursor = end;
  }
}
} // namespace
struct Movie::Impl {
  Bytes bytes;
  std::unique_ptr<Decoder> video;
  Image image;
  Sound audio;
  std::vector<double> audioOffsets;
  unsigned long count = 0, width = 0, height = 0;
  unsigned char scale = 0;
  double frameSeconds = 0, elapsed = 0;
  size_t index = 0;

  explicit Impl(Bytes input) : bytes(std::move(input)) {
    validateHeader(bytes);
    video = std::make_unique<Decoder>(bytes);
    double us = 0;
    if (status(smk_info_all(video->value, nullptr, &count, &us)) < 0 ||
        status(smk_info_video(video->value, &width, &height, &scale)) < 0 || count == 0 ||
        count > 100000 || !std::isfinite(us) || us < 100 || us > 10000000)
      throw std::runtime_error("Invalid Smacker video metadata");
    frameSeconds = us / 1000000.0;
    if (frameSeconds * double(count) > 3600)
      throw std::runtime_error("Movie exceeds one-hour playback budget");
    decodeAudio();
    smk_enable_all(video->value, SMK_VIDEO_TRACK);
    if (status(smk_first(video->value)) < 0)
      throw std::runtime_error("Could not decode the first Smacker frame");
    capture();
  }
  void decodeAudio() {
    Decoder decoder(bytes);
    unsigned char mask = 0, channels[7]{}, depth[7]{};
    unsigned long rate[7]{};
    if (status(smk_info_audio(decoder.value, &mask, channels, depth, rate)) < 0)
      throw std::runtime_error("Invalid Smacker audio metadata");
    int track = 0;
    while (track < 7 && !(mask & (1u << track)))
      ++track;
    if (track == 7) {
      audioOffsets.assign(count, 0);
      return;
    }
    if ((channels[track] != 1 && channels[track] != 2) ||
        (depth[track] != 8 && depth[track] != 16) || rate[track] < 1000 || rate[track] > 192000)
      throw std::runtime_error("Unsupported Smacker audio format");
    audio.sampleRate = int(rate[track]);
    if (status(smk_enable_all(decoder.value, static_cast<unsigned char>(1u << track))) < 0)
      throw std::runtime_error("Could not enable Smacker audio");
    const size_t sampleBytes = depth[track] / 8, frameBytes = sampleBytes * channels[track];
    const size_t maxSamples = size_t(rate[track]) * 3600;
    for (unsigned long frame = 0; frame < count; ++frame) {
      int result = status(frame == 0 ? smk_first(decoder.value) : smk_next(decoder.value));
      if (result < 0 || result == SMK_DONE)
        throw std::runtime_error("Could not decode Smacker audio frame");
      audioOffsets.push_back(double(audio.samples.size()) / audio.sampleRate);
      auto size = smk_get_audio_size(decoder.value, static_cast<unsigned char>(track));
      const auto *pcm = smk_get_audio(decoder.value, static_cast<unsigned char>(track));
      if (size % frameBytes || size / frameBytes > maxSamples - audio.samples.size() ||
          (!pcm && size))
        throw std::runtime_error("Smacker audio exceeds PCM bounds");
      for (size_t offset = 0; offset < size; offset += frameBytes) {
        float value = 0;
        for (size_t channel = 0; channel < channels[track]; ++channel) {
          const size_t p = offset + channel * sampleBytes;
          if (sampleBytes == 1)
            value += (int(pcm[p]) - 128) / 128.0f;
          else {
            const uint16_t bits = uint16_t(pcm[p]) | (uint16_t(pcm[p + 1]) << 8);
            value += (bits >= 32768 ? int(bits) - 65536 : int(bits)) / 32768.0f;
          }
        }
        audio.samples.push_back(value / channels[track]);
      }
    }
  }
  void capture() {
    const auto *palette = smk_get_palette(video->value);
    const auto *pixels = smk_get_video(video->value);
    if (!palette || !pixels)
      throw std::runtime_error("Smacker decoder returned an empty frame");
    image.width = int(width);
    image.height = int(height) * (scale ? 2 : 1);
    image.pixels.resize(size_t(image.width) * image.height);
    image.indices.resize(image.pixels.size());
    for (size_t i = 0; i < 256; ++i)
      image.palette[i] = 0xff000000u | (uint32_t(palette[i * 3]) << 16) |
                         (uint32_t(palette[i * 3 + 1]) << 8) | palette[i * 3 + 2];
    for (int y = 0; y < image.height; ++y)
      for (int x = 0; x < image.width; ++x) {
        size_t out = size_t(y) * image.width + x;
        auto color = pixels[size_t(scale ? y / 2 : y) * width + size_t(x)];
        image.indices[out] = color;
        image.pixels[out] = image.palette[color];
      }
  }
};
Movie::Movie(const GameData &data, const std::string &filename)
    : impl_(std::make_unique<Impl>(data.read(filename))) {}
Movie::~Movie() = default;
const Image &Movie::image() const {
  return impl_->image;
}
const Sound &Movie::audio() const {
  return impl_->audio;
}
const std::vector<double> &Movie::audioFrameOffsets() const {
  return impl_->audioOffsets;
}
double Movie::duration() const {
  return impl_->frameSeconds * impl_->count;
}
double Movie::fps() const {
  return 1.0 / impl_->frameSeconds;
}
size_t Movie::frameCount() const {
  return size_t(impl_->count);
}
size_t Movie::frameIndex() const {
  return impl_->index;
}
void Movie::rewind() {
  if (status(smk_first(impl_->video->value)) < 0)
    throw std::runtime_error("Could not rewind Smacker video");
  impl_->index = 0;
  impl_->elapsed = 0;
  impl_->capture();
}
bool Movie::advance(float seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    return impl_->elapsed < duration();
  impl_->elapsed = std::min(impl_->elapsed + double(seconds), duration());
  auto target = std::min(size_t(impl_->elapsed / impl_->frameSeconds), size_t(impl_->count - 1));
  while (impl_->index < target) {
    int result = status(smk_next(impl_->video->value));
    if (result < 0 || result == SMK_DONE)
      throw std::runtime_error("Could not decode Smacker video frame");
    ++impl_->index;
  }
  impl_->capture();
  return impl_->elapsed < duration();
}
} // namespace a2
