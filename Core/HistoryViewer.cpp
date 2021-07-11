#include "stdafx.h"
#include "HistoryViewer.h"
#include "RewindData.h"
#include "Console.h"
#include "BaseControlDevice.h"
#include "SoundMixer.h"
#include "NotificationManager.h"
#include "MovieRecorder.h"
#include "SaveStateManager.h"
#include "ControlManager.h"

HistoryViewer::HistoryViewer(shared_ptr<Console> console)
{
	_console = console;
	_position = 0;
	_pollCounter = 0;
}

HistoryViewer::~HistoryViewer()
{
}

void HistoryViewer::SetHistoryData(std::deque<RewindData>& history)
{
	_history = history;

	_console->GetControlManager()->UnregisterInputProvider(this);
	_console->GetControlManager()->RegisterInputProvider(this);

	SeekTo(0);
}

uint32_t HistoryViewer::GetHistoryLength()
{
	//Returns history length in number of frames
	return (uint32_t)(_history.size() * HistoryViewer::BufferSize);
}

void HistoryViewer::GetHistorySegments(uint32_t* segmentBuffer, uint32_t& bufferSize)
{
	uint32_t segmentIndex = 0;
	for (size_t i = 0; i < _history.size(); i++) {
		if (_history[i].EndOfSegment) {
			segmentBuffer[segmentIndex] = (uint32_t)i;
			segmentIndex++;

			if (segmentIndex == bufferSize) {
				//Reached buffer size, can't return any more values
				break;
			}
		}
	}
	bufferSize = segmentIndex;
}

uint32_t HistoryViewer::GetPosition()
{
	return _position;
}

void HistoryViewer::SeekTo(uint32_t seekPosition)
{
	//Seek to the specified position
	if (seekPosition < _history.size()) {
		_console->Lock();

		bool wasPaused = _console->IsPaused();
		_console->Resume();
		_position = seekPosition;
		RewindData rewindData = _history[_position];
		rewindData.LoadState(_console);

		_console->GetSoundMixer()->StopAudio(true);
		_pollCounter = 0;

		if (wasPaused) {
			_console->Pause();
		}

		_console->Unlock();
	}
}

bool HistoryViewer::CreateSaveState(string outputFile, uint32_t position)
{
	if (position < _history.size()) {
		std::stringstream stateData;
		_console->GetSaveStateManager()->GetSaveStateHeader(stateData);
		_history[position].GetStateData(stateData);

		ofstream output(outputFile, ios::binary);
		if (output) {
			output << stateData.rdbuf();
			output.close();
			return true;
		}
	}
	return false;
}

bool HistoryViewer::SaveMovie(string movieFile, uint32_t startPosition, uint32_t endPosition)
{
	//Take a savestate to be able to restore it after generating the movie file
	//(the movie generation uses the console's inputs, which could affect the emulation otherwise)
	stringstream state;
	_console->Lock();
	_console->GetSaveStateManager()->SaveState(state);

	//Convert the rewind data to a .mmo file
	unique_ptr<MovieRecorder> recorder(new MovieRecorder(_console));
	bool result = recorder->CreateMovie(movieFile, _history, startPosition, endPosition);

	//Resume the state and resume
	_console->GetSaveStateManager()->LoadState(state);
	_console->Unlock();
	return result;
}

// Copies a given state to the main window's state
void HistoryViewer::ResumeGameplay(shared_ptr<Console> console, uint32_t resumePosition)
{
	console->Lock();
	if (_console->GetRomInfo().RomFile.GetSha1Hash() != console->GetRomInfo().RomFile.GetSha1Hash()) {
		//Load game on the main window if they aren't the same		
		console->LoadRom(console->GetRomInfo().RomFile, console->GetRomInfo().PatchFile);
		// Mesen does console->Initialize(_console->GetRomPath(), _console->GetPatchFile());
		// but that's probably equivalent
	}
	if (resumePosition < _history.size()) {
		_history[resumePosition].LoadState(console);
	}
	else {
		_history[_history.size() - 1].LoadState(console);
	}
	console->Unlock();
}

bool HistoryViewer::SetInput(BaseControlDevice* device)
{

	uint8_t port = device->GetPort();
	if (_position < _history.size()) {
		std::deque<ControlDeviceState>& stateData = _history[_position].InputLogs[port];
		if (_pollCounter < stateData.size()) {
			ControlDeviceState state = stateData[_pollCounter];
			device->SetRawState(state);
		}
	}
	if (port == 0 && _pollCounter < HistoryViewer::BufferSize) {
		_pollCounter++;
	}
	return true;
}

void HistoryViewer::ProcessEndOfFrame()
{
	if (_pollCounter == HistoryViewer::BufferSize) {
		_pollCounter = 0;
		_position++;

		if (_position >= _history.size()) {
			//Reached the end of history data
			_console->Pause();
			return;
		}

		RewindData rewindData = _history[_position];
		rewindData.LoadState(_console);
	}
}
