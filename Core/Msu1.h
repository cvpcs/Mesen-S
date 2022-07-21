#pragma once
#include "stdafx.h"
#include "PcmReader.h"
#include "../Utilities/ISerializable.h"
#include "../Utilities/VirtualFile.h"

class Spc;

class Msu1 final : public ISerializable
{
private:
	Spc * _spc;
	PcmReader _pcmReader;
	uint8_t _volume = 100;
	uint16_t _trackSelect = 0;
	uint32_t _tmpDataPointer = 0;
	uint32_t _dataPointer = 0;
	string _trackPath;

	bool _repeat = false;
	bool _paused = false;
	bool _audioBusy = false; //Always false
	bool _dataBusy = false; //Always false
	bool _trackMissing = false;

	ifstream _dataFile;
	uint32_t _dataSize;
	
	void LoadTrack(uint32_t startOffset = 8);

public:
	Msu1(string dataFilePath, string trackPath, Spc* spc);
	
	static Msu1* Init(VirtualFile romFile, Spc* spc);

	void Write(uint16_t addr, uint8_t value);
	uint8_t Read(uint16_t addr);
	
	void MixAudio(int16_t *buffer, size_t sampleCount, uint32_t sampleRate);
	
	void Serialize(Serializer &s);
};