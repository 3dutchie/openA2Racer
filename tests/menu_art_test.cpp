#include "menu_art.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
void word(Bytes &b, uint16_t n) {
  b.push_back(uint8_t(n));
  b.push_back(uint8_t(n >> 8));
}
void dword(Bytes &b, uint32_t n) {
  for (int i = 0; i < 4; ++i)
    b.push_back(uint8_t(n >> (i * 8)));
}
void write(const std::filesystem::path &path, const Bytes &b) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
  check(bool(out), "Cannot write menu fixture");
}
uint32_t color(int sprite) {
  int index = sprite % 253 + 1;
  return 0xff000000u | uint32_t(index << 16) | uint32_t((255 - index) << 8) | 64;
}
// All pixels are generated here. Even the complete sprite table is procedural;
// its numbered colors let checks distinguish selected, idle and pressed states.
struct Fixture {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("a2-menu-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Fixture(int count) {
    std::filesystem::create_directory(path);
    Bytes pcx(128);
    pcx[0] = 10;
    pcx[1] = 5;
    pcx[2] = 1;
    pcx[3] = 8;
    pcx[65] = 1;
    pcx[8] = 127;
    pcx[9] = 2;
    pcx[10] = 223;
    pcx[11] = 1;
    pcx[66] = 128;
    pcx[67] = 2;
    pcx.resize(128 + 640 * 480, 0);
    pcx.push_back(12);
    for (int i = 0; i < 256; ++i) {
      pcx.push_back(i == 0 ? 0 : uint8_t(i));
      pcx.push_back(i == 0 ? 0 : uint8_t(255 - i));
      pcx.push_back(i == 0 ? 0 : 64);
    }
    Bytes named, blob;
    word(named, 5);
    for (const auto *name :
         {"hoofd.pcx", "OPTIES.PCX", "Bedien.pcx", "Nspel_1.pcx", "Tstand.pcx"}) {
      size_t start = named.size();
      named.resize(start + 20, 0);
      std::copy(name, name + std::char_traits<char>::length(name), named.begin() + start);
      dword(named, uint32_t(blob.size()));
      blob.insert(blob.end(), pcx.begin(), pcx.end());
    }
    for (const auto *stem : {"RCS", "A2MRCS"}) {
      write(path / (std::string(stem) + ".IND"), named);
      write(path / (std::string(stem) + ".IMG"), blob);
    }
    Bytes index, sprites;
    word(index, uint16_t(count));
    word(sprites, uint16_t(count));
    for (int i = 0; i < count; ++i) {
      dword(index, uint32_t(sprites.size()));
      word(sprites, 3);
      word(sprites, 1);
      // Deliberately nonzero hotspots: menus use top-left source coordinates.
      word(sprites, 1);
      word(sprites, 1);
      sprites.push_back(uint8_t(i % 253 + 1));
      sprites.push_back(0);
      sprites.push_back(255);
    }
    write(path / "A2MRC.IND", index);
    write(path / "A2MRC.IMG", sprites);
    write(path / "ETAPPE1.TRK", {});
  }
  ~Fixture() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};
uint32_t pixel(const Renderer &renderer, int x, int y) {
  return renderer.pixels().at(size_t(y) * 640 + x);
}
void fallback() {
  Fixture fixture(2);
  GameData data(fixture.path);
  Assets assets(data);
  Renderer renderer;
  MenuArt art(renderer, assets);
  check(!art.original(), "Incomplete public fixtures should select fallback artwork");
  for (auto page : {MenuPage::Main, MenuPage::Players, MenuPage::Controls, MenuPage::Options,
                    MenuPage::Scores, MenuPage::Car}) {
    art.background(page);
    art.topButtons();
    art.controlPanel();
    art.optionsPanel();
    art.scoreTabs();
    art.routeCard(0);
  }
  art.loading();
  art.playerDialog();
  art.button(MenuButton::DialogOK, 194, 303);
  check(pixel(renderer, 200, 310) != 0xff000000, "Fallback dialog button is missing");
  check(art.textWidth("WWW") > art.textWidth("III"), "Menu text should use proportional glyphs");
  check(art.textWidth("J\xc3\xa9r\xc3\xb4me") > 0, "UTF-8 profile names must be measured");
  check(art.textWidth("A\nWWW") == art.textWidth("WWW"), "Multiline text width is incorrect");
  check(art.textWidth(std::string_view("\xff\xc0\xe0", 3)) > 0,
        "Malformed UTF-8 needs safe replacement");
  renderer.clear(0xff123456);
  art.serifText(10, 10, "J\xc3\xa9r\xc3\xb4me", 0xffffff00, 18);
  int foreground = 0, shadow = 0;
  for (auto pixel : renderer.pixels()) {
    foreground += pixel == 0xffffff00;
    shadow += pixel == 0xff101010;
  }
  check(foreground > 100 && shadow > 20, "Serif text and its shadow must be visible");
  auto bounds = MenuArt::buttonRect(MenuButton::Start, 0, 124);
  check(bounds.contains(0, 124) && bounds.contains(217, 168) && !bounds.contains(218, 168) &&
            !bounds.contains(217, 169),
        "Menu hit regions must use exclusive right/bottom edges");
}
void composition() {
  Fixture fixture(329);
  GameData data(fixture.path);
  Assets assets(data);
  Renderer renderer;
  MenuArt art(renderer, assets);
  check(art.original(), "Complete menu sprite archive was not detected");
  renderer.clear(0xff123456);
  art.button(MenuButton::Start, 10, 20);
  check(pixel(renderer, 10, 20) == color(156), "Idle Start sprite is incorrect");
  check(pixel(renderer, 11, 20) == 0xff000000, "Black button pixels must remain opaque");
  check(pixel(renderer, 12, 20) == 0xff123456, "Menu palette key 255 must be transparent");
  art.button(MenuButton::Start, 10, 20, true);
  check(pixel(renderer, 10, 20) == color(140), "Highlighted Start sprite is incorrect");
  art.button(MenuButton::Start, 10, 20, true, .05f);
  check(pixel(renderer, 10, 20) == color(141), "Highlighted button animation must advance");
  art.button(MenuButton::Start, 10, 20, true, 0, true);
  check(pixel(renderer, 10, 20) == color(157), "Pressed Start sprite is incorrect");
  art.profileHighlight(9);
  check(pixel(renderer, 60, 365) == color(266), "Tenth profile highlight has the wrong row");
  art.routeCard(5);
  check(pixel(renderer, 348, 247) == color(5), "Sixth stage route card is missing");
  art.playerDialog();
  check(pixel(renderer, 151, 94) == color(281), "Original two-button dialog is missing");
  art.radio(330, 164, true);
  check(pixel(renderer, 330, 164) == color(300), "Active original radio sprite is incorrect");
  art.volume(0, 115, 4);
  check(pixel(renderer, 45, 151) == color(319) && pixel(renderer, 149, 151) == color(322),
        "Volume lamps must reflect the selected level");
}
void original(const std::filesystem::path &path) {
  GameData data(path);
  Assets assets(data);
  Renderer renderer;
  MenuArt art(renderer, assets);
  check(art.original(), "Original CD does not expose the 329-sprite menu archive");
  art.background(MenuPage::Main);
  check(renderer.pixels() == assets.image("hoofd.pcx").pixels, "Main menu PCX must remain intact");
  art.button(MenuButton::Start, 0, 124);
  auto idle = renderer.pixels();
  art.button(MenuButton::Start, 0, 124, true);
  check(renderer.pixels() != idle, "Original menu selection must change the button artwork");
  art.background(MenuPage::Options);
  art.optionsPanel();
  art.volume(0, 115, 4);
  int green = 0;
  for (int y = 151; y < 164; ++y)
    for (int x = 45; x < 64; ++x) {
      auto color = pixel(renderer, x, y);
      green += ((color >> 8) & 255) > ((color >> 16) & 255) + 80;
    }
  check(green > 30, "Volume lamps must use the OPTIES palette, not gray main-menu colors");
  art.loading();
  check(renderer.pixels() == assets.image("hi_wait.pcx").pixels,
        "Loading must display the full original Even wachten image");
  std::cout << "Original CD menu artwork integration passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    fallback();
    composition();
    if (argc > 1)
      original(argv[1]);
    std::cout
        << "Menu artwork checks passed: states, transparency, serif text, fallback, loading\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Menu artwork test failed: " << e.what() << '\n';
    return 1;
  }
}
