#include "compiler/Parser.h"
#include "compiler/Window.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <portable-file-dialogs.h>

extern FILE *yyin;
extern int yydebug;

int
yyparse(s22::Parser *);

namespace s22
{
	constexpr auto COMPILER_WINDOW_TITLE = "Compiler";
	constexpr auto LOGS_WINDOW_TITLE = "Logs";

	inline static void
	compiler_window()
	{
		s22_defer { ImGui::End(); };
		if (ImGui::Begin(COMPILER_WINDOW_TITLE) == false)
			return;

		static char filepath[1024] = {};
		ImGui::Text("%s", filepath);

		if (ImGui::Button("Browse"))
		{
			auto files = pfd::open_file("Browse me").result();
			if (files.empty() == false)
				strcpy_s(filepath, files[0].c_str());
		}

		static bool debug_enabled = false;
		ImGui::SameLine(); ImGui::Checkbox("Debug", &debug_enabled);

		static s22::Program program = {};
		if (ImGui::Button("Compile"))
		{
			auto parser = s22::parser_instance();
			parser->dispose();
		
			// Input stream, open first argument as a file
			fopen_s(&yyin, filepath, "r");
			if (yyin == nullptr)
				return;
			lexer_flush_buffer();

			yydebug = debug_enabled ? 1 : 0;

			// Run parser
			yyparse(parser);

			program = parser->program_write();

			fclose(yyin);
		}

		if (program.empty())
			return;

		if (ImGui::BeginTable("Quadruples", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders))
		{
			s22_defer { ImGui::EndTable(); };

			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableSetupColumn("Label"); ImGui::TableSetupColumn("Instruction"); ImGui::TableSetupColumn("dst"); ImGui::TableSetupColumn("src1"); ImGui::TableSetupColumn("src2");
			ImGui::TableHeadersRow();

			for (const auto &instruction : program)
			{
				for (const auto &part : instruction)
				{
					ImGui::TableNextColumn();
					ImGui::Text("%s", part.c_str());
				}
			}
		}
	}

	inline static void
	logs_window()
	{
		s22_defer { ImGui::End(); };
		if (ImGui::Begin(LOGS_WINDOW_TITLE) == false)
			return;

		ImGui::PushFont(IMGUI_FONT_MONO);
		s22_defer { ImGui::PopFont(); };

		auto parser = s22::parser_instance();

		const float footer_height_to_reserve = ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("Logs", ImVec2{0.f, -footer_height_to_reserve}))
		{
			ImGuiListClipper clipper = {(int)parser->logs.size()};
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					ImGui::Text("%s", parser->logs[i].c_str());
			}

			static size_t last_count = 0;
			if (last_count != parser->logs.size())
			{
				ImGui::SetScrollHereY(1.f);
				last_count = parser->logs.size();
			}
		}
		ImGui::EndChild();

		if (ImGui::Button("Clear"))
			parser->logs.clear();
	}

	inline static bool
	frame()
	{
		static bool first_dock = true;
		if (first_dock)
		{
			first_dock = false;
			auto dockspace_id = ImGui::GetID(IMGUI_DOCKSPACE_ID);

			ImGui::DockBuilderRemoveNode(dockspace_id);
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

			auto dockspace_top = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.7f, nullptr, &dockspace_id);
			auto dockspace_bot = dockspace_id;

			ImGui::DockBuilderDockWindow(COMPILER_WINDOW_TITLE, dockspace_top);
			ImGui::DockBuilderDockWindow(LOGS_WINDOW_TITLE, dockspace_bot);
			ImGui::DockBuilderFinish(dockspace_id);
		}

		compiler_window();
		logs_window();

		return true;
	}
}

int
main(int, char**)
{
	s22::window_run(s22::frame);
	return 0;
}