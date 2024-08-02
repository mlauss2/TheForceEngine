// main.cpp : Defines the entry point for the application.
#include "version.h"
#include <SDL.h>
#include <TFE_System/types.h>
#include <TFE_System/profiler.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Archive/gobArchive.h>
#include <TFE_Game/igame.h>
#include <TFE_Game/saveSystem.h>
#include <TFE_Game/reticle.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/system.h>
#include <TFE_System/CrashHandler/crashHandler.h>
#include <TFE_System/frameLimiter.h>
#include <TFE_System/tfeMessage.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_RenderShared/texturePacker.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Ui/ui.h>
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_FrontEndUI/modLoader.h>
#include <TFE_A11y/accessibility.h>
#include <algorithm>
#include <cinttypes>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#if ENABLE_EDITOR == 1
#include <TFE_Editor/editor.h>
#endif
#if ENABLE_FORCE_SCRIPT == 1
#include <TFE_ForceScript/forceScript.h>
#endif

#include <TFE_Audio/midiPlayer.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#ifdef min
#undef min
#undef max
#pragma comment(lib, "SDL2main.lib")
#endif
#endif

#define PROGRAM_ERROR   1
#define PROGRAM_SUCCESS 0

#if (!defined(_DEBUG) || defined(NDEBUG)) && !defined(__EMSCRIPTEN__)
#define INSTALL_CRASH_HANDLER 1
#else
#define INSTALL_CRASH_HANDLER 0
#endif

using namespace TFE_Input;
using namespace TFE_A11Y;

struct TFEMainContext
{
	bool loop;
	AppState curState;
	u32 frame;
	bool soundPaused;
	f32  refreshRate;
	s32  displayIndex;
	u32  baseWindowWidth;
	u32  baseWindowHeight;
	u32  displayWidth;
	u32  displayHeight;
	u32  monitorWidth;
	u32  monitorHeight;
	char screenshotTime[TFE_MAX_PATH];
	s32  startupGame;
	IGame* curGame;
	const char* loadRequestFilename;
	bool showPerf;
	bool relativeMode;
	TFE_Settings_Graphics* graphics;
	int argc;
	char **argv;
	bool nullAudioDevice;
};

static TFEMainContext s_mainContext;

static void parseOption(const char* name, const std::vector<const char*>& values, bool longName);
static bool validatePath();

static void handleEvent(SDL_Event& Event)
{
	TFE_Ui::setUiInput(&Event);
	TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();

	switch (Event.type)
	{
		case SDL_QUIT:
		{
			TFE_System::logWrite(LOG_MSG, "Main", "App Quit");
			s_mainContext.loop = false;
		} break;
		case SDL_WINDOWEVENT:
		{
			if (Event.window.event == SDL_WINDOWEVENT_RESIZED || Event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				TFE_RenderBackend::resize(Event.window.data1, Event.window.data2);
			}
		} break;
		case SDL_CONTROLLERDEVICEADDED:
		{
			const s32 cIdx = Event.cdevice.which;
			if (SDL_IsGameController(cIdx))
			{
				SDL_GameController* controller = SDL_GameControllerOpen(cIdx);
				SDL_Joystick* j = SDL_GameControllerGetJoystick(controller);
				SDL_JoystickID joyId = SDL_JoystickInstanceID(j);

				//Save the joystick id to used in the future events
				SDL_GameControllerOpen(0);
			}
		} break;
		case SDL_MOUSEBUTTONDOWN:
		{
			TFE_Input::setMouseButtonDown(MouseButton(Event.button.button - SDL_BUTTON_LEFT));
		} break;
		case SDL_MOUSEBUTTONUP:
		{
			TFE_Input::setMouseButtonUp(MouseButton(Event.button.button - SDL_BUTTON_LEFT));
		} break;
		case SDL_MOUSEWHEEL:
		{
			TFE_Input::setMouseWheel(Event.wheel.x, Event.wheel.y);
		} break;
		case SDL_KEYDOWN:
		{
			if (Event.key.keysym.scancode)
			{
				TFE_Input::setKeyDown(KeyboardCode(Event.key.keysym.scancode), Event.key.repeat != 0);
			}

			if (Event.key.keysym.scancode)
			{
				TFE_Input::setBufferedKey(KeyboardCode(Event.key.keysym.scancode));
			}
		} break;
		case SDL_KEYUP:
		{
			if (Event.key.keysym.scancode)
			{
				const KeyboardCode code = KeyboardCode(Event.key.keysym.scancode);
				TFE_Input::setKeyUp(KeyboardCode(Event.key.keysym.scancode));

				// Fullscreen toggle.
				bool altHeld = TFE_Input::keyDown(KEY_LALT) || TFE_Input::keyDown(KEY_RALT);
				if (code == KeyboardCode::KEY_F11 || (code == KeyboardCode::KEY_RETURN && altHeld))
				{
					windowSettings->fullscreen = !windowSettings->fullscreen;
					TFE_RenderBackend::enableFullscreen(windowSettings->fullscreen);
				}
				else if (code == KeyboardCode::KEY_PRINTSCREEN)
				{
					static u64 _screenshotIndex = 0;

					char screenshotDir[TFE_MAX_PATH];
					TFE_Paths::appendPath(TFE_PathType::PATH_USER_DOCUMENTS, "Screenshots/", screenshotDir);
										
					char screenshotPath[TFE_MAX_PATH];
					sprintf(screenshotPath, "%stfe_screenshot_%s_%" PRIu64 ".png", screenshotDir, s_mainContext.screenshotTime, _screenshotIndex);
					_screenshotIndex++;

					TFE_RenderBackend::queueScreenshot(screenshotPath);
				}
				else if (code == KeyboardCode::KEY_F2 && altHeld)
				{
					static u64 _gifIndex = 0;
					static bool _recording = false;

					if (!_recording)
					{
						char screenshotDir[TFE_MAX_PATH];
						TFE_Paths::appendPath(TFE_PathType::PATH_USER_DOCUMENTS, "Screenshots/", screenshotDir);

						char gifPath[TFE_MAX_PATH];
						sprintf(gifPath, "%stfe_gif_%s_%" PRIu64 ".gif", screenshotDir, s_mainContext.screenshotTime, _gifIndex);
						_gifIndex++;

						TFE_RenderBackend::startGifRecording(gifPath);
						_recording = true;
					}
					else
					{
						TFE_RenderBackend::stopGifRecording();
						_recording = false;
					}
				}
			}
		} break;
		case SDL_TEXTINPUT:
		{
			TFE_Input::setBufferedInput(Event.text.text);
		} break;
		case SDL_CONTROLLERAXISMOTION:
		{
			// Axis are now handled interally so the deadzone can be changed.
			if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
			{ TFE_Input::setAxis(AXIS_LEFT_X, f32(Event.caxis.value) / 32768.0f); }
			else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
			{ TFE_Input::setAxis(AXIS_LEFT_Y, -f32(Event.caxis.value) / 32768.0f); }

			if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
			{ TFE_Input::setAxis(AXIS_RIGHT_X, f32(Event.caxis.value) / 32768.0f); }
			else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
			{ TFE_Input::setAxis(AXIS_RIGHT_Y, -f32(Event.caxis.value) / 32768.0f); }

			const s32 deadzone = 3200;
			if ((Event.caxis.value < -deadzone) || (Event.caxis.value > deadzone))
			{
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
				{ TFE_Input::setAxis(AXIS_LEFT_TRIGGER, f32(Event.caxis.value) / 32768.0f); }
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
				{ TFE_Input::setAxis(AXIS_RIGHT_TRIGGER, f32(Event.caxis.value) / 32768.0f); }
			}
			else
			{
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
				{ TFE_Input::setAxis(AXIS_LEFT_TRIGGER, 0.0f); }
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
				{ TFE_Input::setAxis(AXIS_RIGHT_TRIGGER, 0.0f); }
			}
		} break;
		case SDL_CONTROLLERBUTTONDOWN:
		{
			if (Event.cbutton.button < CONTROLLER_BUTTON_COUNT)
			{
				TFE_Input::setButtonDown(Button(Event.cbutton.button));
			}
		} break;
		case SDL_CONTROLLERBUTTONUP:
		{
			if (Event.cbutton.button < CONTROLLER_BUTTON_COUNT)
			{
				TFE_Input::setButtonUp(Button(Event.cbutton.button));
			}
		} break;
		default:
		{
		} break;
	}
}

static bool sdlInit()
{
	const int code = SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);
	if (code != 0) { return false; }

	TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();
	bool fullscreen    = windowSettings->fullscreen || TFE_Settings::getTempSettings()->forceFullscreen;
	s_mainContext.displayWidth     = windowSettings->width;
	s_mainContext.displayHeight    = windowSettings->height;
	s_mainContext.baseWindowWidth  = windowSettings->baseWidth;
	s_mainContext.baseWindowHeight = windowSettings->baseHeight;

	// Get the displays and their bounds.
	s_mainContext.displayIndex = TFE_RenderBackend::getDisplayIndex(windowSettings->x, windowSettings->y);
	// Reset the display if the window is out of bounds.
	if (s_mainContext.displayIndex < 0)
	{
		MonitorInfo mInfo;
		s_mainContext.displayIndex = 0;
		TFE_RenderBackend::getDisplayMonitorInfo(0, &mInfo);

		windowSettings->x = mInfo.x;
		windowSettings->y = mInfo.y + 32;
		windowSettings->width  = min((s32)windowSettings->width,  mInfo.w);
		windowSettings->height = min((s32)windowSettings->height, mInfo.h);
		windowSettings->baseWidth  = windowSettings->width;
		windowSettings->baseHeight = windowSettings->height;
		TFE_Settings::writeToDisk();

		s_mainContext.displayWidth     = windowSettings->width;
		s_mainContext.displayHeight    = windowSettings->height;
		s_mainContext.baseWindowWidth  = windowSettings->baseWidth;
		s_mainContext.baseWindowHeight = windowSettings->baseHeight;
	}

	// Determine the display mode settings based on the desktop.
	SDL_DisplayMode mode = {};
	SDL_GetDesktopDisplayMode(s_mainContext.displayIndex, &mode);
	s_mainContext.refreshRate = (f32)mode.refresh_rate;

	if (fullscreen)
	{
		s_mainContext.displayWidth  = mode.w;
		s_mainContext.displayHeight = mode.h;
	}
	else
	{
		s_mainContext.displayWidth  = std::min(s_mainContext.displayWidth,  (u32)mode.w);
		s_mainContext.displayHeight = std::min(s_mainContext.displayHeight, (u32)mode.h);
	}

	s_mainContext.monitorWidth  = mode.w;
	s_mainContext.monitorHeight = mode.h;

#ifdef SDL_HINT_APP_NAME  // SDL 2.0.18+
	SDL_SetHint(SDL_HINT_APP_NAME, "The Force Engine");
#endif

	return true;
}

static void setAppState(AppState newState, int argc, char* argv[])
{
	const TFE_Settings_Graphics* config = TFE_Settings::getGraphicsSettings();

#if ENABLE_EDITOR == 1
	if (newState != APP_STATE_EDITOR)
	{
		TFE_Editor::disable();
	}
#endif

	switch (newState)
	{
	case APP_STATE_MENU:
	case APP_STATE_SET_DEFAULTS:
		break;
	case APP_STATE_EDITOR:

		if (validatePath())
		{
		#if ENABLE_EDITOR == 1
			TFE_Editor::enable();
		#endif
		}
		else
		{
			newState = APP_STATE_NO_GAME_DATA;
		}
		break;
	case APP_STATE_LOAD:
	{
		bool pathIsValid = validatePath();
		if (pathIsValid && s_mainContext.loadRequestFilename)
		{
			newState = APP_STATE_GAME;
			TFE_FrontEndUI::setAppState(APP_STATE_GAME);

			TFE_Game* gameInfo = TFE_Settings::getGame();
			if (s_mainContext.curGame)
			{
				freeGame(s_mainContext.curGame);
				s_mainContext.curGame = nullptr;
			}
			s_mainContext.soundPaused = false;
			s_mainContext.curGame = createGame(gameInfo->id);
			TFE_SaveSystem::setCurrentGame(s_mainContext.curGame);
			if (!s_mainContext.curGame)
			{
				TFE_System::logWrite(LOG_ERROR, "AppMain", "Cannot create game '%s'.", gameInfo->game);
				newState = APP_STATE_CANNOT_RUN;
			}
			else if (!TFE_SaveSystem::loadGame(s_mainContext.loadRequestFilename))
			{
				TFE_System::logWrite(LOG_ERROR, "AppMain", "Cannot run game '%s'.", gameInfo->game);
				freeGame(s_mainContext.curGame);
				s_mainContext.curGame = nullptr;
				newState = APP_STATE_CANNOT_RUN;
			}
			else
			{
				TFE_Input::enableRelativeMode(true);
			}
		}
		else if (!pathIsValid)
		{
			newState = APP_STATE_NO_GAME_DATA;
		}
		else
		{
			newState = s_mainContext.curState;
		}
		s_mainContext.loadRequestFilename = nullptr;
	} break;
	case APP_STATE_GAME:
		if (validatePath())
		{
			TFE_Game* gameInfo = TFE_Settings::getGame();
			if (!s_mainContext.curGame || gameInfo->id != s_mainContext.curGame->id)
			{
				s_mainContext.soundPaused = false;
				if (s_mainContext.curGame)
				{
					freeGame(s_mainContext.curGame);
					s_mainContext.curGame = nullptr;
				}
				s_mainContext.curGame = createGame(gameInfo->id);
				TFE_SaveSystem::setCurrentGame(s_mainContext.curGame);
				if (!s_mainContext.curGame)
				{
					TFE_System::logWrite(LOG_ERROR, "AppMain", "Cannot create game '%s'.", gameInfo->game);
					newState = APP_STATE_CANNOT_RUN;
				}
				else if (!s_mainContext.curGame->runGame(argc, (const char**)argv, nullptr))
				{
					TFE_System::logWrite(LOG_ERROR, "AppMain", "Cannot run game '%s'.", gameInfo->game);
					freeGame(s_mainContext.curGame);
					s_mainContext.curGame = nullptr;
					newState = APP_STATE_CANNOT_RUN;
				}
				else
				{
					TFE_Input::enableRelativeMode(true);
				}
			}
		}
		else
		{
			newState = APP_STATE_NO_GAME_DATA;
		}
		break;
	};

	s_mainContext.curState = newState;
}

static bool systemMenuKeyCombo()
{
	return TFE_System::systemUiRequestPosted() || (inputMapping_getActionState(IAS_SYSTEM_MENU) == STATE_PRESSED);
}

static void parseCommandLine(s32 argc, char* argv[])
{
	if (argc < 1) { return; }

	const char* curOptionName = nullptr;
	bool longName = false;
	std::vector<const char*> values;
	for (s32 i = 1; i < argc; i++)
	{
		const char* opt = argv[i];
		const size_t len = strlen(opt);

		// Is this an option name or value?
		const char* optValue = nullptr;
		if (len && opt[0] == '-')
		{
			if (curOptionName)
			{
				parseOption(curOptionName, values, longName);
			}
			if (len > 2 && opt[0] == '-' && opt[1] == '-')
			{
				longName = true;
				curOptionName = opt + 2;
			}
			else
			{
				longName = false;
				curOptionName = opt + 1;
			}
			values.clear();
		}
		else if (len && opt[0] != '-')
		{
			values.push_back(opt);
		}
	}
	if (curOptionName)
	{
		parseOption(curOptionName, values, longName);
	}
}

static void generateScreenshotTime()
{
#ifdef _WIN32
	__time64_t time;
	_time64(&time);
	const char* timeString = _ctime64(&time);
	if (timeString)
	{
		strcpy(s_mainContext.screenshotTime, timeString);
	}

#else
	time_t tt = time(NULL);
	memset(s_mainContext.screenshotTime, 0, 1024);
	strcpy(s_mainContext.screenshotTime, ctime(&tt));
#endif
	// Replace ':' with '_'
	size_t len = strlen(s_mainContext.screenshotTime);
	for (size_t i = 0; i < len; i++)
	{
		if (s_mainContext.screenshotTime[i] == ':')
		{
			s_mainContext.screenshotTime[i] = '_';
		}
		else if (s_mainContext.screenshotTime[i] == ' ')
		{
			s_mainContext.screenshotTime[i] = '-';
		}
		if (s_mainContext.screenshotTime[i] == '\n')
		{
			s_mainContext.screenshotTime[i] = 0;
			break;
		}
	}
}

static bool validatePath()
{
	if (!TFE_Paths::hasPath(PATH_SOURCE_DATA)) { return false; }

	char testFile[TFE_MAX_PATH];
	// if (game->id == Game_Dark_Forces)
	{
		// Does DARK.GOB exist?
		sprintf(testFile, "%s%s", TFE_Paths::getPath(PATH_SOURCE_DATA), "DARK.GOB");
		if (!FileUtil::exists(testFile))
		{
			TFE_System::logWrite(LOG_ERROR, "Main", "Invalid game source path: '%s' - '%s' does not exist.", TFE_Paths::getPath(PATH_SOURCE_DATA), testFile);
			TFE_Paths::setPath(PATH_SOURCE_DATA, "");
		}
		else if (!GobArchive::validate(testFile, 130))
		{
			TFE_System::logWrite(LOG_ERROR, "Main", "Invalid game source path: '%s' - '%s' GOB is invalid, too few files.", TFE_Paths::getPath(PATH_SOURCE_DATA), testFile);
			TFE_Paths::setPath(PATH_SOURCE_DATA, "");
		}
	}
	return TFE_Paths::hasPath(PATH_SOURCE_DATA);
}

static void deinit(void)
{
	if (s_mainContext.curGame)
	{
		freeGame(s_mainContext.curGame);
		s_mainContext.curGame = nullptr;
	}
	s_mainContext.soundPaused = false;
	game_destroy();
	reticle_destroy();
	inputMapping_shutdown();
	
	// Cleanup
	TFE_FrontEndUI::shutdown();
	TFE_Audio::shutdown();
	TFE_MidiPlayer::destroy();
	TFE_Image::shutdown();
	TFE_Palette::freeAll();
	TFE_RenderBackend::updateSettings();
	TFE_Settings::shutdown();
	TFE_Jedi::texturepacker_freeGlobal();
	TFE_RenderBackend::destroy();
	TFE_SaveSystem::destroy();
	SDL_Quit();
	
#ifdef ENABLE_FORCE_SCRIPT
	TFE_ForceScript::destroy();
#endif
	
	TFE_System::logWrite(LOG_MSG, "Progam Flow", "The Force Engine Game Loop Ended.");
	TFE_System::logClose();
	TFE_System::freeMessages();
}

static void mainloop(void)
{
	if (!s_mainContext.loop || TFE_System::quitMessagePosted())
	{
		deinit();
#ifdef __EMSCRIPTEN__
		emscripten_cancel_main_loop();
#else
		exit(PROGRAM_SUCCESS);
#endif
	}


	TFE_FRAME_BEGIN();
	TFE_System::frameLimiter_begin();

	bool enableRelative = TFE_Input::relativeModeEnabled();
	if (enableRelative != s_mainContext.relativeMode)
	{
		s_mainContext.relativeMode = enableRelative;
		SDL_SetRelativeMouseMode(s_mainContext.relativeMode ? SDL_TRUE : SDL_FALSE);
	}

	// System events
	SDL_Event event;
	while (SDL_PollEvent(&event)) { handleEvent(event); }

	// Handle mouse state.
	s32 mouseX, mouseY;
	s32 mouseAbsX, mouseAbsY;
	u32 state = SDL_GetRelativeMouseState(&mouseX, &mouseY);
	SDL_GetMouseState(&mouseAbsX, &mouseAbsY);
	TFE_Input::setRelativeMousePos(mouseX, mouseY);
	TFE_Input::setMousePos(mouseAbsX, mouseAbsY);
	inputMapping_updateInput();

	// Can we save?
	TFE_FrontEndUI::setCanSave(s_mainContext.curGame ? s_mainContext.curGame->canSave() : false);

	// Update the System UI.
	AppState appState = TFE_FrontEndUI::update();
	s_mainContext.loadRequestFilename = TFE_SaveSystem::loadRequestFilename();
	if (s_mainContext.loadRequestFilename)
	{
		appState = APP_STATE_LOAD;
	}

	if (appState == APP_STATE_QUIT)
	{
		s_mainContext.loop = false;
	}
	else if (appState != s_mainContext.curState)
	{
		if (appState == APP_STATE_EXIT_TO_MENU)	// Return to the menu from the game.
		{
			if (s_mainContext.curGame)
			{
				freeGame(s_mainContext.curGame);
				s_mainContext.curGame = nullptr;
			}
			s_mainContext.soundPaused = false;
			appState = APP_STATE_MENU;
		}

		char* selectedMod = TFE_FrontEndUI::getSelectedMod();
		if (selectedMod && selectedMod[0] && appState == APP_STATE_GAME)
		{
			char* newArgs[16];
			for (s32 i = 0; i < s_mainContext.argc && i < 15; i++)
			{
				newArgs[i] = s_mainContext.argv[i];
			}
			newArgs[s_mainContext.argc] = selectedMod;
			setAppState(appState, s_mainContext.argc + 1, newArgs);
		}
		else
		{
			setAppState(appState, s_mainContext.argc, s_mainContext.argv);
		}
	}

	if (TFE_A11Y::hasPendingFont()) { TFE_A11Y::loadPendingFont(); } // Can't load new fonts between TFE_Ui::begin() and TFE_Ui::render();
	TFE_Ui::begin();
	TFE_System::update();

	// Update
	if (TFE_FrontEndUI::uiControlsEnabled() && task_canRun())
	{
		if (TFE_FrontEndUI::isConsoleOpen() && !TFE_FrontEndUI::isConsoleAnimating() && TFE_Input::keyPressed(KEY_ESCAPE))
		{
			TFE_FrontEndUI::toggleConsole();
			// "Eat" the key so it doesn't extend to the Escape menu.
			TFE_Input::clearKeyPressed(KEY_ESCAPE);
			inputMapping_clearKeyBinding(KEY_ESCAPE);
			if (s_mainContext.curGame)
			{
					s_mainContext.curGame->pauseGame(false);
					TFE_Input::enableRelativeMode(true);
				}
			}
			else if (inputMapping_getActionState(IAS_CONSOLE) == STATE_PRESSED)
			{
				bool isOpening = TFE_FrontEndUI::toggleConsole();
				if (s_mainContext.curGame)
				{
					s_mainContext.curGame->pauseGame(isOpening);
					TFE_Input::enableRelativeMode(!isOpening);
				}
			}
			else if (TFE_Input::keyPressed(KEY_F9) && TFE_Input::keyDown(KEY_LALT))
			{
				s_mainContext.showPerf = !s_mainContext.showPerf;
			}
			else if (TFE_Input::keyPressed(KEY_F10) && TFE_Input::keyDown(KEY_LALT))
			{
				TFE_FrontEndUI::toggleProfilerView();
			}
			
			bool toggleSystemMenu = systemMenuKeyCombo();
			if (TFE_FrontEndUI::isConfigMenuOpen() && (toggleSystemMenu || TFE_Input::keyPressed(KEY_ESCAPE)))
			{
				// "Eat" the escape key so it doesn't also open the Escape menu.
				TFE_Input::clearKeyPressed(KEY_ESCAPE);
				inputMapping_clearKeyBinding(KEY_ESCAPE);
				
				s_mainContext.curState = TFE_FrontEndUI::menuReturn();
				
				if ((s_mainContext.soundPaused || TFE_Settings::getSoundSettings()->disableSoundInMenus) && s_mainContext.curGame)
				{
					s_mainContext.curGame->pauseSound(false);
					s_mainContext.soundPaused = false;
				}
			}
			else if (toggleSystemMenu)
			{
				TFE_FrontEndUI::enableConfigMenu();
				TFE_FrontEndUI::setMenuReturnState(s_mainContext.curState);
				
				if (TFE_Settings::getSoundSettings()->disableSoundInMenus && s_mainContext.curGame)
				{
					s_mainContext.curGame->pauseSound(true);
					s_mainContext.soundPaused = true;
				}
			}
			else if (s_mainContext.soundPaused && !TFE_FrontEndUI::isConfigMenuOpen())
			{
				if (s_mainContext.curGame)
				{
					s_mainContext.curGame->pauseSound(false);
			}
			s_mainContext.soundPaused = false;
		}
	}

#ifdef ENABLE_FORCE_SCRIPT
	TFE_ForceScript::update();
#endif

	const bool isConsoleOpen = TFE_FrontEndUI::isConsoleOpen();
	bool endInputFrame = true;
	if (s_mainContext.curState == APP_STATE_EDITOR)
	{
#if ENABLE_EDITOR == 1
		if (TFE_Editor::update(isConsoleOpen))
		{
			TFE_FrontEndUI::setAppState(APP_STATE_MENU);
		}
#endif
	}
	else if (s_mainContext.curState == APP_STATE_GAME)
	{
		if (!s_mainContext.curGame)
		{
			s_mainContext.curState = APP_STATE_MENU;
		}
		else
		{
			TFE_SaveSystem::update();
			s_mainContext.curGame->loopGame();
			endInputFrame = TFE_Jedi::task_run() != 0;
		}
	}
	else
	{
		TFE_RenderBackend::clearWindow();
	}

	bool drawFps = s_mainContext.curGame && s_mainContext.graphics->showFps;
	if (s_mainContext.curGame) { drawFps = drawFps && (!s_mainContext.curGame->isPaused()); }

	TFE_FrontEndUI::setCurrentGame(s_mainContext.curGame);
	TFE_FrontEndUI::draw(s_mainContext.curState == APP_STATE_MENU || s_mainContext.curState == APP_STATE_NO_GAME_DATA || s_mainContext.curState == APP_STATE_SET_DEFAULTS,
			     s_mainContext.curState == APP_STATE_NO_GAME_DATA, s_mainContext.curState == APP_STATE_SET_DEFAULTS, drawFps);

	// Make sure the clear the no game data state if the data becomes valid.
	if (TFE_FrontEndUI::isNoDataMessageSet() && validatePath())
	{
		TFE_FrontEndUI::clearNoDataState();
	}

	bool swap = s_mainContext.curState != APP_STATE_EDITOR && (s_mainContext.curState != APP_STATE_MENU || TFE_FrontEndUI::isConfigMenuOpen());
#if ENABLE_EDITOR == 1
	if (s_mainContext.curState == APP_STATE_EDITOR)
	{
		swap = TFE_Editor::render();
	}
#endif

	// Blit the frame to the window and draw UI.
	TFE_RenderBackend::swap(swap);

	// Handle framerate limiter.
	TFE_System::frameLimiter_end();

	// Clear transitory input state.
	if (endInputFrame)
	{
		TFE_Input::endFrame();
		inputMapping_endFrame();
	}
	s_mainContext.frame++;

	if (endInputFrame)
	{
		TFE_FRAME_END();
	}
}

int main(int argc, char* argv[])
{
	#if INSTALL_CRASH_HANDLER
	TFE_CrashHandler::setProcessExceptionHandlers();
	TFE_CrashHandler::setThreadExceptionHandlers();
	#endif

	// Paths
	bool pathsSet = true;
	pathsSet &= TFE_Paths::setProgramPath();
	pathsSet &= TFE_Paths::setProgramDataPath("TheForceEngine");
	pathsSet &= TFE_Paths::setUserDocumentsPath("TheForceEngine");
	TFE_System::logOpen("the_force_engine_log.txt");
	TFE_System::logWrite(LOG_MSG, "Main", "The Force Engine %s", c_gitVersion);
	if (!pathsSet)
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot set paths.");
		return PROGRAM_ERROR;
	}

	// Before loading settings, read in the Input key lists.
	if (!TFE_Input::loadKeyNames("UI_Text/KeyText.txt"))
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot load key names.");
		return PROGRAM_ERROR;
	}

	if (!TFE_System::loadMessages("UI_Text/TfeMessages.txt"))
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot load TFE messages.");
		return PROGRAM_ERROR;
	}

	// Initialize settings so that the paths can be read.
	bool firstRun;
	if (!TFE_Settings::init(firstRun))
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot load settings.");
		return PROGRAM_ERROR;
	}

	s_mainContext.loop = true;
	s_mainContext.nullAudioDevice = false;
	s_mainContext.refreshRate  = 0;
	s_mainContext.displayIndex = 0;
	s_mainContext.baseWindowWidth  = 1280;
	s_mainContext.baseWindowHeight = 720;
	s_mainContext.displayWidth  = 1280;
	s_mainContext.displayHeight = 720;
	s_mainContext.monitorWidth  = 1280;
	s_mainContext.monitorHeight = 720;
	memset(s_mainContext.screenshotTime, 0, TFE_MAX_PATH);
	s_mainContext.startupGame = -1;
	s_mainContext.curGame = nullptr;
	s_mainContext.loadRequestFilename = nullptr;
	s_mainContext.curState = APP_STATE_UNINIT;
	s_mainContext.soundPaused = false;
	s_mainContext.showPerf = false;
	s_mainContext.relativeMode = false;
	s_mainContext.frame = 0;
	s_mainContext.graphics = nullptr;
	s_mainContext.argc = argc;
	s_mainContext.argv = argv;
	
	// Override settings with command line options.
	parseCommandLine(argc, argv);

	// Setup game paths.
	// Get the current game.
	const TFE_Game* game = TFE_Settings::getGame();
	const TFE_GameHeader* gameHeader = TFE_Settings::getGameHeader(game->game);
	TFE_Paths::setPath(PATH_SOURCE_DATA, gameHeader->sourcePath);
	TFE_Paths::setPath(PATH_EMULATOR, gameHeader->emulatorPath);

	// Validate the current game path.
	validatePath();

	TFE_System::logWrite(LOG_MSG, "Paths", "Program Path: \"%s\"",   TFE_Paths::getPath(PATH_PROGRAM));
	TFE_System::logWrite(LOG_MSG, "Paths", "Program Data: \"%s\"",   TFE_Paths::getPath(PATH_PROGRAM_DATA));
	TFE_System::logWrite(LOG_MSG, "Paths", "User Documents: \"%s\"", TFE_Paths::getPath(PATH_USER_DOCUMENTS));
	TFE_System::logWrite(LOG_MSG, "Paths", "Source Data: \"%s\"",    TFE_Paths::getPath(PATH_SOURCE_DATA));

	// Create a screenshot directory
	char screenshotDir[TFE_MAX_PATH];
	TFE_Paths::appendPath(TFE_PathType::PATH_USER_DOCUMENTS, "Screenshots/", screenshotDir);
	if (!FileUtil::directoryExits(screenshotDir))
	{
		FileUtil::makeDirectory(screenshotDir);
	}

	// Create a mods temporary directory.
	char tempPath[TFE_MAX_PATH];
	sprintf(tempPath, "%sTemp/", TFE_Paths::getPath(PATH_PROGRAM_DATA));
	if (!FileUtil::directoryExits(tempPath))
	{
		FileUtil::makeDirectory(tempPath);
	}
	generateScreenshotTime();

	// Initialize SDL
	if (!sdlInit())
	{
		TFE_System::logWrite(LOG_CRITICAL, "SDL", "Cannot initialize SDL.");
		TFE_System::logClose();
		return PROGRAM_ERROR;
	}
	TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();
	s_mainContext.graphics = TFE_Settings::getGraphicsSettings();
	TFE_System::init(s_mainContext.refreshRate, s_mainContext.graphics->vsync, c_gitVersion);
	// Setup the GPU Device and Window.
	u32 windowFlags = 0;
	if (windowSettings->fullscreen || TFE_Settings::getTempSettings()->forceFullscreen)
	{
		TFE_System::logWrite(LOG_MSG, "Display", "Fullscreen enabled.");
		windowFlags |= WINFLAG_FULLSCREEN;
	}
	if (s_mainContext.graphics->vsync) { TFE_System::logWrite(LOG_MSG, "Display", "Vertical Sync enabled."); windowFlags |= WINFLAG_VSYNC; }
	
	WindowState windowState =
	{
		"",
		s_mainContext.displayWidth,
		s_mainContext.displayHeight,
		s_mainContext.baseWindowWidth,
		s_mainContext.baseWindowHeight,
		s_mainContext.monitorWidth,
		s_mainContext.monitorHeight,
		windowFlags,
		s_mainContext.refreshRate
	};
	sprintf(windowState.name, "The Force Engine  %s", TFE_System::getVersionString());
	if (!TFE_RenderBackend::init(windowState))
	{
		TFE_System::logWrite(LOG_CRITICAL, "GPU", "Cannot initialize GPU/Window.");
		TFE_System::logClose();
		return PROGRAM_ERROR;
	}
	TFE_FrontEndUI::initConsole();
	TFE_Audio::init(s_mainContext.nullAudioDevice, TFE_Settings::getSoundSettings()->audioDevice);
	TFE_MidiPlayer::init(TFE_Settings::getSoundSettings()->midiOutput, (MidiDeviceType)TFE_Settings::getSoundSettings()->midiType);
	TFE_Image::init();
	TFE_Palette::createDefault256();
	TFE_FrontEndUI::init();
	game_init();
	inputMapping_startup();
	TFE_SaveSystem::init();
	TFE_A11Y::init();

	// Uncomment to test memory region allocator.
	// TFE_Memory::region_test();

	// Color correction.
	const ColorCorrection colorCorrection = { s_mainContext.graphics->brightness, 
						  s_mainContext.graphics->contrast,
						  s_mainContext.graphics->saturation,
						  s_mainContext.graphics->gamma };
	TFE_RenderBackend::setColorCorrection(s_mainContext.graphics->colorCorrection, &colorCorrection);

	// Optional Reticle.
	reticle_init();

	// Test
	#ifdef ENABLE_FORCE_SCRIPT
	TFE_ForceScript::init();
	#endif
		
	// Start up the game and skip the title screen.
	if (firstRun)
	{
		TFE_FrontEndUI::setAppState(APP_STATE_SET_DEFAULTS);
	}
	else if (s_mainContext.startupGame >= Game_Dark_Forces && validatePath())
	{
		TFE_FrontEndUI::setAppState(APP_STATE_GAME);
	}

	// Try to set the game right away, so the load menu works.
	TFE_Game* gameInfo = TFE_Settings::getGame();
	TFE_SaveSystem::setCurrentGame(gameInfo->id);

	// Setup the framelimiter.
	TFE_System::frameLimiter_set(s_mainContext.graphics->frameRateLimit);

	// Start reading the mods immediately?
	TFE_FrontEndUI::modLoader_read();

	// Game loop
	s_mainContext.frame = 0u;
	s_mainContext.showPerf = false;
	s_mainContext.relativeMode = false;
	TFE_System::logWrite(LOG_MSG, "Progam Flow", "The Force Engine Game Loop Started");

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(mainloop, 0, 1);
#else
	while (1) { mainloop(); }
#endif
	// Do not put anything after here, it won't be called.
	return PROGRAM_SUCCESS;
}

static void parseOption(const char* name, const std::vector<const char*>& values, bool longName)
{
	if (!longName)	// short names use the same style as the originals.
	{
		if (name[0] == 'g')		// Directly load a game, skipping the titlescreen.
		{
			// -gDARK
			const char* gameToLoad = &name[1];
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Game to load: %s", gameToLoad);
			if (!strcasecmp(gameToLoad, "dark"))
			{
				s_mainContext.startupGame = Game_Dark_Forces;
			}
		}
		else if (strcasecmp(name, "nosound") == 0)
		{
			// -noaudio
			s_mainContext.nullAudioDevice = true;
		}
		else if (strcasecmp(name, "fullscreen") == 0)
		{
			TFE_Settings::getTempSettings()->forceFullscreen = true;
		}
		else if (strcasecmp(name, "skip_load_delay") == 0)
		{
			TFE_Settings::getTempSettings()->skipLoadDelay = true;
		}
	}
	else  // long names use the more traditional style of arguments which allow for multiple values.
	{
		if (strcasecmp(name, "game") == 0 && values.size() >= 1)	// Directly load a game, skipping the titlescreen.
		{
			// --game DARK
			const char* gameToLoad = values[0];
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Game to load: %s", gameToLoad);
			if (!strcasecmp(gameToLoad, "dark"))
			{
				s_mainContext.startupGame = Game_Dark_Forces;
			}
		}
		else if (strcasecmp(name, "nosound") == 0)
		{
			// --noaudio
			s_mainContext.nullAudioDevice = true;
		}
		else if (strcasecmp(name, "fullscreen") == 0)
		{
			TFE_Settings::getTempSettings()->forceFullscreen = true;
		}
		else if (strcasecmp(name, "skip_load_delay") == 0)
		{
			TFE_Settings::getTempSettings()->skipLoadDelay = true;
		}
	}
}