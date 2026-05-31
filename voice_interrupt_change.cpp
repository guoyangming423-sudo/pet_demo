void monitor_thread_begin(void){
    int prepare_ret;

    // 播放开始前先把采集设备准备好，尽量减少监听线程第一次读时出现 Broken pipe
    pthread_mutex_lock(&pcm_mutex);
    prepare_ret = snd_pcm_prepare(catch_handle);
    pthread_mutex_unlock(&pcm_mutex);

    if(prepare_ret < 0){
        fprintf(stderr, "monitor_thread_begin: snd_pcm_prepare error: %s\n", snd_strerror(prepare_ret));
    }

    pthread_mutex_lock(&interrput_mutex);
    interrupt_flag = 0;
    interrupt_confirm_count = 0;
    monitor_start_us = current_time_us();
    monitor_enabled = 1;
    prebuffer_locked();
    pthread_mutex_unlock(&interrput_mutex);
}
