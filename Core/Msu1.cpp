#include "stdafx.h"
#include "Msu1.h"
#include "Spc.h"
#include "../Utilities/Serializer.h"
#include "../Utilities/FolderUtilities.h"

Msu1* Msu1::Init(VirtualFile romFile, Spc* spc)
{
	string romFolder = romFile.GetFolderPath();
	string romName = FolderUtilities::GetFilename(romFile.GetFileName(), false);

	// assume the MSU1 folder is the ROM folder
	string msu1Folder = romFolder;
	// if msu1.dir exists, read it and use its contents as the msu1 folder path
	ifstream msu1DirFile(FolderUtilities::CombinePath(romFolder, "msu1.dir"));
	if (msu1DirFile) {
		std::ostringstream sbuf;
		sbuf << msu1DirFile.rdbuf();
		msu1Folder = sbuf.str();
	}

	// try to calculate data file and track path
	string dataFilePath;
	string trackPath;
	if (ifstream(FolderUtilities::CombinePath(msu1Folder, romName + ".msu"))) {
		dataFilePath = FolderUtilities::CombinePath(msu1Folder, romName + ".msu");
		trackPath = FolderUtilities::CombinePath(msu1Folder, romName);
	} else if (ifstream(FolderUtilities::CombinePath(msu1Folder, "msu1.rom"))) {
		dataFilePath = FolderUtilities::CombinePath(msu1Folder, "msu1.rom");
		trackPath = FolderUtilities::CombinePath(msu1Folder, "track");
	}
	
	if (!dataFilePath.empty()) {
		return new Msu1(dataFilePath, trackPath, spc);
	} else {
		return nullptr;
	}
}

Msu1::Msu1(string dataFilePath, string trackPath, Spc* spc)
{
	_spc = spc;
	_dataFile.open(dataFilePath, ios::binary);
	_trackPath = trackPath;

	if(_dataFile) {
		_dataFile.seekg(0, ios::end);
		_dataSize = (uint32_t)_dataFile.tellg();
	} else {
		_dataSize = 0;
	}
}

void Msu1::Write(uint16_t addr, uint8_t value)
{
	switch(addr) {
		case 0x2000: _tmpDataPointer = (_tmpDataPointer & 0xFFFFFF00) | value; break;
		case 0x2001: _tmpDataPointer = (_tmpDataPointer & 0xFFFF00FF) | (value << 8); break;
		case 0x2002: _tmpDataPointer = (_tmpDataPointer & 0xFF00FFFF) | (value << 16); break;
		case 0x2003:
			_tmpDataPointer = (_tmpDataPointer & 0x00FFFFFF) | (value << 24);
			_dataPointer = _tmpDataPointer;
			_dataFile.seekg(_dataPointer, ios::beg);
			break;

		case 0x2004: _trackSelect = (_trackSelect & 0xFF00) | value; break;
		case 0x2005:
			_trackSelect = (_trackSelect & 0xFF) | (value << 8);
			LoadTrack();
			break;

		case 0x2006: _volume = value; break;
		case 0x2007:
			if(!_audioBusy) {
				_repeat = (value & 0x02) != 0;
				_paused = (value & 0x01) == 0;
				_pcmReader.SetLoopFlag(_repeat);
			}
			break;
	}
}

uint8_t Msu1::Read(uint16_t addr)
{
	switch(addr) {
		case 0x2000:
			//status
			return (_dataBusy << 7) | (_audioBusy << 6) | (_repeat << 5) | ((!_paused) << 4) | (_trackMissing << 3) | 0x01;

		case 0x2001:
			//data
			if(!_dataBusy && _dataPointer < _dataSize) {
				_dataPointer++;
				return (uint8_t)_dataFile.get();
			}
			return 0;

		case 0x2002: return 'S';
		case 0x2003: return '-';
		case 0x2004: return 'M';
		case 0x2005: return 'S';
		case 0x2006: return 'U';
		case 0x2007: return '1';
	}

	return 0;
}

void Msu1::MixAudio(int16_t* buffer, size_t sampleCount, uint32_t sampleRate)
{
	if(!_paused) {
		_pcmReader.SetSampleRate(sampleRate);
		_pcmReader.ApplySamples(buffer, sampleCount, _spc->IsMuted() ? 0 : _volume);
	}
}

void Msu1::LoadTrack(uint32_t startOffset)
{
	_trackMissing = !_pcmReader.Init(_trackPath + "-" + std::to_string(_trackSelect) + ".pcm", _repeat, startOffset);
}

void Msu1::Serialize(Serializer &s)
{
	uint32_t offset = _pcmReader.GetOffset();
	s.Stream(_trackSelect, _tmpDataPointer, _dataPointer, _repeat, _paused, _volume, _trackMissing, _audioBusy, _dataBusy, offset);
	if(!s.IsSaving()) {
		_dataFile.seekg(_dataPointer, ios::beg);
		LoadTrack(offset);
	}
}
