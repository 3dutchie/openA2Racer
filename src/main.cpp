#include "assets.hpp"
#include "audio.hpp"
#include "menu_art.hpp"
#include "movie.hpp"
#include "profiles.hpp"
#include "scene.hpp"
#include <SDL.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

namespace a2 {
namespace fs = std::filesystem;
fs::path fromUTF8(std::string_view text) {
  return fs::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
}
struct Options {
  fs::path data, screenshot, preferences;
  bool headless = false, demo = false, race = false, validate = false, mute = false,
       skipIntro = false;
  std::string movie, menu;
  int frames = 0, stage = 0, car = 0, camera = -1, graphics = -1;
};
void usage() {
  std::cout << "OpenA2 0.4.0 - native A2 Racer reconstruction\n"
               "Usage: opena2 [--data CD_DIRECTORY_OR_IMAGE] [options]\n"
               "  --race             Start racing immediately\n"
               "  --stage 1..6       Select campaign stage\n"
               "  --car 1..6         Select car\n"
               "  --camera 1..3      Cockpit, chase, or helicopter view\n"
               "  --enhanced         Enhanced graphics (F4 switches live)\n"
               "  --classic          Original graphics\n"
               "  --demo             Automatic driving for a demonstration\n"
               "  --headless         Run deterministic simulation/rendering without a window\n"
               "  --frames N         Exit after N frames (headless defaults to 360)\n"
               "  --screenshot PATH  Save the last rendered frame as BMP\n"
               "  --validate         Check all original resources and exit\n"
               "  --skip-intro       Open the menu without the intro movie\n"
               "  --movie NAME.SMK   Play a movie directly\n"
               "  --menu PAGE        Preview main/players/controls/options/scores/loading\n"
               "  --prefs DIRECTORY  Separate portable preferences and profiles directory\n"
               "  --mute             Disable sound\n"
               "  --help             Show this help\n"
               "Original A2 Racer CD data is required, and is not included.\n";
}
Options parse(int argc, char **argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto value = [&]() {
      if (i + 1 >= argc)
        throw std::runtime_error("Missing value for " + a);
      return std::string(argv[++i]);
    };
    auto integer = [&](int low, int high) {
      auto s = value();
      size_t n = 0;
      int v = std::stoi(s, &n);
      if (n != s.size() || v < low || v > high)
        throw std::runtime_error("Invalid value for " + a);
      return v;
    };
    if (a == "--help" || a == "-h") {
      usage();
      std::exit(0);
    } else if (a == "--data")
      o.data = value();
    else if (a == "--screenshot")
      o.screenshot = value();
    else if (a == "--headless")
      o.headless = true;
    else if (a == "--demo")
      o.demo = true;
    else if (a == "--race")
      o.race = true;
    else if (a == "--validate")
      o.validate = true;
    else if (a == "--skip-intro")
      o.skipIntro = true;
    else if (a == "--movie")
      o.movie = value();
    else if (a == "--menu")
      o.menu = value();
    else if (a == "--prefs")
      o.preferences = value();
    else if (a == "--mute")
      o.mute = true;
    else if (a == "--enhanced")
      o.graphics = 1;
    else if (a == "--classic")
      o.graphics = 0;
    else if (a == "--frames")
      o.frames = integer(1, 10000000);
    else if (a == "--stage")
      o.stage = integer(1, 6) - 1;
    else if (a == "--car")
      o.car = integer(1, 6) - 1;
    else if (a == "--camera")
      o.camera = integer(1, 3) - 1;
    else
      throw std::runtime_error("Unknown option: " + a);
  }
  if (o.headless) {
    o.race = o.menu.empty();
    if (!o.frames)
      o.frames = 360;
  }
  return o;
}
fs::path findData(const Options &options) {
  if (!options.data.empty())
    return options.data;
  if (auto env = std::getenv("OPENA2_DATA"))
    return fs::path(env);
  std::vector<fs::path> roots{fs::current_path()};
  if (char *base = SDL_GetBasePath()) {
    roots.push_back(fromUTF8(base));
    SDL_free(base);
  }
  for (auto root : roots)
    for (int i = 0; i < 5; ++i) {
      for (const auto &child :
           {fs::path("A2-Racer_Win_NL/CD"), fs::path("CD"), fs::path("data"), fs::path(".")}) {
        auto p = root / child;
        std::error_code ec;
        if (fs::exists(p / "RCS.IND", ec) || fs::exists(p / "rcs.ind", ec))
          return p;
      }
      if (root == root.parent_path())
        break;
      root = root.parent_path();
    }
  throw std::runtime_error("Original game data not found. Use --data with your CD directory or "
                           "CD.iso,\nor drop the directory/image onto this window.");
}
void validateData(const GameData &data) {
  size_t images = 0, models = 0, sounds = 0;
  for (const auto &stem : {"RCS", "A2MRCS"}) {
    Archive a(data, stem);
    for (const auto &name : a.names()) {
      auto extension = fs::path(name).extension().string();
      if (extension == ".pcx") {
        decodePCX(a.member(name));
        ++images;
      } else if (extension == ".d3d") {
        decodeMesh(a.member(name));
        ++models;
      } else if (extension == ".wav") {
        decodeWAV(a.member(name));
        ++sounds;
      }
    }
  }
  Assets assets(data);
  size_t sprites = 0;
  for (const auto &stem : {"A2MRC", "GAMESPR"})
    sprites += decodeSprites(data, stem, assets.image("hoofd.pcx").palette).size();
  for (int i = 1; i <= 3; ++i)
    loadTrack(data, i);
  std::cout << "Validated " << images << " PCX, " << models << " meshes, " << sounds << " WAV, "
            << sprites << " sprites, 3 tracks: " << data.description() << "\n";
}
struct Settings {
  bool music = true, effects = true, mute = false, radar = true, fullscreen = false,
       enhanced = false, horizon = true, dashboard = true, lowResolution = false;
  int musicVolume = 4, effectsVolume = 4;
  std::array<SDL_Scancode, 4> keys{SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN,
                                   SDL_SCANCODE_UP};
  int camera = 1, car = 0, unlocked = 0;
  std::string name = "SPELER";
};
enum class Page {
  Main,
  Players,
  Controls,
  Scores,
  Loading,
  Name,
  ConfirmDelete,
  ConfirmRestart,
  Car,
  Stages,
  Options,
  Help,
  Slots,
  Playing,
  Results,
  Movie,
  Error
};
class App {
public:
  explicit App(Options options) : options_(std::move(options)) {
    if (!options_.headless && !options_.validate) {
      SDL_SetMainReady();
      if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0)
        throw std::runtime_error(SDL_GetError());
      window_ = SDL_CreateWindow("OpenA2 - A2 Racer", SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, 960, 720, SDL_WINDOW_RESIZABLE);
      if (!window_)
        throw std::runtime_error(SDL_GetError());
      display_ =
          SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
      if (!display_)
        display_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
      if (!display_)
        throw std::runtime_error(SDL_GetError());
      texture_ = SDL_CreateTexture(display_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                   640, 480);
      if (!texture_)
        throw std::runtime_error(SDL_GetError());
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
      SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
      openController();
      char *path = options_.preferences.empty() ? SDL_GetPrefPath("OpenA2", "OpenA2") : nullptr;
      if (path) {
        saveDir_ = fromUTF8(path);
        SDL_free(path);
      } else
        saveDir_ = options_.preferences.empty() ? fs::current_path() / "opena2-saves"
                                                : options_.preferences;
      fs::create_directories(saveDir_);
      readSettings();
    }
    if (!options_.preferences.empty()) {
      saveDir_ = options_.preferences;
      fs::create_directories(saveDir_);
      readSettings();
    }
    if (!saveDir_.empty()) {
      profiles_ = std::make_unique<ProfileStore>(saveDir_);
      try {
        profiles_->load();
      } catch (const std::exception &e) {
        notify(std::string("PROFIELEN: ") + e.what());
      }
      if (profiles_->selected() >= 0)
        profileRow_ = profiles_->selected();
    }
    draft_ = settings_;
    settings_.mute = options_.mute || settings_.mute;
    if (options_.camera >= 0)
      settings_.camera = options_.camera;
    if (options_.graphics >= 0)
      settings_.enhanced = options_.graphics != 0;
    renderer_.setEnhanced(settings_.enhanced);
    try {
      load(findData(options_));
    } catch (const std::exception &e) {
      if (options_.headless || options_.validate)
        throw;
      error_ = e.what();
      page_ = Page::Error;
    }
  }
  ~App() {
    audio_.reset();
    if (controller_)
      SDL_GameControllerClose(controller_);
    if (texture_)
      SDL_DestroyTexture(texture_);
    if (display_)
      SDL_DestroyRenderer(display_);
    if (window_)
      SDL_DestroyWindow(window_);
    SDL_Quit();
  }
  int run() {
    if (options_.validate) {
      validateData(*data_);
      return 0;
    }
    auto previous = std::chrono::steady_clock::now();
    float accumulator = 0;
    int frame = 0;
    while (!quit_) {
      auto current = std::chrono::steady_clock::now();
      float dt = std::chrono::duration<float>(current - previous).count();
      previous = current;
      if (options_.headless || options_.frames)
        dt = 1.0f / 60.0f;
      else
        dt = std::min(dt, 0.1f);
      if (!options_.headless)
        events();
      animation_ += dt;
      messageTime_ = std::max(0.0f, messageTime_ - dt);
      if (page_ == Page::Movie && !movie_->advance(dt))
        finishMovie();
      if (page_ == Page::Playing && !paused_) {
        const float approachSpeed = race_->player.speed;
        accumulator += dt;
        Input input = controls();
        while (accumulator >= 1.0f / 120.0f) {
          race_->update(1.0f / 120.0f, input);
          accumulator -= 1.0f / 120.0f;
        }
        for (auto &event : race_->takeEvents()) {
          if (audio_ && settings_.effects)
            audio_->play(event.sound);
        }
        if (race_->status() == RaceStatus::Finished || race_->status() == RaceStatus::Failed) {
          if (finishTime_ == 0) {
            finishSpeed_ = approachSpeed;
            selected_ = 0;
            if (attemptActive_ && profiles_) {
              try {
                profiles_->recordStage(attempt_, race_->stage(),
                                       race_->status() == RaceStatus::Finished, race_->player.money,
                                       race_->player.elapsed);
                saveProfiles();
              } catch (const std::exception &e) {
                notify(std::string("PROFIEL: ") + e.what());
              }
              attemptActive_ = false;
            }
            settings_.unlocked = std::max(
                settings_.unlocked,
                std::min(5, race_->stage() + (race_->status() == RaceStatus::Finished ? 1 : 0)));
            writeSettings();
          }
          finishTime_ += dt;
          if (race_->status() == RaceStatus::Failed || finishTime_ >= 3) {
            page_ = Page::Results;
            if (race_->status() == RaceStatus::Finished && race_->stage() == 5)
              playMovie("A2WIN.SMK", Page::Results);
          }
        }
      } else
        accumulator = 0;
      if (audio_ && race_ && page_ != Page::Movie) {
        const Settings &levels = page_ == Page::Options ? draft_ : settings_;
        audio_->levels(levels.music ? levels.musicVolume / 5.f : 0,
                       levels.effects ? levels.effectsVolume / 5.f : 0);
        bool playing = page_ == Page::Playing && !paused_;
        bool police = false;
        if (playing)
          for (auto &v : race_->vehicles)
            if (v.kind == VehicleKind::Police &&
                std::abs(v.distance - race_->player.distance) < 350)
              police = true;
        audio_->engine(race_->player.speed,
                       playing && levels.effects && race_->status() == RaceStatus::Running,
                       police && levels.effects);
        audio_->music(race_->stage(), levels.music);
        audio_->muted(levels.mute);
        audio_->paused(paused_);
      }
      draw();
      if (!options_.headless)
        present();
      ++frame;
      if (options_.frames && frame >= options_.frames)
        quit_ = true;
      if (!options_.headless && !display_)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!options_.screenshot.empty() && !renderer_.saveBMP(options_.screenshot))
      throw std::runtime_error("Could not save screenshot");
    if (race_)
      std::cout << "Frame " << frame << ", stage " << race_->stage() + 1 << ", distance "
                << race_->player.distance << ", speed " << race_->player.speed << ", money "
                << race_->player.money << "\n";
    return 0;
  }

private:
  Options options_;
  Settings settings_, draft_;
  std::unique_ptr<ProfileStore> profiles_;
  ProfileAttempt attempt_{};
  bool attemptActive_ = false;
  std::unique_ptr<MenuArt> menu_;
  int profileRow_ = 0, scoreTab_ = 0, displayTab_ = 0, binding_ = -1;
  bool allBindings_ = false, selectName_ = true;
  std::string editedName_;
  Page helpReturn_ = Page::Main;
  struct Hit {
    MenuRect rect;
    int action;
  };
  std::vector<Hit> hits_;
  Page hitPage_ = Page::Error;
  std::vector<uint32_t> lowPixels_;
  Page page_ = Page::Main;
  SDL_Window *window_ = nullptr;
  SDL_Renderer *display_ = nullptr;
  SDL_Texture *texture_ = nullptr;
  SDL_GameController *controller_ = nullptr;
  Renderer renderer_;
  std::unique_ptr<GameData> data_;
  std::unique_ptr<Assets> assets_;
  std::array<Track, 3> tracks_;
  std::unique_ptr<Race> race_;
  std::unique_ptr<Scene> scene_;
  std::unique_ptr<Audio> audio_;
  std::unique_ptr<Movie> movie_;
  Page afterMovie_ = Page::Main;
  bool exitAfterMovie_ = false;
  fs::path saveDir_;
  bool quit_ = false, paused_ = false, hasRace_ = false;
  int selected_ = 0, slot_ = 0;
  float animation_ = 0, messageTime_ = 0, finishTime_ = 0, finishSpeed_ = 0;
  std::string error_, message_;
  void load(const fs::path &path) {
    auto data = std::make_unique<GameData>(path);
    auto assets = std::make_unique<Assets>(*data);
    std::array<Track, 3> tracks;
    for (int i = 0; i < 3; ++i)
      tracks[size_t(i)] = loadTrack(*data, i + 1);
    auto menu = std::make_unique<MenuArt>(renderer_, *assets);
    if (!options_.headless && !options_.validate) {
      renderer_.clear(0xff000000);
      menu->loading();
      present();
      SDL_PumpEvents();
    }
    // Prime resources needed by the first visible frame, so errors remain actionable.
    assets->image("hoofd.pcx");
    assets->image("Sky.pcx");
    assets->mesh("porche.d3d");
    audio_.reset();
    movie_.reset();
    menu_.reset();
    scene_.reset();
    race_.reset();
    renderer_.resetTextureCache();
    data_ = std::move(data);
    assets_ = std::move(assets);
    menu_ = std::move(menu);
    tracks_ = std::move(tracks);
    race_ = std::make_unique<Race>(tracks_);
    scene_ = std::make_unique<Scene>(renderer_, *assets_);
    if (!options_.headless && !options_.validate)
      audio_ = std::make_unique<Audio>(*assets_);
    page_ = profiles_ && !profiles_->active() ? Page::Players : Page::Main;
    selected_ = 0;
    hasRace_ = false;
    if (options_.race || options_.demo)
      start(options_.stage, options_.car);
    else if (!options_.skipIntro && !options_.validate && options_.menu.empty())
      playMovie("A2INTRO.SMK", page_);
    if (!options_.menu.empty()) {
      const std::map<std::string, Page> screens{
          {"main", Page::Main},       {"players", Page::Players}, {"controls", Page::Controls},
          {"options", Page::Options}, {"scores", Page::Scores},   {"loading", Page::Loading}};
      auto found = screens.find(options_.menu);
      if (found == screens.end())
        throw std::runtime_error("Unknown menu page");
      page_ = found->second;
      draft_ = settings_;
      displayTab_ = settings_.enhanced ? 0 : (settings_.lowResolution ? 1 : 2);
    }
    if (!options_.movie.empty()) {
      exitAfterMovie_ = true;
      playMovie(options_.movie, Page::Main);
    }
  }
  void playMovie(const std::string &name, Page after) {
    if (!data_->exists(name))
      return;
    try {
      if (audio_)
        audio_->movie(nullptr);
      movie_ = std::make_unique<Movie>(*data_, name);
      afterMovie_ = after;
      page_ = Page::Movie;
      paused_ = false;
      if (audio_) {
        audio_->movie(&movie_->audio());
        audio_->muted(settings_.mute);
      }
    } catch (const std::exception &e) {
      movie_.reset();
      message_ = std::string("VIDEO: ") + e.what();
      messageTime_ = 3;
      page_ = after;
      if (exitAfterMovie_)
        quit_ = true;
    }
  }
  void finishMovie() {
    if (audio_)
      audio_->movie(nullptr);
    movie_.reset();
    page_ = afterMovie_;
    selected_ = 0;
    if (exitAfterMovie_)
      quit_ = true;
  }
  void notify(std::string text) {
    message_ = std::move(text);
    messageTime_ = 3;
  }
  void loading() {
    if (!menu_ || options_.headless || options_.validate)
      return;
    renderer_.clear(0xff000000);
    menu_->loading();
    present();
    SDL_PumpEvents();
  }
  void start(int stage, int car) {
    loading();
    race_->start(stage, car);
    if (profiles_ && profiles_->active() && !options_.headless) {
      race_->player.money = profiles_->active()->money;
      attempt_ = profiles_->beginAttempt(stage);
      attemptActive_ = true;
    } else
      attemptActive_ = false;
    hasRace_ = true;
    page_ = Page::Playing;
    paused_ = false;
    finishTime_ = 0;
    selected_ = 0;
    settings_.car = car;
    // Decode and filter the first race frame while the genuine loading artwork
    // is displayed. No artificial timer is added to fast loads.
    scene_->draw(*race_, settings_.camera, settings_.radar, settings_.horizon, settings_.dashboard);
    writeSettings();
  }
  void openController() {
    if (controller_)
      return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
      if (SDL_IsGameController(i)) {
        controller_ = SDL_GameControllerOpen(i);
        if (controller_)
          break;
      }
  }
  Input controls() {
    Input in;
    if (options_.demo) {
      in.throttle = 1;
      float target = 45;
      for (const auto &v : race_->vehicles)
        if (v.distance > race_->player.distance && v.distance - race_->player.distance < 300 &&
            std::abs(v.lateral - target) < 20)
          target = target < 45 ? 65 : 25;
      in.steer = std::clamp((target - race_->player.lateral) * 0.035f - race_->player.heading * 5,
                            -1.0f, 1.0f);
      return in;
    }
    if (options_.headless)
      return in;
    const auto *k = SDL_GetKeyboardState(nullptr);
    const bool standard =
        settings_.keys == std::array<SDL_Scancode, 4>{SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                                      SDL_SCANCODE_DOWN, SDL_SCANCODE_UP};
    in.throttle = float(k[settings_.keys[3]] || (standard && k[SDL_SCANCODE_W]));
    in.brake = float(k[settings_.keys[2]] || (standard && k[SDL_SCANCODE_S]));
    in.steer = float(k[settings_.keys[1]] || (standard && k[SDL_SCANCODE_D])) -
               float(k[settings_.keys[0]] || (standard && k[SDL_SCANCODE_A]));
    in.handbrake = k[SDL_SCANCODE_SPACE];
    if (controller_) {
      float axis = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
      if (std::abs(axis) > 0.15f)
        in.steer = axis;
      in.throttle = std::max(
          in.throttle,
          std::max(0.0f, SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) /
                             32767.0f));
      in.brake = std::max(
          in.brake,
          std::max(0.0f, SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERLEFT) /
                             32767.0f));
      if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_A))
        in.throttle = 1;
      in.handbrake =
          in.handbrake || SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_X);
    }
    return in;
  }
  static std::string timeText(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0)
      return "--u--m--s";
    int n = int(std::min(seconds, 359999.0));
    char text[32];
    std::snprintf(text, sizeof(text), "%02du%02dm%02ds", n / 3600, n / 60 % 60, n % 60);
    return text;
  }
  static void eraseCodepoint(std::string &s) {
    if (s.empty())
      return;
    do {
      unsigned char c = static_cast<unsigned char>(s.back());
      s.pop_back();
      if ((c & 0xc0) != 0x80)
        break;
    } while (!s.empty());
  }
  std::string clippedName(std::string text, int width, int size = 18) {
    if (menu_->textWidth(text, size) <= width)
      return text;
    while (!text.empty() && menu_->textWidth(text + "...", size) > width)
      eraseCodepoint(text);
    return text + "...";
  }
  std::string keyName(SDL_Scancode key) const {
    if (key == SDL_SCANCODE_LEFT)
      return "Links";
    if (key == SDL_SCANCODE_RIGHT)
      return "Rechts";
    if (key == SDL_SCANCODE_DOWN)
      return "Omlaag";
    if (key == SDL_SCANCODE_UP)
      return "Omhoog";
    return SDL_GetScancodeName(key);
  }
  void go(Page page) {
    page_ = page;
    selected_ = 0;
    hits_.clear();
    if (page == Page::Controls || page == Page::Options) {
      draft_ = settings_;
      binding_ = -1;
      displayTab_ = settings_.enhanced ? 0 : (settings_.lowResolution ? 1 : 2);
    }
  }
  const Profile *profile() const { return profiles_ ? profiles_->active() : nullptr; }
  void saveProfiles() {
    if (profiles_)
      profiles_->save();
  }
  void askName() {
    editedName_ = "Speler";
    selectName_ = true;
    go(Page::Name);
    SDL_StartTextInput();
  }
  void chooseProfile() {
    if (!profiles_)
      return;
    if (!profiles_->slots()[size_t(profileRow_)]) {
      askName();
      return;
    }
    profiles_->select(profileRow_);
    saveProfiles();
    hasRace_ = false;
    attemptActive_ = false;
    settings_.unlocked = std::min(5, profile()->stage);
    go(Page::Main);
  }
  void commitName() {
    if (!profiles_)
      return;
    profiles_->create(profileRow_, editedName_);
    saveProfiles();
    SDL_StopTextInput();
    hasRace_ = false;
    attemptActive_ = false;
    settings_.unlocked = 0;
    go(Page::Main);
  }
  void leaveDraft(bool commit) {
    if (commit) {
      if (page_ == Page::Controls)
        settings_.keys = draft_.keys;
      else
        settings_ = draft_;
    }
    renderer_.setEnhanced(settings_.enhanced);
    binding_ = -1;
    go(Page::Main);
    writeSettings();
  }
  void quitGame() {
    if (data_ && data_->exists("A2EXIT.SMK")) {
      exitAfterMovie_ = true;
      playMovie("A2EXIT.SMK", Page::Main);
    } else
      quit_ = true;
  }
  std::vector<std::string> choices() const {
    switch (page_) {
    case Page::Main:
      return {"START", "BEDIENING", "OPTIES", "SCORE", "OPNIEUW", "NIEUWE SPELER"};
    case Page::Players:
      return {"SELECTEREN", "VERWIJDEREN"};
    case Page::Name:
      return {"OK", "ANNULEREN"};
    case Page::ConfirmDelete:
    case Page::ConfirmRestart:
      return {"JA", "NEE"};
    case Page::Controls:
      return {"OK", "ANNULEREN", "INSTELLEN"};
    case Page::Options:
      return {"MUZIEKVOLUME", "EFFECTVOLUME", "HORIZON", "GELUID",
              "DASHBOARD",    "WEERGAVE",     "OK",      "ANNULEREN"};
    case Page::Scores:
    case Page::Help:
      return {"TERUG"};
    case Page::Car:
      return {"VORIGE AUTO", "VOLGENDE AUTO", "SELECTEREN"};
    case Page::Stages:
      return {"ETAPPE 1", "ETAPPE 2", "ETAPPE 3", "ETAPPE 4", "ETAPPE 5", "ETAPPE 6", "TERUG"};
    case Page::Slots:
      return {"VORIG SLOT", "VOLGEND SLOT", "OPSLAAN", "LADEN", "OUD SLOT LADEN", "TERUG"};
    case Page::Results:
      return {"VERDER", "HOOFDMENU"};
    default:
      return {};
    }
  }
  int soundMode() const {
    if (draft_.mute)
      return 3;
    return draft_.effects ? (draft_.music ? 2 : 0) : (draft_.music ? 1 : 3);
  }
  void setSoundMode(int value) {
    draft_.mute = false;
    draft_.effects = value == 0 || value == 2;
    draft_.music = value == 1 || value == 2;
  }
  void setDisplay(int value) {
    displayTab_ = (value % 3 + 3) % 3;
    draft_.enhanced = displayTab_ == 0;
    draft_.lowResolution = displayTab_ == 1;
    renderer_.setEnhanced(draft_.enhanced);
  }
  void adjustOption(int direction) {
    switch (selected_) {
    case 0:
      draft_.musicVolume = std::clamp(draft_.musicVolume + direction, 0, 5);
      break;
    case 1:
      draft_.effectsVolume = std::clamp(draft_.effectsVolume + direction, 0, 5);
      break;
    case 2:
      draft_.horizon = !draft_.horizon;
      break;
    case 3:
      setSoundMode((soundMode() + direction + 4) % 4);
      break;
    case 4:
      draft_.dashboard = !draft_.dashboard;
      break;
    case 5:
      setDisplay(displayTab_ + direction);
      break;
    }
  }
  void activate() {
    try {
      switch (page_) {
      case Page::Main:
        if (selected_ == 0) {
          if (!profile()) {
            go(Page::Players);
            break;
          }
          if (hasRace_) {
            go(race_->status() == RaceStatus::Finished || race_->status() == RaceStatus::Failed
                   ? Page::Results
                   : Page::Playing);
            paused_ = false;
          } else if (profile()->stage >= 6) {
            go(Page::ConfirmRestart);
            selected_ = 1;
          } else
            start(profile()->stage, settings_.car);
        } else if (selected_ == 1)
          go(Page::Controls);
        else if (selected_ == 2)
          go(Page::Options);
        else if (selected_ == 3)
          go(Page::Scores);
        else if (selected_ == 4) {
          go(Page::ConfirmRestart);
          selected_ = 1;
        } else {
          go(Page::Players);
          if (profiles_ && profiles_->selected() >= 0)
            profileRow_ = profiles_->selected();
        }
        break;
      case Page::Players:
        if (selected_ == 0)
          chooseProfile();
        else if (profiles_ && profiles_->slots()[size_t(profileRow_)]) {
          go(Page::ConfirmDelete);
          selected_ = 1;
        }
        break;
      case Page::Name:
        if (selected_ == 0)
          commitName();
        else {
          SDL_StopTextInput();
          go(Page::Players);
        }
        break;
      case Page::ConfirmDelete:
        if (selected_ == 0 && profiles_) {
          profiles_->remove(profileRow_);
          saveProfiles();
          hasRace_ = false;
          attemptActive_ = false;
        }
        go(Page::Players);
        break;
      case Page::ConfirmRestart:
        if (selected_ == 0 && profile()) {
          profiles_->restart(profiles_->selected());
          saveProfiles();
          hasRace_ = false;
          attemptActive_ = false;
          settings_.unlocked = 0;
        }
        go(Page::Main);
        break;
      case Page::Controls:
        if (selected_ < 2)
          leaveDraft(selected_ == 0);
        else {
          binding_ = 0;
          allBindings_ = true;
        }
        break;
      case Page::Options:
        if (selected_ >= 6)
          leaveDraft(selected_ == 6);
        else
          adjustOption(1);
        break;
      case Page::Scores:
        go(Page::Main);
        break;
      case Page::Help:
        page_ = helpReturn_;
        selected_ = 0;
        hits_.clear();
        break;
      case Page::Car:
        if (selected_ == 0)
          settings_.car = (settings_.car + 5) % 6;
        else if (selected_ == 1)
          settings_.car = (settings_.car + 1) % 6;
        else {
          writeSettings();
          go(Page::Main);
        }
        break;
      case Page::Stages:
        if (selected_ == 6)
          go(Page::Main);
        else if (profile() && (profile()->stage >= selected_))
          start(selected_, settings_.car);
        else
          notify("SPEEL EERST DE VORIGE ETAPPE UIT");
        break;
      case Page::Slots:
        if (selected_ == 0)
          slot_ = (slot_ + 9) % 10;
        else if (selected_ == 1)
          slot_ = (slot_ + 1) % 10;
        else if (selected_ == 2)
          saveSlot(slot_);
        else if (selected_ == 3)
          loadSlot(slot_);
        else if (selected_ == 4)
          loadSlot(slot_, true);
        else
          go(Page::Main);
        break;
      case Page::Results:
        if (selected_ == 1)
          go(Page::Main);
        else if (race_->status() == RaceStatus::Finished && race_->stage() < 5)
          start(race_->stage() + 1, race_->player.car);
        else if (race_->status() == RaceStatus::Failed)
          start(race_->stage(), race_->player.car);
        else {
          go(Page::ConfirmRestart);
          selected_ = 1;
        }
        break;
      default:
        break;
      }
    } catch (const std::exception &e) {
      notify(e.what());
    }
  }
  void key(SDL_Keycode key) {
    if (binding_ >= 0) {
      if (key == SDLK_ESCAPE) {
        binding_ = -1;
        return;
      }
      SDL_Scancode code = SDL_GetScancodeFromKey(key);
      if (code == SDL_SCANCODE_UNKNOWN)
        return;
      if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_p || key == SDLK_g ||
          key == SDLK_m || key == SDLK_r || (key >= SDLK_F1 && key <= SDLK_F12) ||
          (key >= SDLK_1 && key <= SDLK_9) || (key >= SDLK_KP_1 && key <= SDLK_KP_9)) {
        notify("DEZE TOETS IS GERESERVEERD VOOR EEN SPELFUNCTIE");
        return;
      }
      for (int i = 0; i < 4; ++i)
        if (i != binding_ && draft_.keys[size_t(i)] == code) {
          if (allBindings_ && i < binding_) {
            notify("KIES VOOR ELKE ACTIE EEN ANDERE TOETS");
            return;
          }
          draft_.keys[size_t(i)] = draft_.keys[size_t(binding_)];
          break;
        }
      draft_.keys[size_t(binding_)] = code;
      if (allBindings_ && binding_ < 3)
        ++binding_;
      else
        binding_ = -1;
      return;
    }
    if (page_ == Page::Name) {
      if ((key == SDLK_a) && (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI))) {
        selectName_ = true;
        return;
      }
      if (key == SDLK_BACKSPACE) {
        if (selectName_)
          editedName_.clear();
        else
          eraseCodepoint(editedName_);
        selectName_ = false;
      } else if (key == SDLK_TAB)
        selected_ = 1 - selected_;
      else if (key == SDLK_RETURN)
        activate();
      else if (key == SDLK_ESCAPE) {
        SDL_StopTextInput();
        go(Page::Players);
      }
      return;
    }
    // Profile name entry above keeps literal digits. Elsewhere, both number
    // rows mirror the function keys, including keyboard layouts with a keypad.
    if (key >= SDLK_1 && key <= SDLK_9)
      key = SDL_Keycode(SDLK_F1 + (key - SDLK_1));
    else if (key >= SDLK_KP_1 && key <= SDLK_KP_9)
      key = SDL_Keycode(SDLK_F1 + (key - SDLK_KP_1));
    if (key == SDLK_F4 || key == SDLK_g) {
      toggleGraphics();
      return;
    }
    if (key == SDLK_F11) {
      fullscreen();
      return;
    }
    if (key == SDLK_m) {
      settings_.mute = !settings_.mute;
      writeSettings();
      return;
    }
    if (page_ == Page::Movie) {
      if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_SPACE)
        finishMovie();
      return;
    }
    if (page_ == Page::Playing) {
      if (key == SDLK_ESCAPE) {
        go(Page::Main);
        paused_ = false;
      } else if (key == SDLK_p)
        paused_ = !paused_;
      else if (key == SDLK_SPACE && race_->fineDelay() > 0)
        race_->dismissFine();
      else if (key == SDLK_RETURN) {
        paused_ = false;
        race_->dismissFine();
        if (race_->status() == RaceStatus::Finished)
          finishTime_ = 3;
      } else if (key >= SDLK_F1 && key <= SDLK_F3)
        settings_.camera = int(key - SDLK_F1);
      else if (key == SDLK_F5)
        settings_.radar = !settings_.radar;
      else if (key == SDLK_F6)
        saveSlot(0);
      else if (key == SDLK_F9)
        loadSlot(0);
      else if (key == SDLK_r)
        start(race_->stage(), race_->player.car);
      return;
    }
    if (key == SDLK_F1) {
      if (page_ == Page::Help)
        return;
      helpReturn_ = page_;
      go(Page::Help);
      return;
    }
    if (key == SDLK_ESCAPE) {
      if (page_ == Page::Options || page_ == Page::Controls)
        leaveDraft(false);
      else if (page_ == Page::Help) {
        page_ = helpReturn_;
        selected_ = 0;
        hits_.clear();
      } else if (page_ == Page::ConfirmDelete)
        go(Page::Players);
      else
        go(Page::Main);
      return;
    }
    if (page_ == Page::Main) {
      if (key == SDLK_c) {
        go(Page::Car);
        return;
      }
      if (key == SDLK_t) {
        go(Page::Stages);
        return;
      }
      if (key == SDLK_s || key == SDLK_l) {
        go(Page::Slots);
        return;
      }
    }
    if (page_ == Page::Players) {
      if (key == SDLK_UP)
        profileRow_ = (profileRow_ + 9) % 10;
      else if (key == SDLK_DOWN)
        profileRow_ = (profileRow_ + 1) % 10;
      else if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_TAB)
        selected_ = 1 - selected_;
      else if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
        selected_ = 1;
        activate();
      } else if (key == SDLK_RETURN || key == SDLK_SPACE)
        activate();
      return;
    }
    if (page_ == Page::Scores && (key == SDLK_LEFT || key == SDLK_RIGHT)) {
      scoreTab_ = (scoreTab_ + (key == SDLK_LEFT ? 6 : 1)) % 7;
      return;
    }
    if (page_ == Page::Options && (key == SDLK_LEFT || key == SDLK_RIGHT)) {
      adjustOption(key == SDLK_LEFT ? -1 : 1);
      return;
    }
    if ((page_ == Page::ConfirmDelete || page_ == Page::ConfirmRestart) &&
        (key == SDLK_LEFT || key == SDLK_RIGHT)) {
      selected_ = 1 - selected_;
      return;
    }
    auto c = choices();
    if (c.empty())
      return;
    if (key == SDLK_UP)
      selected_ = (selected_ + int(c.size()) - 1) % int(c.size());
    else if (key == SDLK_DOWN || key == SDLK_TAB)
      selected_ = (selected_ + 1) % int(c.size());
    else if (key == SDLK_RETURN || key == SDLK_SPACE)
      activate();
    else if (page_ == Page::Car && (key == SDLK_LEFT || key == SDLK_RIGHT))
      settings_.car = (settings_.car + (key == SDLK_LEFT ? 5 : 1)) % 6;
  }
  void pointer(int x, int y, bool click, bool doubleClick = false) {
    if (page_ == Page::Name) {
      for (const auto &hit : hits_)
        if (hit.rect.contains(x, y)) {
          selected_ = hit.action;
          if (click)
            activate();
          return;
        }
      if (click && MenuRect{174, 235, 203, 28}.contains(x, y))
        selectName_ = true;
      return;
    }
    if (page_ == Page::Movie) {
      if (click)
        finishMovie();
      return;
    }
    if (page_ == Page::Playing || page_ == Page::Error)
      return;
    if ((page_ == Page::Main || page_ == Page::Players) &&
        MenuArt::topButtonRect(MenuButton::Exit).contains(x, y) && click) {
      quitGame();
      return;
    }
    if (page_ != Page::Help && MenuArt::topButtonRect(MenuButton::Help).contains(x, y) && click) {
      helpReturn_ = page_;
      go(Page::Help);
      return;
    }
    if (page_ == Page::Players && MenuRect{60, 141, 505, 250}.contains(x, y)) {
      profileRow_ = std::clamp((y - 141) / 25, 0, 9);
      if (click && doubleClick) {
        selected_ = 0;
        activate();
      }
      return;
    }
    if (page_ == Page::Scores && y >= 120 && y <= 146) {
      constexpr int edges[]{35, 112, 194, 276, 358, 440, 522, 605};
      for (int i = 0; i < 7; ++i)
        if (x >= edges[i] && x < edges[i + 1] && click)
          scoreTab_ = i;
      return;
    }
    if (page_ == Page::Controls) {
      constexpr MenuRect fields[]{
          {349, 170, 96, 34}, {482, 170, 96, 34}, {349, 281, 96, 34}, {482, 281, 96, 34}};
      for (int i = 0; i < 4; ++i)
        if (fields[i].contains(x, y) && click) {
          binding_ = i;
          allBindings_ = false;
          return;
        }
    }
    if (page_ == Page::Options && click) {
      if (MenuRect{18, 150, 21, 21}.contains(x, y)) {
        selected_ = 0;
        adjustOption(-1);
        return;
      }
      if (MenuRect{174, 150, 21, 21}.contains(x, y)) {
        selected_ = 0;
        adjustOption(1);
        return;
      }
      if (MenuRect{18, 220, 21, 21}.contains(x, y)) {
        selected_ = 1;
        adjustOption(-1);
        return;
      }
      if (MenuRect{174, 220, 21, 21}.contains(x, y)) {
        selected_ = 1;
        adjustOption(1);
        return;
      }
      if (x >= 326 && x <= 375) {
        for (int i = 0; i < 2; ++i)
          if (y >= 160 + 20 * i && y <= 180 + 20 * i) {
            draft_.horizon = i == 0;
            selected_ = 2;
            return;
          }
        for (int i = 0; i < 4; ++i)
          if (y >= 243 + 20 * i && y <= 263 + 20 * i) {
            setSoundMode(i);
            selected_ = 3;
            return;
          }
        for (int i = 0; i < 2; ++i)
          if (y >= 355 + 20 * i && y <= 375 + 20 * i) {
            draft_.dashboard = i == 0;
            selected_ = 4;
            return;
          }
      }
      if (y >= 116 && y <= 145 && x >= 224) {
        selected_ = 5;
        setDisplay(x < 374 ? 0 : (x < 478 ? 1 : 2));
        return;
      }
    }
    if (hitPage_ != page_)
      return;
    for (const auto &hit : hits_)
      if (hit.rect.contains(x, y)) {
        selected_ = hit.action;
        if (click)
          activate();
        return;
      }
  }
  void events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        quit_ = true;
      else if (e.type == SDL_TEXTINPUT && page_ == Page::Name) {
        std::string candidate = selectName_ ? std::string() : editedName_;
        candidate += e.text.text;
        size_t chars = 0;
        for (unsigned char c : candidate)
          if ((c & 0xc0) != 0x80)
            ++chars;
        if (chars <= ProfileStore::maxNameCharacters &&
            candidate.size() <= ProfileStore::maxNameBytes)
          editedName_ = std::move(candidate);
        selectName_ = false;
      } else if (e.type == SDL_KEYDOWN && (!e.key.repeat || page_ == Page::Name))
        key(e.key.keysym.sym);
      else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST &&
               page_ == Page::Playing)
        paused_ = true;
      else if (e.type == SDL_DROPFILE) {
        std::string path = e.drop.file;
        SDL_free(e.drop.file);
        try {
          load(fromUTF8(path));
        } catch (const std::exception &ex) {
          error_ = ex.what();
          page_ = Page::Error;
        }
      } else if (e.type == SDL_CONTROLLERDEVICEADDED)
        openController();
      else if (e.type == SDL_CONTROLLERDEVICEREMOVED && controller_ &&
               SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_)) ==
                   e.cdevice.which) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
      } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        auto b = e.cbutton.button;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_UP)
          key(SDLK_UP);
        if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
          key(SDLK_DOWN);
        if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
          key(SDLK_LEFT);
        if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
          key(SDLK_RIGHT);
        if (b == SDL_CONTROLLER_BUTTON_A &&
            (page_ != Page::Playing || race_->fineDelay() > 0))
          key(SDLK_RETURN);
        if (b == SDL_CONTROLLER_BUTTON_B)
          key(SDLK_ESCAPE);
        if (b == SDL_CONTROLLER_BUTTON_START)
          key(page_ == Page::Playing ? SDLK_p : SDLK_RETURN);
        if (b == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
          key(SDLK_F4);
        if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
          settings_.camera = (settings_.camera + 1) % 3;
      } else if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN) {
        int w, h;
        SDL_GetWindowSize(window_, &w, &h);
        float scale = std::min(w / 640.f, h / 480.f);
        int x = e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x,
            y = e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y;
        x = int((x - (w - 640 * scale) * .5f) / scale);
        y = int((y - (h - 480 * scale) * .5f) / scale);
        pointer(x, y, e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT,
                e.type == SDL_MOUSEBUTTONDOWN && e.button.clicks >= 2);
      }
    }
  }
  void fullscreen() {
    settings_.fullscreen = !settings_.fullscreen;
    if (window_)
      SDL_SetWindowFullscreen(window_, settings_.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  }
  void drawPlayers() {
    menu_->background(MenuPage::Players);
    menu_->button(MenuButton::Select, 110, 410);
    menu_->button(MenuButton::Delete, 390, 410);
    menu_->profileHighlight(profileRow_);
    for (int i = 0; i < 10; ++i) {
      const Profile *p =
          profiles_ && profiles_->slots()[size_t(i)] ? &*profiles_->slots()[size_t(i)] : nullptr;
      uint32_t color = i == profileRow_ ? 0xffffffff : 0xffe5df58;
      int y = 141 + 25 * i;
      menu_->serifText(67, y, p ? clippedName(p->name, 138, 15) : "L E E G", color, 15);
      menu_->serifText(211, y,
                       p ? (p->stage >= 6 ? "Uitgereden" : "Etappe " + std::to_string(p->stage + 1))
                         : "",
                       color, 15);
      menu_->serifText(351, y, p ? std::to_string(p->money) : "0", color, 15);
      menu_->serifText(462, y, p ? timeText(p->totalTime) : "--u--m--s", color, 15);
    }
  }
  void addButton(MenuButton button, int x, int y, int action) {
    hits_.push_back({menu_->button(button, x, y, selected_ == action, animation_), action});
  }
  void drawInformation() {
    const auto *p = profile();
    const std::string values[]{
        p ? clippedName(p->name, 112, 13) : "Speler",
        std::to_string(hasRace_ ? race_->player.money : (p ? p->money : 1000)),
        timeText(p ? p->totalTime : 0), controller_ ? "Controller" : "Toetsenbord",
        settings_.enhanced ? "Verbeterd" : (settings_.lowResolution ? "320 x 200" : "640 x 480")};
    for (int i = 0; i < 5; ++i)
      menu_->serifText(520, 140 + 16 * i, values[i], 0xffffffff, 13);
    menu_->routeCard(hasRace_ ? race_->stage() : (p ? std::min(5, p->stage) : 0));
  }
  void draw() {
    hits_.clear();
    hitPage_ = page_;
    if (page_ == Page::Movie) {
      renderer_.clear(0xff000000);
      renderer_.blit(movie_->image(), 0, 0, 640, 480);
      renderer_.text(480, 466, "ENTER / ESC: OVERSLAAN", 0xffeeeeee);
    } else if (page_ == Page::Playing) {
      if (race_->status() == RaceStatus::Finished) {
        // Keep the scored snapshot fixed while the presentation coasts clear
        // of the gantry. This seconds-based slowdown is reconstructed.
        Race presentation = *race_;
        const float coast = std::exp(-finishTime_);
        presentation.player.distance = std::min(race_->track().length(),
            race_->player.distance + finishSpeed_ * (1 - coast) * (10.0f / 3.6f));
        presentation.player.speed = finishSpeed_ * coast;
        presentation.player.elapsed += finishTime_;
        presentation.player.timeLeft = std::max(0.0f, presentation.player.timeLeft - finishTime_);
        for (auto &v : presentation.vehicles)
          v.distance = std::clamp(v.distance + v.speed * finishTime_ * (10.0f / 3.6f) *
                                 (v.oncoming ? -1 : 1), 0.0f, presentation.track().length());
        scene_->draw(presentation, settings_.camera, settings_.radar, settings_.horizon,
                     settings_.dashboard);
        scene_->drawFinish(finishTime_, settings_.camera == 0 && settings_.dashboard ? 0 : 1);
      } else
        scene_->draw(*race_, settings_.camera, settings_.radar, settings_.horizon,
                     settings_.dashboard);
      if (paused_)
        scene_->drawPause();
    } else if (page_ == Page::Error) {
      renderer_.clear(0xff202735);
      renderer_.text(42, 60, "OPEN A2", 0xffffd35a, 4);
      renderer_.text(42, 145, "ORIGINELE SPELGEGEVENS NODIG", 0xffded9ec, 2);
      renderer_.text(42, 195, error_, 0xffdddddd);
      renderer_.text(42, 320, "DROP YOUR CD FOLDER OR CD.ISO HERE", 0xffffd35a, 2);
      renderer_.text(42, 368, "OPENA2 --DATA /PATH/TO/CD");
    } else if (page_ == Page::Loading) {
      renderer_.clear(0xff000000);
      menu_->loading();
    } else {
      renderer_.clear(0xff202735);
      if (page_ == Page::Players || page_ == Page::Name || page_ == Page::ConfirmDelete)
        drawPlayers();
      else if (page_ == Page::Controls)
        menu_->background(MenuPage::Controls);
      else if (page_ == Page::Options)
        menu_->background(MenuPage::Options);
      else if (page_ == Page::Scores || page_ == Page::Results)
        menu_->background(MenuPage::Scores);
      else {
        menu_->background(MenuPage::Main);
        drawInformation();
      }
      menu_->topButtons();
      if (page_ == Page::Main) {
        constexpr MenuButton buttons[]{MenuButton::Start,   MenuButton::Controls,
                                       MenuButton::Options, MenuButton::Scores,
                                       MenuButton::Restart, MenuButton::Players};
        for (int i = 0; i < 6; ++i)
          addButton(buttons[i], 0, 124 + 53 * i, i);
      } else if (page_ == Page::Players) {
        addButton(MenuButton::Select, 110, 410, 0);
        addButton(MenuButton::Delete, 390, 410, 1);
      } else if (page_ == Page::Name) {
        menu_->playerDialog();
        menu_->serifText(260, 116, "Vul je naam in en word lid", 0xffe5df58, 13);
        menu_->serifText(260, 137, "van de A2 Racing Club!", 0xffe5df58, 13);
        renderer_.rect(174, 235, 203, 28, selectName_ ? 0xff4f5362 : 0xff262626);
        std::string text = clippedName(editedName_, 196, 20);
        menu_->serifText(175, 236, text, 0xffffffff, 20, false);
        if (int(animation_ * 2) % 2 == 0)
          renderer_.rect(std::min(374, 175 + menu_->textWidth(text, 20)), 239, 1, 21, 0xffffffff);
        addButton(MenuButton::DialogOK, 194, 303, 0);
        addButton(MenuButton::DialogCancel, 347, 303, 1);
      } else if (page_ == Page::ConfirmDelete || page_ == Page::ConfirmRestart) {
        menu_->playerDialog();
        menu_->serifText(260, 116,
                         page_ == Page::ConfirmDelete ? "Speler verwijderen?"
                                                      : "Opnieuw vanaf etappe 1?",
                         0xffe5df58, 13);
        menu_->serifText(260, 140, "Scores blijven bewaard.", 0xffe5df58, 13);
        renderer_.rect(174, 235, 203, 28, 0xff80718f);
        if (page_ == Page::ConfirmDelete && profiles_ && profiles_->slots()[size_t(profileRow_)])
          menu_->serifText(174, 235,
                           clippedName(profiles_->slots()[size_t(profileRow_)]->name, 203, 20),
                           0xffffffff, 20);
        addButton(MenuButton::Yes, 194, 303, 0);
        addButton(MenuButton::No, 347, 303, 1);
      } else if (page_ == Page::Controls) {
        menu_->controlPanel();
        constexpr MenuRect fields[]{
            {349, 170, 96, 34}, {482, 170, 96, 34}, {349, 281, 96, 34}, {482, 281, 96, 34}};
        for (int i = 0; i < 4; ++i) {
          if (binding_ == i)
            renderer_.rect(fields[i].x + 4, fields[i].y + 4, fields[i].w - 8, fields[i].h - 8,
                           0xff625116);
          menu_->serifText(fields[i].x + 8, fields[i].y + 8,
                           binding_ == i ? "...?"
                                         : clippedName(keyName(draft_.keys[size_t(i)]), 80, 14),
                           0xffffffff, 14, false);
        }
        if (binding_ >= 0)
          menu_->serifText(305, 355, "Druk op een toets. Esc: annuleren", 0xffffff70, 14);
        addButton(MenuButton::OK, 0, 336, 0);
        addButton(MenuButton::Cancel, 0, 389, 1);
        addButton(MenuButton::Configure, 365, 385, 2);
      } else if (page_ == Page::Options) {
        menu_->optionsPanel(displayTab_);
        menu_->volume(0, 115, draft_.musicVolume);
        menu_->volume(0, 185, draft_.effectsVolume);
        menu_->radio(330, 164, draft_.horizon);
        menu_->radio(330, 184, !draft_.horizon);
        for (int i = 0; i < 4; ++i)
          menu_->radio(330, 247 + 20 * i, soundMode() == i);
        menu_->radio(330, 359, draft_.dashboard);
        menu_->radio(330, 379, !draft_.dashboard);
        addButton(MenuButton::OK, 0, 336, 6);
        addButton(MenuButton::Cancel, 0, 389, 7);
        // Small keyboard-focus marker does not cover the recovered controls.
        constexpr int focusY[]{151, 221, 164, 247, 359, 122};
        if (selected_ < 6)
          menu_->serifText(selected_ < 2 ? 2 : 212, focusY[selected_], ">", 0xffffff60, 17, false);
      } else if (page_ == Page::Scores) {
        menu_->scoreTabs(scoreTab_);
        auto scores = profiles_ ? profiles_->scoreboard(scoreTab_ - 1) : std::vector<ScoreEntry>{};
        for (int i = 0; i < 10; ++i) {
          int y = 177 + i * 24;
          menu_->serifText(41, y, std::to_string(i + 1), 0xffe5df58, 15);
          if (size_t(i) >= scores.size())
            continue;
          const auto &entry = scores[size_t(i)];
          menu_->serifText(80, y, clippedName(entry.name, 152, 15), 0xffe5df58, 15);
          if (scoreTab_ == 0)
            menu_->serifText(242, y,
                             entry.completed ? "Uitgereden"
                                             : "Etappe " + std::to_string(entry.reachedStage + 1),
                             0xffe5df58, 15);
          menu_->serifText(scoreTab_ == 0 ? 398 : 300, y, std::to_string(entry.money), 0xffe5df58,
                           15);
          menu_->serifText(488, y, timeText(entry.bestTime), 0xffe5df58, 15);
        }
        if (scores.empty())
          menu_->serifText(78, 183, "Rijd een etappe uit voor je eerste score.", 0xffe5df58, 17);
        addButton(MenuButton::Back, 220, 439, 0);
      } else if (page_ == Page::Car) {
        scene_->carPreview(settings_.car, animation_ * .4f);
        menu_->serifText(35, 280, cars()[size_t(settings_.car)].name, 0xffffff70, 24);
        addButton(MenuButton::Back, 0, 336, 0);
        addButton(MenuButton::Next, 0, 389, 1);
        addButton(MenuButton::OK, 365, 385, 2);
      } else if (page_ == Page::Stages) {
        for (int i = 0; i < 6; ++i) {
          int y = 150 + i * 38;
          menu_->serifText(35, y, stageName(i), selected_ == i ? 0xffffffff : 0xffe5df58, 15);
          hits_.push_back({{20, y - 2, 300, 32}, i});
        }
        menu_->routeCard(std::clamp(selected_, 0, 5));
        addButton(MenuButton::Back, 0, 410, 6);
      } else if (page_ == Page::Slots) {
        menu_->serifText(35, 125, "SLOT " + std::to_string(slot_ + 1), 0xffe5df58, 24);
        const auto filename = saveFile(slot_);
        std::error_code ec;
        menu_->serifText(35, 157, fs::exists(filename, ec) ? "Opgeslagen spel" : "Leeg slot",
                         0xffffffff, 18);
        const MenuButton buttons[]{MenuButton::Back,   MenuButton::Next,   MenuButton::OK,
                                   MenuButton::Select, MenuButton::Select, MenuButton::Back};
        const char *labels[]{"Vorig slot", "Volgend slot",           "Opslaan",
                             "Laden",      "Oud globaal slot laden", "Terug"};
        for (int i = 0; i < 6; ++i) {
          int y = 187 + i * 43;
          addButton(buttons[i], 0, y, i);
          menu_->serifText(240, y + 10, labels[i], 0xffe5df58, 17);
        }
      } else if (page_ == Page::Results) {
        bool won = race_->status() == RaceStatus::Finished;
        menu_->serifText(60, 150, won ? "Etappe voltooid!" : "Race voorbij", 0xffe5df58, 28);
        menu_->serifText(60, 200, won ? stageName(race_->stage()) : race_->failureReason(),
                         0xffffffff, 20);
        menu_->serifText(60, 240, "Plaats " + std::to_string(race_->position()) + " / 6",
                         0xffe5df58, 22);
        menu_->serifText(60, 276,
                         "Geld: " + std::to_string(race_->player.money) +
                             "   Tijd: " + timeText(race_->player.elapsed),
                         0xffe5df58, 20);
        addButton(MenuButton::Next, 0, 336, 0);
        addButton(MenuButton::Back, 0, 389, 1);
      } else if (page_ == Page::Help) {
        renderer_.dim(.3f);
        renderer_.rect(30, 115, 580, 300, 0xff253847);
        const char *lines[]{"PIJLEN / WASD: rijden (of je ingestelde toetsen)",
                            "SPATIE: handrem    F1-F3 / 1-3: camera",
                            "F4 / 4 / G: graphics    F5 / 5: radar",
                            "P / ENTER: pauze / verder    R: etappe opnieuw",
                            "F6 / 6: opslaan    F9 / 9: laden",
                            "M: geluid    F11: volledig scherm    ESC: hoofdmenu",
                            "C: auto kiezen    T: etappe kiezen    S: opslaan / laden",
                            "Controller: stick, RT/LT, A, X en schouderknoppen",
                            "Verzamel geld, benzine en tijd. Ontwijk de politie."};
        for (int i = 0; i < 9; ++i)
          menu_->serifText(45, 135 + i * 27, lines[i], i == 0 ? 0xffffff70 : 0xffffffff, 16, false);
        addButton(MenuButton::Back, 220, 439, 0);
      }
    }
    if (messageTime_ > 0 && page_ != Page::Error) {
      int y = page_ == Page::Playing ? 80 : 455;
      renderer_.rect(70, y, 500, 21, 0xff202735);
      renderer_.text(78, y + 7, message_, 0xffffd35a);
    }
  }
  void present() {
    const Settings &display = page_ == Page::Options ? draft_ : settings_;
    const int targetWidth =
        display.lowResolution && !renderer_.enhanced() ? 320 : renderer_.pixelWidth();
    const int targetHeight =
        display.lowResolution && !renderer_.enhanced() ? 200 : renderer_.pixelHeight();
    int width, height;
    SDL_QueryTexture(texture_, nullptr, nullptr, &width, &height);
    if (width != targetWidth || height != targetHeight) {
      SDL_Texture *next = SDL_CreateTexture(display_, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING, targetWidth, targetHeight);
      if (!next)
        throw std::runtime_error(SDL_GetError());
      SDL_SetTextureScaleMode(next,
                              renderer_.enhanced() ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
      SDL_DestroyTexture(texture_);
      texture_ = next;
    }
    const uint32_t *pixels = renderer_.pixels().data();
    if (targetWidth == 320) {
      lowPixels_.resize(320 * 200);
      for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320; ++x)
          lowPixels_[size_t(y) * 320 + x] =
              pixels[size_t(y * renderer_.pixelHeight() / 200) * renderer_.pixelWidth() +
                     x * renderer_.pixelWidth() / 320];
      pixels = lowPixels_.data();
    }
    SDL_UpdateTexture(texture_, nullptr, pixels, targetWidth * int(sizeof(uint32_t)));
    SDL_SetRenderDrawColor(display_, 0, 0, 0, 255);
    SDL_RenderClear(display_);
    int w, h;
    SDL_GetRendererOutputSize(display_, &w, &h);
    float scale = std::min(w / 640.f, h / 480.f);
    SDL_Rect dst{int((w - 640 * scale) / 2), int((h - 480 * scale) / 2), int(640 * scale),
                 int(480 * scale)};
    SDL_RenderCopy(display_, texture_, nullptr, &dst);
    SDL_RenderPresent(display_);
  }
  void readSettings() {
    std::ifstream file(saveDir_ / "settings.cfg");
    std::string key;
    int value;
    while (file >> key >> value) {
      if (key == "effects")
        settings_.effects = value != 0;
      if (key == "horizon")
        settings_.horizon = value != 0;
      if (key == "dashboard")
        settings_.dashboard = value != 0;
      if (key == "lowResolution")
        settings_.lowResolution = value != 0;
      if (key == "musicVolume")
        settings_.musicVolume = std::clamp(value, 0, 5);
      if (key == "effectsVolume")
        settings_.effectsVolume = std::clamp(value, 0, 5);
      for (int i = 0; i < 4; ++i)
        if (key == "key" + std::to_string(i) && value > 0 && value < SDL_NUM_SCANCODES)
          settings_.keys[size_t(i)] = SDL_Scancode(value);
      if (key == "music")
        settings_.music = value != 0;
      if (key == "mute")
        settings_.mute = value != 0;
      if (key == "radar")
        settings_.radar = value != 0;
      if (key == "enhanced")
        settings_.enhanced = value != 0;
      if (key == "camera")
        settings_.camera = std::clamp(value, 0, 2);
      if (key == "car")
        settings_.car = std::clamp(value, 0, 5);
      if (key == "unlocked")
        settings_.unlocked = std::clamp(value, 0, 5);
    }
  }
  void writeSettings() {
    if (saveDir_.empty() || options_.headless)
      return;
    std::ofstream f(saveDir_ / "settings.cfg");
    f << "music " << settings_.music << "\nmute " << settings_.mute << "\nradar " << settings_.radar
      << "\ncamera " << settings_.camera << "\ncar " << settings_.car << "\nunlocked "
      << settings_.unlocked << "\nenhanced " << settings_.enhanced << "\neffects "
      << settings_.effects << "\nhorizon " << settings_.horizon << "\ndashboard "
      << settings_.dashboard << "\nlowResolution " << settings_.lowResolution << "\nmusicVolume "
      << settings_.musicVolume << "\neffectsVolume " << settings_.effectsVolume << "\n";
    for (int i = 0; i < 4; ++i)
      f << "key" << i << " " << int(settings_.keys[size_t(i)]) << "\n";
  }
  void toggleGraphics() {
    if (page_ == Page::Options) {
      setDisplay(draft_.enhanced ? 2 : 0);
      notify(draft_.enhanced ? "GRAFISCH: VERBETERD" : "GRAFISCH: ORIGINEEL");
      return;
    }
    renderer_.setEnhanced(!settings_.enhanced);
    settings_.enhanced = renderer_.enhanced();
    settings_.lowResolution = false;
    notify(settings_.enhanced ? "GRAFISCH: VERBETERD (F4)" : "GRAFISCH: ORIGINEEL (F4)");
    writeSettings();
  }
  fs::path saveFile(int slot, bool legacy = false) const {
    if (profiles_ && profiles_->active() && !legacy)
      return profiles_->savePath(slot);
    return saveDir_ / ("slot" + std::to_string(slot) + ".sav");
  }
  void saveSlot(int slot) {
    if (!hasRace_) {
      message_ = "START EERST EEN RACE";
      messageTime_ = 3;
      return;
    }
    try {
      auto target = saveFile(slot), temp = target;
      fs::create_directories(target.parent_path());
      temp += ".tmp";
      {
        std::ofstream f(temp);
        if (!f)
          throw std::runtime_error("Cannot create save file");
        race_->save(f);
        f.flush();
        if (!f)
          throw std::runtime_error("Cannot write save file");
      }
      std::error_code ec;
      fs::rename(temp, target, ec);
      if (ec) {
        fs::copy_file(temp, target, fs::copy_options::overwrite_existing);
        fs::remove(temp);
      }
      message_ = "OPGESLAGEN IN SLOT " + std::to_string(slot + 1);
    } catch (const std::exception &e) {
      message_ = std::string("SAVE ERROR: ") + e.what();
    }
    messageTime_ = 3;
  }
  void loadSlot(int slot, bool legacy = false) {
    try {
      std::ifstream f(saveFile(slot, legacy));
      if (!f)
        throw std::runtime_error("Slot is empty");
      loading();
      race_->load(f);
      if (profiles_ && profile() &&
          (race_->status() == RaceStatus::Running || race_->status() == RaceStatus::Countdown)) {
        attempt_ = profiles_->beginAttempt(race_->stage());
        attemptActive_ = true;
      } else
        attemptActive_ = false;
      hasRace_ = true;
      page_ = race_->status() == RaceStatus::Finished || race_->status() == RaceStatus::Failed
                  ? Page::Results
                  : Page::Playing;
      paused_ = false;
      finishTime_ = 0;
      message_ = "SPEL GELADEN";
    } catch (const std::exception &e) {
      message_ = std::string("LOAD ERROR: ") + e.what();
    }
    messageTime_ = 3;
  }
};
} // namespace a2
int main(int argc, char **argv) {
  try {
    return a2::App(a2::parse(argc, argv)).run();
  } catch (const std::exception &e) {
    std::cerr << "OpenA2: " << e.what() << "\n";
    return 1;
  }
}
