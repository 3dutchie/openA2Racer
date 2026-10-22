#include "movie.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void write(const std::filesystem::path &p, const Bytes &bytes) {
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
}
void synthetic() {
  auto path = std::filesystem::temp_directory_path() /
              ("a2-movie-test-" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(path);
  try {
    for (const auto *file : {"RCS.IND", "RCS.IMG", "ETAPPE1.TRK"})
      write(path / file, Bytes(4));
    for (const auto &bytes : std::vector<Bytes>{Bytes{}, Bytes(12), Bytes(104, 0)}) {
      write(path / "BAD.SMK", bytes);
      bool rejected = false;
      GameData refreshed(path);
      try {
        Movie bad(refreshed, "BAD.SMK");
      } catch (const std::exception &) {
        rejected = true;
      }
      check(rejected, "Invalid movie accepted");
    }
    std::filesystem::remove_all(path);
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    throw;
  }
  std::cout << "Synthetic movie rejection checks passed\n";
}
void original(const std::filesystem::path &path) {
  GameData data(path);
  const char *files[] = {"A2INTRO.SMK", "A2WIN.SMK", "A2EXIT.SMK"};
  size_t expected[] = {601, 263, 80};
  for (size_t n = 0; n < 3; ++n) {
    Movie movie(data, files[n]);
    check(movie.frameCount() == expected[n], "Original movie frame count");
    check(movie.image().width == 320 && movie.image().height == 200, "Original movie dimensions");
    check(movie.fps() > 14 && movie.fps() < 16 && movie.duration() > 5, "Original movie timing");
    check(movie.audio().sampleRate == 22050 && !movie.audio().samples.empty(),
          "Original movie audio");
    check(movie.audioFrameOffsets().size() == expected[n], "Audio frame offset coverage");
    const auto first = movie.image().pixels;
    uint64_t checksum = 0;
    size_t visited = 1;
    for (size_t f = 1; f < movie.frameCount(); ++f) {
      check(movie.advance(float(1.0 / movie.fps() + 0.000001)), "Movie ended before final frame");
      check(movie.frameIndex() == f, "Skipped or repeated video frame");
      for (auto p : movie.image().pixels)
        checksum = checksum * 1099511628211ull + p;
      ++visited;
    }
    check(visited == expected[n] && movie.frameIndex() == expected[n] - 1,
          "Incomplete original movie decode");
    check(!movie.advance(1), "Movie did not stop at end");
    movie.rewind();
    check(movie.frameIndex() == 0 && movie.image().pixels == first, "Movie rewind mismatch");
    for (auto sample : movie.audio().samples)
      check(std::isfinite(sample) && sample >= -1 && sample <= 1, "Nonfinite movie PCM");
    std::cout << files[n] << ": decoded " << visited << " frames, " << movie.audio().samples.size()
              << " PCM samples, checksum " << checksum << '\n';
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    synthetic();
    if (argc > 1)
      original(argv[1]);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Movie test: " << e.what() << '\n';
    return 1;
  }
}
