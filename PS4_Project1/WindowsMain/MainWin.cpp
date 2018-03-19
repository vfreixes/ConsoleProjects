#include <Windows.h>

#include "../Game/Common.h"

int __stdcall WinMain(__in HINSTANCE, __in_opt HINSTANCE, __in_opt LPSTR, __in int) {
	Game::GameData *gameData = Game::CreateGameData();

	Game::Input input = {};
	Game::RenderCommands renderCommands = Game::Update(input, *gameData);

	Game::DestroyGameData(gameData);

	OutputDebugStringW(L"HelloWorld");

	return 0;
}