#pragma once

struct ImFont;

namespace s22
{
	int
	window_init();

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

	// Return false to exit
	using Frame_Proc = bool (*)();

	int
	window_run(Frame_Proc frame);
}