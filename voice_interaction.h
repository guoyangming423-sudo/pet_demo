#ifndef VOICE_INTERACTION_H
#define VOICE_INTERACTION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <alsa/asoundlib.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <pocketsphinx.h>
#include <snowboy-detect.h>

// ==================== 全局宏定义 ====================
// 音频设备配置
#define AUDIO_DEVICE         "hw:0,0"       // ALSA音频设备名
#define CHANNELS             2               // 声道数（立体声）
#define WAVCHANNELS          1               // WAV文件声道数
#define SAMPLE_RATE          16000           // 采样率（必须和模型匹配）
#define BITS                 16              // 位深
#define RECORD_BUFFER_SIZE   2048            // 录音缓冲区大小
#define THREAD_BUFFER_SIZE   1024            // 监听线程缓冲区大小
#define SILENCE_THRESHOLD    100             // 静音检测阈值（音量）
#define MIN_RECORD_FRAMES    16000           // 最小录音帧数（1秒）才能停止录音
#define SILENCE_FRAMES       4               // 连续静音帧数（超过则停止录音）

// 文件路径配置
#define WAKEUP_PROMPT_FILE   "/tmp/wakeup_prompt.wav"  // 唤醒回复音频文件
#define RECORD_FILE          "/tmp/record.wav"         // 临时录音文件
#define TTS_OUTPUT_FILE      "/tmp/tts_output.wav"     // TTS输出音频文件

// snowboy 唤醒词检测配置
#define MODEL_PATH           "/root/models/snowboy.umdl"
#define RESOURCE_PATH        "/root/common.res"
#define BUFFER_SIZE          512                         // 唤醒检测缓冲区大小
#define VOLUME_THRESHOLD     50                          // 唤醒音量检测阀值

// 中断检测配置
#define PREBUFFER_FRAMES     8000             // 预缓冲帧数
#define INTERRUPT_POLL_US    20000            // 中断检测轮询时间（微秒）
#define INTERRUPT_THRESHOLD  300              // 中断检测阈值
#define INTERRUPT_IGNORE_US  300000           // 中断忽略时间（微秒）
#define INTERRUPT_CONFIRM_FRAMES 3            // 中断确认帧数

// 千问API配置（需替换为你的真实密钥/接口地址）
#define ASR_APPKEY           "your_appkey"           // 阿里云ASR AppKey
#define ASR_ACCESS_TOKEN     "your_access_token"            // 阿里云ASR Token
#define ASR_API_URL          "https://nls-gateway-cn-shanghai.aliyuncs.com/stream/v1/asr"  // ASR接口地址
#define QWEN_MODEL           "qwen-turbo"                // 千问对话模型
#define ALIYUN_API_KEY       "your_api_key"       // 阿里云API密钥
#define QWEN_TEXT_GENERATION_API_URL "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation"  // 对话接口
#define QWEN_TTS_API_URL "https://nls-gateway-cn-shanghai.aliyuncs.com/stream/v1/tts"  // TTS接口
#define TTS_VOICE            "aiqi"                // TTS音色

// 缓冲区大小限制
#define TOKEN_HEADER_BUF_SIZE  256
#define POST_DATA_MAX_LEN      2048
#define AUTH_STR_MAX_LEN       256
#define SYSTEM_CMD_MAX_LEN     256

// ==================== 结构体定义 ====================
// WAV文件头结构体
#pragma pack(push, 1)
typedef struct {
    char riff[4];         // "RIFF"
    int32_t  file_len;        // 文件总长度 - 8
    char wave[4];         // "WAVE"
    char fmt[4];          // "fmt "
    int32_t  fmt_len;         // 格式块长度（16 for PCM）
    int16_t audio_fmt;      // 音频格式（1=PCM）
    int16_t channels;       // 声道数
    int32_t  sample_rate;     // 采样率
    int32_t  byte_rate;       // 字节率 = 采样率 * 声道数 * 位深/8
    int16_t block_align;    // 块对齐 = 声道数 * 位深/8
    int16_t bits;           // 位深
    char data[4];         // "data"
    int32_t  data_len;        // 音频数据长度
} WavHeader;
#pragma pack(pop)

#endif
