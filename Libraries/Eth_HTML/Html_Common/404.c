// Includes --------------------------------------------------------------------
#include "404.h"

// Parsed html
static const char str_0[] = "\
<!DOCTYPE html PUBLIC \"HTML 4.01 Transitional/EN\">\r\
<html>\r\
<head>\r\
  <meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\">\r\
  <title>error 404</title>\r\
</head>\r\
\r\
<body>\r\
  <font size=\"+2\">\r\
    Error 404 - Not found<br>\r\
    <br>\r\
  </font>\r\
  <font size=\"+1\">\r\
    <a href=\"HTML_Main.html\">Home</a><br>\r\
  </font>\r\
</body>\r\
</html>";

// Public functions ------------------------------------------------------------
BaseType_t Send_404(HTTPClient_t *pxClient)
{
	// Send html header
	SendHTML_Header_404(pxClient);

	SendHTML_Block(pxClient, str_0, sizeof(str_0) - 1);
	
	// Send last empty block
	return SendHTML_Block(pxClient, "", 0);
}

