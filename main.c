#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct config
{
	char url[256];
	char email[64];
	char key[64];
	char record[64];
	char zoneID[64];
	char recordID[64];
	char ip[64];
	char data[256];
};

char *timestamp();

void getString(char *buffer, size_t size, FILE *fp);

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);

int main()
{
	CURL *curl;
	CURLcode res;
	FILE *fp;
	FILE *log;
	log = fopen("ipup.log", "a");
	char ipstr[64];
	char buff[64];
	char result[1024];
	
	curl_global_init(CURL_GLOBAL_DEFAULT);
 
	curl = curl_easy_init();
	if(curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, "http://ipv4.icanhazip.com");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&ipstr);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK)
		{
			fprintf(stderr, "%s: curl_easy_perform() failed to get IP: %s\n", timestamp(), curl_easy_strerror(res));
			fprintf(log, "%s: curl_easy_perform() failed to get IP: %s\n", timestamp(), curl_easy_strerror(res));
			exit(EXIT_FAILURE);
		}
	if ((fp = fopen("ip", "r")) != NULL)
	{
		fgets(buff, (sizeof(buff)), fp);
		fclose(fp);
	}
	if (!strcmp(ipstr, buff))
	{
		printf("IP has not changed!\n");
		exit(EXIT_SUCCESS);
	}
	fprintf(log, "%s: IP has changed to %s", timestamp(), ipstr);
	fprintf(stdout, "%s: IP has changed to %s", timestamp(), ipstr);
	fp = fopen("ip", "w");
	fprintf(fp, "%s", ipstr);
	fclose(fp);
	curl_easy_cleanup(curl);
	}
	
	struct config *conf = malloc(sizeof(struct config));
	
	fp = fopen("conf", "r");
	getString(conf->url, sizeof(conf->url), fp);
	getString(conf->email, sizeof(conf->email), fp);
	getString(conf->key, sizeof(conf->key), fp);
	getString(conf->record, sizeof(conf->record), fp);
	getString(conf->zoneID, sizeof(conf->zoneID), fp);
	getString(conf->recordID, sizeof(conf->recordID), fp);
	fclose(fp);
	fp = fmemopen(ipstr, sizeof(ipstr), "r");
	getString(conf->ip, sizeof(conf->ip), fp);
	fclose(fp);
	fp = fmemopen(conf->data, sizeof(conf->data), "w");
	fprintf(fp, "{\"id\":\"%s\",\"type\":\"A\",\"name\":\"%s\",\"content\":\"%s\",\"ttl\":120}", conf->zoneID, conf->record, conf->ip);
	fclose(fp);
	
	curl = curl_easy_init();
	if(curl)
	{
		char buff2[64];
		char buff3[64];
		
		struct curl_slist *headers = NULL;
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		
		snprintf(buff2, sizeof(buff2), "X-Auth-Email: ");
		strncat(buff2, conf->email, strlen(conf->email));
		headers = curl_slist_append(headers, buff2);
		
		snprintf(buff3, sizeof(buff3), "X-Auth-Key: ");
		strncat(buff3, conf->key, strlen(conf->key));
		headers = curl_slist_append(headers, buff3);

		curl_easy_setopt(curl, CURLOPT_URL, conf->url);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, conf->data);
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
			if (strstr(result, good) != NULL)
			{
				fprintf(log, "%s: SUCCESS - updated IP with cloudflare to %s", timestamp(), ipstr);
				fprintf(stdout, "%s: SUCCESS - updated IP with cloudflare to %s", timestamp(), ipstr);
			}
			else
			{
				fprintf(log, "%s: FAILURE - dumping result: %s", timestamp(), result);
				fprintf(stdout, "%s: FAILURE - dumping result: %s", timestamp(), result);
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
        fprintf(stderr , "String length out of bounds, exiting");
        exit(EXIT_FAILURE);
    }
    while (((c = getc(fp)) == '\n' || c == '\0') && c != EOF)
	{}
	if (c == EOF)
	{
		fprintf(stderr , "No string was found, exiting");
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
		fprintf(stderr , "Buffer too small for string. Flushing stream to next newline char or end of file");
		buffer[i] = '\0';
        while ((c = getc(fp)) != '\n' && c != '\0' && c != EOF)
        {
            i++;
            if (i > 1024)
            {
                fprintf(stderr , "Buffer overflow, exiting");
                exit(EXIT_FAILURE);
            }
        }
    }
    buffer[i] = '\0';
}

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	strncpy((char*)userp, (char*)buffer, realsize);
	return realsize;
}
