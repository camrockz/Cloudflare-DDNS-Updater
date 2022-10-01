/* 
 * Copyright 2020, Camrockz <https://github.com/camrockz>
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>

#ifdef SYSTEMD
#include <systemd/sd-daemon.h>
#define LOG(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#define WATCHDOG() sd_notify(0, "WATCHDOG=1")
#define SLEEP() sleep(watchdogTime/3000000)
#else
#define LOG(format, ...) syslog(0, format, ##__VA_ARGS__)
#define WATCHDOG() (void)0
#define SLEEP() sleep(30)
void beDaemon();
#endif

#ifdef NDEBUG
#define DBG(format, ...) (void)0;
#else
#define DBG(format, ...) LOG(format, ##__VA_ARGS__);
#endif

#define BUFF_SIZE 256 
#define CONF_FILE "/etc/ipup/ipup.conf"

struct config
{
    const char *email;
    const char *key;
    const char *record;
    const char *zoneID;
    const char *recordID;
};

struct memory
{
   char *response;
   size_t size;
};

char *timestamp();

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);

int checkIP(struct memory *ip, CURL* curl);

int getConfig(const char *filename);

int updateIP(struct memory *ip);

struct config config;
struct config *conf = &config;

#ifdef SYSTEMD
    uint64_t watchdogTime;
    uint64_t* ptr = &watchdogTime;
#endif


int main()
{

#ifdef SYSTEMD
    sd_watchdog_enabled(0, ptr);
#else
    beDaemon();
    openlog(NULL, LOG_PID, LOG_DAEMON);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl;
    curl = curl_easy_init();

    struct memory ip;
    ip.response = NULL;
    ip.size = 0;
    
    const char* conf_file = CONF_FILE;
    if (getConfig(conf_file) < 0)
    {
        exit(EXIT_FAILURE);
    }

    int check;
    while (1)
    {
        if ((check = checkIP(&ip, curl) == 1))
        {
            if (updateIP(&ip))
            {
                ip.size = 0;
            }
            else
            {
                WATCHDOG();
                SLEEP();
            }
        }
        else
            WATCHDOG();
    }
}

#ifndef SYSTEMD
void beDaemon()
{
    pid_t pid;
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);
    if (setsid() < 0)
        exit(EXIT_FAILURE);
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);
    umask(0);
    if (chdir("/") <0)
        exit(EXIT_FAILURE);
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}
#endif

char *timestamp()
{
    static char timestamp[64];
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%F %X", local);
    return timestamp;
}

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;
 
    char *ptr = (char *)realloc(mem->response, mem->size + realsize + 1);
    if (ptr == NULL)
    return 0;  /* out of memory! */
    
    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), buffer, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;
 
    return realsize;
}

int checkIP(struct memory *ip, CURL* curl)
{
    char oldIP[BUFF_SIZE] = {};
    if (ip->response != NULL)
        memcpy(oldIP, ip->response, sizeof(oldIP));

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ip);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

    do
    {
        ip->size = 0;
        CURLcode res = CURLE_COULDNT_CONNECT;
        while (res != CURLE_OK)
        {
            res = curl_easy_perform(curl);
            if (res != CURLE_OK)
            {
                DBG("curl_easy_perform() failed to get IP: %s\n", curl_easy_strerror(res));
                WATCHDOG();
                sleep(10);
            }
        }
        size_t len = strlen(ip->response);
        if (len > 0 && ip->response[len-1] == '\n')
            ip->response[len-1] = '\0';
        if (strcmp(ip->response, oldIP))
            break;

#ifndef NDEBUG
        if (!strcmp(ip->response, oldIP))
            DBG("IP has not changed!\n");
#endif

        WATCHDOG();
        SLEEP();
    }
    while (!strcmp(ip->response, oldIP));

    {
        LOG("IP has changed to %s\n", ip->response);
        return 1;
    }
}

int getConfig(const char* filename)
{
    int fd;
    char filebuff[32*BUFF_SIZE];
    if ((fd = open(filename, O_RDONLY)) < 0)
    {
        LOG("FILE ERROR - could not open config file\n");
        return -1;
    }
    if (read(fd, filebuff, sizeof(filebuff)) < 0)
    {
        LOG("FILE ERROR - could not read config file\n");
        return -1;
    }
    close(fd);

    char *mem = (char *)calloc(3, BUFF_SIZE);
    char *line[32];
    char *option[2];
    int i = 0;
    do
    {
        if (i == 0)
            line[0] = strtok(filebuff, "\n");
        else
            line[i] = strtok(NULL, "\n");
        ++i;
    }
    while (line[i-1] != NULL);

    i = 0;
    do
    {
        if (line[i][0] == '#')
            ++i;
        else
        {
            option[0] = strtok(line[i], " '\n''='");
            option[1] = strtok(NULL, " '\n''='");
            if (!strcmp(option[0], "email"))
            {
                conf->email = mem;
                strncpy((char*)conf->email, option[1], BUFF_SIZE - 1);
            }
            else if (!strcmp(option[0], "key"))
            {
                conf->key = &mem[BUFF_SIZE];
                strncpy((char*)conf->key, option[1], BUFF_SIZE - 1);
            }
            else if (!strcmp(option[0], "record"))
            {
                conf->record = &mem[2*BUFF_SIZE];
                strncpy((char*)conf->record, option[1], BUFF_SIZE - 1);
            }
            else
            {
                LOG("CONFIG ERROR - invalid config options\n");
                return -1;
            }
            ++i;
        }
    }
    while (line[i] != NULL);

    CURL *curlLocal = curl_easy_init();
    CURLcode res = CURLE_COULDNT_CONNECT;
    struct curl_slist *headers = NULL;

    if (curlLocal)
    {
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");

        char buff[BUFF_SIZE];
        snprintf(buff, sizeof(buff), "X-Auth-Email: %s", conf->email);
        headers = curl_slist_append(headers, buff);
        snprintf(buff, sizeof(buff), "X-Auth-Key: %s", conf->key);
        headers = curl_slist_append(headers, buff);

        char urlZoneID[BUFF_SIZE];
        snprintf(urlZoneID, sizeof(urlZoneID), "https://api.cloudflare.com/client/v4/zones?name=%s", conf->record);

        struct memory result;
        result.response = NULL;
        result.size = 0;
        curl_easy_setopt(curlLocal, CURLOPT_URL, urlZoneID);
        curl_easy_setopt(curlLocal, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curlLocal, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curlLocal, CURLOPT_WRITEDATA, (void *)&result);
        curl_easy_setopt(curlLocal, CURLOPT_TIMEOUT, 10);

        for (int i = 0; (res != CURLE_OK) && (i < 3); ++i)
        {
            res = curl_easy_perform(curlLocal);
            if (res == CURLE_COULDNT_RESOLVE_HOST)
                sleep(10);
        }
        if (res != CURLE_OK)
        {
            LOG("curl_easy_perform() failed to get zoneID: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curlLocal);
            return -1;
        }

        json_object *parsed = json_object_new_object();
        parsed = json_tokener_parse(result.response);
        json_object *jobResult = json_object_new_object();
        json_object_object_get_ex(parsed, "result", &jobResult);
        json_object *array = json_object_new_array();
        array = json_object_array_get_idx(jobResult, 0);
        json_object *zoneID = json_object_new_object();
        json_object_object_get_ex(array, "id", &zoneID);

        const char* ptr = json_object_get_string(zoneID);
        size_t len = (size_t)json_object_get_string_len(zoneID);
        char *zoneIDptr = (char *)realloc((void *)conf->zoneID, len + 1);
        conf->zoneID = zoneIDptr;
        memcpy((void *)conf->zoneID, ptr, len);
        zoneIDptr[len] = 0;


        json_object_put(parsed);
        free(result.response);
        result.response = NULL;
        result.size = 0;
        char urlRecordID[BUFF_SIZE];
        snprintf(urlRecordID, sizeof(urlRecordID), "https://api.cloudflare.com/client/v4/zones/%s/dns_records?name=%s", conf->zoneID, conf->record);

        curl_easy_setopt(curlLocal, CURLOPT_URL, urlRecordID);
        CURLcode res = CURLE_COULDNT_CONNECT;

        for (int i = 0; (res != CURLE_OK) && (i < 3); ++i)
        {
            res = curl_easy_perform(curlLocal);
            if (res == CURLE_COULDNT_RESOLVE_HOST)
                sleep(10);
        }
        if (res != CURLE_OK)
        {
            LOG("curl_easy_perform() failed to get recordID: %s\n", curl_easy_strerror(res));
            free(result.response);
            curl_easy_cleanup(curlLocal);
            return -1;
        }

        parsed = json_object_new_object();
        parsed = json_tokener_parse(result.response);
        jobResult = json_object_new_object();
        json_object_object_get_ex(parsed, "result", &jobResult);
        array = json_object_new_array();
        array = json_object_array_get_idx(jobResult, 0);
        json_object *recordID = json_object_new_object();
        json_object_object_get_ex(array, "id", &recordID);
        //conf->recordID = json_object_get_string(recordID);

        ptr = json_object_get_string(recordID);
        len = (size_t)json_object_get_string_len(recordID);
        char* recordIDptr = (char *)realloc((void *)conf->recordID, len + 1);
        conf->recordID = recordIDptr;
        memcpy((void *)conf->recordID, ptr, len);
        recordIDptr[len] = 0;

        json_object_put(parsed);
        free(result.response);

        curl_easy_cleanup(curlLocal);
        return 0;
    }
    else
    {
        LOG("curl_easy_init failed: %s\n", curl_easy_strerror(res));
        return -1;
    }
}

int updateIP(struct memory *ip)
{
    CURL* curlLocal = curl_easy_init();
    CURLcode res = CURLE_COULDNT_CONNECT;
    struct curl_slist *headers = NULL;

    if (curlLocal)
    {
        char data[BUFF_SIZE];
        snprintf(data, sizeof(data), "{\"id\":\"%s\",\"type\":\"A\",\"name\":\"%s\",\"content\":\"%s\",\"ttl\":120}", conf->zoneID, conf->record, ip->response);

        char url[BUFF_SIZE];
        snprintf(url, sizeof(url), "https://api.cloudflare.com/client/v4/zones/%s/dns_records/%s", conf->zoneID, conf->recordID);

        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");

        char buff[BUFF_SIZE];
        snprintf(buff, sizeof(buff), "X-Auth-Email: %s", conf->email);
        headers = curl_slist_append(headers, buff);

        snprintf(buff, sizeof(buff), "X-Auth-Key: %s", conf->key);
        headers = curl_slist_append(headers, buff);

        struct memory result;
        result.response = NULL;
        result.size = 0;
        curl_easy_setopt(curlLocal, CURLOPT_URL, url);
        curl_easy_setopt(curlLocal, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curlLocal, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curlLocal, CURLOPT_USERAGENT, "libcrp/0.1");
        curl_easy_setopt(curlLocal, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curlLocal, CURLOPT_WRITEDATA, (void *)&result);
        curl_easy_setopt(curlLocal, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curlLocal, CURLOPT_TIMEOUT, 10);

        for (int i = 0; (res != CURLE_OK) && (i < 3); ++i)
        {
            res = curl_easy_perform(curlLocal);
            if (res == CURLE_COULDNT_RESOLVE_HOST)
                sleep(10);
        }

        if (res != CURLE_OK)
        {
            LOG("curl_easy_perform() failed to update IP with cloudflare: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curlLocal);
            free(result.response);
            return -1;
        }

        json_object *parsed = json_object_new_object();
        parsed = json_tokener_parse(result.response);
        json_object *success =  json_object_new_boolean(0);
        json_object_object_get_ex(parsed, "success", &success);

        if (!json_object_get_boolean(success))
        {
            LOG("FAILURE - could not update IP with cloudflare\n");
            json_object_put(parsed);
            free(result.response);
            curl_easy_cleanup(curlLocal);
            return -1;
        }
        LOG("SUCCESS - updated IP with cloudflare to %s\n", ip->response);
        json_object_put(parsed);
        free(result.response);
        curl_easy_cleanup(curlLocal);
        return 0;
    }
    else
    {
        LOG("curl_easy_init failed: %s\n", curl_easy_strerror(res));
        return -1;
    }
}
