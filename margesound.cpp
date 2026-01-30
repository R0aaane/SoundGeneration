#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

#pragma pack(push, 1)
struct RiffHeader
{
    char riff[4];
    uint32_t size;
    char wave[4];
};

struct ChunkHeader
{
    char id[4];
    uint32_t size;
};

struct WavFormat
{
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

static bool fourccEquals(const char id[4], const char* s)
{
    return id[0] == s[0] && id[1] == s[1] && id[2] == s[2] && id[3] == s[3];
}

// モノラルWAVファイルを読み込む
bool loadMonoWAV(const char* filepath, std::vector<double>& data, int& fs)
{
    std::ifstream fin(filepath, std::ios::binary);
    if (!fin)
    {
        std::cerr << "ファイルを開けません: " << filepath << std::endl;
        return false;
    }

    RiffHeader rh{};
    fin.read(reinterpret_cast<char*>(&rh), sizeof(rh));
    if (!fin || !fourccEquals(rh.riff, "RIFF") || !fourccEquals(rh.wave, "WAVE"))
    {
        std::cerr << "WAVファイルではありません: " << filepath << std::endl;
        return false;
    }

    bool fmtFound = false, dataFound = false;
    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
    std::vector<char> rawData;

    while (fin && !(fmtFound && dataFound))
    {
        ChunkHeader ch{};
        fin.read(reinterpret_cast<char*>(&ch), sizeof(ch));
        if (!fin) break;

        if (fourccEquals(ch.id, "fmt "))
        {
            if (ch.size < 16)
            {
                std::cerr << "fmtチャンクが不正: " << filepath << std::endl;
                return false;
            }

            std::vector<uint8_t> fmtBuf(ch.size);
            fin.read(reinterpret_cast<char*>(fmtBuf.data()), ch.size);
            if (!fin) return false;

            auto read_u16 = [&](size_t off) {
                uint16_t v;
                std::memcpy(&v, fmtBuf.data() + off, sizeof(v));
                return v;
                };
            auto read_u32 = [&](size_t off) {
                uint32_t v;
                std::memcpy(&v, fmtBuf.data() + off, sizeof(v));
                return v;
                };

            audioFormat = read_u16(0);
            numChannels = read_u16(2);
            sampleRate = read_u32(4);
            bitsPerSample = read_u16(14);

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
            fin.seekg(ch.size, std::ios::cur);
        }

        if (ch.size % 2 == 1) fin.seekg(1, std::ios::cur);
    }

    if (!fmtFound || !dataFound)
    {
        std::cerr << "必要なチャンクが見つかりません: " << filepath << std::endl;
        return false;
    }

    if (audioFormat != 1)
    {
        std::cerr << "PCM形式のみ対応: " << filepath << std::endl;
        return false;
    }

    if (numChannels != 1)
    {
        std::cerr << "モノラルファイルを指定してください: " << filepath
            << " (現在のチャンネル数: " << numChannels << ")" << std::endl;
        return false;
    }

    if (bitsPerSample != 16)
    {
        std::cerr << "16bitのみ対応: " << filepath << std::endl;
        return false;
    }

    fs = static_cast<int>(sampleRate);

    const size_t sampleCount = rawData.size() / sizeof(int16_t);
    data.resize(sampleCount);

    const int16_t* p = reinterpret_cast<const int16_t*>(rawData.data());
    for (size_t i = 0; i < sampleCount; ++i)
    {
        data[i] = static_cast<double>(p[i]) / 32768.0;
    }

    return true;
}

// ステレオWAVファイルを書き出す
bool saveStereoWAV(const char* filepath,
    const std::vector<double>& ch0,
    const std::vector<double>& ch1,
    int fs)
{
    if (ch0.size() != ch1.size())
    {
        std::cerr << "チャンネルのサイズが一致しません" << std::endl;
        return false;
    }

    std::ofstream fout(filepath, std::ios::binary | std::ios::trunc);
    if (!fout)
    {
        std::cerr << "ファイルを作成できません: " << filepath << std::endl;
        return false;
    }

    int length = static_cast<int>(ch0.size());

    // インターリーブされたステレオデータを生成
    std::vector<int16_t> stereoData(length * 2);
    for (int i = 0; i < length; ++i)
    {
        // クリッピング
        double val0 = std::max(-1.0, std::min(1.0, ch0[i]));
        double val1 = std::max(-1.0, std::min(1.0, ch1[i]));

        stereoData[i * 2 + 0] = static_cast<int16_t>(val0 * 32767.0);
        stereoData[i * 2 + 1] = static_cast<int16_t>(val1 * 32767.0);
    }

    // WAVヘッダを書き込む
    uint32_t dataSize = length * 2 * sizeof(int16_t);

    // RIFF header
    fout.write("RIFF", 4);
    uint32_t riffSize = 4 + 8 + 16 + 8 + dataSize;
    fout.write(reinterpret_cast<char*>(&riffSize), 4);
    fout.write("WAVE", 4);

    // fmt chunk
    fout.write("fmt ", 4);
    uint32_t fmtSize = 16;
    fout.write(reinterpret_cast<char*>(&fmtSize), 4);

    WavFormat fmt;
    fmt.audioFormat = 1;        // PCM
    fmt.numChannels = 2;        // ステレオ
    fmt.sampleRate = fs;
    fmt.byteRate = fs * 2 * 2;  // fs * channels * bytes_per_sample
    fmt.blockAlign = 4;         // channels * bytes_per_sample
    fmt.bitsPerSample = 16;
    fout.write(reinterpret_cast<char*>(&fmt), sizeof(fmt));

    // data chunk
    fout.write("data", 4);
    fout.write(reinterpret_cast<char*>(&dataSize), 4);
    fout.write(reinterpret_cast<char*>(stereoData.data()), dataSize);

    fout.close();
    return true;
}

int main(int argc, char* argv[])
{
    std::cout << "=== モノラルWAVファイル結合ツール ===" << std::endl << std::endl;

    const char* file1 = "input1.wav";
    const char* file2 = "input2.wav";
    const char* output = "output_stereo.wav";

    if (argc >= 4)
    {
        file1 = argv[1];
        file2 = argv[2];
        output = argv[3];
    }
    else
    {
        std::cout << "使い方: " << argv[0] << " <入力1.wav> <入力2.wav> <出力.wav>" << std::endl;
        std::cout << "デフォルト設定で実行します:" << std::endl;
        std::cout << "  入力1: " << file1 << std::endl;
        std::cout << "  入力2: " << file2 << std::endl;
        std::cout << "  出力: " << output << std::endl << std::endl;
    }

    // ファイル1を読み込み
    std::vector<double> ch0;
    int fs1 = 0;

    std::cout << "ファイル1を読み込み中: " << file1 << std::endl;
    if (!loadMonoWAV(file1, ch0, fs1))
    {
        return 1;
    }
    std::cout << "  サンプル数: " << ch0.size() << std::endl;
    std::cout << "  サンプリング周波数: " << fs1 << " Hz" << std::endl;
    std::cout << "  再生時間: " << (double)ch0.size() / fs1 << " 秒" << std::endl;

    // ファイル2を読み込み
    std::vector<double> ch1;
    int fs2 = 0;

    std::cout << std::endl << "ファイル2を読み込み中: " << file2 << std::endl;
    if (!loadMonoWAV(file2, ch1, fs2))
    {
        return 1;
    }
    std::cout << "  サンプル数: " << ch1.size() << std::endl;
    std::cout << "  サンプリング周波数: " << fs2 << " Hz" << std::endl;
    std::cout << "  再生時間: " << (double)ch1.size() / fs2 << " 秒" << std::endl;

    // サンプリング周波数の確認
    if (fs1 != fs2)
    {
        std::cerr << std::endl << "エラー: サンプリング周波数が一致しません ("
            << fs1 << " Hz vs " << fs2 << " Hz)" << std::endl;
        return 1;
    }

    // サンプル数を揃える
    size_t minLength = std::min(ch0.size(), ch1.size());
    if (ch0.size() != ch1.size())
    {
        std::cout << std::endl << "警告: サンプル数が異なるため、短い方に合わせます" << std::endl;
        std::cout << "  使用するサンプル数: " << minLength << std::endl;
        ch0.resize(minLength);
        ch1.resize(minLength);
    }

    // ステレオファイルを保存
    std::cout << std::endl << "ステレオファイルを作成中: " << output << std::endl;
    if (!saveStereoWAV(output, ch0, ch1, fs1))
    {
        return 1;
    }

    std::cout << std::endl << "成功！ステレオファイルを作成しました: " << output << std::endl;
    std::cout << "  チャンネル数: 2" << std::endl;
    std::cout << "  サンプル数: " << minLength << std::endl;
    std::cout << "  サンプリング周波数: " << fs1 << " Hz" << std::endl;
    std::cout << std::endl;
    std::cout << "次のステップ:" << std::endl;
    std::cout << "  1. music_2ch を実行して音源方向を推定" << std::endl;
    std::cout << "  2. input.wav を " << output << " にリネーム、または" << std::endl;
    std::cout << "     music_2ch.cpp 内のファイル名を変更してください" << std::endl;

    return 0;
}