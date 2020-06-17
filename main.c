#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
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
		fprintf(stderr , "Buffer too small for string. Flushing stream no next newline char or end of file");
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

int main()
{
	CURL *curl;
	CURLcode res;
	FILE *fp;
	FILE *log;
	log = fopen("ipup.log", "a");
	struct stat file_info;
	char ipstr[64];
	char buff[64];
	char timestamp[64];
	time_t now;
	time(&now);
	struct tm *local = localtime(&now);
	strftime(timestamp, sizeof buff, "%F %X", local);
	
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
			fprintf(stderr, "%s: curl_easy_perform() failed to get IP: %s\n", timestamp, curl_easy_strerror(res));
			fprintf(log, "%s: curl_easy_perform() failed to get IP: %s\n", timestamp, curl_easy_strerror(res));
			exit(EXIT_FAILURE);
		}
	fp = fopen("ip", "r");
	fgets(buff, (sizeof(buff)), fp);
	fclose(fp);
	//if (!strcmp(ipstr, buff))
	//	exit(EXIT_SUCCESS);
	fprintf(log, "%s: IP has changed. ", timestamp);
	fprintf(stdout, "%s: IP has changed. ", timestamp);
	fp = fopen("ip", "w");
	fprintf(fp, "%s", ipstr);
	fclose(fp);
	curl_easy_cleanup(curl);
	}
  	
  	struct config *conf = malloc(sizeof(struct config));
	//size_t len;
	
	fp = fopen("conf", "r");
	getString(conf->url, sizeof(conf->url), fp);
	
	//len = strlen(conf->url);
	//if (len > 0 && conf->url[len-1] == '\n')
    //conf->url[--len] = '\0';
    
	getString(conf->email, sizeof(conf->email), fp);
	getString(conf->key, sizeof(conf->key), fp);
	getString(conf->record, sizeof(conf->record), fp);
	getString(conf->zoneID, sizeof(conf->zoneID), fp);
	getString(conf->recordID, sizeof(conf->recordID), fp);
	fclose(fp);
	fp = fmemopen(ipstr, sizeof(ipstr), "r");
	getString(conf->ip, sizeof(conf->ip), fp);
	fclose(fp);
	//printf("%s", conf->url);
	fp = fmemopen(conf->data, sizeof(conf->data), "w");
	fprintf(fp, "{\"id\":\"%s\",\"type\":\"A\",\"name\":\"%s\",\"content\":\"%s\",\"ttl\":120}", conf->zoneID, conf->record, conf->ip);
	fclose(fp);
	//printf("%s", conf->data);
	
	 /* get a curl handle */ 
	curl = curl_easy_init();
	if(curl)
	{
		char buff2[64];
		char buff3[64];
		struct curl_slist *headers = NULL;
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		sprintf(buff2, "X-Auth-Email: ");
		strncat(buff2, conf->email, strlen(conf->email));
		headers = curl_slist_append(headers, buff2);
		sprintf(buff3, "X-Auth-Key: ");
		strncat(buff3, conf->key, strlen(conf->key));
		headers = curl_slist_append(headers, buff3);
		printf("%s%s\n", buff2, buff3);
    /* enable uploading */ 

		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
 
    /* HTTP PUT please */ 
	//curl_easy_setopt(curl, CURLOPT_PUT, 1L);
 
    /* specify target URL, and note that this URL should include a file
    name, not only a directory */ 
		curl_easy_setopt(curl, CURLOPT_URL, conf->url);

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    /* now specify which file to upload */ 
		curl_easy_setopt(curl, CURLOPT_READDATA, conf->data);
 
    /* provide the size of the upload, we specicially typecast the value
       to curl_off_t since we must be sure to use the correct data size */ 
		curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
					 (curl_off_t)file_info.st_size);
 
    /* Now run off and do what you've been told! */ 
		res = curl_easy_perform(curl);
    /* Check for errors */ 
		if(res != CURLE_OK)
		{
			fprintf(log, "curl_easy_perform() failed to update IP with cloudflare: %s\n", curl_easy_strerror(res));
			fprintf(stderr, "curl_easy_perform() failed to update IP with cloudflare: %s\n", curl_easy_strerror(res));
			exit(EXIT_FAILURE);
		}
		else
		{
			fprintf(log, "SUCCESS - updated IP with cloudflare\n");
			fprintf(stdout, "SUCCESS - updated IP with cloudflare\n");
		}
 
    /* always cleanup */ 
		curl_easy_cleanup(curl);
	}
	
	curl_global_cleanup();
	free(conf);
	fclose(log);
	return 0;
	}
