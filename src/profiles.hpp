#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace a2 {
struct Profile {
  uint64_t id = 0;
  std::string name;
  // Next campaign stage, zero based. Six means the campaign was completed.
  int stage = 0, money = 1000;
  double totalTime = 0;
  std::array<double, 6> bestTimes{};
  uint64_t attemptSerial = 0, lastRecordedAttempt = 0;
  int attemptStage = -1;
};
struct ProfileAttempt {
  uint64_t profileId = 0, serial = 0;
  int stage = -1;
};
struct ScoreEntry {
  uint64_t profileId = 0;
  std::string name;
  int reachedStage = 0, money = 0;
  double bestTime = 0;
  bool completed = false;
};

// Portable port-owned storage. Never reads or overwrites SLOT.A2R/SCORE.A2R.
class ProfileStore {
public:
  static constexpr size_t capacity = 10;
  static constexpr size_t maxNameCharacters = 31, maxNameBytes = 124;
  explicit ProfileStore(std::filesystem::path directory);
  // Missing files return false; malformed files throw and leave state unchanged.
  bool load();
  void save() const;
  const std::array<std::optional<Profile>, capacity> &slots() const { return slots_; }
  int selected() const { return selected_; }
  Profile *active();
  const Profile *active() const;
  void select(int index);
  void create(int index, std::string name);
  void rename(int index, std::string name);
  void remove(int index);
  // Keep best stage times and earned score entries when restarting a campaign.
  void restart(int index);
  ProfileAttempt beginAttempt(int stage = -1);
  // Finished results advance sequential campaigns and update best scores.
  // Failed results update money only. Duplicate/stale tokens return false.
  bool recordStage(ProfileAttempt, int stage, bool finished, int money, double elapsed);
  // -1 = overall; 0..5 = a stage. Entries are earned results, never seeded names.
  std::vector<ScoreEntry> scoreboard(int stage = -1) const;
  std::filesystem::path savePath(int saveSlot) const;
  std::filesystem::path filePath() const;
  static std::string normalizeName(std::string_view name);

private:
  std::filesystem::path directory_;
  std::array<std::optional<Profile>, capacity> slots_{};
  std::array<std::vector<ScoreEntry>, 7> scores_{};
  int selected_ = -1;
  uint64_t nextId_ = 1;
  Profile &require(int index);
  void updateScore(int board, const ScoreEntry &);
  std::string document() const;
  void parse(std::string_view);
};
} // namespace a2
