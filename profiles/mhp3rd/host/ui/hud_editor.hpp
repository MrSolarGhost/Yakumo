#pragma once

namespace mhp3rd::ui {

void open_hud_editor();
[[nodiscard]] bool hud_editor_open();
// Direct pointer/touch dragging, or Move Block with keyboard/gamepad confirmation.
void hud_editor_frame(bool back);

} // namespace mhp3rd::ui