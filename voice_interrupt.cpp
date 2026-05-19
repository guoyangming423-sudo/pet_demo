#include "voice_interaction.h"
#include <sys/time.h>

extern int running;
extern snd_pcm_t *catch_handle;
extern pthread_mutex_t pcm_mutex;

static pthread_t monitor_tid;
static int interrupt_started = 0;   // 监听线程是否成功创建标志
static pthread_mutex_t interrput_mutex = PTHREAD_MUTEX_INITIALIZER;   // 保护打断状态和预录缓冲的互斥锁

static int monitor_enabled = 0;   // 监听线程是否启用的标志
static int interrupt_flag = 0;    // 是否打断播放的标志
static unsigned long long monitor_start_us = 0;   // 监听开始时间
static int interrupt_confirm_count = 0;   // 打断确认次数

static int16_t prebuffer_ring[PREBUFFER_FRAMES];   // 保存播放时监听线程读取的录音
static size_t prebuffer_write_pos = 0;   // 预录缓冲区当前写入位置
static size_t ring_frames = 0;   // 预录环形缓冲中当前有效的音频帧数

static int16_t frozen_prefix[PREBUFFER_FRAMES];   // 打断时前缀音频，供下次 record_wav 拼接
static size_t prefix_frames = 0;   // 打断时前缀缓冲区保存的有效音频
static int prefix_flag = 0;   // 是否将前缀音频给 record_wav 的标志

// 获取当前时间的微秒数
static unsigned long long current_time_us(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000000ULL + (unsigned long long)tv.tv_usec;
}


// 清空预录缓冲区
static void prebuffer_locked(void){
    memset(prebuffer_ring, 0, sizeof(prebuffer_ring));
    prebuffer_write_pos = 0;
    ring_frames = 0;

    memset(frozen_prefix, 0, sizeof(frozen_prefix));
    prefix_frames = 0;
    prefix_flag = 0;
}

// 将监听线程读取的音频写入预录缓冲区
static void push_prebuffer_locked(const int16_t *data, size_t frames){
    size_t i;

    for(i = 0; i < frames; i++){
        prebuffer_ring[prebuffer_write_pos] = data[i];
        prebuffer_write_pos = (prebuffer_write_pos + 1) % PREBUFFER_FRAMES;
        if(ring_frames < PREBUFFER_FRAMES){
            ring_frames++;
        }
    }
}

// 打断后将缓冲区有效音频保存到 frozen_prefix 供 record_wav 拼接
static void prefix_locked(void){
    size_t start_pos;
    size_t i;

    if(ring_frames == 0){
        prefix_frames = 0;
        prefix_flag = 0;
        return;
    }

    start_pos = (prebuffer_write_pos + PREBUFFER_FRAMES - ring_frames) % PREBUFFER_FRAMES;
    for(i = 0; i < ring_frames; i++){
        frozen_prefix[i] = prebuffer_ring[(start_pos + i) % PREBUFFER_FRAMES];
    }

    prefix_frames = ring_frames;
    prefix_flag = 1;
}

// 监听线程函数
static void* monitor_thread(void* arg){
    int16_t buf[THREAD_BUFFER_SIZE * CHANNELS];
    int16_t mono_buf[THREAD_BUFFER_SIZE];

    (void)arg;

    while(running){
        int enabled = 0;
        int n;
        int i;
        long sum;
        long avg;
        int gain;
        long long sum_volume;
        float avg_volume;
        unsigned long long now_us;
        unsigned long long elapsed_us;

        pthread_mutex_lock(&interrput_mutex);
        enabled = monitor_enabled;
        pthread_mutex_unlock(&interrput_mutex);
        if(!enabled){
            usleep(100 * 1000);   // 监听线程未启用时
            continue;
        }

        pthread_mutex_lock(&pcm_mutex);
        n = snd_pcm_readi(catch_handle, buf, THREAD_BUFFER_SIZE);
        pthread_mutex_unlock(&pcm_mutex);
        if(n < 0){
            int prepare_ret = 0;

            fprintf(stderr, "monitor_thread's snd_pcm_readi error: %s\n", snd_strerror(n));
            if(n == -EPIPE || n == -ESTRPIPE || n == -EBADFD){
                pthread_mutex_lock(&pcm_mutex);
                prepare_ret = snd_pcm_prepare(catch_handle);
                pthread_mutex_unlock(&pcm_mutex);
                if(prepare_ret < 0){
                    fprintf(stderr, "monitor_thread's snd_pcm_prepare error: %s\n", snd_strerror(prepare_ret));
                }
            }
            usleep(INTERRUPT_POLL_US);
            continue;
        }
        if(n == 0){
            usleep(5000);
            continue;
        }

        for(i = 0; i < n; i++){
            if(CHANNELS == 2)  mono_buf[i] = (buf[i*2] + buf[i*2 + 1]) / 2;
            else  mono_buf[i] = buf[i];
        }

        // 去 DC 偏置
        sum = 0;
        for(i = 0; i < n; i++){
            sum += mono_buf[i];
        }
        avg = sum / n;
        for(i = 0; i < n; i++){
            mono_buf[i] -= avg;
        }

        // 加大音量
        gain = 2;
        for(i = 0; i < n; i++){
            int32_t tmp = mono_buf[i] * gain;
            if(tmp > INT16_MAX)  tmp = INT16_MAX;
            else if(tmp < INT16_MIN)  tmp = INT16_MIN;
            mono_buf[i] = (int16_t)tmp;
        }

        // 计算平均音量作为静音判定阈值
        sum_volume = 0;
        for(i = 0; i < n; i++){
            sum_volume += abs(mono_buf[i]);
        }
        avg_volume = (float)sum_volume / n;
        now_us = current_time_us();

        pthread_mutex_lock(&interrput_mutex);
        if(monitor_enabled){
            push_prebuffer_locked(mono_buf, (size_t)n);   // 将监听线程读取的音频写入预录缓冲区
            elapsed_us = now_us - monitor_start_us;

            if(elapsed_us < INTERRUPT_IGNORE_US){
                interrupt_confirm_count = 0;
            }else if(avg_volume > INTERRUPT_THRESHOLD){
                interrupt_confirm_count++;
                if(!interrupt_flag && interrupt_confirm_count >= INTERRUPT_CONFIRM_FRAMES){
                    prefix_locked();
                    interrupt_flag = 1;   // 设置打断标志
                    monitor_enabled = 0;   // 停止监听
                    fprintf(stderr, "==== play_wav is interrupted! avg_volume=%.2f confirm=%d ====\n", avg_volume, interrupt_confirm_count);
                }
            }else{
                interrupt_confirm_count = 0;
            }
        }
        pthread_mutex_unlock(&interrput_mutex);
    }
    return NULL;
}

// 初始化打断模块并创建监听线程
int interrput_init(void){
    if(interrupt_started)  return 0;
    if(pthread_create(&monitor_tid, NULL, monitor_thread, NULL) != 0){
        perror("interrput_init: pthread_create error");
        return -1;
    }

    interrupt_started = 1;
    return 0;
}

// 监听线程资源回收
void interrput_uninit(void){
    if(interrupt_started){
        pthread_join(monitor_tid, NULL);
        interrupt_started = 0;
    }
}

// 开启监听线程并清空上一轮打断状态
void monitor_thread_begin(void){
    pthread_mutex_lock(&interrput_mutex);
    interrupt_flag = 0;
    interrupt_confirm_count = 0;
    monitor_start_us = current_time_us();
    monitor_enabled = 1;
    prebuffer_locked();
    pthread_mutex_unlock(&interrput_mutex);
}

// 播放结束后关闭监听线程
void interrput_end(void){
    pthread_mutex_lock(&interrput_mutex);
    monitor_enabled = 0;
    interrupt_confirm_count = 0;
    pthread_mutex_unlock(&interrput_mutex);
}

// 查询监听线程是否已经检测到说话并请求中断播放
int interrput_requested(void){
    int ret;
    pthread_mutex_lock(&interrput_mutex);
    ret = interrupt_flag;
    pthread_mutex_unlock(&interrput_mutex);

    return ret;
}

// 将前缀音频拼接到下一次 record_wav 
size_t consume_prefix(int16_t *out, size_t max_frames){
    size_t real_frames = 0;

    pthread_mutex_lock(&interrput_mutex);
    if(prefix_flag){
        real_frames = prefix_frames;
        if(real_frames > max_frames)  real_frames = max_frames;

        memcpy(out, frozen_prefix, real_frames * sizeof(int16_t));
        prefix_flag = 0;
        prefix_frames = 0;
    }
    pthread_mutex_unlock(&interrput_mutex);

    return real_frames;
}
