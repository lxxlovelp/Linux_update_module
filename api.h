#pragma once
#include "httplib.h"


void setupUploadApi(httplib::Server& svr);
void setupDownloadApi(httplib::Server& svr);