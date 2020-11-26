/*
 * @Author: Alex.Pan
 * @Date: 2019-11-26 11:08:10
 * @LastEditTime: 2020-11-23 19:51:28
 * @LastEditors: Alex.Pan
 * @Description:
 * @FilePath: \ESP32\project\SmartLight\components\webserver\web_server.c
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */

#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_https_server.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "mb_master.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
// #include "upload.h"

#define SSID_FILENAME   ("/lfs/web/ap_ssid_name.txt")

#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD


#define USER_MAX_AP_LIST_NUM    (10)


#define FS_BASE_PATH    CONFIG_WEB_SERVER_FILE_PATH


#define HTTP_FILE_LIST       ("/lfs/web/htmllist.txt")

#define HTTP_HEADER_LEN     (500)
#define HTTP_CONTEXT_MAX_SEDN_LEN    (1*1024)
#define HTTP_CONTEXT_MAX_RCV_LEN    (4*1024)


#define HTTP_FILE_NUM_MAX       30
#define HTTP_FILE_NAME_MAX_LEN  50
#define HTTP_URL_LEN    (32+HTTP_FILE_NAME_MAX_LEN)
#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define GET_MACHINE_INFO    ("getMachineInfo")
#define GET_CONFIG    ("getConfig")
#define GET_WORK_INFO    ("getWorkInfo")
#define GET_CHARTS    ("getCharts")
#define OFFLINEBUNDLE   ("offlineBundle")
#define GET_PSRTIME    ("getPsrTime")
#define LOGIN ("login")
#define DB_TABLE_SIZE   512

typedef struct
{
    char                filename[HTTP_FILE_NAME_MAX_LEN];
    char                put_uri[HTTP_URL_LEN];
    char                get_uri[HTTP_URL_LEN];
    bool                isupload;
    uint32_t            size;
} http_file_list_t;
http_file_list_t httpfilelist[HTTP_FILE_NUM_MAX];


typedef struct
{
    uint8_t ssid[32];      /**< SSID of target AP*/
    uint8_t password[64];  /**< password of target AP*/
} user_ap_list_t;

uint8_t aplitindex = 0;
user_ap_list_t aplist[USER_MAX_AP_LIST_NUM] =
{
    {"Honor 8X Max", "qwertqet"},
    {"Hikeralex", "qwertwer"},
    {"lektec360", "pyc123456"},



};


static const char *TAG = "WEBServer";



const char *web_server_HttpStatus_Ok                   = "OK";
const char *web_serverHttpStatus_Created              = "Created";
const char *web_server_HttpStatus_Found                = "Found";
const char *web_server_HttpStatus_BadRequest           = "BadRequest";
const char *web_server_HttpStatus_NotFound             = "NotFound";
const char *web_server_HttpStatus_InternalServerError  = "InternalServerError";

const char *STRING_SPACE = " ";
const char *STRING_ENTER = "\n";

typedef enum
{
    HTTP_CODE_Ok = 0,
    HTTP_CODE_Created,
    HTTP_CODE_Found,
    HTTP_CODE_BadRequest,
    HTTP_CODE_NotFound,
    HTTP_CODE_InternalServerError,

    HTTP_CODE_Buff
} HTTP_CODE_E;

typedef enum
{
    HTTP_CONTENT_TYPE_Html = 0,
    HTTP_CONTENT_TYPE_Js,
    HTTP_CONTENT_TYPE_Css,
    HTTP_CONTENT_TYPE_Json,
    HTTP_CONTENT_TYPE_Icon,
    HTTP_CONTENT_TYPE_Png,
    HTTP_CONTENT_TYPE_Gif,

    HTTP_CONTENT_TYPE_Stream,
    HTTP_CONTENT_TYPE_Buff
} HTTP_CONTENT_TYPE_E;

typedef enum
{
    HTTP_CACHE_CTL_TYPE_No = 0,
    HTTP_CACHE_CTL_TYPE_MaxAge_1h,
    HTTP_CACHE_CTL_TYPE_MaxAge_1d,
    HTTP_CACHE_CTL_TYPE_MaxAge_1w,
    HTTP_CACHE_CTL_TYPE_MaxAge_1m,
    HTTP_CACHE_CTL_TYPE_MaxAge_1y,

    HTTP_CACHE_CTL_TYPE_Buff
} HTTP_CACHE_CTL_TYPE_E;

typedef enum
{
    HTTP_ENCODING_Gzip = 0,

    HTTP_ENCODING_Buff
} HTTP_ENCODING_E;

const char szHttpCodeMap[][5] =
{
    "200",
    "201",
    "302",
    "400",
    "404",
    "500",

    ""
};

const char szHttpStatusMap[][20] =
{
    "OK",
    "Created",
    "Found",
    "BadRequest",
    "NotFound",
    "InternalServerError",

    ""
};

const char szHttpMethodStr[][10] =
{
    "GET",
    "POST",
    "HEAD",
    "PUT",
    "DELETE",
    ""
};

const char szHttpUserAgentStringmap[][10] =
{
    "Windows",
    "Android",
    "curl",
    "Wget",
    ""
};

const char szHttpContentTypeStr[][25] =
{
    "text/html, charset=utf-8",
    "application/x-javascript",
    "text/css",
    "application/json",
    "image/x-icon",
    "image/png",
    "image/gif",
    "image/svg+xml",

    "application/octet-stream",
    ""
};

const char szHttpEncodingStr[][10] =
{
    "gzip",

    ""
};

const char szHttpCacheControlStr[][20] =
{
    "no-cache",
    "max-age=3600",
    "max-age=86400",
    "max-age=604800",
    "max-age=2592000",
    "max-age=31536000",

    ""
};

#define HTML_FILE_STYLE_LEN (10)
#define HTML_FILE_STYLE_NUM (4)
const char htmlfilestyle[HTML_FILE_STYLE_NUM][HTML_FILE_STYLE_LEN] =
{
    ".css",
    ".js",
    ".html",
    ".svg"
};



const char aGzipSuffix[HTTP_ENCODING_Buff + 1][5] = {".gz", ""};



char HTML_UploadHtml[] = "<!DOCTYPE html>\r\n<html>\r\n<head>\r\n <meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\r\n <title>Upgrade</title>\r\n</head>\r\n<body>\r\n	<table border=\"0\" width=\"70%\" align=\"center\" cellpadding=\"6\" id=\"tab\" cellspacing=\"0\" >\r\n		<tr><th colspan=\"4\">Firmware\</th></tr>\r\n		<tr><td colspan=\"4\"><hr/></td></tr>\r\n		<tr align=\"left\">\r\n			<th width=\"40%\">File</th>\r\n			<th width=\"15%\">Size</th>\r\n			<th width=\"20%\">Status</th>\r\n			<th width=\"25%\"></th>\r\n		</tr>\r\n <tr align=\"left\">\r\n <td><input type=\"file\" id=\"binFile\" accept=\".bin\" onchange=\"return fileChg(this);\"></td>\r\n <td>----</td>\r\n <td>----</td>\r\n <td><input type=\"button\" onclick=\"upgread()\" value=\"Upgrade\"/></td>\r\n </tr>\r\n		<tr><td colspan=\"4\"><hr/></td></tr>\r\n <tr><td colspan=\"4\">&nbsp;</td></tr>\r\n		<tr><th colspan=\"4\">Html Upgrade</th></tr>\r\n <tr><td colspan=\"4\"><hr/></td></tr>\r\n <tr><td colspan=\"4\"><hr/></td></tr>\r\n		<tr>\r\n			<td colspan=\"3\"></td>\r\n			<td>\r\n <input type=\"button\" onclick=\"addFile()\" value=\"Add\"/>\r\n <input type=\"button\" onclick=\"uploadFile()\" value=\"Upgrade\"/>\r\n <input type=\"button\" onclick=\"reboot()\" value=\"reboot\"/>\r\n </td>\r\n		</tr>\r\n	</table>\r\n <script type=\"text/javascript\">\r\n window.onload = function() {\r\n			addFile();\r\n }\r\n	 function addFile() {\r\n			var t = document.getElementById('tab');\r\n			var r = t.insertRow(t.rows.length-2);\r\n			r.insertCell(0).innerHTML=\"<input type=\\\"file\\\" onchange=\\\"return fileChg(this);\\\">\";\r\n			r.insertCell(1).innerHTML=\"----\";\r\n			r.insertCell(2).innerHTML=\"----\";\r\n			r.insertCell(3).innerHTML=\"<a href=\\\"javascript:void(0);\\\" onclick=\\\"this.parentNode.parentNode.parentNode.removeChild(this.parentNode.parentNode)\\\">Delete</a>\";\r\n }\r\n		function fileChg(obj) {\r\n			var fz=obj.files[0].size;\r\n			if( fz > 1024*1024 ){\r\n				fz=(fz/1024/1024).toFixed(1) + \"MB\";\r\n			}else if(fz > 1024){\r\n				fz=(fz/1024).toFixed(1) + \"KB\";\r\n			}else{\r\n				fz=fz+\"B\";\r\n			}\r\n			var sta = obj.parentNode.parentNode.cells;\r\n sta[1].innerHTML = fz;\r\n sta[2].innerHTML = \"Waiting\";\r\n }\r\n\r\n		function uploadFile() {\r\n			var files = new Array();\r\n			var tableObj = document.getElementById(\"tab\");\r\n			for (var i = 8; i < tableObj.rows.length-2; i++) {\r\n				file = tableObj.rows[i].cells[0].getElementsByTagName(\"input\")[0];\r\n				if ( file.files[0] == null ){\r\n					continue;\r\n				}\r\n				files.push(file.files[0]);\r\n tableObj.rows[i].cells[2].innerHTML = \"wait for upload\";\r\n			}\r\n			if (files.length == 0){\r\n			 alert(\"choose file\");\r\n			 return;\r\n			}\r\n			if( sendHead(files)){\r\n sendFile(files, 0);\r\n }\r\n\r\n }\r\n function sendHead(fileObj) {\r\n			var dataArr=[];\r\n			for ( var i in fileObj ){\r\n				var data = {};\r\n				data.Name = fileObj[i].name;\r\n				data.Length = parseInt(fileObj[i].size);\r\n				dataArr.push(data);\r\n			}\r\n xhr = new XMLHttpRequest();\r\n xhr.open(\"post\", \"/html/header\", false);\r\n xhr.send(JSON.stringify(dataArr));\r\n return true;\r\n }\r\n function sendFile(fileObj, index) {\r\n if ( index >= fileObj.length){\r\n alert(\"Success\");\r\n return;\r\n }\r\n var t = document.getElementById('tab');\r\n xhr = new XMLHttpRequest();\r\n url = \"/html/\"+fileObj[index].name\r\n xhr.open(\"put\", url, true);\r\n xhr.upload.onprogress = function progressFunction(evt) {\r\n if (evt.lengthComputable) {\r\n t.rows[parseInt(8)+parseInt(index)].cells[2].innerHTML = Math.round(evt.loaded / evt.total * 100) + \"%\";\r\n }\r\n };\r\n t.rows[parseInt(8)+parseInt(index)].cells[2].innerHTML = \"%0\";\r\n xhr.onreadystatechange = function () {\r\n if ( xhr.readyState == 2 ){\r\n t.rows[parseInt(8)+parseInt(index)].cells[2].innerHTML = \"checking\";\r\n }else if (xhr.readyState == 4) {\r\n if( xhr.status == 201){\r\n t.rows[parseInt(8)+parseInt(index)].cells[2].innerHTML = \"Success\";\r\n index=index+1;\r\n sendFile(fileObj, index);\r\n }else{\r\n t.rows[parseInt(8)+parseInt(index)].cells[2].innerHTML = \"Failed\";\r\n }\r\n }\r\n }\r\n xhr.send(fileObj[index]);\r\n }\r\n function reboot(){\r\n xhr = new XMLHttpRequest();\r\n xhr.open(\"post\", \"/control\", true);\r\n xhr.onreadystatechange = function () {\r\n if (xhr.readyState == 4) {\r\n if( xhr.status == 200){\r\n alert(\"Rebootig\");\r\n }else{\r\n alert(\"reboot failed\");\r\n }\r\n }\r\n }\r\n xhr.send(\"{\\\"Action\\\":0}\");\r\n }\r\n function upgread(){\r\n var file = document.getElementById(\"binFile\").files[0];\r\n if(file == null){\r\n alert(\"choose firmware\");\r\n return;\r\n }\r\n var t = document.getElementById('tab');\r\n xhr = new XMLHttpRequest();\r\n xhr.upload.onprogress = function progressFunction(evt) {\r\n if (evt.lengthComputable) {\r\n t.rows[3].cells[2].innerHTML= Math.round(evt.loaded / evt.total * 100) + \"%\";\r\n }\r\n };\r\n xhr.open(\"put\", \"/upgrade\", true);\r\n t.rows[3].cells[2].innerHTML = \"0%\";\r\n xhr.onreadystatechange = function () {\r\n if ( xhr.readyState == 2 ){\r\n t.rows[3].cells[2].innerHTML = \"Checking\";\r\n }else if (xhr.readyState == 4) {\r\n if( xhr.status == 201){\r\n t.rows[3].cells[2].innerHTML = \"Success\";\r\n alert(\"Success!Rebooting!\");\r\n }else{\r\n t.rows[3].cells[2].innerHTML = \"Upgrade Failed!\";\r\n alert(\"Failed\");\r\n }\r\n }\r\n }\r\n xhr.send(file);\r\n }\r\n </script>\r\n</body>\r\n</html>\r\n";

char HTML_NotFound[] = "<html><head><title>404 Not Found</title></head><center><h1>404 Not Found</h1></center><hr><center>SmartPlug</center></body></html>";

char HTML_GetConfigOk[] = "{\"success\":true, \"msg\":\"set success\"}";
char HTML_ResultOk[] = "{\"result\":\"success\", \"msg\":\"\"}";
char HTML_BadRequest[] = "{\"result\":\"failed\", \"msg\":\"bad request\"}";
char HTML_InternalServerError[] = "{\"result\":\"failed\", \"msg\":\"internal server error\"}";

httpd_uri_t httpfileget[HTTP_FILE_NUM_MAX] = {0};
httpd_uri_t httpfileput[HTTP_FILE_NUM_MAX] = {0};
esp_err_t SmartLight_RegiterHtmlUrl(char *pData, httpd_handle_t handle);
esp_err_t httpfile_get_send(httpd_req_t *req, char *filename);
http_file_list_t *HTTP_GetFileList(char *pcName);

size_t getfilesize(char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (NULL == fp)
    {
        ESP_LOGE(TAG, "getfilesize file %s not found!", filename);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);

    fclose(fp);
    return size;
}


void HTTP_FileListInit(void)
{
    uint32_t i = 0;

    for (i = 0; i < HTTP_FILE_NUM_MAX; i ++)
    {
        httpfilelist[i].size    = 0;
        httpfilelist[i].isupload    = false;
        memset(httpfilelist[i].filename, 0, HTTP_FILE_NAME_MAX_LEN);
        memset(httpfilelist[i].get_uri, 0, HTTP_URL_LEN);
        memset(httpfilelist[i].put_uri, 0, HTTP_URL_LEN);
    }
}


http_file_list_t *HTTP_GetFileList(char *pcName)
{
    uint32_t i = 0;

    if (pcName == NULL)
    {
        return &httpfilelist[0];
    }

    for (i = 0; i < HTTP_FILE_NUM_MAX; i ++)
    {

        if (strcmp(pcName, httpfilelist[i].filename) == 0)
        {
            return &httpfilelist[i];
        }
    }
    return NULL;
}

esp_err_t SmartLight_ParseHtmlfilelist(char *pData)
{
    cJSON *pJsonArr = NULL;
    cJSON *pJsonIteam = NULL;

    http_file_list_t *pstHtml;


    if (pData == NULL)
    {
        ESP_LOGI(TAG, "PSR_ParseHtmlData, pData is NULL.");
        return ESP_FAIL;
    }

    cJSON *pJsonRoot = cJSON_Parse(pData);
    if (pJsonRoot == NULL)
    {
        ESP_LOGI(TAG, "cJSON_Parse failed, pDateStr:%s.", pData);
        goto error;
    }


    uint32_t iCount = cJSON_GetArraySize(pJsonRoot);
    ESP_LOGD(TAG, "/html/header JSON Arry num:%d.", iCount);
    if ((iCount <= 0)||(iCount > HTTP_FILE_NUM_MAX))
    {
        ESP_LOGE(TAG, "cJSON_GetArraySize failed, iCount:%d.", iCount);
        goto error;
    }

    HTTP_FileListInit();

    pstHtml = HTTP_GetFileList(NULL);

    for (size_t uiLoop = 0; uiLoop < iCount; uiLoop++)
    {
        pJsonArr = cJSON_GetArrayItem(pJsonRoot, uiLoop);
        if (!cJSON_IsInvalid(pJsonArr))
        {
            pJsonIteam = NULL;
            pJsonIteam = cJSON_GetObjectItem(pJsonArr, "Name");
            if (cJSON_IsString(pJsonIteam))
            {
                memset(pstHtml->filename, 0, HTTP_FILE_NAME_MAX_LEN);
                strcpy(pstHtml->filename, "/lfs/web/");
                strcat(pstHtml->filename, cJSON_GetStringValue(pJsonIteam));
                ESP_LOGD(TAG, "filename %s", (char *)&pstHtml->filename[0]);

                memset(pstHtml->get_uri, 0, HTTP_URL_LEN);
                strcpy((char *)&pstHtml->get_uri[0], "/");
                strcat((char *)&pstHtml->get_uri[0], cJSON_GetStringValue(pJsonIteam));
                ESP_LOGD(TAG, "get url is %s", (char *)&pstHtml->get_uri[0]);

                memset(pstHtml->put_uri, 0, HTTP_URL_LEN);
                strcpy((char *)&pstHtml->put_uri[0], "/html/");
                strcat((char *)&pstHtml->put_uri[0], cJSON_GetStringValue(pJsonIteam));
                ESP_LOGD(TAG, "put url is %s", (char *)&pstHtml->put_uri[0]);

            }
            else
            {
                memset(pstHtml->filename, 0, HTTP_FILE_NAME_MAX_LEN);
            }

            pJsonIteam = NULL;
            pJsonIteam = cJSON_GetObjectItem(pJsonArr, "Length");
            if (cJSON_IsNumber(pJsonIteam))
            {
                pstHtml->size = pJsonIteam->valueint;
                ESP_LOGD(TAG, "File size %d", pstHtml->size);
            }
            else
            {
                pstHtml->size = 0;
            }

            if ((0 != pstHtml->size) && (0 != strlen((char *)&pstHtml->filename[0])))
            {
                pstHtml->isupload = true;
            }
            else
            {
                pstHtml->isupload = false;
            }

            ESP_LOGD(TAG, "File isupload %d\n", pstHtml->isupload);

        }
        else
        {
            goto error;
        }

        pstHtml++;
    }

    cJSON_Delete(pJsonRoot);
    return ESP_OK;

error:
    cJSON_Delete(pJsonRoot);
    return ESP_FAIL;

}




/**
 * @brief
 * @note
 * @param  *req:
 * @retval
 */
/* An HTTP GET handler */
esp_err_t httpheader_post_handler(httpd_req_t *req)
{
    char  *buf;
    size_t buf_len;

    //http_header_debug(req);

    buf_len = req->content_len;
    buf = malloc(buf_len);
    if (NULL == buf)
    {
        ESP_LOGE(TAG, "ERR malloc \n");
    }
    memset(buf, 0, buf_len);
    httpd_req_recv(req, buf, buf_len);
    ESP_LOGD(TAG, "Found Content len %d, Content: %s", buf_len, buf);

    FILE *f = fopen("/lfs/web/htmllist.txt", "wb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    fwrite(buf, 1, buf_len, f);

    fclose(f);

    SmartLight_ParseHtmlfilelist(buf);

    httpd_handle_t handle = req->handle;
    SmartLight_RegiterHtmlUrl(buf, handle);


    free(buf);

    /* Set some custom headers */
    ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[HTTP_CONTENT_TYPE_Json]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[HTTP_CACHE_CTL_TYPE_No]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "Keep-Alive"));

    /* Send response with custom headers and body set as the
    * string passed in user context*/
    if (NULL == req->user_ctx)
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, NULL, 0));
    }
    else
    {
        const char *resp_str = (const char *)req->user_ctx;
        ESP_ERROR_CHECK(httpd_resp_send(req, resp_str, strlen(resp_str)));
    }

    return ESP_OK;
}

httpd_uri_t htmlheaderload =
{
    .uri = "/html/header",
    .method = HTTP_POST,
    .handler = httpheader_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = NULL
};



/**
 * @brief
 * @note
 * @param  *req:
 * @retval
 */
/* An HTTP GET handler */
esp_err_t upload_get_handler(httpd_req_t *req)
{

    ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[0]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[5]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "Keep-Alive"));

    /* Send response with custom headers and body set as the
    * string passed in user context*/
    const char *resp_str = (const char *)req->user_ctx;
    if (NULL == resp_str)
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, NULL, 0));
    }
    else
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, resp_str, strlen(resp_str)));
    }

    return ESP_OK;
}

httpd_uri_t upload =
{
    .uri = "/upload",
    .method = HTTP_GET,
    .handler = upload_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = HTML_UploadHtml
};


void delay_ms(size_t ms)
{
    size_t i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 1000; j++)
            ;
}





/**
 * @brief
 * @note
 * @param  *req:
 * @retval
 */
/* An HTTP GET handler */
esp_err_t modbus_get_handler(httpd_req_t *req)
{
    /* Set some custom headers */
    //ESP_ERROR_CHECK(httpd_resp_set_status(req, szHttpContentTypeStr[0]));
    /*向modbus获取所有数据信息*/
    // char *param_buf = NULL;
    // modbus_param_get(param_buf);
    char *param_buf = modbus_param_get();
    

    ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[HTTP_CONTENT_TYPE_Html]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[HTTP_CACHE_CTL_TYPE_No]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "Keep-Alive"));

    /* Send response with custom headers and body set as the
    * string passed in user context*/
    // const char *resp_str = (const char *)&getCharts[0];
    // const char *resp_str = resp_str = NULL;
    
    if (NULL == param_buf)
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, NULL, 0));
    }
    else
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, param_buf, strlen(param_buf)));
    }

    if(NULL != param_buf)
        free(param_buf);

    return ESP_OK;
}

httpd_uri_t modbus_get =
{
    .uri = "/modbusGet",
    .method = HTTP_GET,
    .handler = modbus_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = NULL
};





/**
 * @brief
 * @note
 * @param  *req:
 * @retval
 */
/* An HTTP GET handler */
esp_err_t modbus_set_handler(httpd_req_t *req)
{
    char  *buf;
    size_t buf_len;
    // char *cmdbuf;
    // size_t count = 0;
    const char *resp_str = NULL;

    buf_len = req->content_len;
    buf = malloc(buf_len);
    if (NULL == buf)
    {
        return ESP_FAIL;
    }
    httpd_req_recv(req, buf, buf_len);
    ESP_LOGI(TAG, "control rcv %s", buf);

    if(ESP_OK == modbus_param_set(buf))
    {
        resp_str = &HTML_BadRequest[0];
    }
    else
    {
        resp_str = &HTML_GetConfigOk[0];
    }
    
    free(buf);

    /* Set some custom headers */
    ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[HTTP_CONTENT_TYPE_Html]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[HTTP_CACHE_CTL_TYPE_No]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
    //ESP_ERROR_CHECK(httpd_resp_set_hdr(req,"Connection","Keep-Alive"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "close"));

    /* Send response with custom headers and body set as the
    * string passed in user context*/
    if (NULL == resp_str)
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, NULL, 0));
    }
    else
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, resp_str, strlen(resp_str)));
    }

    return ESP_OK;
}

httpd_uri_t modbus_set =
{
    .uri = "/modbusSet",
    .method = HTTP_POST,
    .handler = modbus_set_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = HTML_GetConfigOk
};



/**
 * @description:
 * @param {type}
 * @return:
 */
esp_err_t httphome_get_handler(httpd_req_t *req)
{
    char  *buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK)
        {
            ESP_LOGD(TAG, "Found header => Host: %s", buf);
        }

        /* Set some custom headers */
        ESP_ERROR_CHECK(httpd_resp_set_status(req, "302 Found"));
        ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[HTTP_CONTENT_TYPE_Html]));
        ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[HTTP_CACHE_CTL_TYPE_No]));
        ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
        ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "Keep-Alive"));


        char *urlstr = NULL;
        size_t count = 0;
        urlstr = malloc(40);
        if (NULL != urlstr)
        {
            count = sprintf(urlstr, "%s", "http://");
            count += sprintf(urlstr + count, "%s", buf);
            count += sprintf(urlstr + count, "%s", "/index.html");
            ESP_LOGD(TAG, "The Location url:%s", urlstr);

            ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Location", urlstr));
        }

        /* Send response with custom headers and body set as the
        * string passed in user context*/
        if (NULL == req->user_ctx)
        {
            ESP_ERROR_CHECK(httpd_resp_send(req, NULL, 0));
        }
        else
        {
            const char *resp_str = (const char *)req->user_ctx;
            ESP_ERROR_CHECK(httpd_resp_send(req, resp_str, strlen(resp_str)));
        }

        free(urlstr);
        free(buf);
    }

    return ESP_OK;
}
httpd_uri_t httpgethome =
{
    .uri = "/",
    .method = HTTP_GET,
    .handler = httphome_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = NULL
};

/**
 * @description:
 * @param {type}
 * @return:
 */
esp_err_t http_ota_put_handler(httpd_req_t *req)
{
    char  *buf = NULL;
    size_t buf_len = 0;
    size_t buf_rcvd_len = 0;
    int rcv_len = HTTPD_SOCK_ERR_FAIL;
    size_t total_len = 0;
    size_t left_len = 0;
    static size_t update_file_total_size;
    static const esp_partition_t *update_partition;
    static esp_ota_handle_t ota_handle;
    esp_err_t err = ESP_OK;
    const char *resp_str;


    update_file_total_size = left_len = total_len = req->content_len;
    buf_rcvd_len = 0;

    ESP_LOGI(TAG,"http ota file_size:%d\n", update_file_total_size);

    /* Get download partition information and erase download partition data */
    /*查找升级分区*/
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG,"Find partion (%s), address 0x%x, size %d",update_partition->label,
                                                    update_partition->address,
                                                    update_partition->size);
    if (update_partition == NULL)
    {
        ESP_LOGE(TAG,"Firmware download failed! Get next update partition  error!");
        goto HTTP_OTA_FAIL;
    }

    ESP_LOGI(TAG,"esp http ota begin");
    err = esp_ota_begin(update_partition,update_file_total_size,&ota_handle);
    if(ESP_OK != err)
    {
        ESP_LOGE(TAG,"http ota begin failed (%s)",esp_err_to_name(err));
        goto HTTP_OTA_FAIL;
    }

    do
    {

        buf_len = MIN(left_len, HTTP_CONTEXT_MAX_RCV_LEN);

        buf = malloc(buf_len);
        if (NULL == buf)
        {
            ESP_LOGE(TAG, "%d bytes memery malloc failed\n", buf_len);
            goto HTTP_OTA_FAIL;
        }
        //ESP_LOGI(TAG,"malloc memery %d",buf_len);
        do
        {
            memset(buf, 0, buf_len);
            rcv_len = httpd_req_recv(req, buf, buf_len);
            err = esp_ota_write(ota_handle,buf, rcv_len);
            if(ESP_OK != err)
            {
                ESP_LOGE(TAG,"ymodem ota write failed (%s)",esp_err_to_name(err));
                goto HTTP_OTA_FAIL;
            }
        }
        while (rcv_len == HTTPD_SOCK_ERR_TIMEOUT);
        free(buf);
        buf = NULL;
    }
    while ((buf_rcvd_len < total_len) && (rcv_len != 0));

    esp_ota_end(ota_handle);



    /* Set some custom headers */
    ESP_ERROR_CHECK(httpd_resp_set_status(req, "201 Created"));
    ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[HTTP_CONTENT_TYPE_Html]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[HTTP_CACHE_CTL_TYPE_No]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "Keep-Alive"));

    resp_str = &HTML_GetConfigOk[0];
 
    /* Send response with custom headers and body set as the
    * string passed in user context*/
    if (NULL == req->user_ctx)
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, NULL, 0));
    }
    else
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, resp_str, strlen(resp_str)));
    }

    ESP_LOGI(TAG,"Download firmware to flash success.\n");
    ESP_LOGI(TAG,"System now will restart...\r\n");

    esp_ota_set_boot_partition(update_partition);
    vTaskDelay(100);
    /* Reset the device, Start new firmware */
    esp_restart();

    return ESP_OK;

HTTP_OTA_FAIL:

    if(NULL != buf)
    {
        free(buf);
        buf = NULL;
    }

        /* Set some custom headers */
    ESP_ERROR_CHECK(httpd_resp_set_status(req, "201 Created"));
    ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[HTTP_CONTENT_TYPE_Html]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[HTTP_CACHE_CTL_TYPE_No]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "Keep-Alive"));

    resp_str = &HTML_BadRequest[0];
 
    /* Send response with custom headers and body set as the
    * string passed in user context*/
    if (NULL == req->user_ctx)
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, NULL, 0));
    }
    else
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, resp_str, strlen(resp_str)));
    }

    return ESP_FAIL;

       
}

httpd_uri_t upgrade =
{
    .uri = "/upgrade",
    .method = HTTP_PUT,
    .handler = http_ota_put_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = NULL
};

/**
 * @brief
 * @note
 * @param  *req:
 * @retval
 */
/* An HTTP GET handler */
esp_err_t httpfile_put_handler(httpd_req_t *req)
{
    char  *buf;
    size_t buf_len = 0;
    size_t buf_rcvd_len = 0;
    int rcv_len = HTTPD_SOCK_ERR_FAIL;
    size_t total_len = 0;
    size_t left_len = 0;
    size_t size = 0;
    http_file_list_t *phtml = NULL;


    left_len = total_len = req->content_len;
    buf_rcvd_len = 0;

    for (size_t i = 0; i < HTTP_FILE_NUM_MAX; i++)
    {
        if (0 == strcmp(&httpfilelist[i].put_uri[0], &req->uri[0]))
        {
            phtml = &httpfilelist[i];
            break;
        }
    }
    if (NULL == phtml)
    {
        ESP_LOGE(TAG, "No put method");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Opening file %s total len %d", phtml->filename, total_len);

    FILE *f = fopen(phtml->filename, "wb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s", phtml->filename);
        return ESP_FAIL;
    }

    do
    {

        buf_len = MIN(left_len, HTTP_CONTEXT_MAX_RCV_LEN);

        buf = malloc(buf_len);
        if (NULL == buf)
        {
            ESP_LOGE(TAG, "%d bytes memery malloc failed\n", buf_len);
            return ESP_FAIL;
        }

        do
        {
            memset(buf, 0, buf_len);
            rcv_len = httpd_req_recv(req, buf, buf_len);
            if (rcv_len > 0)
            {
                fwrite(buf, 1, rcv_len, f);
                buf_rcvd_len += rcv_len;
                left_len -= rcv_len;
            }
            else
            {

            }
        }
        while (rcv_len == HTTPD_SOCK_ERR_TIMEOUT);
        free(buf);
    }
    while ((buf_rcvd_len < total_len) && (rcv_len != 0));

    fclose(f);
    size = getfilesize(phtml->filename);
    ESP_LOGD(TAG, "File %s size %d", phtml->filename, (int)size);


    /* Set some custom headers */
    ESP_ERROR_CHECK(httpd_resp_set_status(req, "201 Created"));
    ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[HTTP_CONTENT_TYPE_Html]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[HTTP_CACHE_CTL_TYPE_No]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "Keep-Alive"));

    /* Send response with custom headers and body set as the
    * string passed in user context*/
    if (NULL == req->user_ctx)
    {
        ESP_ERROR_CHECK(httpd_resp_send(req, NULL, 0));
    }
    else
    {
        const char *resp_str = (const char *)req->user_ctx;
        ESP_ERROR_CHECK(httpd_resp_send(req, resp_str, strlen(resp_str)));
    }

    return ESP_OK;
}





/**
 * @description:
 * @param {type}
 * @return:
 */
esp_err_t httpfile_get_send(httpd_req_t *req, char *filename)
{
    size_t totalsize = 0;
    size_t leftsize = 0;
    size_t cur_send_size = 0;
    size_t index = 0;
    char *buf = NULL;


    leftsize = totalsize = getfilesize(filename);
    if (0 != totalsize)
    {
        if (leftsize <= HTTP_CONTEXT_MAX_SEDN_LEN)
        {
            cur_send_size = MIN(leftsize, HTTP_CONTEXT_MAX_SEDN_LEN);
            buf = malloc(cur_send_size);
            if (NULL == buf)
            {
                ESP_LOGE(TAG, "Memery malloc failed");
                return ESP_ERR_NO_MEM;
            }

            FILE *f = fopen(filename, "rb");
            if (NULL == f)
            {
                ESP_LOGE(TAG, "file %s open failed!", filename);
                return ESP_ERR_NOT_FOUND;
            }
            fread(buf, 1, cur_send_size, f);

            httpd_resp_send(req, buf, cur_send_size);

            fclose(f);
            free(buf);
        }
        else
        {
            do
            {

                cur_send_size = MIN(leftsize, HTTP_CONTEXT_MAX_SEDN_LEN);
                buf = malloc(cur_send_size);
                if (NULL == buf)
                {
                    ESP_LOGE(TAG, "Memery malloc failed");
                    return ESP_ERR_NO_MEM;
                }

                FILE *f = fopen(filename, "rb");
                if (NULL == f)
                {
                    ESP_LOGE(TAG, "file %s open failed!", filename);
                    return ESP_ERR_NOT_FOUND;
                }

                fseek(f, index, SEEK_SET);

                fread(buf, 1, cur_send_size, f);
                httpd_resp_send_chunk(req, buf, cur_send_size);

                ESP_LOGD(TAG, "filename%s, cur_send_size %d", filename, cur_send_size);
                index += cur_send_size;
                leftsize -= cur_send_size;
                if (leftsize <= 0)
                {
                    leftsize = 0;
                    ESP_LOGD(TAG, "filename%s, end", filename);
                }
                fclose(f);
                free(buf);

            }
            while (leftsize > 0);
            ESP_ERROR_CHECK(httpd_resp_send_chunk(req, NULL, 0));
        }

        return ESP_OK;
    }
    ESP_LOGW(TAG, "File %s not found", filename);

    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief
 * @note
 * @param  *req:
 * @retval
 */
/* An HTTP GET handler */
esp_err_t httpfile_get_handler(httpd_req_t *req)
{
    size_t i = 0;
    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    ESP_LOGD(TAG, "httpfile_get_handler req->uri %s", req->uri);



    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", szHttpCacheControlStr[HTTP_CACHE_CTL_TYPE_MaxAge_1y]));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Accept-Ranges", "bytes"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "Keep-Alive"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "content-encoding", "gzip"));

    /* Send response with custom headers and body set as the
     * string passed in user context*/

    for (i = 0; i < HTTP_FILE_NUM_MAX; i++)
    {
        if ((0 == strcmp(&httpfilelist[i].get_uri[0], req->uri)) && httpfilelist[i].isupload)
        {
            ESP_LOGD(TAG, "%s is find,the get  url is %s", &httpfilelist[i].filename[0], &httpfilelist[i].get_uri[0]);

            char *str = NULL;
            str = strstr(req->uri, htmlfilestyle[0]);
            if (NULL != str)
            {
                ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[2]));
            }

            str = NULL;
            str = strstr(req->uri, htmlfilestyle[1]);
            if (NULL != str)
            {
                ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[1]));
            }

            str = NULL;
            str = strstr(req->uri, htmlfilestyle[2]);
            if (NULL != str)
            {
                ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[0]));
            }

            str = NULL;
            str = strstr(req->uri, htmlfilestyle[3]);
            if (NULL != str)
            {
                ESP_ERROR_CHECK(httpd_resp_set_type(req, szHttpContentTypeStr[7]));
            }

            return httpfile_get_send(req, (char *)&httpfilelist[i].filename[0]);
        }
    }

    ESP_LOGW(TAG, "no html file found");

    return ESP_FAIL;
}




esp_err_t SmartLight_RegiterHtmlUrl(char *pData, httpd_handle_t handle)
{
    for (size_t i = 0; i < HTTP_FILE_NUM_MAX; i++)
    {
        if (0 != strlen((char *)(char *)&httpfilelist[i].put_uri[0]))
        {
            httpd_unregister_uri_handler(handle, (char *)&httpfilelist[i].put_uri[0], HTTP_PUT);
        }
        if (0 != strlen((char *)(char *)&httpfilelist[i].get_uri[0]))
        {
            httpd_unregister_uri_handler(handle, (char *)&httpfilelist[i].get_uri[0], HTTP_GET);
        }
    }

    for (size_t i = 0; i < HTTP_FILE_NUM_MAX; i++)
    {

        if (0 != strlen((char *)(char *)&httpfilelist[i].put_uri[0]))
        {
            httpfileput[i].uri = (char *)&httpfilelist[i].put_uri[0];
            httpfileput[i].method = HTTP_PUT;
            httpfileput[i].handler = httpfile_put_handler;
            httpfileput[i].user_ctx = NULL;
            httpd_register_uri_handler(handle, &httpfileput[i]);
            ESP_LOGD(TAG, "html file put uri %s is registed", (char *)&httpfileput[i].uri[0]);
        }

        if (0 != strlen((char *)(char *)&httpfilelist[i].get_uri[0]))
        {
            httpfileget[i].uri = (char *)&httpfilelist[i].get_uri[0];
            httpfileget[i].method = HTTP_GET;
            httpfileget[i].handler = httpfile_get_handler;
            httpfileget[i].user_ctx = NULL;
            httpd_register_uri_handler(handle, &httpfileget[i]);
            ESP_LOGD(TAG, "html file get uri %s is registed", (char *)&httpfileget[i].uri[0]);
        }
    }

    return ESP_OK;
}


httpd_handle_t start_webserver(void)
{
    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    // extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    // extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    // conf.cacert_pem = cacert_pem_start;
    // conf.cacert_len = cacert_pem_end - cacert_pem_start;

    // extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    // extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    // conf.prvtkey_pem = prvtkey_pem_start;
    // conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;
    // conf.httpd.max_uri_handlers = 100;
    // conf.httpd.max_resp_headers = 100;

    // esp_err_t ret = httpd_ssl_start(&server, &conf);
    // if (ESP_OK != ret)
    // {
    //     ESP_LOGI(TAG, "Error starting server!");
    //     return NULL;
    // }

    config.max_uri_handlers = 100;
    config.max_resp_headers = 100;



    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
        // if (1)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");

        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &upload));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &htmlheaderload));   //处理文件列表
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpgethome));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &upgrade));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpgetconfig));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpcontorl));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpconfig));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpgetcharts));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpgetworkinfo));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpgetmachineinfo));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpofflineBundle));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpgetPsrTime));
        // ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httplogin));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &modbus_get));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &modbus_set));


        for (size_t i = 0; i < HTTP_FILE_NUM_MAX; i++)
        {
            if (httpfilelist[i].isupload && 0 != strlen((char *)&httpfilelist[i].get_uri[0]))
            {

                if (NULL != httpfileget)
                {
                    httpfileget[i].uri = (char *)&httpfilelist[i].get_uri[0];
                    httpfileget[i].method = HTTP_GET;
                    httpfileget[i].handler = httpfile_get_handler;
                    httpfileget[i].user_ctx = NULL;
                    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &httpfileget[i]));
                    ESP_LOGD(TAG, "uri %s is registed", (char *)&httpfilelist[i].get_uri[0]);
                }
            }
        }

        return server;
    }

    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

esp_err_t webfilelistinit(void)
{
    char *filename = HTTP_FILE_LIST;
    FILE *f = NULL;
    size_t size = 0;
    char *buf = NULL;

    HTTP_FileListInit();

    size = getfilesize(HTTP_FILE_LIST);

    if (0 != size)
    {

        f = fopen(filename, "rb");
        if (NULL == f)
        {
            ESP_LOGE(TAG, "file %s open failed!", filename);
            return ESP_FAIL;
        }

        buf = malloc(size);
        if (NULL == buf)
        {
            ESP_LOGE(TAG, "malloc %d memery failed!", size);
            return ESP_FAIL;
        }

        fseek(f, 0, SEEK_SET);

        fread(buf, 1, size, f);

        SmartLight_ParseHtmlfilelist(buf);

        free(buf);

        fclose(f);
    }
    else
    {
        ESP_LOGW(TAG, "file %s not found!", filename);
        return ESP_OK;
    }


    return ESP_OK;
}


static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *) arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *) arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}


/**
 * @description:
 * @param {type}
    * @return:
 */
void web_ser_init(void)
{
    static httpd_handle_t server = NULL;

    webfilelistinit();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &disconnect_handler, &server));

    server = start_webserver();
}

