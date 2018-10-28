#include "pch.h"
#include "finder.h"
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/platform.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/builder.h>
#include LCUI_APP_H

class App : public LCUI::Application {
	void Load(Platform::String^ entryPoint);
};

void App::Load(Platform::String^ entryPoint)
{
	char *argv[] = { "LC-Finder" };

	LCFinder_Init(1, argv);
}

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	App app;
	LCUI::Initialize();
	LCUI::Run(app);
	return 0;
}
