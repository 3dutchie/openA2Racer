#pragma once
#include "assets.hpp"
#include "renderer.hpp"
#include <memory>
#include <string_view>

namespace a2 {
enum class MenuPage { Main, Players, Controls, Options, Scores, Car };
enum class MenuButton {
  Start,
  Controls,
  Options,
  Scores,
  Restart,
  Players,
  OK,
  Cancel,
  Select,
  Delete,
  Back,
  Configure,
  Exit,
  Help,
  Yes,
  No,
  Next,
  DialogOK,
  DialogCancel
};
struct MenuRect {
  int x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};
// Original menu artwork is loaded from the player's CD. Dynamic text uses a
// bundled OFL serif font matching the supplied screenshots, not Windows GDI.
class MenuArt {
public:
  MenuArt(Renderer &, Assets &);
  ~MenuArt();
  void background(MenuPage);
  MenuRect button(MenuButton, int x, int y, bool selected = false, float animation = 0,
                  bool pressed = false);
  static MenuRect buttonRect(MenuButton, int x, int y);
  void topButtons(bool exitSelected = false, bool helpSelected = false);
  void routeCard(int stage, int x = 348, int y = 247);
  void serifText(int x, int y, std::string_view text, uint32_t color = 0xffffff00, int size = 18,
                 bool shadow = true);
  int textWidth(std::string_view text, int size = 18) const;
  void playerDialog(bool twoButtons = true);
  void controlPanel();
  void optionsPanel(int tab = 0);
  void scoreTabs(int selected = 0);
  void profileHighlight(int row);
  void radio(int x, int y, bool selected);
  // Coordinates are relative to the 203x128 volume block at (0,115).
  // Draw the music row with (0,115), effects row with (0,185), levels0..5.
  void volume(int x, int y, int level);
  void loading();
  void sprite(size_t index, int x, int y);
  bool original() const { return original_; }

private:
  Renderer &renderer_;
  Assets &assets_;
  MenuPage page_ = MenuPage::Main;
  bool original_ = false;
  struct Font;
  std::unique_ptr<Font> font_;
  const std::vector<Sprite> &sprites();
};
} // namespace a2
