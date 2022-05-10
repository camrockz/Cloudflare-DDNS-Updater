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
#include <json-c/json.h>

#ifdef SYSTEMD
#include <systemd/sd-daemon.h>
#define LOG(format, ...) fprintf(stderr, format, ##__VA_ARGS__);
#define WATCHDOG() sd_notify(0, "WATCHDOG=1");
#define SLEEP() sleep(watchdogTime/3000000);
#else
#define LOG(format, ...) syslog(0, format, ##__VA_ARGS__);
#define WATCHDOG() (void)0;
#define SLEEP() sleep(30);
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

void handler(int sig);

char *timestamp();

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);

int checkIP(struct memory *ip, CURL* curl);

int getConfig(char *filename);

int updateIP(struct memory *ip);

struct config *conf;

int main()
{
#ifdef SYSTEMD
    uint64_t watchdogTime;
    uint64_t* ptr = &watchdogTime;
    sd_watchdog_enabled(0, ptr);
#else
    beDaemon();
    openlog(NULL, LOG_PID, LOG_DAEMON);
#endif
    
    struct sigaction sigact;
    sigact.sa_handler = handler;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGABRT, &sigact, NULL);
    sigaction(SIGHUP, &sigact, NULL);
    struct memory ip;
    CURL *curl;
    ip.response = NULL;
    ip.size = 0;


    if ((conf = malloc(sizeof(struct config))) == NULL)
        exit(EXIT_FAILURE);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (getConfig(CONF_FILE) < 0)
    {
        curl_global_cleanup();
        free(conf);
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        int check;
        while ((check = checkIP(&ip, curl)) == 0)
        {
            WATCHDOG();
            SLEEP();
        }
        if (check == 1)
        {
            if (updateIP(&ip))
            {
                ip.response = NULL;
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
    return 0;
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
    if (setsid < 0)
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

void handler(int sig)
{
    if ((sig == SIGINT) || (sig == SIGTERM) || (sig == SIGABRT) || (sig == SIGHUP))
    {
        char tmp[] = "\nSignal to terminate, exiting";
        ssize_t stopWarningMe = write(STDERR_FILENO, tmp, sizeof(tmp));
        close(STDERR_FILENO);
        exit(EXIT_SUCCESS);
    }
}

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
 
    char *ptr = realloc(mem->response, mem->size + realsize + 1);
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
    CURLcode res = -1;
    char buff[BUFF_SIZE];
    if (ip->response != NULL)
        memcpy(buff, ip->response, sizeof(buff));
    ip->response = NULL;
    ip->size = 0;
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, ip);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        
        for (int i = 0; (res != CURLE_OK) && (i < 3); i++)
        {
            res = curl_easy_perform(curl);
            if (res == CURLE_COULDNT_RESOLVE_HOST)
                sleep(10);
        }
        if (res != CURLE_OK)
        {
            LOG("curl_easy_perform() failed to get IP: %s\n", curl_easy_strerror(res));
            return -1;
        }
    }
    size_t len = strlen(ip->response);
    if (len > 0 && ip->response[len-1] == '\n')
        ip->response[--len] = '\0';
    if (!strcmp(ip->response, buff))
    {
        DBG("IP has not changed!\n");
        return 0;
    }
    else
    {
        char printIP[32];
        memcpy(printIP, ip->response, sizeof(printIP));
        LOG("IP has changed to %s\n", printIP);
        return 1;
    }
}

int getConfig(char* filename)
{
    int fd;
    char *line[32];
    char *option[2];
    CURL *curlLocal;
    CURLcode res = -1;
    char filebuff[32*BUFF_SIZE], buff2[BUFF_SIZE], buff3[BUFF_SIZE], urlZoneID[BUFF_SIZE], urlRecordID[BUFF_SIZE];
    json_object *parsed, *parsed2, *zoneID, *recordID, *jobResult, *arrayFirst;
    struct memory result, result2;
    result.response = NULL;
    result.size = 0;
    result2.response = NULL;
    result2.size = 0;
    struct curl_slist *headers = NULL;
    
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
    char *mem = calloc(3, BUFF_SIZE);
    int i = 0;
    do
    {
        if (i == 0)
            line[0] = strtok(filebuff, "\n");
        else
            line[i] = strtok(NULL, "\n");
        i++;
    }
    while (line[i-1] != NULL);
    i = 0;
    do
    {
        if (line[i][0] == '#')
            i++;
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
            i++;
        }
    }
    while (line[i] != NULL);
        
    curlLocal = curl_easy_init();
    if (curlLocal)
    {
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");

        snprintf(buff2, sizeof(buff2), "X-Auth-Email: %s", conf->email);
        headers = curl_slist_append(headers, buff2);

        snprintf(buff3, sizeof(buff3), "X-Auth-Key: %s", conf->key);
        headers = curl_slist_append(headers, buff3);
        snprintf(urlZoneID, sizeof(urlZoneID), "https://api.cloudflare.com/client/v4/zones?name=%s", conf->record);

        curl_easy_setopt(curlLocal, CURLOPT_URL, urlZoneID);
        curl_easy_setopt(curlLocal, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curlLocal, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curlLocal, CURLOPT_WRITEDATA, (void *)&result);
        curl_easy_setopt(curlLocal, CURLOPT_TIMEOUT, 10);
        for (int i = 0; (res != CURLE_OK) && (i < 3); i++)
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
        parsed = json_tokener_parse(result.response);
        json_object_object_get_ex(parsed, "result", &jobResult);
        arrayFirst = json_object_array_get_idx(jobResult, 0);
        json_object_object_get_ex(arrayFirst, "id", &zoneID);
        conf->zoneID = json_object_get_string(zoneID);
        snprintf(urlRecordID, sizeof(urlRecordID), "https://api.cloudflare.com/client/v4/zones/%s/dns_records?name=%s", conf->zoneID, conf->record);
        free(result.response);
        curl_easy_setopt(curlLocal, CURLOPT_URL, urlRecordID);
        curl_easy_setopt(curlLocal, CURLOPT_WRITEDATA, (void *)&result2);
        CURLcode res = -1;
        for (int i = 0; (res != CURLE_OK) && (i < 3); i++)
        {
            res = curl_easy_perform(curlLocal);
            if (res == CURLE_COULDNT_RESOLVE_HOST)
                sleep(10);
        }
        if (res != CURLE_OK)
        {
            LOG("curl_easy_perform() failed to get recordID: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curlLocal);
            return -1;
        }
        parsed2 = json_tokener_parse(result2.response);
        json_object_object_get_ex(parsed2, "result", &jobResult);
        arrayFirst = json_object_array_get_idx(jobResult, 0);
        json_object_object_get_ex(arrayFirst, "id", &recordID);
        conf->recordID = json_object_get_string(recordID);
        free(result2.response);
        curl_easy_cleanup(curlLocal);
        }
    return 0;
}

int updateIP(struct memory *ip)
{
    struct curl_slist *headers = NULL;
    CURLcode res = -1;
    CURL* curlLocal;
    char url[BUFF_SIZE], data[BUFF_SIZE], buff2[BUFF_SIZE], buff3[BUFF_SIZE];
    json_object *parsed4, *success;
    struct memory result3;
    result3.response = NULL;
    result3.size = 0;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    snprintf(buff2, sizeof(buff2), "X-Auth-Email: %s", conf->email);
    headers = curl_slist_append(headers, buff2);
    snprintf(buff3, sizeof(buff3), "X-Auth-Key: %s", conf->key);
    headers = curl_slist_append(headers, buff3);
    
    snprintf(data, sizeof(data), "{\"id\":\"%s\",\"type\":\"A\",\"name\":\"%s\",\"content\":\"%s\",\"ttl\":120}", conf->zoneID, conf->record, ip->response);
    snprintf(url, sizeof(url), "https://api.cloudflare.com/client/v4/zones/%s/dns_records/%s", conf->zoneID, conf->recordID);
    curlLocal = curl_easy_init();
    if (curlLocal)
    {
        curl_easy_setopt(curlLocal, CURLOPT_URL, url);
        curl_easy_setopt(curlLocal, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curlLocal, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curlLocal, CURLOPT_USERAGENT, "libcrp/0.1");
        curl_easy_setopt(curlLocal, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curlLocal, CURLOPT_WRITEDATA, (void *)&result3);
        curl_easy_setopt(curlLocal, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curlLocal, CURLOPT_TIMEOUT, 10);
        for (int i = 0; (res != CURLE_OK) && (i < 3); i++)
        {
            res = curl_easy_perform(curlLocal);
            if (res == CURLE_COULDNT_RESOLVE_HOST)
                sleep(10);
        }
        if (res != CURLE_OK)
        {
            LOG("curl_easy_perform() failed to update IP with cloudflare: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curlLocal);
            free(result3.response);
            return -1;
        }
        else
        {
            parsed4 = json_tokener_parse(result3.response);
            json_object_object_get_ex(parsed4, "success", &success);
            free(result3.response);
            if (json_object_get_boolean(success) != 0)
            {
                LOG("SUCCESS - updated IP with cloudflare to %s\n", ip->response);
                curl_easy_cleanup(curlLocal);
                json_object_put(parsed4);
                return 0;
            }
            else
            {
                LOG("FAILURE - could not update IP with cloudflare\n");
                curl_easy_cleanup(curlLocal);
                json_object_put(parsed4);
                return -1;
            }
        }
    }
    return 0;
}
