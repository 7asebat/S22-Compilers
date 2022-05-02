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

		if (ImGui::Button("Compile"))
		{
			// Input stream, open first argument as a file
			fopen_s(&yyin, filepath, "r");
			if (yyin == nullptr)
				return false;

			// Run parser
			auto parser = s22::parser_instance();
			yyparse(parser);

			// TODO: Multiple invocations

			fclose(yyin);
		}
		return true;
	});

	return 0;
}