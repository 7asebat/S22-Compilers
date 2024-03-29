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
	constexpr auto SOURCE_CODE_WINDOW_TITLE = "Source Code";
	constexpr auto QUADRUPLES_WINDOW_TITLE = "Quadruples";
	constexpr auto SYMBOL_TABLE_WINDOW_TITLE = "Symbol Table";
	constexpr auto LOGS_WINDOW_TITLE = "Logs";

	constexpr ImGuiWindowFlags WINDOW_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

	inline static void
	source_code_window()
	{
		s22_defer { ImGui::End(); };
		if (ImGui::Begin(SOURCE_CODE_WINDOW_TITLE, nullptr, WINDOW_FLAGS) == false)
			return;

		auto parser = s22::parser_instance();
		auto &source_code = parser->ui_source_code;

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
				auto f = fopen(filepath, "r");
				s22_defer { fclose(f); };

				// Get file size
				fseek(f, 0, SEEK_END);
				auto fsize = ftell(f);
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
		if (ImGui::SameLine(); ImGui::Button("Compile"))
		{
			auto lexer_buf = lexer_scan_buffer(source_code.buf, source_code.count + 2);
			if (lexer_buf == nullptr)
			{
				parser_log(Error{"lexer buffer is nullptr"});
				return;
			}
			s22_defer
			{
				lexer_delete_buffer(lexer_buf);
			};

			// Discard old data, run parser
			yydebug = debug_enabled ? 1 : 0;
			parser->dispose();
			yyparse(parser);

			if (parser->has_errors == false)
			{
				parser->ui_program = parser->program_write();
				parser->ui_table = scope_get_ui_table(&parser->global);
			}
			else
			{
				parser->ui_program.clear();
				parser->ui_table.rows.clear();
			}
		}
		ImGui::SameLine(); ImGui::Checkbox("Debug", &debug_enabled);

		ImGui::InputTextMultiline(
			"##Source_Code",
			source_code.buf, sizeof(source_code.buf),
			ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_AllowTabInput,
			[](ImGuiInputTextCallbackData* data) -> int {
				auto scode = (UI_Source_Code *)data->UserData;
				scode->count = data->BufTextLen;
				return 0;
			},
			(void*)&source_code
		);
	}

	inline static void
	quadruples_window()
	{
		s22_defer { ImGui::End(); };
		if (ImGui::Begin(QUADRUPLES_WINDOW_TITLE, nullptr, WINDOW_FLAGS) == false)
			return;
		
		auto &ui_program = parser_instance()->ui_program;
		if (ui_program.empty())
		{
			ImGui::Text("Nothing to show...");
		}
		else if (ImGui::BeginTable("Quadruples", 5, TABLE_FLAGS))
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
	symbol_table_build(UI_Symbol_Table &table, size_t depth)
	{
		for (size_t i = 0; i < table.rows.size(); i++)
		{
			auto &row = table.rows[i];
			ImGui::TableNextRow();

			if (std::holds_alternative<UI_Symbol_Row>(row))
			{
				for (bool first_cell = true; const auto &cell : std::get<UI_Symbol_Row>(row))
				{
					ImGui::TableNextColumn();
					if (first_cell)
					{
						ImGui::TreeNodeEx(cell.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
						first_cell = false;
					}
					else
					{
						ImGui::Text("%s", cell.c_str());
					}
				}
			}
			else
			{
				auto label = std::format("scope##{}_{}", depth, i);
				ImGui::TableNextColumn();
				if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanFullWidth))
				{
					if (std::holds_alternative<const Scope *>(row))
					{
						auto &scope = std::get<const Scope *>(row);
						row = scope_get_ui_table(scope); // scope ptr -> table
					}
					
					symbol_table_build(std::get<UI_Symbol_Table>(row), depth + 1);
					ImGui::TreePop();
				}
				else
				{
					if (std::holds_alternative<const Scope *>(row) == false)
					{
						auto &table = std::get<UI_Symbol_Table>(row);
						row = table.scope; // table -> scope ptr
					}
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

		auto &ui_table = parser_instance()->ui_table;
		if (ui_table.rows.empty())
		{
			ImGui::Text("Nothing to show...");
		}
		else if (ImGui::BeginTable("Symbol Table", 4, TABLE_FLAGS))
		{
			s22_defer { ImGui::EndTable(); };

			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableSetupColumn("Symbol ID"); ImGui::TableSetupColumn("Type"); ImGui::TableSetupColumn("Location"); ImGui::TableSetupColumn("Constant/Initialized/Used");
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

		auto parser = s22::parser_instance();

		const float footer_height_to_reserve = ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("Logs", ImVec2{0.f, -footer_height_to_reserve}))
		{
			ImGuiListClipper clipper = {(int)parser->ui_logs.size()};
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					ImGui::Text("%s", parser->ui_logs[i].c_str());
			}

			static size_t last_count = 0;
			if (last_count != parser->ui_logs.size())
			{
				ImGui::SetScrollHereY(1.f);
				last_count = parser->ui_logs.size();
			}
		}
		ImGui::EndChild();

		if (ImGui::Button("Clear"))
			parser->ui_logs.clear();
	}
}

int
main(int, char**)
{
	using namespace s22;

	s22::window_run(
		[] { ImGui::GetStyle().CellPadding = ImVec2{4.f, 8.f}; },
		[] {
		auto dockspace_id = ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

		static bool first_dock = true;
		if (first_dock)
		{
			ImGui::DockBuilderRemoveNodeChildNodes(dockspace_id);

			ImGuiID top_left, top_right, bot;
			ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.7f, &top_left, &bot);
			ImGui::DockBuilderSplitNode(top_left, ImGuiDir_Left, 0.5f, &top_left, &top_right);

			ImGui::DockBuilderDockWindow(SOURCE_CODE_WINDOW_TITLE, top_left);
			ImGui::DockBuilderDockWindow(QUADRUPLES_WINDOW_TITLE, top_right);
			ImGui::DockBuilderDockWindow(SYMBOL_TABLE_WINDOW_TITLE, top_right);
			ImGui::DockBuilderDockWindow(LOGS_WINDOW_TITLE, bot);
		
			ImGui::DockBuilderFinish(dockspace_id);
			first_dock = false;
		}

		source_code_window();
		quadruples_window();
		symbol_table_window();
		logs_window();

		return true;
	});
	return 0;
}