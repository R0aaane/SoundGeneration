#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <limits>
#include <Windows.h>

class SoundData
{
public:
	SoundData();
	SoundData(int length, int bits, int fs);

	bool saveWAVFile(const char* filePath);
	void fade(int startWidth, int endWidth);

	void addWave(SoundData& wave, int start);

	bool saveCSVFile(const char* filePath);
	void createSinWave(double a, double f0, int wnum);

	int length;					//　データの長さ
	int bits;					//　量子化ビット数
	int fs;						//　標本化周波数
	std::vector<double> data;	//　波形データ

};

struct Chunk
{
	char id[4];
};

const double pi = acos(-1.0);

int main(void)
{
	SoundData sound;

	double a = 0.05;
	double f0 = 500;


	sound.createSinWave(a, f0, 5);
	sound.fade(4410, 4410);
	sound.saveWAVFile("test.wav");
	std::cout << "サウンドファイルを保存しました。" << std::endl;
		

	sound.saveCSVFile("test.csv");
	std::cout << "波形データを保存しました！" << std::endl;
	return 0;
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
	for (size_t i =0; i < data.size(); ++i)
	{
		data[i] = 0.0;
	}
}

bool SoundData::saveWAVFile(const char* filePath)
{
	std::ofstream fout;
	fout.open(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!fout) return false;

	//モノラル、2バイト量子化を前提に保存する波形を生成
	std::vector<short> wdata(data.size());
	for (int i = 0; i < length; ++i)
	{
		//横幅を-1.0～1.0にクリッピング
		double amp = (data[i] > 1.0) ? 1.0 : data[i];
		amp = (amp < -1.0) ? -1.0 : amp;
		wdata[i] = (short)(amp * SHRT_MAX);

	}

	int32_t wsize = sizeof(short) * length;

	int32_t wsiez = sizeof(short) * length;

	Chunk riffChunk = { 'R', 'I', 'F','F' };
	int32_t riffSize = 12 + sizeof(PCMWAVEFORMAT) + 8 + wsize;
	Chunk waveChunk = { 'W', 'A', 'V', 'E' };
	Chunk formatChunk = { 'f', 'm', 't', ' ' };
	int32_t fsize = sizeof(PCMWAVEFORMAT);
	PCMWAVEFORMAT wform;
	wform.wf.wFormatTag = 1;
	wform.wf.nChannels = 1;
	wform.wf.nSamplesPerSec = (DWORD)fs;
	wform.wf.nAvgBytesPerSec = (DWORD)(bits * fs / 8);
	wform.wf.nBlockAlign = (WORD)(bits / 8);
	wform.wBitsPerSample = bits;
	Chunk dataChunk = { 'd', 'a', 't', 'a' };
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

	fout << "標本化周波数," << fs << std::endl;
	fout << "量子化ビット数," << bits << std::endl;
	fout << "音の個数," << length << std::endl << std::endl;
	fout << "index, 時間[sec],変位" << std::endl;
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