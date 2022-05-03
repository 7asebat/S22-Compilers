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
	constexpr auto SOURCE_CODE_WINDOW_TITLE = "Source Code";
	constexpr auto SYMBOL_TABLE_WINDOW_TITLE = "Symbol Table";
	constexpr auto LOGS_WINDOW_TITLE = "Logs";

	constexpr ImGuiWindowFlags WINDOW_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

	inline static s22::UI_Program ui_program = {};
	inline static s22::UI_Symbol_Table ui_table = {};

	inline static void
	compiler_window()
	{
		s22_defer { ImGui::End(); };
		if (ImGui::Begin(COMPILER_WINDOW_TITLE, nullptr, WINDOW_FLAGS) == false)
			return;
		
		auto parser = s22::parser_instance();
		auto &source_code = parser->source_code;

		static char filepath[1024] = {};
		if (filepath[0] == '\0')
		{
			ImGui::Text("No file selected");
		}
		else
		{
			ImGui::Text("File: %s", filepath);
		}

		if (ImGui::Button("Browse"))
		{
			auto files = pfd::open_file("Open file").result();
			if (files.empty() == false)
			{
				strcpy_s(filepath, files[0].c_str());
				FILE *f = fopen(filepath, "r");
				s22_defer { fclose(f); };

				// Get file size
				fseek(f, 0, SEEK_END);
				size_t fsize = ftell(f);
				rewind(f);

				if (fsize + 2 > sizeof(source_code.buf))
				{
					parser_log(Error{"file too large; max file size is 8KB"});
				}
				else
				{
					memset(&source_code, 0, sizeof(source_code));
					fread(source_code.buf, fsize, 1, f);
					source_code.count = fsize;
				}
			}
		}

		static bool debug_enabled = false;
		
		if (ImGui::Button("Compile"))
		{

			auto lexer_buf = lexer_scan_buffer(source_code.buf, source_code.count + 2);
			if (lexer_buf == nullptr)
			{
				parser_log(Error{"lexer buffer is nullptr"});
				return;
			}
			s22_defer { lexer_delete_buffer(lexer_buf); };

			// Discard old data, run parser;
			yydebug = debug_enabled ? 1 : 0;
			parser->dispose();
			yyparse(parser);

			if (parser->has_errors == false)
			{
				ui_program = parser->program_write();
				ui_table = scope_get_ui_table(&parser->global);
			}
			else
			{
				ui_program = {};
				ui_table = {};
			}
		}
		ImGui::SameLine();
		ImGui::Checkbox("Debug", &debug_enabled);

		if (ui_program.empty())
			return;

		if (ImGui::BeginTable("Quadruples", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders))
		{
			s22_defer { ImGui::EndTable(); };

			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableSetupColumn("Label"); ImGui::TableSetupColumn("Instruction"); ImGui::TableSetupColumn("dst"); ImGui::TableSetupColumn("src1"); ImGui::TableSetupColumn("src2");
			ImGui::TableHeadersRow();

			for (const auto &instruction : ui_program)
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
	source_code_window()
	{
		s22_defer { ImGui::End(); };
		if (ImGui::Begin(SOURCE_CODE_WINDOW_TITLE, nullptr, WINDOW_FLAGS) == false)
			return;

		auto &source_code = parser_instance()->source_code;
		ImGui::InputTextMultiline(
			"##Source_Code",
			source_code.buf, sizeof(source_code.buf),
			ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_AllowTabInput,
			[](ImGuiInputTextCallbackData* data) -> int {
				auto scode = (Source_Code *)data->UserData;
				scode->count = data->BufTextLen;
				return 0;
			},
			(void*)&source_code
		);
	}

	inline static void
	symbol_table_build(UI_Symbol_Table &table, size_t depth)
	{
		for (size_t i = 0; i < table->size(); i++)
		{
			auto &row = table.table[i];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (depth == 0)
			{
				ImGui::Text("|");
			}
			else
			{
				ImGui::Text("|%s", std::string(depth, '>').c_str());
			}

			if (std::holds_alternative<UI_Symbol_Row>(row))
			{
				for (const auto &cell : std::get<UI_Symbol_Row>(row))
				{
					ImGui::TableNextColumn();
					ImGui::Text("%s", cell.c_str());
				}
			}
			else if (std::holds_alternative<const Scope*>(row))
			{
				auto label = std::format("expand##{}-{}", (void*)table.scope, i);

				if (ImGui::SameLine(); ImGui::SmallButton(label.c_str())) // Inner table
				{
					row = scope_get_ui_table(std::get<const Scope*>(row));
				}
			}
			else
			{
				auto label = std::format("collapse##{}-{}", (void*)table.scope, i);

				if (ImGui::SameLine(); ImGui::SmallButton(label.c_str())) // Inner table
				{
					auto &table = std::get<UI_Symbol_Table>(row);
					row = table.scope;
				}
				else
				{
					symbol_table_build(std::get<UI_Symbol_Table>(row), depth+1);
				}
			}
		}
	}

	inline static void
	symbol_table_window()
	{
		s22_defer { ImGui::End(); };
		if (ImGui::Begin(SYMBOL_TABLE_WINDOW_TITLE, nullptr, WINDOW_FLAGS) == false)
			return;

		if (ui_table->empty())
		{
			ImGui::Text("Nothing to show...");
			return;
		}

		if (ImGui::BeginTable("Symbol Table", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders))
		{
			s22_defer { ImGui::EndTable(); };

			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableSetupColumn("Scope"); ImGui::TableSetupColumn("Symbol ID"); ImGui::TableSetupColumn("Type"); ImGui::TableSetupColumn("Location"); ImGui::TableSetupColumn("Constant/Initialized/Used");
			ImGui::TableHeadersRow();

			symbol_table_build(ui_table, 0);
		}
	}

	inline static void
	logs_window()
	{
		s22_defer { ImGui::End(); };
		if (ImGui::Begin(LOGS_WINDOW_TITLE, nullptr, WINDOW_FLAGS) == false)
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

			auto dockspace_top_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.7f, nullptr, &dockspace_id);
			auto dockspace_top_right = ImGui::DockBuilderSplitNode(dockspace_top_left, ImGuiDir_Right, 0.5f, nullptr, &dockspace_top_left);

			auto dockspace_bot = dockspace_id;

			ImGui::DockBuilderDockWindow(SOURCE_CODE_WINDOW_TITLE, dockspace_top_left);
			ImGui::DockBuilderDockWindow(COMPILER_WINDOW_TITLE, dockspace_top_right);
			ImGui::DockBuilderDockWindow(SYMBOL_TABLE_WINDOW_TITLE, dockspace_top_right);
			ImGui::DockBuilderDockWindow(LOGS_WINDOW_TITLE, dockspace_bot);
			ImGui::DockBuilderFinish(dockspace_id);
		}

		source_code_window();
		compiler_window();
		symbol_table_window();
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