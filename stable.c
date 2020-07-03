#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct config
{
    char email[64];
    char key[64];
    char record[64];
    char zoneID[64];
    char recordID[64];
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
    char buff[64];
    char buff2[128];
    char buff3[128];
    char url[256];
    char data[256];
    
    static struct memory ip;
    static struct memory result;
    struct config *conf = malloc(sizeof(struct config));
    
    log = fopen("ipup.log", "a");
    
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
            exit(EXIT_FAILURE);
        }
    }
    if ((fp = fopen("ip", "r")) != NULL)
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
        exit(EXIT_SUCCESS);
    }
    fprintf(log, "%s: IP has changed to %s\n", timestamp(), ip.response);
    fprintf(stdout, "%s: IP has changed to %s\n", timestamp(), ip.response);
    curl_easy_cleanup(curl);
    
    if ((fp = fopen("conf", "r")) == NULL)
    {
        fprintf(log, "%s: FILE ERROR - could not open conf file\n", timestamp());
        fprintf(stdout, "%s: FILE ERROR - could not open conf file\n", timestamp());
        exit(EXIT_FAILURE);
    }
    getString(conf->email, sizeof(conf->email), fp);
    getString(conf->key, sizeof(conf->key), fp);
    getString(conf->record, sizeof(conf->record), fp);
    getString(conf->zoneID, sizeof(conf->zoneID), fp);
    getString(conf->recordID, sizeof(conf->recordID), fp);
    fclose(fp);
    snprintf(data, sizeof(data), "{\"id\":\"%s\",\"type\":\"A\",\"name\":\"%s\",\"content\":\"%s\",\"ttl\":120}", conf->zoneID, conf->record, ip.response);
    snprintf(url, sizeof(url), "https://api.cloudflare.com/client/v4/zones/%s/dns_records/%s", conf->zoneID, conf->recordID);
    
    curl = curl_easy_init();
    if(curl)
    {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        snprintf(buff2, sizeof(buff2), "X-Auth-Email: %s", conf->email);
        headers = curl_slist_append(headers, buff2);
        
        snprintf(buff3, sizeof(buff3), "X-Auth-Key: %s", conf->key);
        headers = curl_slist_append(headers, buff3);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcrp/0.1");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&result);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
        {
            fprintf(log, "%s: curl_easy_perform() failed to update IP with cloudflare: %s\n", timestamp(), curl_easy_strerror(res));
            fprintf(stderr, "%s: curl_easy_perform() failed to update IP with cloudflare: %s\n", timestamp(), curl_easy_strerror(res));
            exit(EXIT_FAILURE);
        }
        else
        {
            char good[] = {"success\":true,\"errors\":[]"};
            if (strstr(result.response, good) != NULL)
            {
                fprintf(log, "%s: SUCCESS - updated IP with cloudflare to %s\n", timestamp(), ip.response);
                fprintf(stdout, "%s: SUCCESS - updated IP with cloudflare to %s\n", timestamp(), ip.response);
                if ((fp = fopen("ip", "w")) != NULL)
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
                fprintf(log, "%s: FAILURE - dumping result: %s\n", timestamp(), result.response);
                fprintf(stdout, "%s: FAILURE - dumping result: %s\n", timestamp(), result.response);
            }
        }
        curl_easy_cleanup(curl);
    }
    
curl_global_cleanup();
free(conf);
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
    if (size < 1)
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
