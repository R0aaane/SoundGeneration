#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>
#include <cstring>

using Complex = std::complex<double>;
const double PI = acos(-1.0);

// 2チャンネル音声データ
struct StereoData
{
    int length;
    int fs;
    std::vector<double> ch0;
    std::vector<double> ch1;

    bool loadWAV(const char* filepath);
};

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
#pragma pack(pop)

static bool fourccEquals(const char id[4], const char* s)
{
    return id[0] == s[0] && id[1] == s[1] && id[2] == s[2] && id[3] == s[3];
}

bool StereoData::loadWAV(const char* filepath)
{
    std::ifstream fin(filepath, std::ios::binary);
    if (!fin) return false;

    RiffHeader rh{};
    fin.read(reinterpret_cast<char*>(&rh), sizeof(rh));
    if (!fin || !fourccEquals(rh.riff, "RIFF") || !fourccEquals(rh.wave, "WAVE"))
        return false;

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
            if (ch.size < 16) return false;
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

            std::cout << "Format: " << audioFormat << ", Channels: " << numChannels
                << ", Rate: " << sampleRate << ", Bits: " << bitsPerSample << std::endl;
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

    if (!fmtFound || !dataFound) return false;
    if (audioFormat != 1 || numChannels != 2 || bitsPerSample != 16)
    {
        std::cerr << "エラー: PCM/ステレオ(2ch)/16bit のみ対応" << std::endl;
        return false;
    }

    fs = static_cast<int>(sampleRate);
    const size_t totalSamples = rawData.size() / sizeof(int16_t);
    length = static_cast<int>(totalSamples / 2);

    ch0.resize(length);
    ch1.resize(length);

    const int16_t* p = reinterpret_cast<const int16_t*>(rawData.data());
    for (int i = 0; i < length; ++i)
    {
        ch0[i] = static_cast<double>(p[i * 2 + 0]) / 32768.0;
        ch1[i] = static_cast<double>(p[i * 2 + 1]) / 32768.0;
    }

    return true;
}

// 2x2 エルミート行列の固有値・固有ベクトルを解析的に計算
void eigen2x2(const Complex& r00, const Complex& r01, const Complex& r11,
    double& lambda1, double& lambda2,
    Complex& v1_0, Complex& v1_1,
    Complex& v2_0, Complex& v2_1)
{
    // 実対称（エルミート）行列の固有値
    double a = std::real(r00);
    double d = std::real(r11);
    double b = std::abs(r01); // |r01| (r01 = r10*)

    double trace = a + d;
    double det = a * d - b * b;
    double discriminant = trace * trace / 4.0 - det;

    if (discriminant < 0) discriminant = 0;

    lambda1 = trace / 2.0 + std::sqrt(discriminant);
    lambda2 = trace / 2.0 - std::sqrt(discriminant);

    // 固有ベクトル (lambda1 > lambda2 を仮定)
    if (std::abs(b) > 1e-10)
    {
        // v1 = [r01, lambda1 - a]^T (正規化前)
        Complex temp = r01;
        double norm = std::sqrt(std::norm(temp) + (lambda1 - a) * (lambda1 - a));
        v1_0 = temp / norm;
        v1_1 = Complex(lambda1 - a, 0) / norm;

        // v2 = [r01, lambda2 - a]^T (正規化前)
        norm = std::sqrt(std::norm(temp) + (lambda2 - a) * (lambda2 - a));
        v2_0 = temp / norm;
        v2_1 = Complex(lambda2 - a, 0) / norm;
    }
    else
    {
        // 対角行列の場合
        v1_0 = Complex(1, 0);
        v1_1 = Complex(0, 0);
        v2_0 = Complex(0, 0);
        v2_1 = Complex(1, 0);
    }
}

// 2チャンネル用のMUSIC法
std::vector<double> music2ch(const StereoData& data,
    double micSpacing,  // マイク間隔 [m]
    double frequency,   // 解析周波数 [Hz]
    int startSample,    // 解析開始位置
    int windowSize,     // 解析窓サイズ
    double soundSpeed = 343.0)
{
    if (startSample + windowSize > data.length)
    {
        windowSize = data.length - startSample;
    }

    // 共分散行列を計算 (2x2)
    Complex r00(0, 0), r01(0, 0), r11(0, 0);

    for (int i = startSample; i < startSample + windowSize; ++i)
    {
        double s0 = data.ch0[i];
        double s1 = data.ch1[i];

        r00 += Complex(s0 * s0, 0);
        r01 += Complex(s0 * s1, 0);
        r11 += Complex(s1 * s1, 0);
    }

    r00 /= double(windowSize);
    r01 /= double(windowSize);
    r11 /= double(windowSize);

    std::cout << "共分散行列:" << std::endl;
    std::cout << "  R[0][0] = " << r00 << std::endl;
    std::cout << "  R[0][1] = " << r01 << std::endl;
    std::cout << "  R[1][1] = " << r11 << std::endl;

    // 固有値分解
    double lambda1, lambda2;
    Complex v1_0, v1_1, v2_0, v2_1;
    eigen2x2(r00, r01, r11, lambda1, lambda2, v1_0, v1_1, v2_0, v2_1);

    std::cout << "固有値:" << std::endl;
    std::cout << "  λ1 = " << lambda1 << " (信号)" << std::endl;
    std::cout << "  λ2 = " << lambda2 << " (ノイズ)" << std::endl;

    // 2チャンネルの場合、音源は1つのみ推定可能
    // ノイズ部分空間 = 小さい固有値に対応する固有ベクトル (v2)
    Complex noise_v0 = v2_0;
    Complex noise_v1 = v2_1;

    // MUSICスペクトル計算 (-90° to +90°)
    std::vector<double> spectrum(181);
    double omega = 2.0 * PI * frequency;

    for (int i = 0; i <= 180; ++i)
    {
        double angle = (i - 90) * PI / 180.0; // -90° to +90°

        // ステアリングベクトル a(θ) = [1, e^(-jωτ)]^T
        double tau = micSpacing * std::sin(angle) / soundSpeed;
        Complex a0(1, 0);
        Complex a1 = std::exp(Complex(0, -omega * tau));

        // a^H * v_noise
        Complex inner = std::conj(a0) * noise_v0 + std::conj(a1) * noise_v1;
        double denominator = std::norm(inner);

        // MUSIC疑似スペクトル
        spectrum[i] = (denominator > 1e-10) ? 1.0 / denominator : 0.0;
    }

    return spectrum;
}

// スペクトルからピーク検出
double findPeakAngle(const std::vector<double>& spectrum)
{
    double maxVal = 0;
    int maxIdx = 0;

    for (int i = 0; i < spectrum.size(); ++i)
    {
        if (spectrum[i] > maxVal)
        {
            maxVal = spectrum[i];
            maxIdx = i;
        }
    }

    return (maxIdx - 90); // -90 to +90 度
}

// スペクトル保存
void saveSpectrum(const char* filename, const std::vector<double>& spectrum)
{
    std::ofstream fout(filename);
    if (!fout) return;

    fout << "角度[度],MUSIC疑似スペクトル" << std::endl;
    for (int i = 0; i < spectrum.size(); ++i)
    {
        double angle = i - 90;
        fout << angle << "," << spectrum[i] << std::endl;
    }
    fout.close();
}

int main()
{
    std::cout << "=== 2チャンネルMUSIC法による音源方向推定 ===" << std::endl << std::endl;

    // ステレオWAVファイル読み込み
    StereoData data;
    if (!data.loadWAV("input.wav"))
    {
        std::cerr << "WAV読み込み失敗（PCM/ステレオ/16bitのファイルを指定してください）" << std::endl;
        return 1;
    }

    std::cout << "読み込み成功!" << std::endl;
    std::cout << "  サンプル数: " << data.length << std::endl;
    std::cout << "  サンプリング周波数: " << data.fs << " Hz" << std::endl;
    std::cout << "  再生時間: " << (double)data.length / data.fs << " 秒" << std::endl;
    std::cout << std::endl;

    // パラメータ設定
    double micSpacing = 0.15;      // マイク間隔 15cm
    double frequency = 1000.0;     // 解析周波数 1kHz
    int startSample = 0;           // 解析開始位置
    int windowSize = std::min(8192, data.length); // 解析窓サイズ

    std::cout << "=== パラメータ ===" << std::endl;
    std::cout << "マイク間隔: " << micSpacing * 100 << " cm" << std::endl;
    std::cout << "解析周波数: " << frequency << " Hz" << std::endl;
    std::cout << "解析窓サイズ: " << windowSize << " サンプル ("
        << (double)windowSize / data.fs << " 秒)" << std::endl;
    std::cout << std::endl;

    // MUSIC法実行
    std::cout << "=== MUSIC法を実行中 ===" << std::endl;
    std::vector<double> spectrum = music2ch(data, micSpacing, frequency,
        startSample, windowSize);

    // ピーク検出
    double peakAngle = findPeakAngle(spectrum);

    std::cout << std::endl << "=== 推定結果 ===" << std::endl;
    std::cout << "推定された音源方向: " << peakAngle << " 度" << std::endl;
    std::cout << "  (0度 = マイク軸に垂直（正面）" << std::endl;
    std::cout << "  +90度 = マイク2の方向" << std::endl;
    std::cout << "  -90度 = マイク1の方向)" << std::endl;
    std::cout << std::endl;

    // 結果を保存
    saveSpectrum("music_spectrum_2ch.csv", spectrum);
    std::cout << "スペクトルを music_spectrum_2ch.csv に保存しました" << std::endl;

    return 0;
}