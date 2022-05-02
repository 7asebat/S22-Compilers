#pragma once

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

	// Return false to exit
	typedef bool (*Frame_Proc)();

	int
	window_run(Frame_Proc frame);
}