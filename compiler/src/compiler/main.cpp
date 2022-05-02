#include "compiler/Parser.h"
#include "compiler/Window.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

extern FILE *yyin;
extern int yydebug;

int
yyparse(s22::Parser *);

int
main(int argc, char *argv[])
{
	s22::window_run([]() -> bool
	{
		static char filepath[1024] = {};
		if (ImGui::Button("Browse"))
		{
			auto files = pfd::open_file("Browse me").result();
			if (files.empty() == false)
				strcpy_s(filepath, files[0].c_str());
		}

		ImGui::SameLine();
		static bool debug_enabled = false;
		ImGui::Checkbox("Debug", &debug_enabled);

		ImGui::Text("%s", filepath);

		static s22::Program program = {};

		if (ImGui::Button("Compile"))
		{
			// Input stream, open first argument as a file
			fopen_s(&yyin, filepath, "r");
			if (yyin == nullptr)
				return false;

			yydebug = debug_enabled ? 1 : 0;

			// Run parser
			auto parser = s22::parser_instance();
			yyparse(parser);

			program = parser->program_write();

			fclose(yyin);
			parser->dispose();
		}

		if (program.empty())
			return true;

		if (ImGui::BeginTable("Quadruples", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableSetupColumn("Label");
			ImGui::TableSetupColumn("Instruction");
			ImGui::TableSetupColumn("dst");
			ImGui::TableSetupColumn("src1");
			ImGui::TableSetupColumn("src2");
			ImGui::TableHeadersRow();

			for (const auto &instruction: program)
			{
				for (const auto &part: instruction)
				{
					ImGui::TableNextColumn();
					ImGui::Text("%s", part.c_str());
				}
			}

			ImGui::EndTable();
		}

		return true;
	});

	return 0;
}