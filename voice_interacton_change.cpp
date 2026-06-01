int qwen_tts(const char *text, const char *out_wav){
    CURL *curl = curl_easy_init();
    if(curl == NULL){
        fprintf(stderr, "qwen_tts's curl_easy_init error\n");
        return -1;
    }

    struct curl_slist *headers = NULL;    // HTTP 请求头指针
    CURLcode curl_code;
    FILE *f = NULL;
    long http_code = 0;
    struct curl_slist *new_headers = NULL;
    int ret = 0;
    cJSON *root = NULL;
    char *post_data = NULL;

    // 打开输出文件
    f = fopen(out_wav, "wb");
    if(f == NULL){
        perror("can't open out_wav");
        goto tts_cleanup;
    }

    // 用 cJSON 动态构造 JSON 请求体，避免固定缓冲区溢出，也避免 text 中的引号/换行破坏 JSON
    root = cJSON_CreateObject();
    if(root == NULL){
        fprintf(stderr, "qwen_tts: cJSON_CreateObject error\n");
        goto tts_cleanup;
    }

    if(cJSON_AddStringToObject(root, "appkey", ASR_APPKEY) == NULL ||
       cJSON_AddStringToObject(root, "text", text) == NULL ||
       cJSON_AddStringToObject(root, "token", ASR_ACCESS_TOKEN) == NULL ||
       cJSON_AddStringToObject(root, "format", "wav") == NULL ||
       cJSON_AddNumberToObject(root, "sample_rate", 16000) == NULL ||
       cJSON_AddStringToObject(root, "voice", TTS_VOICE) == NULL){
        fprintf(stderr, "qwen_tts: build json body error\n");
        goto tts_cleanup;
    }

    post_data = cJSON_PrintUnformatted(root);
    if(post_data == NULL){
        fprintf(stderr, "qwen_tts: cJSON_PrintUnformatted error\n");
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);          // 总请求超时

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

    char *content_type;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
    printf("Content-Type: %s\n", content_type);

    fclose(f);
    f = NULL;
    if(post_data) free(post_data);
    if(root) cJSON_Delete(root);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 0;

tts_cleanup:
    if(f) fclose(f);
    remove(out_wav);
    if(post_data) free(post_data);
    if(root) cJSON_Delete(root);
    if(headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return -1;
}
