#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

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

void getString(char *buffer, size_t size, FILE *fp);

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);

int main()
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    FILE *log;
    char buff[128], buff2[128], buff3[128];
    char filebuff[4096];
    char url[512], urlZoneID[512], urlRecordID[512];
    char data[512];
    json_object *parsed, *parsed2, *parsed3, *parsed4, *email, *key, *record, *zoneID,*recordID, *jobResult, *arrayFirst, *success;
    struct curl_slist *headers = NULL;
    struct memory ip, result, result2, result3;
    json_bool firstRun = FALSE;

    struct config *conf = malloc(sizeof(struct config));
    log = fopen("../ipup.log", "a");
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, "http://ipv4.icanhazip.com");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&ip);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
        {
            fprintf(stderr, "%s: curl_easy_perform() failed to get IP: %s\n", timestamp(), curl_easy_strerror(res));
            fprintf(log, "%s: curl_easy_perform() failed to get IP: %s\n", timestamp(), curl_easy_strerror(res));
            fclose(log);
            exit(EXIT_FAILURE);
        }
    }
    if ((fp = fopen(".ip", "r")) != NULL)
    {
        getString(buff, (sizeof(buff)), fp);
        fclose(fp);
    }
    size_t len = strlen(ip.response);
    if (len > 0 && ip.response[len-1] == '\n')
        ip.response[--len] = '\0';
    if (!strcmp(ip.response, buff))
    {
        printf("IP has not changed!\n");
        curl_easy_cleanup(curl);
        fclose(log);
        exit(EXIT_SUCCESS);
    }
    fprintf(log, "%s: IP has changed to %s\n", timestamp(), ip.response);
    fprintf(stdout, "%s: IP has changed to %s\n", timestamp(), ip.response);
    
    if ((fp = fopen("../config.json", "r")) == NULL)
    {
        fprintf(log, "%s: FILE ERROR - could not open config.json file\n", timestamp());
        fprintf(stdout, "%s: FILE ERROR - could not open config.json file\n", timestamp());
        fclose(log);
        exit(EXIT_FAILURE);
    }
    fread(filebuff, sizeof(filebuff), 1, fp);
    parsed = json_tokener_parse(filebuff);
    json_object_object_get_ex(parsed, "email", &email);
    conf->email = json_object_get_string(email);
    json_object_object_get_ex(parsed, "key", &key);
    conf->key = json_object_get_string(key);
    json_object_object_get_ex(parsed, "record", &record);
    conf->record = json_object_get_string(record);
    json_object_object_get_ex(parsed, "zoneID", &zoneID);
    json_object_object_get_ex(parsed, "recordID", &recordID);
    if (json_object_get_string(zoneID) == NULL || json_object_get_string(recordID) == NULL)
    {
        if(curl)
        {
            firstRun = TRUE;
            headers = curl_slist_append(headers, "Accept: application/json");
            headers = curl_slist_append(headers, "Content-Type: application/json");
        
            snprintf(buff2, sizeof(buff2), "X-Auth-Email: %s", conf->email);
            headers = curl_slist_append(headers, buff2);
        
            snprintf(buff3, sizeof(buff3), "X-Auth-Key: %s", conf->key);
            headers = curl_slist_append(headers, buff3);
            snprintf(urlZoneID, sizeof(urlZoneID), "https://api.cloudflare.com/client/v4/zones?name=%s", conf->record);
        
            curl_easy_setopt(curl, CURLOPT_URL, urlZoneID);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&result);
            res = curl_easy_perform(curl);
            if(res != CURLE_OK)
            {
                fprintf(log, "%s: curl_easy_perform() failed to get zoneID: %s\n", timestamp(), curl_easy_strerror(res));
                fprintf(stderr, "%s: curl_easy_perform() failed to get zoneID: %s\n", timestamp(), curl_easy_strerror(res));
                fclose(log);
                exit(EXIT_FAILURE);
            }
            parsed2 = json_tokener_parse(result.response);
            free(result.response);
            json_object_object_get_ex(parsed2, "result", &jobResult);
            arrayFirst = json_object_array_get_idx(jobResult, 0);
            json_object_object_get_ex(arrayFirst, "id", &zoneID);
            conf->zoneID = json_object_get_string(zoneID);
            snprintf(urlRecordID, sizeof(urlRecordID), "https://api.cloudflare.com/client/v4/zones/%s/dns_records?name=%s", conf->zoneID, conf->record);
            curl_easy_setopt(curl, CURLOPT_URL, urlRecordID);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&result2);
            res = curl_easy_perform(curl);
            if(res != CURLE_OK)
            {
                fprintf(log, "%s: curl_easy_perform() failed to get recordID: %s\n", timestamp(), curl_easy_strerror(res));
                fprintf(stderr, "%s: curl_easy_perform() failed to get recordID: %s\n", timestamp(), curl_easy_strerror(res));
                fclose(log);
                exit(EXIT_FAILURE);
            }
            parsed3 = json_tokener_parse(result2.response);
            free(result2.response);
            json_object_object_get_ex(parsed3, "result", &jobResult);
            arrayFirst = json_object_array_get_idx(jobResult, 0);
            json_object_object_get_ex(arrayFirst, "id", &recordID);
            conf->recordID = json_object_get_string(recordID);
            fclose(fp);
            json_object_object_add(parsed, "zoneID", zoneID);
            json_object_get(zoneID);
            json_object_object_add(parsed, "recordID", recordID);
            json_object_get(recordID);
            json_object_to_file_ext("../config.json", parsed, JSON_C_TO_STRING_PRETTY);
        }

    }
    else
    {
    conf->zoneID = json_object_get_string(zoneID);
    conf->recordID = json_object_get_string(recordID);
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    snprintf(buff2, sizeof(buff2), "X-Auth-Email: %s", conf->email);
    headers = curl_slist_append(headers, buff2);
    snprintf(buff3, sizeof(buff3), "X-Auth-Key: %s", conf->key);
    headers = curl_slist_append(headers, buff3);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    snprintf(data, sizeof(data), "{\"id\":\"%s\",\"type\":\"A\",\"name\":\"%s\",\"content\":\"%s\",\"ttl\":120}", conf->zoneID, conf->record, ip.response);
    snprintf(url, sizeof(url), "https://api.cloudflare.com/client/v4/zones/%s/dns_records/%s", conf->zoneID, conf->recordID);
    if(curl)
    {

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcrp/0.1");
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&result3);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
        {
            fprintf(log, "%s: curl_easy_perform() failed to update IP with cloudflare: %s\n", timestamp(), curl_easy_strerror(res));
            fprintf(stderr, "%s: curl_easy_perform() failed to update IP with cloudflare: %s\n", timestamp(), curl_easy_strerror(res));
            fclose(log);
            exit(EXIT_FAILURE);
        }
        else
        {
            parsed4 = json_tokener_parse(result3.response);
            json_object_object_get_ex(parsed4, "success", &success);
            if (json_object_get_boolean(success) == TRUE)
            {
                fprintf(log, "%s: SUCCESS - updated IP with cloudflare to %s\n", timestamp(), ip.response);
                fprintf(stdout, "%s: SUCCESS - updated IP with cloudflare to %s\n", timestamp(), ip.response);
                if ((fp = fopen(".ip", "w")) != NULL)
                {
                    fprintf(fp, "%s", ip.response);
                    fclose(fp);
                }
                else
                {
                    fprintf(log, "%s: FILE ERROR - could not write IP file\n", timestamp());
                    fprintf(stdout, "%s: FILE ERROR - could not write IP file\n", timestamp());
                }
            }
            else
            {
                fprintf(log, "%s: FAILURE - dumping result: %s\n", timestamp(), result3.response);
                fprintf(stdout, "%s: FAILURE - dumping result: %s\n", timestamp(), result3.response);
            }
        }
        curl_easy_cleanup(curl);
        free(result3.response);
        json_object_put(parsed);
        if (firstRun == TRUE)
        {
            json_object_put(parsed2);
            json_object_put(parsed3);
        }
        json_object_put(parsed4);
    }
    
curl_global_cleanup();
free(conf);
free(ip.response);
fclose(log);
return 0;
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

void getString(char *buffer, size_t size, FILE *fp)
{
    char c;
    int i = 0;
    if (size < 2)
    {
        fprintf(stderr , "String length out of bounds, exiting\n");
        exit(EXIT_FAILURE);
    }
    while (((c = getc(fp)) == '\n' || c == '\0') && c != EOF)
    {}
    if (c == EOF)
    {
        fprintf(stderr , "No string was found, exiting\n");
        exit(EXIT_FAILURE);
    }
    buffer[i] = c;
    i++;
    while ((c = getc(fp)) != '\n' && c != '\0' && i < size && c != EOF)
    {
        buffer[i] = c;
        i++;
    }
    if (c != '\n' && c != '\0' && c != EOF)
    {
        fprintf(stderr , "Buffer too small for string. Flushing stream to next newline char or end of file\n");
        buffer[i] = '\0';
        while ((c = getc(fp)) != '\n' && c != '\0' && c != EOF)
        {
            i++;
            if (i > 1024)
            {
                fprintf(stderr , "String too long, exiting\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    buffer[i] = '\0';
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;
 
    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if(ptr == NULL)
    return 0;  /* out of memory! */
    
    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), buffer, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;
 
    return realsize;
}
