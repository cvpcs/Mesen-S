#include "stdafx.h"
#include "../Core/Console.h"
#include "../Core/VideoRenderer.h"
#include "../Core/SoundMixer.h"
#include "../Core/MovieManager.h"
#include "../Core/RewindManager.h"
#include "../Core/EmuSettings.h"

#ifdef _WIN32
#include "../Windows/Renderer.h"
#include "../Windows/SoundManager.h"
#include "../Windows/WindowsKeyManager.h"
#else
#include "../Linux/SdlRenderer.h"
#include "../Linux/SdlSoundManager.h"
#include "../Linux/LinuxKeyManager.h"
#endif

extern shared_ptr<Console> _console;
shared_ptr<Console> _historyConsole;
unique_ptr<IRenderingDevice> _historyRenderer;
unique_ptr<IAudioDevice> _historySoundManager;
enum class VideoCodec;

extern "C"
{
	DllExport bool __stdcall HistoryViewerEnabled()
	{
		shared_ptr<RewindManager> rewindManager = _console->GetRewindManager();
		return rewindManager ? rewindManager->HasHistory() : false;
	}

	DllExport void __stdcall HistoryViewerInitialize(void* windowHandle, void* viewerHandle)
	{
		_historyConsole.reset(new Console());
		// TODO: something about initializing with settings?
		_historyConsole->Initialize();

		_historyConsole->Lock();
		_historyConsole->LoadRom(_console->GetRomInfo().RomFile, _console->GetRomInfo().PatchFile);
		_historyConsole->CopyRewindData(_console);
		_historyConsole->Unlock();

		//Force some settings
		VideoConfig config = _historyConsole->GetSettings()->GetVideoConfig();
		config.VideoScale = 2;
		_historyConsole->GetSettings()->SetVideoConfig(config);
// TODO
//		_historyConsole->GetSettings()->SetEmulationSpeed(100);
//		_historyConsole->GetSettings()->ClearFlags(EmulationFlags::InBackground | EmulationFlags::Rewind /*|EmulationFlags::ForceMaxSpeed | EmulationFlags::DebuggerWindowEnabled*/);
		
#ifdef _WIN32
		_historyRenderer.reset(new Renderer(_historyConsole, (HWND)viewerHandle, false));
		_historySoundManager.reset(new SoundManager(_historyConsole, (HWND)windowHandle));
#else 
		_historyRenderer.reset(new SdlRenderer(_historyConsole, viewerHandle, false));
		_historySoundManager.reset(new SdlSoundManager(_historyConsole));
#endif
	}

	DllExport void __stdcall HistoryViewerRelease()
	{
		_historyConsole->Stop(true);
		_historyConsole->Release(); // Mesen had True, "For ShutDown"
		_historyRenderer.reset();
		_historySoundManager.reset();
		_historyConsole.reset();
	}

	DllExport uint32_t __stdcall HistoryViewerGetHistoryLength()
	{
		if (_historyConsole) {
			return _historyConsole->GetHistoryViewer()->GetHistoryLength();
		}
		return 0;
	}

	DllExport void __stdcall HistoryViewerGetSegments(uint32_t* segmentBuffer, uint32_t& bufferSize)
	{
		if (_historyConsole) {
			_historyConsole->GetHistoryViewer()->GetHistorySegments(segmentBuffer, bufferSize);
		}
	}

	DllExport bool __stdcall HistoryViewerCreateSaveState(const char* outputFile, uint32_t position)
	{
		if (_historyConsole) {
			return _historyConsole->GetHistoryViewer()->CreateSaveState(outputFile, position);
		}
		return false;
	}

	DllExport bool __stdcall HistoryViewerSaveMovie(const char* movieFile, uint32_t startPosition, uint32_t endPosition)
	{
		if (_historyConsole) {
			return _historyConsole->GetHistoryViewer()->SaveMovie(movieFile, startPosition, endPosition);
		}
		return false;
	}

	DllExport void __stdcall HistoryViewerResumeGameplay(uint32_t resumeAtSecond)
	{
		if (_historyConsole) {
			_historyConsole->GetHistoryViewer()->ResumeGameplay(_console, resumeAtSecond);
		}
	}

	DllExport void __stdcall HistoryViewerSetPosition(uint32_t seekPosition)
	{
		if (_historyConsole) {
			_historyConsole->GetHistoryViewer()->SeekTo(seekPosition);
		}
	}

	DllExport uint32_t __stdcall HistoryViewerGetPosition()
	{
		if (_historyConsole) {
			return _historyConsole->GetHistoryViewer()->GetPosition();
		}
		return 0;
	}

}