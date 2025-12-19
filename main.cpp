#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <limits>
#include <windows.h>

class SoundData
{
public:
	SoundData();
	SoundData(int length, int bits, int fs);

	bool saveWAVFile(const char* filePath, int fadeWidth = 0);
	bool loadWAVFile(const char* filePath);

	void fade(int startWidth, int endWidth);

	void addWave(SoundData& wave, int start);
	bool saveCSVFile(const char* filePath);
	void createSinWave(double a, double f0, int wnum);
	void createSquareWave50(double a, double f0, int wnum);

	int length;
	int bits;
	int fs;
	std::vector<double> data;
};

struct Chunk
{
	char id[4];
};

const double pi = acos(-1.0);

int main(void)
{
	SoundData sound;
	if (!sound.loadWAVFile("input.wav"))
	{
		std::cerr << "WAV読み込みに失敗しました（PCM/mono/16bitのみ対応）\n";
		return 1;
	}

	for (int i = 0; i < 100 && i < sound.length; ++i)
	{
		std::cout << i << ": " << sound.data[i] << std::endl;
	}


	sound.saveCSVFile("waveform.csv");
	std::cout << "波形データを waveform.csv に保存しました\n";
	return 0;
	

	//音声データと波形データの出力
	/*
	SoundData sound(6 * 44100, 16, 44100);

	int soundWidth = 26460;
	double a = 0.2;
	double f[8] = { 523.25, 587.33,659.26, 698.45, 783.99, 880.0,
		987.77, 1046.5 };
	
	int pos = 0;
	for (int i = 0; i < 8; ++i)
	{
		SoundData s(soundWidth, 16, 44100);
		//s.createSinWave(a, f[i], 1);
		s.createSquareWave50(a, f[i], 50);
		s.fade(441, 441 * 2);
		sound.addWave(s, pos);
		pos += soundWidth + 1000;
	}

	sound.saveWAVFile("test.wav");
	std::cout << "サウンドファイルを保存しました！" << std::endl;

	sound.saveCSVFile("test.csv");	
	std::cout << "波形データを保存しました！" << std::endl;
	return 0;
	*/
}

SoundData::SoundData()
	: SoundData(44100, 16, 44100)
{
}

SoundData::SoundData(int length, int bits, int fs)
	: length(length)
	, bits(bits)
	, fs(fs)
	, data(length)
{
	for (size_t i = 0; i < data.size(); ++i)
	{
		data[i] = 0.0;
	}
}

bool SoundData::saveWAVFile(const char* filePath, int fadeWidth)
{
	std::ofstream fout;
	fout.open(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!fout) return false;

	//モノナル、2バイト量子化を前提に保存する波形を生成
	std::vector<short> wdata(data.size());
	for (int i = 0; i < length; ++i)
	{
		//　振幅を-1.0～1.0にクリッピング
		double amp = (data[i] > 1.0) ? 1.0 : data[i];
		amp = (amp < -1.0) ? -1.0 : amp;
		wdata[i] = (short)(amp * SHRT_MAX);
	}

	int32_t wsize = sizeof(short) * length;

	Chunk riffChunk = { {'R', 'I', 'F', 'F'} };
	int32_t riffSize = 12 + sizeof(PCMWAVEFORMAT) + 8 + wsize;
	Chunk waveChunk = { {'W', 'A', 'V', 'E'} };
	Chunk formatChunk = { {'f', 'm', 't', ' '} };
	int32_t fsize = sizeof(PCMWAVEFORMAT);
	PCMWAVEFORMAT wform;
	wform.wf.wFormatTag = 1;
	wform.wf.nChannels = 1;
	wform.wf.nSamplesPerSec = (DWORD)fs;
	wform.wf.nAvgBytesPerSec = (DWORD)(bits * fs / 8);
	wform.wf.nBlockAlign = (WORD)(bits / 8);
	wform.wBitsPerSample = bits; \
		Chunk dataChunk = { {'d', 'a', 't', 'a'} };
	int32_t dsize = wsize;
	fout.write((char*)&riffChunk, sizeof(Chunk));
	fout.write((char*)&riffSize, sizeof(int32_t));
	fout.write((char*)&waveChunk, sizeof(Chunk));
	fout.write((char*)&formatChunk, sizeof(Chunk));
	fout.write((char*)&fsize, sizeof(int32_t));
	fout.write((char*)&wform, sizeof(PCMWAVEFORMAT));
	fout.write((char*)&dataChunk, sizeof(Chunk));
	fout.write((char*)&dsize, sizeof(int32_t));
	fout.write((char*)wdata.data(), wsize);

	fout.close();

	return true;

}

void SoundData::fade(int startWidth, int endWidth)
{
	double a = 1.0;
	double startStep = 0.0;
	double endStep = 0.0;
	if (startWidth > 0)
	{
		a = 0.0;
		startStep = 1.0 / (double)startWidth;
	}
	if (endWidth > 0)
	{
		endStep = 1.0 / (double)endWidth;
	}

	for (size_t i = 0; i < data.size(); ++i)
	{

		data[i] *= a;
		if (i < startWidth)
		{
			a += startStep;
			a = (a > 1.0) ? 1.0 : a;
		}
		else if (i == startWidth)
		{
			a = 1.0;
		}
		else if (i >= data.size() - 1 - endWidth)
		{
			a -= endStep;
			a = (a < 0.0) ? 0.0 : a;
		}
	}
}

bool SoundData::saveCSVFile(const char* filePath)
{
	std::ofstream fout;
	fout.open(filePath, std::ios::out | std::ios::trunc);
	if (!fout) return false;

	fout << "標本化周波数" << fs << std::endl;
	fout << "量子化ビット数" << bits << std::endl;
	fout << "音の個数" << length << std::endl << std::endl;
	fout << "index, 時間[sec], 変位" << std::endl;
	double w = 1.0 / fs;
	for (int i = 0; i < length; ++i)
	{
		fout << i << "," << (double)i * w << ","
			<< data[i] << std::endl;
	}
	fout.close();

	return true;
}

void SoundData::createSinWave(double a, double f0, int wnum)
{
	for (int i = 0; i < length; ++i)
	{
		data[i] = 0.0;
		for (int j = 0; j < wnum; ++j)
		{
			data[i] += a * std::sin(2.0 * pi * (j + 1) * f0 * i / fs);
		}
	}
}

void SoundData::addWave(SoundData& wave, int start)
{
	for (int i = 0; i < wave.length; ++i)
	{
		int j = start + i;
		if (j >= length) break;

		data[j] += wave.data[i];
	}
}

void SoundData::createSquareWave50(double a, double f0, int wnum)
{
	double t = 1.0 / fs;
	double ampl = 4.0 * a / pi;
	for (int i = 0; i < length; ++i)
	{
		data[i] = 0.0;
		for (int j = 0; j < wnum; ++j)
		{
			int n = 2 * j + 1;
			data[i] += (1.0 / (double)n) * std::sin(2.0 * pi * n * f0 * t * i);
		}
		data[i] *= ampl;
	}
}

#pragma pack(push, 1)
struct RiffHeader
{
	char riff[4]; // "RIFF"
	uint32_t size;
	char wave[4]; // "WAVE"
};

struct ChunkHeader
{
	char id[4];
	uint32_t size;
};
#pragma pack(pop)

static bool fourccEquals(const char id[4], const char* s)
{
	return id[0] == s[0] && id[1] == s[1] && id[2] == s[2] && id[3] == s[3];
}

bool SoundData::loadWAVFile(const char* filePath)
{
	std::ifstream fin(filePath, std::ios::binary);
	if (!fin) return false;

	RiffHeader rh{};
	fin.read(reinterpret_cast<char*>(&rh), sizeof(rh));
	if (!fin) return false;

	if (!fourccEquals(rh.riff, "RIFF") || !fourccEquals(rh.wave, "WAVE"))
		return false;

	// fmt と data を探す
	bool fmtFound = false;
	bool dataFound = false;

	uint16_t audioFormat = 0;
	uint16_t numChannels = 0;
	uint32_t sampleRate = 0;
	uint16_t bitsPerSample = 0;

	std::vector<char> rawData;

	while (fin && !(fmtFound && dataFound))
	{
		ChunkHeader ch{};
		fin.read(reinterpret_cast<char*>(&ch), sizeof(ch));
		if (!fin) break;

		if (fourccEquals(ch.id, "fmt "))
		{
			std::vector<unsigned char> fmtBuf(ch.size);
			fin.read(reinterpret_cast<char*>(fmtBuf.data()), ch.size);
			if (!fin) return false;

			// fmt は最低 16 バイト必要
			if (ch.size < 16) return false;

			// 安全に little-endian 値を読み込む（memcpy）
			auto read_u16 = [&](size_t off) -> uint16_t {
				uint16_t v;
				std::memcpy(&v, fmtBuf.data() + off, sizeof(v));
				return v;
				};
			auto read_u32 = [&](size_t off) -> uint32_t {
				uint32_t v;
				std::memcpy(&v, fmtBuf.data() + off, sizeof(v));
				return v;
				};

			audioFormat = read_u16(0);
			numChannels = read_u16(2);
			sampleRate = read_u32(4);
			bitsPerSample = read_u16(14);

			// ここで出力（代入後）
			std::cout
				<< "fmt: audioFormat=" << audioFormat
				<< " channels=" << numChannels
				<< " sampleRate=" << sampleRate
				<< " bits=" << bitsPerSample
				<< std::endl;

			fmtFound = true;
		}
		else if (fourccEquals(ch.id, "data"))
		{
			rawData.resize(ch.size);
			fin.read(rawData.data(), ch.size);
			if (!fin) return false;

			dataFound = true;
		}
		else
		{
			// その他チャンクはスキップ
			fin.seekg(ch.size, std::ios::cur);
		}

		// WAVは偶数境界（パディング1バイト）を持つ場合がある
		if (ch.size % 2 == 1)
			fin.seekg(1, std::ios::cur);
	}

	if (!fmtFound || !dataFound) return false;

	// まずは「PCM(1), mono(1ch), 16bit」限定で実装
	if (audioFormat != 1 || numChannels != 1 || bitsPerSample != 16)
		return false;

	fs = static_cast<int>(sampleRate);
	bits = static_cast<int>(bitsPerSample);

	const size_t sampleCount = rawData.size() / sizeof(int16_t);
	length = static_cast<int>(sampleCount);
	data.assign(sampleCount, 0.0);

	const int16_t* p = reinterpret_cast<const int16_t*>(rawData.data());
	for (size_t i = 0; i < sampleCount; ++i)
	{
		// -32768..32767 を -1..1 に正規化
		data[i] = static_cast<double>(p[i]) / 32768.0;
	}

	return true;
}