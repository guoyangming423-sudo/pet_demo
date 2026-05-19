#include "voice_interaction.h"

// 函数声明
int init_audio();
int record_wav(const char *filename);
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
char *qwen_asr(const char *wav_file);
char *qwen_chat(const char *prompt);
int qwen_tts(const char *text, const char *out_wav);
int play_wav(const char *file);
int init_snowboy(void);
void uninit_snowboy(void);
int detect_wake(void);

// 打断模块相关函数声明
int interrput_init(void);
void interrput_uninit(void);
void monitor_thread_begin(void);
void interrput_end(void);
int interrput_requested(void);
size_t consume_prefix(int16_t *out, size_t max_frames);

int running = 1;   // 程序运行标志(1=运行，0=退出)
snd_pcm_t *catch_handle = NULL;   // ALSA录音设备句柄
static snowboy::SnowboyDetect* g_snowboy_detector = NULL;
pthread_mutex_t pcm_mutex = PTHREAD_MUTEX_INITIALIZER;

// 信号退出
void sig_handler(int sig) {
    running = 0;
}

int main()
{
    // 强制 stderr 无缓冲
    setbuf(stderr, NULL);
    fprintf(stderr, ">>> main: start\n");

    // 注册信号处理函数
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    fprintf(stderr, ">>> main: signal registered\n");

    // 初始化音频采集
    fprintf(stderr, ">>> main: call init_audio()\n");
    if(init_audio() != 0){
        fprintf(stderr, ">>> main: init_audio failed, exit\n");
        return -1;
    }

    // 初始化 Snowboy 唤醒词检测
    fprintf(stderr, ">>> main: call init_snowboy()\n");
    if(init_snowboy() != 0){
        snd_pcm_close(catch_handle);
        catch_handle = NULL;
        fprintf(stderr, ">>> main: init_snowboy failed, exit\n");
        return -1;
    }

    // 初始化监听线程
    fprintf(stderr, ">>> main: call interrput_init()\n");
    if(interrput_init() != 0){
        uninit_snowboy();
        if(catch_handle != NULL){
            snd_pcm_close(catch_handle);
            catch_handle = NULL;
        }
        fprintf(stderr, ">>> main: interrupt_system_init failed, exit\n");
        return -1;
    }

    while(running){
        // 等待唤醒词检测（阻塞，直到检测到唤醒词或程序退出）
        int wake_ret = 0;
        int wake_play_ret = 0;
        wake_ret = detect_wake();
        if(!running)  break;
        if(wake_ret != 1)  continue;

        if(catch_handle != NULL){
            pthread_mutex_lock(&pcm_mutex);
            int ret = snd_pcm_drop(catch_handle);
            if(ret < 0){
                fprintf(stderr, "snd_pcm_drop error: %s\n", snd_strerror(ret));
            }
            ret = snd_pcm_prepare(catch_handle);
            if(ret < 0){
                fprintf(stderr, "snd_pcm_prepare error: %s\n", snd_strerror(ret));
            }
            pthread_mutex_unlock(&pcm_mutex);
        }

        const char *wakeup_text = "我在";
        if(qwen_tts(wakeup_text, WAKEUP_PROMPT_FILE) == 0)  wake_play_ret = play_wav(WAKEUP_PROMPT_FILE);
        else  fprintf(stderr, "wakeup_text's play_wav error\n");
        if(wake_play_ret != 1){
            usleep(500 * 1000);
        }

        // 检测到唤醒词，进入连续对话模式
        fprintf(stderr, "==== 进入连续对话模式 ====\n");   // 调试
        while(running){
            if(record_wav(RECORD_FILE) != 0)  continue;   // 开始录音
            struct stat st;
            if(stat(RECORD_FILE, &st) != 0 || st.st_size == 0){
                fprintf(stderr, "record_wav error or no audio captured\n");
                continue;
            }

            // 语音转文字
            fprintf(stderr, ">>> main: call qwen_asr()\n");
            char *text = qwen_asr(RECORD_FILE);
            usleep(100 * 1000);
            if(text == NULL)  continue;

            // 判断是否退出连续对话
            if(strstr(text, "退出") != NULL){
                free(text);
                break; // 跳出连续对话循环，回到唤醒监听
            }

            // 生成大模型回答文本
            fprintf(stderr, ">>> main: call qwen_chat()\n");
            char *reply = qwen_chat(text);
            free(text);
            if(reply == NULL)  continue;

            // 文字转语音（TTS）并播放
            fprintf(stderr, ">>> main: call qwen_tts()\n");
            if(qwen_tts(reply, TTS_OUTPUT_FILE) != 0){
                free(reply);
                continue;
            }
            int play_ret = play_wav(TTS_OUTPUT_FILE);
            if(play_ret < 0){
                fprintf(stderr, "main's play_wav error\n");
            }else if(play_ret == 1){
                fprintf(stderr, "main: reply playback interrupted\n");
            }

            free(reply);
        }
    }
    // 退出程序清理资源
    uninit_snowboy();
    interrput_uninit();
    if(catch_handle != NULL){
        snd_pcm_close(catch_handle);
        catch_handle = NULL;
    }

    return 0;
}

// 音频采集初始化(ALSA)
int init_audio(){
    int err;
    snd_pcm_hw_params_t* params;

    // 打开音频设备
    err = snd_pcm_open(&catch_handle, AUDIO_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
    if(err < 0){
        fprintf(stderr, "init_audio's snd_pcm_open error: %s (device: %s)\n", snd_strerror(err), AUDIO_DEVICE);
        return -1;
    }

    // 分配参数结构体，配置录音参数
    snd_pcm_hw_params_alloca(&params);
    err = snd_pcm_hw_params_any(catch_handle, params);
    if(err < 0){
        fprintf(stderr, "snd_pcm_hw_params_any error: %s\n", snd_strerror(err));
        snd_pcm_close(catch_handle);
        catch_handle = NULL;
        return -1;
    }
    err = snd_pcm_hw_params_set_access(catch_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(err < 0){
        fprintf(stderr, "snd_pcm_hw_params_set_access error: %s\n", snd_strerror(err));
        snd_pcm_close(catch_handle);
        catch_handle = NULL;
        return -1;
    }
    err = snd_pcm_hw_params_set_format(catch_handle, params, SND_PCM_FORMAT_S16_LE);
    if(err < 0){
        fprintf(stderr, "snd_pcm_hw_params_set_format error: %s\n", snd_strerror(err));
        snd_pcm_close(catch_handle);
        catch_handle = NULL;
        return -1;
    }
    err = snd_pcm_hw_params_set_channels(catch_handle, params, CHANNELS);
    if(err < 0){
        fprintf(stderr, "snd_pcm_hw_params_set_channels error: %s\n", snd_strerror(err));
        snd_pcm_close(catch_handle);
        catch_handle = NULL;
        return -1;
    }

    // 设置采样率（16000Hz）
    unsigned int rate = SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(catch_handle, params, &rate, 0);
    if(err < 0){
        fprintf(stderr, "snd_pcm_hw_params_set_rate_near error: %s\n", snd_strerror(err));
        snd_pcm_close(catch_handle);
        catch_handle = NULL;
        return -1;
    }

    // 应用参数到设备
    err = snd_pcm_hw_params(catch_handle, params);
    if(err < 0){
        fprintf(stderr, "snd_pcm_hw_params error: %s\n", snd_strerror(err));
        snd_pcm_close(catch_handle);
        catch_handle = NULL;
        return -1;
    }

    return 0;
}

// 录音为 WAV (自动停录)
int record_wav(const char *filename){
    FILE* f = fopen(filename, "wb");
    if(!f){
        perror("record_wav's fopen error");
        return -1;
    }

    // 写入临时WAV头
    WavHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.riff, "RIFF", 4);   // 声明文件是 RIFF 格式
    memcpy(hdr.wave, "WAVE", 4);   // 声明是 WAV 音频
    memcpy(hdr.fmt, "fmt ", 4);   // 声明接下来是格式参数
    memcpy(hdr.data, "data", 4);   // 声明接下来是音频数据

    hdr.fmt_len = 16;
    hdr.audio_fmt = 1;
    hdr.channels = WAVCHANNELS;
    hdr.sample_rate = SAMPLE_RATE;
    hdr.bits = BITS;
    hdr.block_align = WAVCHANNELS * (BITS / 8);
    hdr.byte_rate = SAMPLE_RATE * hdr.block_align;
    hdr.data_len = 0;
    hdr.file_len = 36 + hdr.data_len;
    fwrite(&hdr, sizeof(hdr), 1, f);

    int16_t buf[RECORD_BUFFER_SIZE * CHANNELS];
    int16_t mono_buf[RECORD_BUFFER_SIZE];
    int frames = RECORD_BUFFER_SIZE / CHANNELS;
    int16_t prefix_buf[PREBUFFER_FRAMES];
    int total_frames = 0;   // 录音总帧数的 “计数器”
    int silence_count = 0;   // 连续静音计数
    int speech_detected = 0;   // 是否检测到语音的标志
    size_t prefix_frames;   // 前缀缓冲区音频帧数

    prefix_frames = consume_prefix(prefix_buf, PREBUFFER_FRAMES);   // 获取监听线程前缀音频
    if(prefix_frames > 0){
        fwrite(prefix_buf, sizeof(int16_t), prefix_frames, f);
        total_frames = (int)prefix_frames;
        speech_detected = 1;
    }


    pthread_mutex_lock(&pcm_mutex);
    snd_pcm_prepare(catch_handle);   // 确保设备准备好
    pthread_mutex_unlock(&pcm_mutex);

    while(running){
        // 从 PCM 捕获设备读取交织帧到缓冲区
        pthread_mutex_lock(&pcm_mutex);
        int read_frames = snd_pcm_readi(catch_handle, buf, frames);
        pthread_mutex_unlock(&pcm_mutex);
        if(read_frames < 0){
            fprintf(stderr, "record_wav's snd_pcm_readi error: %s\n", snd_strerror(read_frames));
            if(read_frames == -EPIPE || read_frames == -ESTRPIPE){
                pthread_mutex_lock(&pcm_mutex);
                int prepare_ret = snd_pcm_prepare(catch_handle);
                pthread_mutex_unlock(&pcm_mutex);
                if(prepare_ret < 0){
                    fprintf(stderr, "snd_pcm_prepare error: %s\n", snd_strerror(prepare_ret));
                    break;
                }
            }else{
                perror("record_wav's snd_pcm_readi EBADFD error");
                break;
            }
            continue;
        }

        for(int i = 0; i < read_frames; i++){
            if(CHANNELS == 2)  mono_buf[i] = (buf[i*2] + buf[i*2 + 1]) / 2;   // 双声道取平均
            else  mono_buf[i] = buf[i];   // 单声道直接使用
        }

        // 去除 DC 偏置
        long sum = 0;
        for(int i = 0; i < read_frames; i++){
            sum += mono_buf[i];
        }
        int avg = sum / read_frames;
        for(int i = 0; i < read_frames; i++){
            mono_buf[i] -= avg;
        }

        // 加大音量
        int gain = 2;
        for(int i = 0; i < read_frames; i++){
            int32_t tmp = mono_buf[i] * gain;
            if(tmp > INT16_MAX)  tmp = INT16_MAX;
            else if(tmp < INT16_MIN)  tmp = INT16_MIN;
            mono_buf[i] = tmp;
        }

        // 计算平均音量作为静音判定阈值
        long long sum_volume = 0;
        //int samples = read_frames * CHANNELS;
        for(int i = 0; i < read_frames; i++){
            sum_volume += abs(mono_buf[i]);
        }
        float avg_volume = (float)sum_volume / read_frames;
        if(avg_volume > SILENCE_THRESHOLD){
            speech_detected = 1;
        }
        fprintf(stderr, "record_wav: read_frames=%d, avg_volume=%.2f\n", read_frames, avg_volume);

        fwrite(mono_buf, sizeof(int16_t), read_frames, f);
        total_frames += read_frames;
        fprintf(stderr, "read_frames: %d, total_frames: %d\n", read_frames, total_frames);

        //静音检测,满 SILENCE_FRAMES 停录
        if(total_frames < MIN_RECORD_FRAMES)  continue;
        if(avg_volume < SILENCE_THRESHOLD){
            silence_count++;
            if(silence_count >= SILENCE_FRAMES){
                break;
            }
        }else{
            silence_count = 0;
        }
    }
    fprintf(stderr, "WavHeader size: %zu\n", sizeof(WavHeader));

        if(speech_detected == 0){
            fprintf(stderr, "record_wav: no speech detected, deleting file\n");
            fclose(f);
            return -1;
        }

        // 更新真实WAV头长度
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);

        int32_t correct_data_len = file_size - 44;
        int32_t correct_file_len = file_size - 8;

        fseek(f, 4, SEEK_SET);
        fwrite(&correct_file_len, 4, 1, f);

        fseek(f, 40, SEEK_SET);
        fwrite(&correct_data_len, 4, 1, f);

        fclose(f);
        return 0;
}

//CURL 回调函数
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata){
    char **out = (char **)userdata;   // 强转为 char**
    size_t len = size * nmemb;   // 计算 curl 本次传给回调函数的[实际数据总字节数]
    if(len == 0){
        return 0;
    }
    size_t old_len = (*out != NULL) ? strlen(*out) : 0;

    // 扩容内存
    char *temp = (char*)realloc(*out, old_len + len + 1);
    if(temp == NULL){
        fprintf(stderr, "write_callback's realloc error");
        return 0;
    }
    *out = temp;   // 确认扩容成功后，更新指针

    memcpy(*out + old_len, ptr, len);   // 将返回的原始数据复制到自己分配的内存里
    *(*out + old_len +len) = '\0';   // 结尾下标：专门放 \0
    
    return len;
}

// 千问 ASR：音频转文字
char *qwen_asr(const char *wav_file){
    CURL *curl = curl_easy_init();
    if(curl == NULL){
        fprintf(stderr, "qwen_asr's curl_easy_init error\n");
        return NULL;
    }

    char *resp = NULL;
    struct curl_slist *headers = NULL;
    CURLcode res;
    cJSON *root = NULL;
    int ret = 0;
    char *text = NULL;
    FILE *fp = NULL;
    long file_size = 0;
    char *audio_buf = NULL;

    // 1. 读取音频文件（二进制流）
    fp = fopen(wav_file, "rb");
    if(!fp){
        fprintf(stderr, "open wav file error: %s\n", wav_file);
        curl_easy_cleanup(curl);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    audio_buf = (char*)malloc(file_size);
    if(!audio_buf){
        fclose(fp);
        curl_easy_cleanup(curl);
        return NULL;
    }
    fread(audio_buf, 1, file_size, fp);
    fclose(fp);

    // 2. 拼接 URL（参数全部放 Query 里！）
    char url[1024];
    snprintf(url, sizeof(url),
             "%s?appkey=%s&format=wav&sample_rate=16000&enable_punctuation_prediction=true",
             ASR_API_URL, ASR_APPKEY);

    // 3. 构建请求头
    char token_header[512];
    snprintf(token_header, sizeof(token_header), "X-NLS-Token: %s", ASR_ACCESS_TOKEN);
    headers = curl_slist_append(headers, token_header);
    if(headers == NULL){
        fprintf(stderr, "token_header's curl_slist_append error\n");
        goto clean_mime;
    }
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    if(headers == NULL){
        fprintf(stderr, "Content-Type's curl_slist_append error\n");
        goto clean_mime;
    }

    char content_len[32];
    snprintf(content_len, sizeof(content_len), "Content-Length: %ld", file_size);
    headers = curl_slist_append(headers, content_len);
    if(headers == NULL){
        fprintf(stderr, "Content-Length's curl_slist_append error\n");
        goto clean_mime;
    }

    char host_header[64];
    snprintf(host_header, sizeof(host_header), "Host: nls-gateway-cn-shanghai.aliyuncs.com");
    headers = curl_slist_append(headers, host_header);
    if(headers == NULL){
        fprintf(stderr, "Host's curl_slist_append error\n");
        goto clean_mime;
    }

    // 4. 配置 curl（POST + 二进制流）
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, audio_buf);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, file_size);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    // 5. 执行请求
    res = curl_easy_perform(curl);
    fprintf(stderr, "Debug: curl执行结果码: %d, 返回数据：%s\n", res, resp ? resp : "NULL");

    if(res != CURLE_OK){
        fprintf(stderr, "curl_easy_perform error: %s\n", curl_easy_strerror(res));
        free(resp);
        resp = NULL;
        goto clean_mime;
    }

    // 6. 解析结果（按官方 JSON 结构）
    root = cJSON_Parse(resp);
    if(root){
        cJSON *status = cJSON_GetObjectItem(root, "status");
        cJSON *result = cJSON_GetObjectItem(root, "result");

        if(status && cJSON_IsNumber(status) && status->valueint == 20000000 && result && result->valuestring){
            text = strdup(result->valuestring);
        }else{
            fprintf(stderr, "ASR failed, status: %d, msg: %s\n",
                    status ? status->valueint : -1,
                    cJSON_GetObjectItem(root, "message") ? cJSON_GetObjectItem(root, "message")->valuestring : "unknown");
        }
        cJSON_Delete(root);
    }

    clean_mime:
        if(audio_buf) free(audio_buf);
        if(resp) free(resp);
        if(headers) curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return text;
}

// 千问大模型对话
char *qwen_chat(const char *prompt){
    CURL *curl = curl_easy_init();
    if(curl == NULL){
        fprintf(stderr, "qwen_chat's curl_easy_init error\n");
        return NULL;
    }

    char *resp = NULL;   // 存储 API 返回的原始字符串（JSON 格式）
    struct curl_slist *headers = NULL;   // HTTP 请求头指针
    struct curl_slist *new_headers = NULL;
    CURLcode curl_ret;
    cJSON *root = NULL;
    char *reply = NULL;   // 存储 AI 的回答文本

    char post_data[POST_DATA_MAX_LEN];
    int ret = snprintf(post_data, POST_DATA_MAX_LEN, 
            "{\"model\":\"%s\",\"input\":{\"prompt\":\"%s\"}}", 
            QWEN_MODEL, prompt);
    if(ret < 0 || ret >= POST_DATA_MAX_LEN){
        fprintf(stderr, "POST_DATA_MAX_LEN error\n");
        goto cleanup;
    }
    
    // 构造符合千问 API 要求的 HTTP 请求头
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if(headers == NULL){
        fprintf(stderr, "qwen_chat's Content-Type's curl_slist_append error\n");
        goto cleanup;
    }

    char auth[AUTH_STR_MAX_LEN];   // 存储 HTTP 请求头中的认证字符串
    ret = snprintf(auth, AUTH_STR_MAX_LEN, "Authorization: Bearer %s", ALIYUN_API_KEY);
    if(ret < 0 || ret >= AUTH_STR_MAX_LEN){
        fprintf(stderr, "qwen_chat's AUTH_STR_MAX_LEN error\n");
        goto cleanup;
    }
    new_headers = curl_slist_append(headers, auth);   // 把认证头添加到请求头链表
    if(new_headers == NULL){
        fprintf(stderr, "qwen_chat's auth's curl_slist_append error\n");
        goto cleanup;
    }
    headers = new_headers;

    // 配置curl参数
    curl_easy_setopt(curl, CURLOPT_URL, QWEN_TEXT_GENERATION_API_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    // 执行请求
    curl_ret = curl_easy_perform(curl);
    if(curl_ret != CURLE_OK){
        fprintf(stderr, "qwen_chat's curl_easy_perform error: %s\n", curl_easy_strerror(curl_ret));
        free(resp);
        resp = NULL;
        goto cleanup;
    }

    // 解析JSON结果
    root = cJSON_Parse(resp);
    if(root){
        cJSON *output = cJSON_GetObjectItem(root, "output");
        if(output){
            cJSON *text = cJSON_GetObjectItem(output, "text");
            if(text && cJSON_IsString(text) && text->valuestring != NULL && strlen(text->valuestring) > 0) {
                reply = strdup(text->valuestring);
                if(reply == NULL){
                    fprintf(stderr, "qwen_chat: strdup malloc failed\n");
                }
            }else{
                fprintf(stderr, "qwen_chat: output.text is null or not string\n");
            }
        }else{
            fprintf(stderr, "qwen_chat: no output field in resp\n");
            cJSON *msg = cJSON_GetObjectItem(root, "message");
            if(msg && cJSON_IsString(msg)){
                fprintf(stderr, "qwen_chat: API error message: %s\n", msg->valuestring);
            }
        }
        cJSON_Delete(root);
    }else{
        fprintf(stderr, "qwen_chat's cJSON_Parse error: %s\n", resp ? resp : "NULL");
    }

    if(resp){
        free(resp);
        resp = NULL;
    }

    // 成功路径下资源释放
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return reply;

    cleanup:
        if(headers)  curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return NULL;
}

// 千问 TTS：文字转语音
int qwen_tts(const char *text, const char *out_wav){
    CURL *curl = curl_easy_init();
    if(curl == NULL){
        fprintf(stderr, "qwen_tts's curl_easy_init error\n");
        return -1;
    }

    struct curl_slist *headers = NULL;   // HTTP 请求头指针
    CURLcode curl_code;
    FILE *f = NULL;
    long http_code = 0;
    struct curl_slist *new_headers = NULL;
    int ret = 0;

    // 打开输出文件
    f = fopen(out_wav, "wb");
    if(f == NULL){
        perror("can't open out_wav");
        goto tts_cleanup;
    }

    // 构造JSON请求体
    char post_data[POST_DATA_MAX_LEN];
    ret = snprintf(post_data, POST_DATA_MAX_LEN,
    "{"
    "\"appkey\":\"%s\","
    "\"text\":\"%s\","
    "\"token\":\"%s\","
    "\"format\":\"wav\","
    "\"sample_rate\":16000,"
    "\"voice\":\"%s\""
    "}",
    ASR_APPKEY,  
    text,        
    ASR_ACCESS_TOKEN, 
    TTS_VOICE);  
    if(ret < 0 || ret >= POST_DATA_MAX_LEN){
        fprintf(stderr, "POST_DATA_MAX_LEN error\n");
        goto tts_cleanup;
    }

    // 构造 HTTP 请求头
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if(headers == NULL){
        fprintf(stderr, "qwen_tts's Content-Type's curl_slist_append error\n");
        goto tts_cleanup;
    }

    char auth[AUTH_STR_MAX_LEN];   // 请求头中的认证字符串
    ret = snprintf(auth, AUTH_STR_MAX_LEN, "Authorization: Bearer %s", ALIYUN_API_KEY);
    if(ret < 0 || ret >= AUTH_STR_MAX_LEN){
        fprintf(stderr, "qwen_tts's AUTH_STR_MAX_LEN error\n");
        goto tts_cleanup;
    }
    new_headers = curl_slist_append(headers, auth);
    if(new_headers == NULL){
        fprintf(stderr, "qwen_tts's auth's curl_slist_append error\n");
        goto tts_cleanup;
    }
    headers = new_headers;

    // 配置CURL请求
    curl_easy_setopt(curl, CURLOPT_URL, QWEN_TTS_API_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);   // 连接超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);   // 总请求超时

    // 执行请求
    curl_code = curl_easy_perform(curl);
    if(curl_code != CURLE_OK){
        fprintf(stderr, "qwen_tts's curl_easy_perform error: %s\n", curl_easy_strerror(curl_code));
        goto tts_cleanup;
    }

    // 检查 HTTP 状态码
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if(http_code != 200){
        fprintf(stderr, "return's http_code error: %ld\n", http_code);
        goto tts_cleanup;
    }

    char *content_type;       // 调试
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
    printf("Content-Type: %s\n", content_type);

    // 正确路径下释放资源
    fclose(f);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 0;

    tts_cleanup:
        if(f)  fclose(f);
        remove(out_wav);
        if(headers)  curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
}

// 播放 WAV 音频
int play_wav(const char *file){
    if(file == NULL){
        fprintf(stderr, "play_wav error: input file path is NULL\n");
        return -1;
    }

    // 检查文件扩展名是否为 .wav
    const char *ext = strrchr(file, '.');
    if(ext == NULL || strcasecmp(ext, ".wav") != 0){
        fprintf(stderr, "play_wav error: file '%s' is not a WAV file\n", file);
        return -1;
    }
    
    char cmd[SYSTEM_CMD_MAX_LEN];
    int ret = snprintf(cmd, SYSTEM_CMD_MAX_LEN, "aplay %s",file);
    if(ret < 0 || ret >= SYSTEM_CMD_MAX_LEN){
        fprintf(stderr, "play_wav's SYSTEM_CMD_MAX_LEN error\n");
        return -1;
    }

    // 执行播放命令
    pid_t pid = fork();
    if(pid < 0){
        perror("play_wav's fork error");
        return -1;
    }else if(pid == 0){
        // 子进程，执行 aplay
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        // 若执行到这里，说明execl失败
        perror("play_wav's execl error");
        exit(16);
    }else{
        // 父进程，等待子进程结束
        int status;
        int interrupt = 0;
        monitor_thread_begin();

        while(running){
            pid_t wpid = waitpid(pid, &status, WNOHANG);
            if(wpid == pid)  break;   // 子进程自己结束，退出循环
            if(wpid == -1){
                perror("play_wav's waitpid error");
                interrput_end();
                return -1;
            }
            if(interrput_requested()){
                interrupt = 1;
                fprintf(stderr, "play_wav: interrupt requested, stop aplay\n");

                kill(pid, SIGTERM);
                for(int i = 0; i < 20; i++){   // 先非阻塞等待一下子进程
                    wpid = waitpid(pid, &status, WNOHANG);
                    if(wpid == pid)  break;
                    usleep(10 * 1000);
                }
                if(waitpid(pid, &status, WNOHANG) == 0){
                    kill(pid, SIGKILL);   // 如果阻塞等待还没退出，直接杀死
                    waitpid(pid, &status, 0);
                }
                break;
            }
            usleep(INTERRUPT_POLL_US);
        }
        if(!running){
            kill(pid, SIGTERM);
            waitpid(pid, &status, 0);
        }
        interrput_end();
        if(interrupt)  return 1;

        if(WIFEXITED(status)){
            int exit_code = WEXITSTATUS(status);
            if(exit_code != 0){
                fprintf(stderr, "play_wav error: aplay exited with code %d\n", exit_code);
                return -1;
            }
            return 0;
        }else{
            fprintf(stderr, "play_wav process Exit abnormal\n");
            return -1;
        }
    }
}

// 初始化 Snowboy
int init_snowboy(void){
    fprintf(stderr, ">>>> init_snowboy: start\n");
    // 防止重复初始化
    if(g_snowboy_detector != NULL){
        fprintf(stderr, "init_snowboy error: already init.call uninit_snowboy first\n");
        return -1;
    }

    // 初始化配置
    fprintf(stderr, ">>>> init_snowboy: call g_snowboy_detector\n");
    g_snowboy_detector = new snowboy::SnowboyDetect(RESOURCE_PATH, MODEL_PATH);
    if(g_snowboy_detector == NULL){
        fprintf(stderr, "init_snowboy: create detector failed\n");
        return -1;
    }
    fprintf(stderr, ">>>> init_snowboy: g_snowboy_detector OK\n");

    // 设置配置参数
    g_snowboy_detector->SetSensitivity("1.0");
    g_snowboy_detector->SetAudioGain(3.0);
    g_snowboy_detector->ApplyFrontend(true);

    fprintf(stderr, ">>>> init_snowboy: init done\n");

    return 0;   // 成功
}

// 配套的清理函数
void uninit_snowboy(void){
    if(g_snowboy_detector != NULL){
        delete g_snowboy_detector;
        g_snowboy_detector = NULL;
    }
}

// 唤醒词检测
int detect_wake(void){
    int16_t buf[BUFFER_SIZE * CHANNELS];
    int16_t mono_buf[BUFFER_SIZE];
    while(running){
        pthread_mutex_lock(&pcm_mutex);
        int n = snd_pcm_readi(catch_handle, buf, BUFFER_SIZE);
        pthread_mutex_unlock(&pcm_mutex);
        if(n < 0){
            fprintf(stderr, "detect_wake's snd_pcm_readi error: %s\n", snd_strerror(n));
            if(snd_pcm_prepare(catch_handle) < 0){
                fprintf(stderr, "detect_wake's snd_pcm_readi EBADFD error\n");
                return -1;   // 严重错误无法恢复
            }
            continue;
        }
        if(n == 0)  continue;

        for(int i = 0; i < n; i++){
            if(CHANNELS == 2)  mono_buf[i] = (buf[i*2] + buf[i*2 + 1]) / 2;
            else  mono_buf[i] = buf[i];
        }

        // 去 DC 偏置
        long sum = 0;
        for(int i = 0; i < n; i++){
            sum += mono_buf[i];
        }
        int offset = sum / n;
        for(int i = 0; i < n; i++){
            mono_buf[i] -= offset;
        }

        // 计算音量
        long long sum_volume = 0;
        for(int i = 0; i < n; i++){
            sum_volume += abs(mono_buf[i]);
        }
        float avg_volume = (float)sum_volume / n;
        if(avg_volume < VOLUME_THRESHOLD){
            continue;
        }
        fprintf(stderr,"volume=%f\n", avg_volume);

        // 调用 Snowboy 检测唤醒词
        int result = g_snowboy_detector->RunDetection(mono_buf, n);
        fprintf(stderr,"snowboy result = %d\n",result);
        if(result == 1){
            fprintf(stderr, "==== 检测到唤醒词「snowboy」====\n");
            return 1;   // 检测成功
        }
    }
    return 0;
}
