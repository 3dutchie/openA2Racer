#include "profiles.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace a2 {
namespace {
constexpr uint64_t maxIdentity = 1000000000000000ull;
constexpr int maxMoney = 1000000000;
constexpr double maxStageTime = 86400;
void valid(bool condition) {
  if (!condition)
    throw std::runtime_error("Invalid or incompatible OpenA2 player profiles");
}
bool timeInRange(double t, double max = maxStageTime) {
  return std::isfinite(t) && t >= 0 && t <= max;
}
bool better(const ScoreEntry &a, const ScoreEntry &b, bool overall) {
  if (overall && a.reachedStage != b.reachedStage)
    return a.reachedStage > b.reachedStage;
  if (a.bestTime != b.bestTime)
    return a.bestTime < b.bestTime;
  if (a.name != b.name)
    return a.name < b.name;
  return a.profileId < b.profileId;
}
bool improves(const ScoreEntry &a, const ScoreEntry &b, bool overall) {
  return (overall && a.reachedStage > b.reachedStage) ||
         ((!overall || a.reachedStage == b.reachedStage) && a.bestTime < b.bestTime);
}
void replaceFile(const std::filesystem::path &temporary, const std::filesystem::path &target) {
#ifdef _WIN32
  if (!MoveFileExW(temporary.c_str(), target.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::system_error(int(GetLastError()), std::system_category(),
                            "Could not save player profiles");
#else
  std::filesystem::rename(temporary, target);
#endif
}
} // namespace

ProfileStore::ProfileStore(std::filesystem::path directory) : directory_(std::move(directory)) {}
std::filesystem::path ProfileStore::filePath() const {
  return directory_ / "profiles.db";
}
std::string ProfileStore::normalizeName(std::string_view input) {
  while (!input.empty() && input.front() == ' ')
    input.remove_prefix(1);
  while (!input.empty() && input.back() == ' ')
    input.remove_suffix(1);
  if (input.empty() || input.size() > maxNameBytes)
    throw std::invalid_argument("Player name must contain 1 to 31 characters");
  size_t characters = 0;
  for (size_t i = 0; i < input.size();) {
    unsigned char first = static_cast<unsigned char>(input[i++]);
    uint32_t code = first;
    size_t count = 0;
    uint32_t minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) {
      count = 1;
      code &= 31;
      minimum = 0x80;
    } else if (first >= 0xe0 && first <= 0xef) {
      count = 2;
      code &= 15;
      minimum = 0x800;
    } else if (first >= 0xf0 && first <= 0xf4) {
      count = 3;
      code &= 7;
      minimum = 0x10000;
    } else if (first >= 0x80)
      throw std::invalid_argument("Player name must be valid UTF-8");
    if (i + count > input.size())
      throw std::invalid_argument("Player name must be valid UTF-8");
    for (size_t n = 0; n < count; ++n) {
      unsigned char byte = static_cast<unsigned char>(input[i++]);
      if ((byte & 0xc0) != 0x80)
        throw std::invalid_argument("Player name must be valid UTF-8");
      code = (code << 6) | (byte & 63);
    }
    if ((count && code < minimum) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
        code < 32 || (code >= 127 && code <= 159) || ++characters > maxNameCharacters)
      throw std::invalid_argument("Player name must contain 1 to 31 printable characters");
  }
  return std::string(input);
}
Profile *ProfileStore::active() {
  return selected_ < 0 ? nullptr : &*slots_[size_t(selected_)];
}
const Profile *ProfileStore::active() const {
  return selected_ < 0 ? nullptr : &*slots_[size_t(selected_)];
}
Profile &ProfileStore::require(int index) {
  if (index < 0 || index >= int(capacity) || !slots_[size_t(index)])
    throw std::invalid_argument("No player in that profile slot");
  return *slots_[size_t(index)];
}
void ProfileStore::select(int index) {
  if (index != -1)
    require(index);
  selected_ = index;
}
void ProfileStore::create(int index, std::string name) {
  if (index < 0 || index >= int(capacity) || slots_[size_t(index)])
    throw std::invalid_argument("Player profile slot is unavailable");
  name = normalizeName(name);
  if (nextId_ >= maxIdentity)
    throw std::runtime_error("Player profile identity limit reached");
  Profile p;
  p.id = nextId_++;
  p.name = std::move(name);
  slots_[size_t(index)] = std::move(p);
  selected_ = index;
}
void ProfileStore::rename(int index, std::string name) {
  auto normalized = normalizeName(name);
  require(index).name = std::move(normalized);
}
void ProfileStore::remove(int index) {
  require(index);
  slots_[size_t(index)].reset();
  if (selected_ == index)
    selected_ = -1;
}
void ProfileStore::restart(int index) {
  auto &p = require(index);
  p.stage = 0;
  p.money = 1000;
  p.totalTime = 0;
  p.lastRecordedAttempt = p.attemptSerial;
  p.attemptStage = -1;
}
ProfileAttempt ProfileStore::beginAttempt(int stage) {
  if (!active())
    return {};
  auto &p = *active();
  if (stage == -1)
    stage = std::min(p.stage, 5);
  if (stage < 0 || stage >= 6)
    throw std::invalid_argument("Invalid profile race stage");
  if (p.attemptSerial >= maxIdentity)
    throw std::runtime_error("Profile race counter limit reached");
  ++p.attemptSerial;
  p.attemptStage = stage;
  return {p.id, p.attemptSerial, stage};
}
void ProfileStore::updateScore(int board, const ScoreEntry &score) {
  auto &entries = scores_[size_t(board)];
  auto old = std::find_if(entries.begin(), entries.end(),
                          [&](const auto &e) { return e.profileId == score.profileId; });
  if (old != entries.end()) {
    if (!improves(score, *old, board == 0))
      return;
    *old = score;
  } else
    entries.push_back(score);
  std::stable_sort(entries.begin(), entries.end(),
                   [&](const auto &a, const auto &b) { return better(a, b, board == 0); });
  if (entries.size() > capacity)
    entries.resize(capacity);
}
bool ProfileStore::recordStage(ProfileAttempt attempt, int stage, bool finished, int money,
                               double elapsed) {
  if (stage < 0 || stage >= 6 || money < 0 || money > maxMoney || !timeInRange(elapsed) ||
      (finished && elapsed == 0))
    throw std::invalid_argument("Invalid profile race result");
  auto slot = std::find_if(slots_.begin(), slots_.end(),
                           [&](const auto &p) { return p && p->id == attempt.profileId; });
  if (slot == slots_.end())
    return false;
  auto &p = **slot;
  if (attempt.serial == 0 || attempt.serial != p.attemptSerial ||
      attempt.serial <= p.lastRecordedAttempt || attempt.stage != stage || p.attemptStage != stage)
    return false;
  p.lastRecordedAttempt = attempt.serial;
  p.money = money;
  if (!finished)
    return true;
  auto &best = p.bestTimes[size_t(stage)];
  if (best == 0 || elapsed < best)
    best = elapsed;
  if (stage == p.stage) {
    p.totalTime += elapsed;
    ++p.stage;
  }
  ScoreEntry score{p.id, p.name, p.stage, money, elapsed, p.stage == 6};
  updateScore(stage + 1, score);
  if (p.stage > 0) {
    score.bestTime = p.totalTime;
    updateScore(0, score);
  }
  return true;
}
std::vector<ScoreEntry> ProfileStore::scoreboard(int stage) const {
  if (stage < -1 || stage >= 6)
    throw std::invalid_argument("Invalid score table stage");
  return scores_[size_t(stage + 1)];
}
std::filesystem::path ProfileStore::savePath(int slot) const {
  if (!active())
    throw std::logic_error("Select a player before saving a profile race");
  if (slot < 0 || slot >= 10)
    throw std::invalid_argument("Invalid race save slot");
  return directory_ /
         ("profile-" + std::to_string(active()->id) + "-slot" + std::to_string(slot) + ".sav");
}
std::string ProfileStore::document() const {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  out << "OPENA2_PROFILES 1\n" << selected_ << ' ' << nextId_ << '\n' << capacity << '\n';
  for (const auto &slot : slots_) {
    out << bool(slot);
    if (slot) {
      const auto &p = *slot;
      out << ' ' << p.id << ' ' << std::quoted(p.name) << ' ' << p.stage << ' ' << p.money << ' '
          << p.totalTime << ' ' << p.attemptSerial << ' ' << p.lastRecordedAttempt << ' '
          << p.attemptStage;
      for (auto t : p.bestTimes)
        out << ' ' << t;
    }
    out << '\n';
  }
  for (const auto &board : scores_) {
    out << board.size() << '\n';
    for (const auto &s : board)
      out << s.profileId << ' ' << std::quoted(s.name) << ' ' << s.reachedStage << ' ' << s.money
          << ' ' << s.bestTime << ' ' << s.completed << '\n';
  }
  out << "END\n";
  if (!out)
    throw std::runtime_error("Could not encode player profiles");
  return out.str();
}
void ProfileStore::parse(std::string_view text) {
  valid(text.size() <= 65536);
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  std::string magic;
  int version = 0, selected = -1, count = 0;
  uint64_t next = 0;
  valid(bool(in >> magic >> version >> selected >> next >> count));
  valid(magic == "OPENA2_PROFILES" && version == 1 && selected >= -1 && selected < int(capacity) &&
        next > 0 && next <= maxIdentity && count == int(capacity));
  decltype(slots_) slots;
  decltype(scores_) scores;
  std::set<uint64_t> identities;
  for (auto &slot : slots) {
    int used = 0;
    valid(bool(in >> used));
    valid(used == 0 || used == 1);
    if (!used)
      continue;
    Profile p;
    valid(bool(in >> p.id >> std::quoted(p.name) >> p.stage >> p.money >> p.totalTime >>
               p.attemptSerial >> p.lastRecordedAttempt >> p.attemptStage));
    valid(p.id > 0 && p.id < next && identities.insert(p.id).second && p.stage >= 0 &&
          p.stage <= 6 && p.money >= 0 && p.money <= maxMoney &&
          timeInRange(p.totalTime, maxStageTime * 6) &&
          (p.stage == 0 ? p.totalTime == 0 : p.totalTime > 0) && p.attemptSerial <= maxIdentity &&
          p.lastRecordedAttempt <= p.attemptSerial && p.attemptStage >= -1 && p.attemptStage < 6 &&
          (p.attemptSerial != 0 || p.attemptStage == -1));
    valid(normalizeName(p.name) == p.name);
    for (size_t i = 0; i < p.bestTimes.size(); ++i) {
      valid(bool(in >> p.bestTimes[i]));
      valid(timeInRange(p.bestTimes[i]));
      valid(int(i) >= p.stage || p.bestTimes[i] > 0);
    }
    slot = std::move(p);
  }
  valid(selected == -1 || bool(slots[size_t(selected)]));
  for (int board = 0; board < 7; ++board) {
    valid(bool(in >> count));
    valid(count >= 0 && count <= int(capacity));
    std::set<uint64_t> seen;
    for (int i = 0; i < count; ++i) {
      ScoreEntry s;
      int completed = 0;
      valid(bool(in >> s.profileId >> std::quoted(s.name) >> s.reachedStage >> s.money >>
                 s.bestTime >> completed));
      valid(s.profileId > 0 && s.profileId < next && seen.insert(s.profileId).second &&
            s.reachedStage >= 0 && s.reachedStage <= 6 && s.money >= 0 && s.money <= maxMoney &&
            timeInRange(s.bestTime, board == 0 ? maxStageTime * 6 : maxStageTime) &&
            s.bestTime > 0 && (completed == 0 || completed == 1) &&
            bool(completed) == (s.reachedStage == 6) && (board != 0 || s.reachedStage > 0));
      valid(normalizeName(s.name) == s.name);
      s.completed = bool(completed);
      scores[size_t(board)].push_back(std::move(s));
    }
    auto &entries = scores[size_t(board)];
    std::stable_sort(entries.begin(), entries.end(),
                     [&](const auto &a, const auto &b) { return better(a, b, board == 0); });
  }
  std::string end;
  valid(bool(in >> end));
  valid(end == "END");
  in >> std::ws;
  valid(in.eof());
  slots_ = std::move(slots);
  scores_ = std::move(scores);
  selected_ = selected;
  nextId_ = next;
}
bool ProfileStore::load() {
  std::error_code ec;
  bool exists = std::filesystem::exists(filePath(), ec);
  if (ec)
    throw std::system_error(ec, "Could not inspect player profiles");
  if (!exists)
    return false;
  std::ifstream input(filePath(), std::ios::binary);
  if (!input)
    throw std::runtime_error("Could not open player profiles");
  std::string text;
  char c;
  while (input.get(c)) {
    valid(text.size() < 65536);
    text.push_back(c);
  }
  valid(!input.bad());
  parse(text);
  return true;
}
void ProfileStore::save() const {
  std::string text = document();
  ProfileStore check(directory_);
  check.parse(text);
  std::filesystem::create_directories(directory_);
  static std::atomic<uint64_t> serial{0};
  auto temporary = filePath();
  temporary += ".tmp-" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
               std::to_string(serial.fetch_add(1, std::memory_order_relaxed));
  try {
    {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      if (!output)
        throw std::runtime_error("Could not create player profile save");
      output.write(text.data(), std::streamsize(text.size()));
      output.flush();
      if (!output)
        throw std::runtime_error("Could not write player profiles");
    }
    replaceFile(temporary, filePath());
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
}
} // namespace a2
