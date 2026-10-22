#include "profiles.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
template <class F> void rejects(F operation, const char *message) {
  bool rejected = false;
  try {
    operation();
  } catch (const std::exception &) {
    rejected = true;
  }
  check(rejected, message);
}
struct Directory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("a2-profiles-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Directory() { std::filesystem::create_directories(path); }
  ~Directory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
};
std::string read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}
void write(const std::filesystem::path &path, const std::string &text) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  check(bool(file), "Cannot write test fixture");
}
void finish(ProfileStore &store, int stage, double time, int money = 1250) {
  auto attempt = store.beginAttempt(stage);
  check(store.recordStage(attempt, stage, true, money, time), "Result was not accepted");
}
void creationAndIsolation() {
  Directory directory;
  ProfileStore store(directory.path);
  check(!store.load() && store.selected() == -1 && !store.active(),
        "New installation must have no profile");
  check(store.beginAttempt().serial == 0 && store.scoreboard().empty(),
        "Guest must not create a score");
  write(directory.path / "slot0.sav", "OLD GLOBAL RACE SAVE");
  store.create(0, "  Jeroen  ");
  check(store.active()->name == "Jeroen" && store.active()->stage == 0 &&
            store.active()->money == 1000 && store.active()->totalTime == 0,
        "New profile defaults or name trimming differ");
  auto pathA = store.savePath(0);
  auto idA = store.active()->id;
  check(pathA != directory.path / "slot0.sav", "Profile save path overwrites legacy slot");
  rejects([&] { store.create(0, "Overwrite"); }, "Occupied profile was overwritten");
  rejects([&] { store.select(1); }, "Unused player slot could be selected");
  for (int i = 1; i < 10; ++i)
    store.create(i, "Player " + std::to_string(i));
  rejects([&] { store.create(10, "Extra"); }, "More than ten profile slots accepted");
  store.select(0);
  store.rename(0, "Renée \"Fast\"");
  check(store.active()->name == "Renée \"Fast\"", "Unicode/quoted name changed");
  store.remove(0);
  check(store.selected() == -1 && !store.active(),
        "Deleting selected player left dangling selection");
  store.create(0, "Replacement");
  check(store.active()->id != idA && store.savePath(0) != pathA,
        "Reused profile slot accesses deleted player's saves");
  check(read(directory.path / "slot0.sav") == "OLD GLOBAL RACE SAVE", "Legacy save was changed");
  store.select(-1);
  rejects([&] { store.savePath(0); }, "Guest profile has a persistent save path");
}
void names() {
  check(ProfileStore::normalizeName("Naam") == "Naam", "ASCII name rejected");
  check(ProfileStore::normalizeName(std::string(31, 'a')).size() == 31,
        "31-character name rejected");
  for (const std::string &name :
       {std::string(), std::string("  "), std::string(32, 'a'), std::string("x\ny"),
        std::string("x\0y", 3), std::string("\xc0\xaf", 2), std::string("\xe2\x82", 2),
        std::string("\xed\xa0\x80", 3), std::string("\xf4\x90\x80\x80", 4)})
    rejects([&] { ProfileStore::normalizeName(name); },
            "Invalid or oversized profile name accepted");
}
void resultsAndBoards() {
  Directory directory;
  ProfileStore store(directory.path);
  store.create(0, "One");
  auto attempt = store.beginAttempt(0);
  check(!store.recordStage(attempt, 1, true, 1500, 30),
        "Wrong stage was recorded against an attempt");
  check(store.recordStage(attempt, 0, true, 1500, 40), "Finished first stage rejected");
  check(store.active()->stage == 1 && store.active()->money == 1500 &&
            store.active()->totalTime == 40 && store.active()->bestTimes[0] == 40,
        "First stage did not advance profile");
  check(!store.recordStage(attempt, 0, true, 2000, 40) && store.active()->money == 1500 &&
            store.active()->totalTime == 40,
        "Duplicate result changed campaign money/time");
  auto stale = store.beginAttempt(1);
  auto fresh = store.beginAttempt(1);
  check(!store.recordStage(stale, 1, true, 2000, 30), "Replaced race token was recorded");
  check(store.recordStage(fresh, 1, false, 500, 20), "Failed race result rejected");
  check(store.active()->stage == 1 && store.active()->money == 500 &&
            store.active()->totalTime == 40 && store.active()->bestTimes[1] == 0,
        "Failure advanced progress or entered best time");
  finish(store, 0, 35, 800);
  check(store.active()->stage == 1 && store.active()->totalTime == 40 &&
            store.active()->bestTimes[0] == 35,
        "Replay added cumulative campaign time or lost stage best");
  check(store.scoreboard(0).at(0).bestTime == 35, "Best stage replay not recorded");
  for (int stage = 1; stage < 6; ++stage)
    finish(store, stage, 50 + stage, 2000 + stage);
  check(store.active()->stage == 6 && store.scoreboard().at(0).completed,
        "Completed campaign not ranked as complete");
  double total = store.active()->totalTime;
  store.restart(0);
  check(store.active()->stage == 0 && store.active()->money == 1000 &&
            store.active()->totalTime == 0 && store.active()->bestTimes[0] == 35 &&
            store.scoreboard().at(0).bestTime == total,
        "Restart erased earned scores or failed to reset campaign");
  store.create(1, "Two");
  finish(store, 0, 20);
  check(store.scoreboard().at(0).name == "One" && store.scoreboard(0).at(0).name == "Two",
        "Overall completion and stage-time ordering are wrong");
  store.create(2, "Three");
  finish(store, 0, 10);
  finish(store, 1, 10);
  check(store.scoreboard().at(1).name == "Three",
        "Progress must outrank a faster shorter campaign");
  auto oldId = store.active()->id;
  store.remove(2);
  check(store.scoreboard().at(1).profileId == oldId, "Deleting a player erased historical scores");
  for (int n = 0; n < 15; ++n) {
    store.create(2, "Archived " + std::to_string(n));
    finish(store, 0, 100 + n);
    store.remove(2);
  }
  check(store.scoreboard().size() == 10 && store.scoreboard(0).size() == 10,
        "Score archives are not bounded to ten entries");
}
void persistenceAndCorruption() {
  Directory directory;
  ProfileStore store(directory.path);
  store.create(4, "Renée \"Fast\"");
  finish(store, 0, 42.125, 1500);
  auto token = store.beginAttempt(1);
  store.save();
  std::string saved = read(store.filePath());
  ProfileStore restored(directory.path);
  check(restored.load() && restored.selected() == 4 &&
            restored.active()->name == store.active()->name && restored.active()->stage == 1 &&
            restored.active()->bestTimes[0] == 42.125 &&
            restored.scoreboard().at(0).bestTime == 42.125,
        "Profile or score state changed across persistence");
  check(restored.recordStage(token, 1, true, 2000, 43.25),
        "Persisted active race token cannot finish");
  check(!restored.recordStage(token, 1, true, 2000, 43.25),
        "Loaded result could be recorded twice");
  store.save();
  for (const auto &bad : {std::string("OPENA2_PROFILES 99\n"), saved.substr(0, saved.size() / 2),
                          saved + "TRAILING DATA", std::string(65537, 'x')}) {
    write(store.filePath(), bad);
    rejects([&] { store.load(); }, "Corrupt profile document accepted");
    store.save();
    check(read(store.filePath()) == saved, "Rejected profile load changed live state");
  }
  int money = store.active()->money;
  store.active()->money = -1;
  rejects([&] { store.save(); }, "Invalid live profile overwrote previous good save");
  check(read(store.filePath()) == saved, "Failed save changed previous good document");
  store.active()->money = money;
  for (const auto &entry : std::filesystem::directory_iterator(directory.path))
    check(entry.path().extension() == ".db", "Profile save left temporary files behind");
  rejects([&] { store.recordStage(store.beginAttempt(1), 1, true, 2000, -1); },
          "Negative elapsed time accepted");
}
} // namespace
int main() {
  try {
    names();
    creationAndIsolation();
    resultsAndBoards();
    persistenceAndCorruption();
    std::cout << "Profiles: CRUD, UTF-8 names, isolated saves, progression, idempotent results, "
                 "score archives and atomic persistence passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Profiles test failed: " << e.what() << '\n';
    return 1;
  }
}
