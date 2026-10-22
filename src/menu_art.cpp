#include "menu_art.hpp"
#include "../third_party/liberation/font_data.h"
#include <map>
#include <stdexcept>
#include <tuple>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb/stb_truetype.h"

namespace a2 {
namespace {
constexpr uint32_t yellow = 0xffffff00;
std::vector<int> codepoints(std::string_view text) {
  std::vector<int> result;
  for (size_t i = 0; i < text.size();) {
    auto first = static_cast<unsigned char>(text[i++]);
    if (first < 128) {
      result.push_back(first);
      continue;
    }
    int count = first >= 0xf0 && first <= 0xf4   ? 3
                : first >= 0xe0 && first <= 0xef ? 2
                : first >= 0xc2 && first <= 0xdf ? 1
                                                 : -1;
    if (count < 0 || i + size_t(count) > text.size()) {
      result.push_back('?');
      continue;
    }
    int value = first & ((1 << (6 - count)) - 1);
    size_t begin = i;
    for (int n = 0; n < count; ++n) {
      auto next = static_cast<unsigned char>(text[i]);
      if ((next & 0xc0) != 0x80)
        break;
      value = (value << 6) | (next & 63);
      ++i;
    }
    if (i - begin != size_t(count) || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ||
        (count == 1 && value < 128) || (count == 2 && value < 2048) ||
        (count == 3 && value < 65536))
      result.push_back('?');
    else
      result.push_back(value);
  }
  return result;
}
const char *backgroundName(MenuPage page) {
  switch (page) {
  case MenuPage::Players:
    return "Nspel_1.pcx";
  case MenuPage::Controls:
    return "Bedien.pcx";
  case MenuPage::Options:
    return "OPTIES.PCX";
  case MenuPage::Scores:
    return "Score.pcx";
  default:
    return "hoofd.pcx";
  }
}
struct ButtonArt {
  size_t selected, idle, pressed;
  int frames;
  const char *label;
};
ButtonArt art(MenuButton button) {
  switch (button) {
  case MenuButton::Start:
    return {140, 156, 157, 16, "START"};
  case MenuButton::Controls:
    return {9, 25, 26, 16, "BEDIENING"};
  case MenuButton::Options:
    return {65, 81, 82, 16, "OPTIES"};
  case MenuButton::Scores:
    return {90, 106, 107, 16, "SCORE"};
  case MenuButton::Restart:
    return {40, 56, 57, 16, "OPNIEUW"};
  case MenuButton::Players:
    return {115, 131, 132, 16, "NIEUWE SPELER"};
  case MenuButton::OK:
    return {196, 211, 212, 15, "OK"};
  case MenuButton::Cancel:
    return {165, 181, 182, 16, "ANNULEREN"};
  case MenuButton::Select:
    return {251, 252, 253, 1, "SELECTEREN"};
  case MenuButton::Delete:
    return {254, 255, 256, 1, "VERWIJDEREN"};
  case MenuButton::Back:
    return {237, 238, 239, 1, "TERUG"};
  case MenuButton::Configure:
    return {193, 194, 195, 1, "INSTELLEN"};
  case MenuButton::Exit:
    return {34, 35, 36, 1, "EINDE"};
  case MenuButton::Help:
    return {37, 38, 39, 1, "HELP"};
  case MenuButton::Yes:
    return {268, 269, 270, 1, "JA"};
  case MenuButton::No:
    return {271, 272, 273, 1, "NEE"};
  case MenuButton::Next:
    return {327, 328, 328, 1, "VERDER"};
  case MenuButton::DialogOK:
    return {277, 278, 279, 1, "OK"};
  case MenuButton::DialogCancel:
    return {274, 275, 276, 1, "ANNULEREN"};
  }
  return {0, 0, 0, 1, ""};
}
} // namespace

struct MenuArt::Font {
  stbtt_fontinfo info{};
  float capHeight = 1;
  struct Glyph {
    Image image;
    int xOffset = 0, yOffset = 0;
  };
  std::map<std::tuple<int, int, uint32_t>, Glyph> glyphs;
  Font() {
    if (!stbtt_InitFont(&info, menu_font_data::bytes, 0))
      throw std::runtime_error("Invalid bundled menu font");
    int x0, y0, x1, y1;
    stbtt_GetCodepointBox(&info, 'H', &x0, &y0, &x1, &y1);
    capHeight = float(std::max(1, y1 - y0));
  }
  float scale(int size) const { return float(std::clamp(size, 8, 32)) / capHeight; }
  int supported(int cp) const { return stbtt_FindGlyphIndex(&info, cp) ? cp : '?'; }
  const Glyph &glyph(int cp, int size, uint32_t color) {
    cp = supported(cp);
    size = std::clamp(size, 8, 32);
    auto key = std::make_tuple(cp, size, color);
    auto it = glyphs.find(key);
    if (it != glyphs.end())
      return it->second;
    Glyph glyph;
    int width, height;
    auto *bitmap = stbtt_GetCodepointBitmap(&info, scale(size), scale(size), cp, &width, &height,
                                            &glyph.xOffset, &glyph.yOffset);
    glyph.image.width = width;
    glyph.image.height = height;
    glyph.image.indices.assign(size_t(width) * height, 255);
    glyph.image.pixels.assign(size_t(width) * height, color);
    for (size_t p = 0; p < glyph.image.indices.size(); ++p)
      if (bitmap[p] >= 96)
        glyph.image.indices[p] = 0;
    stbtt_FreeBitmap(bitmap, nullptr);
    return glyphs.emplace(key, std::move(glyph)).first->second;
  }
};

MenuArt::MenuArt(Renderer &renderer, Assets &assets)
    : renderer_(renderer), assets_(assets), font_(std::make_unique<Font>()) {
  original_ = assets_.sprites("A2MRC", "hoofd.pcx").size() >= 329;
}
MenuArt::~MenuArt() = default;
const std::vector<Sprite> &MenuArt::sprites() {
  auto name = backgroundName(page_);
  return assets_.sprites("A2MRC", assets_.has(name) ? name : "hoofd.pcx");
}
void MenuArt::background(MenuPage page) {
  page_ = page;
  auto name = backgroundName(page);
  if (!assets_.has(name))
    name = "hoofd.pcx";
  renderer_.clear(0xff222b3b);
  if (assets_.has(name))
    renderer_.blit(assets_.image(name), 0, 0, 640, 480);
}
void MenuArt::sprite(size_t index, int x, int y) {
  if (!original_ || index >= sprites().size())
    return;
  renderer_.blitKey(sprites()[index].image, x, y, 255);
}
MenuRect MenuArt::buttonRect(MenuButton button, int x, int y) {
  switch (button) {
  case MenuButton::Exit:
  case MenuButton::Help:
    return {x, y, 103, 45};
  case MenuButton::Select:
    return {x, y, 134, 37};
  case MenuButton::Delete:
    return {x, y, 136, 37};
  case MenuButton::Back:
    return {x, y, 205, 43};
  case MenuButton::Configure:
    return {x, y, 206, 44};
  case MenuButton::Next:
    return {x, y, 207, 45};
  case MenuButton::Yes:
  case MenuButton::No:
  case MenuButton::DialogOK:
  case MenuButton::DialogCancel:
    return {x, y, 99, 35};
  default:
    return {x, y, 218, 45};
  }
}
MenuRect MenuArt::button(MenuButton button, int x, int y, bool selected, float animation,
                         bool pressed) {
  auto data = art(button);
  auto rect = buttonRect(button, x, y);
  if (original_) {
    int frame = 0;
    if (std::isfinite(animation)) {
      float cycle = std::fmod(std::max(0.f, animation), float(data.frames) / 20);
      frame = std::min(data.frames - 1, int(cycle * 20));
    }
    sprite(pressed ? data.pressed : selected ? data.selected + size_t(frame) : data.idle, x, y);
  } else {
    renderer_.rect(x, y, rect.w, rect.h, selected ? 0xffb48c20 : 0xff7d426b);
    renderer_.rect(x, y, rect.w, 1, 0xffe9d3ee);
    serifText(x + 10, y + 12, data.label, 0xffffffff, 14);
  }
  return rect;
}
void MenuArt::topButtons(bool exitSelected, bool helpSelected) {
  if (page_ == MenuPage::Main || page_ == MenuPage::Players)
    button(MenuButton::Exit, 0, 18, exitSelected);
  button(MenuButton::Help, 535, 18, helpSelected);
}
void MenuArt::routeCard(int stage, int x, int y) {
  if (original_)
    sprite(size_t(std::clamp(stage, 0, 5)), x, y);
  else
    serifText(x, y, "ETAPPE " + std::to_string(std::clamp(stage, 0, 5) + 1), yellow, 20);
}
void MenuArt::serifText(int x, int y, std::string_view text, uint32_t color, int size,
                        bool shadow) {
  size = std::clamp(size, 8, 32);
  int origin = x;
  float cursor = float(x), scale = font_->scale(size);
  auto values = codepoints(text);
  for (size_t i = 0; i < values.size(); ++i) {
    int cp = values[i];
    if (cp == '\n') {
      cursor = float(origin);
      y += size + 6;
      continue;
    }
    if (cp == '\r')
      continue;
    cp = font_->supported(cp);
    auto draw = [&](uint32_t tint, int offset) {
      const auto &glyph = font_->glyph(cp, size, tint);
      if (!glyph.image.pixels.empty())
        renderer_.blitKey(glyph.image, int(std::round(cursor)) + glyph.xOffset + offset,
                          y + size + glyph.yOffset + offset, 255);
    };
    if (shadow)
      draw(0xff101010, 1);
    draw(color, 0);
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&font_->info, cp, &advance, &bearing);
    cursor += advance * scale;
    if (i + 1 < values.size() && values[i + 1] != '\n')
      cursor +=
          stbtt_GetCodepointKernAdvance(&font_->info, cp, font_->supported(values[i + 1])) * scale;
  }
}
int MenuArt::textWidth(std::string_view text, int size) const {
  float cursor = 0, maximum = 0, scale = font_->scale(size);
  auto values = codepoints(text);
  for (size_t i = 0; i < values.size(); ++i) {
    int cp = values[i];
    if (cp == '\n') {
      maximum = std::max(maximum, cursor);
      cursor = 0;
      continue;
    }
    if (cp == '\r')
      continue;
    cp = font_->supported(cp);
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&font_->info, cp, &advance, &bearing);
    cursor += advance * scale;
    if (i + 1 < values.size() && values[i + 1] != '\n')
      cursor +=
          stbtt_GetCodepointKernAdvance(&font_->info, cp, font_->supported(values[i + 1])) * scale;
  }
  return int(std::ceil(std::max(maximum, cursor)));
}
void MenuArt::playerDialog(bool twoButtons) {
  renderer_.dim(.45f);
  if (original_)
    sprite(twoButtons ? 281 : 280, 151, 94);
  else
    renderer_.rect(151, 94, 338, 276, 0xff927b9f);
}
void MenuArt::controlPanel() {
  sprite(192, 304, 122);
  button(MenuButton::Configure, 365, 385);
}
void MenuArt::optionsPanel(int tab) {
  tab = std::clamp(tab, 0, 2);
  sprite(size_t(296 + tab), 0, 115);
  sprite(318, 0, 115);
  sprite(309, 256, 164);
  sprite(310, 351, 164);
  sprite(311, 351, 184);
  sprite(301, 267, 247);
  sprite(302, 351, 247);
  sprite(303, 351, 267);
  sprite(304, 351, 287);
  sprite(305, 351, 307);
  sprite(312, 231, 359);
  sprite(310, 351, 359);
  sprite(311, 351, 379);
}
void MenuArt::scoreTabs(int selected) {
  selected = std::clamp(selected, 0, 6);
  sprite(size_t(240 + selected), 0, 110);
  sprite(selected == 0 ? 247 : 248, 0, 150);
}
void MenuArt::profileHighlight(int row) {
  if (row < 0 || row >= 10)
    return;
  if (original_)
    sprite(266, 60, 140 + 25 * row);
  else
    renderer_.rect(60, 140 + 25 * row, 505, 24, 0xffd90a16);
}
void MenuArt::radio(int x, int y, bool selected) {
  if (original_)
    sprite(selected ? 300 : 299, x, y);
  else {
    renderer_.rect(x + 2, y + 2, 11, 9, 0xff1a2430);
    if (selected)
      renderer_.rect(x + 4, y + 4, 7, 5, 0xffeeeeee);
  }
}
void MenuArt::volume(int x, int y, int level) {
  level = std::clamp(level, 0, 5);
  for (int i = 0; i < 5; ++i) {
    if (original_)
      sprite(size_t(i == 4 ? (i < level ? 321 : 322) : (i < level ? 319 : 320)), x + 45 + 26 * i,
             y + 36);
    else
      renderer_.rect(x + 45 + 26 * i, y + 36, 19, 13, i < level ? 0xff29ce2a : 0xff303030);
  }
}
void MenuArt::loading() {
  if (assets_.has("hi_wait.pcx"))
    renderer_.blit(assets_.image("hi_wait.pcx"), 0, 0, 640, 480);
  else {
    background(MenuPage::Main);
    renderer_.rect(110, 180, 420, 80, 0xff26354b);
    serifText(145, 204, "EVEN WACHTEN...", yellow, 30);
  }
}
} // namespace a2
