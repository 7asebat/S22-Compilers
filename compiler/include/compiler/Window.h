#pragma once

struct ImFont;

namespace s22
{
	// Return 0 on success
	int
	window_init();

	// Poll events, return false if app should exit
	bool
	window_poll();

	void
	window_frame_start();

	void
	window_frame_render();

	void
	window_dispose();

	constexpr auto IMGUI_DOCKSPACE_ID = "DOCKSPACE";
	inline ImFont *IMGUI_FONT_SANS = nullptr;
	inline ImFont *IMGUI_FONT_MONO = nullptr;

	// Return false if app should exit
	using Frame_Proc = bool (*)();

	// Uses all functions above to run an event loop
	// Executes the given frame function until it returns false
	int
	window_run(Frame_Proc frame);
}