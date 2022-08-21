// Includes --------------------------------------------------------------------
/* Application includes */
#include "httpserver-netconn.h"
#include "html_txt_funcs.h"

// Private constants -----------------------------------------------------------
// Debug options ---------------------------------------------------------------
// Variables -------------------------------------------------------------------
// Private function prototypes -------------------------------------------------

// Public functions ------------------------------------------------------------
void SendInput(HTTPClient_t *pxClient, bool disabled, bool readonly,
		char* name, uint16_t nameLength,
		char* value, uint16_t valueLength,
		uint16_t size)
{
	static const char input_b[] = "<input name=\"";
	static const char input_1[] ="\" size=\"";
	static const char input_2[] ="\" value=\"";
	static const char input_3[] = "\"";
	static const char input_e[] = ">";

	SendHTML_Block(pxClient, input_b, sizeof(input_b) - 1);
	/* Send name */
	SendHTML_Block(pxClient, name, nameLength);
	SendHTML_Block(pxClient, input_1, sizeof(input_1) - 1);

	/* Send size */
	/* Create temporary array for 5 digits + terminate string symbol */
	char* tmpStr = (char*)pvPortMalloc(6);
	SetNumToStr(size, tmpStr, 6);
	SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, 6));
	/* Free temporary string buffer */
	vPortFree(tmpStr);
	SendHTML_Block(pxClient, input_2, sizeof(input_2) - 1);

	/* Send value */
	if(*value != 0) SendHTML_Block(pxClient, value, valueLength);
	SendHTML_Block(pxClient, input_3, sizeof(input_3) - 1);

	/* Send modifiers attributes */
	if(disabled)
		SendHTML_Block(pxClient, " disabled",
				sizeof(" disabled") - 1);
	if(readonly)
		SendHTML_Block(pxClient, " readonly",
				sizeof(" disabled") - 1);

	SendHTML_Block(pxClient, input_e, sizeof(input_e) - 1);
}

void SendCheckBox(HTTPClient_t *pxClient, bool disabled, bool readonly,
		char* name, uint16_t nameLength, bool checked)
{
	static const char check_box_b[] = "<input name=\"";//2
	static const char check_box_1[] ="\" ";
	static const char check_box_2[] = "type=\"checkbox\"";
	static const char check_box_e[] = ">";

	SendHTML_Block(pxClient, check_box_b, sizeof(check_box_b) - 1);
	SendHTML_Block(pxClient, name, nameLength);
	SendHTML_Block(pxClient, check_box_1, sizeof(check_box_1) - 1);
	if(checked)
		SendHTML_Block(pxClient, "checked ", sizeof("checked ") - 1);
	SendHTML_Block(pxClient, check_box_2, sizeof(check_box_2) - 1);

	/* Send modifiers attributes */
	if(disabled)
		SendHTML_Block(pxClient, " disabled",
				sizeof(" disabled") - 1);
	if(readonly)
		SendHTML_Block(pxClient, " readonly",
				sizeof(" disabled") - 1);

	SendHTML_Block(pxClient, check_box_e, sizeof(check_box_e) - 1);
}

uint16_t GetSizeOfStr(const char* str, uint16_t size)
{
	for(uint16_t i = 0; i < size; i++)
	{
		if(*str == 0) return i;
		str++;
	}
	return size;
}

bool ValueCmp(const char* str, const char* cmp_str)
{
	for(uint16_t i = 0; i < 1000; i++)
	{
		if(*cmp_str == '\0')
		{
			if(i == 0)
			{
				/* Exception for empty cmp_str */
				if(*str != '\0')
				{
					/* String is not empty */
					return false;
				}
			}
			return true;
		}
		if(*str == *cmp_str)
		{
			str++;
			cmp_str++;
		}
		else return false;
	}
	return false;
}

void GetValue(const char* srcStr, char* dstStr, uint16_t size)
{
	for(uint16_t i = 0; i < size; i++)
	{
		if((*srcStr == 0) || (*srcStr == '&') || (*srcStr == '/') ||
		   (*srcStr == '?') || (*srcStr == '+'))
		{
			// Add terminal symbol
			*dstStr = 0;
			return;
		}
		*dstStr = *srcStr;
		srcStr++;
		dstStr++;
	}
}

/* Return end of destination string */
char* SetValue(const char* srcStr, char* dstStr, uint16_t size)
{
	for(uint16_t i = 0; i < size; i++)
	{
		*dstStr = *srcStr;
		if(*srcStr == 0)
		{
			/* Copied end of source string: return end of destination string */
			return dstStr;
		}
		srcStr++;
		dstStr++;
	}

	/* Reached end of number copied characters,
	   return pointer for last copied character for destination string */
	return dstStr;
}

bool GetNumFromStr(const char** str, int32_t* num, bool trim)
{
	/* Exceptions for the first element of string */
	if(**str == 0)
	{
		/* End of the string */
		return false;
	}

	/* Create variables */
	int32_t res = 0;
	bool formatIsValid = false;

	/* Store current string pointer */
	const char* pStr = *str;

	/* Skip all white spaces and separators at the start of the string */
	for(;;)
	{
		if((*pStr == ' ') || (*pStr == '.') || (*pStr == '+'))
		{
			pStr++;
		}
		else if(*pStr == '%')
		{
			/* Correct the pointer for any coded symbol */
			pStr += 3;
		}
		else
		{
			/* Skip all another symbols */
			break;
		}
	}

	/* Check for the minus symbol */
	bool negative = false;
	if(*pStr == '-')
	{
		negative = true;
		pStr++;
	}

	while(*pStr != '\0')
	{
		/* Watch for digit */
		if((*pStr >= '0') && (*pStr <= '9'))
		{
			/* At least, one digit has been found */
			formatIsValid = true;

			/* Shift the previous digits to the left */
			res *= 10;
			/* Add the new one */
			res += *pStr - '0';

			/* Go to next symbol */
			pStr++;
		}
		else
		{
			/* Another symbols are forbidden. Stop the parsing of the string */
			break;
		}
	}

	if(trim)
	{
		/* Correct the string pointer */
		*str = pStr;
	}

	if(formatIsValid == false)
	{
		/* There is no valid number */
		return false;
	}

	/* Check for negative value */
	if(negative == false) *num = res;
	else *num = -(res);

	return true;
}

/* Return end of destination string */
char* SetNumToStr(int32_t num, char* str, uint16_t size)
{
	/* Validate input parameters */
	if(size == 0) return str;

	/* Check for negative values */
	if(num < 0)
	{
		*str = '-';
		str++;
		size--;
		num = -(num);
	}

	int32_t divider = 1000000000;
	bool first = false;
	do
	{
		if(size == 0) return str;
		if(num >= divider)
		{
			first = true;
			*str = (char)((num/divider) + '0');
			str++;
			size--;
			num = num%divider;
		}
		else if((first) || (divider < 10))
		{
			*str = '0';
			str++;
			size--;
		}
		divider = divider/10;
	}
	while(divider);
	
	if(size == 0) return str;
	*str = 0;
	return str;
}

bool GetFloatFromStr(const char** str, float* fNum, bool trim)
{
	/* Exceptions for the first element of string */
	if(**str == 0)
	{
		/* End of the string */
		return false;
	}

	/* Create variables */
	float res = 0.0F;
	bool afterDecimalPoint = false;
	bool formatIsValid = false;

	/* Divider to place digits after the decimal point */
	float div = 1;

	/* Store current string pointer */
	const char* pStr = *str;

	/* Skip all white spaces and separators except point or comma at the start
	of the string */
	for(;;)
	{
		if((*pStr == ' ') || (*pStr == '+'))
		{
			pStr++;
		}
		else if(*pStr == '%')
		{
			/* Correct the pointer for any coded symbol */
			pStr += 3;
		}
		else
		{
			/* Skip all another symbols */
			break;
		}
	}

	/* Check for the minus symbol */
	bool negative = false;
	if(*pStr == '-')
	{
		negative = true;
		pStr++;
	}

	while(*pStr != '\0')
	{
		/* Watch for digit */
		if((*pStr >= '0') && (*pStr <= '9'))
		{
			/* At least, one digit has been found */
			formatIsValid = true;

			/* Integer part */
			if(!afterDecimalPoint)
			{
				/* Shift the previous digits to the left */
				res *= 10;
				/* Add the new one */
				res += *pStr - '0';
			}
			/* Decimal part */
			else
			{
				div *= 10;
				res += (float)(*pStr - '0') / div;
			}

			/* Go to next symbol */
			pStr++;
		}
		/* Watch for uncoded digital separators */
		else if((*pStr == '.') || (*pStr == ','))
		{
			if(afterDecimalPoint)
			{
				/* Separator already has found */
				break;
			}
			afterDecimalPoint = true;

			/* Go to next symbol */
			pStr++;
		}
		/* Watch for any coded symbols */
		else if(*pStr == '%')
		{
			if(afterDecimalPoint)
			{
				/* Separator already has found */
				break;
			}
			if(*(pStr + 1) == '2')
			{
				/* ,. */
				if(	(*(pStr + 2) == 'C') || (*(pStr + 2) == 'c') ||
					(*(pStr + 2) == 'E') || (*(pStr + 2) == 'e'))
				{
					afterDecimalPoint = true;
				}
			}

			/* Correct pointer for any coded symbol */
			pStr += 3;

			if(afterDecimalPoint)
			{
				/* Keep searching for digits */
			}
			else
			{
				/* Another coded symbols are forbidden.
				Stop the parsing of the string */
				break;
			}
		}
		else
		{
			/* Another symbols are forbidden. Stop the parsing of the string */
			break;
		}
	}

	if(trim)
	{
		/* Correct the string pointer */
		*str = pStr;
	}

	if(formatIsValid == false)
	{
		/* There is no valid number */
		return false;
	}

	/* Check for negative value */
	if(negative == false) *fNum = res;
	else *fNum = -(res);

	return true;
}

/* Return end of destination string */
char* SetFloatToStr(float num,
		uint16_t numDigitsBeforeP, uint16_t numDigitsAfterP,
		char* str, uint16_t size)
{
	/* Validate input parameters */
	if(size == 0) return str;
	if(numDigitsBeforeP + numDigitsAfterP == 0) return str;
	if(numDigitsBeforeP + numDigitsAfterP > 10) return str;

	/* Check for negative values */
	if(num < 0)
	{
		*str = '-';
		str++;
		size--;
		num = -(num);
	}

	/* Get unsigned integer number with rounding */
	uint32_t uintNumber = (uint32_t)(num * PowBase10(numDigitsAfterP + 1));
	if((uintNumber % 10) >= 5) uintNumber = (uintNumber/10) + 1;
	else uintNumber /= 10;

	/* Get divider */
	uint32_t divider = PowBase10(numDigitsBeforeP + numDigitsAfterP - 1);

	/* Trim uintNumber according to total number of digits */
	uintNumber = uintNumber%(divider * 10);

	bool first = false;
	bool point = false;
	do
	{
		if(size == 0) return str;

		/* Point divider service */
		if(numDigitsBeforeP == 0)
		{
			if(first == false)
			{
				if(size == 0) return str;
				first = true;

				/* Show at least one zero before separator */
				*str = ('0');
				str++;
				size--;
			}
			if(point == false)
			{
				if(size == 0) return str;
				point = true;

				/* Store point and do not count the size */
				*str = ('.');
				str++;
				size--;
			}
		}
		else numDigitsBeforeP--;

		if(size == 0) return str;
		else if(uintNumber >= divider)
		{
			first = true;
			*str = (char)((uintNumber/divider) + '0');
			str++;
			size--;
			uintNumber = uintNumber%divider;
		}
		else if((first) || (divider < 10))
		{
			*str = '0';
			str++;
			size--;
		}
		divider = divider/10;
	}
	while(divider);

	/* All digits has been stored */
	if(size == 0) return str;
	*str = 0;
	return str;
}

bool GetHexFromStr(const char** str, int32_t* num, bool trim)
{
	/* Exceptions for the first element of string */
	if(**str == 0)
	{
		/* End of the string */
		return false;
	}

	/* Create variables */
	int32_t res = 0;
	bool formatIsValid = false;

	/* Store current string pointer */
	const char* pStr = *str;

	/* Skip all white spaces and separators at the start of the string */
	for(;;)
	{
		if((*pStr == ' ') || (*pStr == '.') || (*pStr == '+'))
		{
			pStr++;
		}
		else if(*pStr == '%')
		{
			/* Correct the pointer for any coded symbol */
			pStr += 3;
		}
		else
		{
			/* Skip all another symbols */
			break;
		}
	}

	while(*pStr != '\0')
	{
		/* Watch for digit */
		if((*pStr >= '0') && (*pStr <= '9'))
		{
			/* At least, one digit has been found */
			formatIsValid = true;

			/* Shift the previous digits to the left */
			res <<= 4;
			/* Add the new one */
			res += *pStr - '0';

			/* Go to next symbol */
			pStr++;
		}
		/* Watch for the letters */
		else if((*pStr >= 'A') && (*pStr <= 'F'))
		{
			/* At least, one digit has been found */
			formatIsValid = true;

			/* Shift the previous digits to the left */
			res <<= 4;
			/* Add the new one */
			res += *pStr - 'A' + 0x0A;

			/* Go to next symbol */
			pStr++;
		}
		else if((*pStr >= 'a') && (*pStr <= 'f'))
		{
			/* At least, one digit has been found */
			formatIsValid = true;

			/* Shift the previous digits to the left */
			res <<= 4;
			/* Add the new one */
			res += *pStr - 'a' + 0x0A;

			/* Go to next symbol */
			pStr++;
		}
		else
		{
			/* Another symbols are forbidden. Stop the parsing of the string */
			break;
		}
	}

	if(trim)
	{
		/* Correct the string pointer */
		*str = pStr;
	}

	if(formatIsValid == false)
	{
		/* There is no valid number */
		return false;
	}

	/* Store value */
	*num = res;

	return true;
}
/* Return end of destination string */
char* SetHexToStr(uint32_t hex, char* str, uint16_t size, bool alignToLeft)
{
	if(size < 2) return str;
	if(size >= 9) size = 9;
	
	// Catch zero converting
	if(hex == 0)
	{
		if(alignToLeft)
		{
			*str = '0';
			str++;
			*str = 0;
			return str;
		}
		for(; size > 1; )
		{
			*str = '0';
			str++;
			size--;
		}
		*str = 0;
		return str;
	}

	bool firstNoZero = false;
	for(uint8_t i = 0; (i < 8) && (size > 1); i++)
	{
		uint8_t tmpChar = (uint8_t)((hex & 0xF0000000) >> 28);
		hex = (hex << 4);

		if(tmpChar) firstNoZero = true;
		if((firstNoZero == false) && (alignToLeft == false))
		{
			if((8 - i) <= (size - 1)) firstNoZero = true;
		}
		if(firstNoZero)
		{
			if(tmpChar < 10) tmpChar += '0';
			else tmpChar += 'A' - 10;

			*str = tmpChar;
			str++;
			size--;
		}
	}

	/* All digits has been stored */
	*str = 0;
	return str;
}

/* Return end of destination string */
char* SetDateTimeToStr(struct tm* dateTime, char* str, uint16_t size)
{
	if(size == 0) return str;
	if(size >= 6)
	{
		*(str++) = dateTime->tm_hour/10 + '0';
		*(str++) = dateTime->tm_hour%10 + '0';
		*(str++) = ':';
		*(str++) = dateTime->tm_min/10 + '0';
		*(str++) = dateTime->tm_min%10 + '0';
	}
	if(size >= 15)
	{
		*(str++) = ':';
		*(str++) = dateTime->tm_sec/10 + '0';
		*(str++) = dateTime->tm_sec%10 + '0';
		*(str++) = ' ';
		*(str++) = dateTime->tm_mday/10 + '0';
		*(str++) = dateTime->tm_mday%10 + '0';
		*(str++) = '.';
		*(str++) = (dateTime->tm_mon + 1)/10 + '0';
		*(str++) = (dateTime->tm_mon + 1)%10 + '0';
	}
	if(size >= 23)
	{
		*(str++) = '.';
		*(str++) = (YEAR0 + dateTime->tm_year)/1000 + '0';
		*(str++) = ((YEAR0 + dateTime->tm_year)%1000)/100 + '0';
		*(str++) = ((YEAR0 + dateTime->tm_year)%100)/10 + '0';
		*(str++) = (YEAR0 + dateTime->tm_year)%10 + '0';
	}
	*(str++) = 0;
	return str;
}

/* Return end of destination string */
char* SetTimeToStr(struct tm* dateTime, char* str, uint16_t size)
{
	if(size == 0) return str;
	if(size >= 6)
	{
		*(str++) = dateTime->tm_hour/10 + '0';
		*(str++) = dateTime->tm_hour%10 + '0';
		*(str++) = ':';
		*(str++) = dateTime->tm_min/10 + '0';
		*(str++) = dateTime->tm_min%10 + '0';
	}
	if(size >= 8)
	{
		*(str++) = ':';
		*(str++) = dateTime->tm_sec/10 + '0';
		*(str++) = dateTime->tm_sec%10 + '0';
	}
	*(str++) = 0;
	return str;
}

/* Return end of destination string */
char* SetDateToStr(struct tm* dateTime, char* str, uint16_t size)
{
	if(size == 0) return str;
	if(size >= 6)
	{
		*(str++) = dateTime->tm_mday/10 + '0';
		*(str++) = dateTime->tm_mday%10 + '0';
		*(str++) = '.';
		*(str++) = (dateTime->tm_mon + 1)/10 + '0';
		*(str++) = (dateTime->tm_mon + 1)%10 + '0';
	}
	if(size >= 11)
	{
		*(str++) = '.';
		*(str++) = (YEAR0 + dateTime->tm_year)/1000 + '0';
		*(str++) = ((YEAR0 + dateTime->tm_year)%1000)/100 + '0';
		*(str++) = ((YEAR0 + dateTime->tm_year)%100)/10 + '0';
		*(str++) = (YEAR0 + dateTime->tm_year)%10 + '0';
	}
	*(str++) = 0;
	return str;
}

/* Return end of destination string */
char* SetTimeIntervalToStr(uint32_t seconds, char* str, uint16_t size)
{
	if(size == 0) return str;
	
	uint32_t days = seconds/(60*60*24);
	seconds -= days * (60*60*24);
	uint32_t hours = seconds/(60*60);
	seconds -= hours * (60*60);
	uint32_t minutes = seconds/60;
	seconds -= minutes * 60;
	if(size >= 16)
	{
		*(str++) = days/1000 + '0';
		*(str++) = (days%1000)/100 + '0';
		*(str++) = (days%100)/10 + '0';
		*(str++) = days%10 + '0';
		*(str++) = 'd';
		*(str++) = ' ';
	}
	if(size >= 10)
	{
		*(str++) = hours/10 + '0';
		*(str++) = hours%10 + '0';
		*(str++) = ':';
		*(str++) = minutes/10 + '0';
		*(str++) = minutes%10 + '0';
		*(str++) = ':';
		*(str++) = seconds/10 + '0';
		*(str++) = seconds%10 + '0';
	}
	*(str++) = 0;
	return str;
}

bool GetValueFromTxtField(const char** str,
		char* dstStr, uint16_t size, bool trim)
{
	/* Exceptions for the first element of string */
	if(**str == 0)
	{
		/* End of the string */
		return false;
	}

	/* Store current string pointer */
	const char* pStr = *str;

	while(size)
	{
		/* Watch for the digits */
		if((*pStr >= '0') && (*pStr <= '9'))
		{
			*dstStr = *pStr;
			/* Go to the next symbol */
			pStr++;
			dstStr++;
			size--;
		}
		/* Watch for the letters */
		else if((*pStr >= 'A') && (*pStr <= 'Z'))
		{
			*dstStr = *pStr;
			/* Go to the next symbol */
			pStr++;
			dstStr++;
			size--;
		}
		else if((*pStr >= 'a') && (*pStr <= 'z'))
		{
			*dstStr = *pStr;
			/* Go to the next symbol */
			pStr++;
			dstStr++;
			size--;
		}
		/* Watch for symbols */
		else if(*pStr == '+')
		{
			*dstStr = ' ';
			/* Go to the next symbol */
			pStr++;
			dstStr++;
			size--;
		}
		else if((*pStr == '.') || (*pStr == '_') || 	(*pStr == '-') ||
				(*pStr == '*'))
		{
			*dstStr = *pStr;
			/* Go to the next symbol */
			pStr++;
			dstStr++;
			size--;
		}
		/* Watch for coded symbols */
		else if(*pStr == '%')
		{
			if(*(pStr + 1) == '2')
			{
				/* space!"#$%&'()*+,-./ */
				if(*(pStr + 2) == '0') *dstStr = ' ';
				else if(*(pStr + 2) == '1') *dstStr = '!';
				else if(*(pStr + 2) == '2') *dstStr = '"';
				else if(*(pStr + 2) == '3') *dstStr = '#';
				else if(*(pStr + 2) == '4') *dstStr = '$';
				else if(*(pStr + 2) == '5') *dstStr = '%';
				else if(*(pStr + 2) == '6') *dstStr = '&';
				else if(*(pStr + 2) == '7') *dstStr = '\'';
				else if(*(pStr + 2) == '8') *dstStr = '(';
				else if(*(pStr + 2) == '9') *dstStr = ')';
				else if((*(pStr + 2) == 'A') || (*(pStr + 2) == 'a'))
					*dstStr = '*';
				else if((*(pStr + 2) == 'B') || (*(pStr + 2) == 'b'))
					*dstStr = '+';
				else if((*(pStr + 2) == 'C') || (*(pStr + 2) == 'c'))
					*dstStr = ',';
				else if((*(pStr + 2) == 'D') || (*(pStr + 2) == 'd'))
					*dstStr = '-';
				else if((*(pStr + 2) == 'E') || (*(pStr + 2) == 'e'))
					*dstStr = '.';
				else if((*(pStr + 2) == 'F') || (*(pStr + 2) == 'f'))
					*dstStr = '/';
				else
				{
					/* Wrong or unknown coded symbol */
					break;
				}
			}
			else if(*(pStr + 1) == '3')
			{
				/* :;<=>? */
				if((*(pStr + 2) == 'A') || (*(pStr + 2) == 'a'))
					*dstStr = ':';
				else if((*(pStr + 2) == 'B') || (*(pStr + 2) == 'b'))
					*dstStr = ';';
				else if((*(pStr + 2) == 'C') || (*(pStr + 2) == 'c'))
					*dstStr = '<';
				else if((*(pStr + 2) == 'D') || (*(pStr + 2) == 'd'))
					*dstStr = '=';
				else if((*(pStr + 2) == 'E') || (*(pStr + 2) == 'e'))
					*dstStr = '>';
				else if((*(pStr + 2) == 'F') || (*(pStr + 2) == 'f'))
					*dstStr = '?';
				else
				{
					/* Wrong or unknown coded symbol */
					break;
				}
			}
			else if(*(pStr + 1) == '4')
			{
				/* @ */
				if(*(pStr + 2) == '0') *dstStr = '@';
				else
				{
					/* Wrong or unknown coded symbol */
					break;
				}
			}
			else if(*(pStr + 1) == '5')
			{
				/* [\]^_ */
				if((*(pStr + 2) == 'B') || (*(pStr + 2) == 'b'))
					*dstStr = '[';
				else if((*(pStr + 2) == 'C') || (*(pStr + 2) == 'c'))
					*dstStr = '\\';
				else if((*(pStr + 2) == 'D') || (*(pStr + 2) == 'd'))
					*dstStr = ']';
				else if((*(pStr + 2) == 'E') || (*(pStr + 2) == 'e'))
					*dstStr = '^';
				else if((*(pStr + 2) == 'F') || (*(pStr + 2) == 'f'))
					*dstStr = '_';
				else
				{
					/* Wrong or unknown coded symbol */
					break;
				}
			}
			else if(*(pStr + 1) == '6')
			{
				/* ` */
				if(*(pStr + 2) == '0') *dstStr = '`';
				else
				{
					/* Wrong or unknown coded symbol */
					break;
				}
			}
			else if(*(pStr + 1) == '7')
			{
				/* {|}~ */
				if((*(pStr + 2) == 'B') || (*(pStr + 2) == 'b'))
					*dstStr = '{';
				else if((*(pStr + 2) == 'C') || (*(pStr + 2) == 'c'))
					*dstStr = '|';
				else if((*(pStr + 2) == 'D') || (*(pStr + 2) == 'd'))
					*dstStr = '}';
				else if((*(pStr + 2) == 'E') || (*(pStr + 2) == 'e'))
					*dstStr = '~';
				else
				{
					/* Wrong or unknown coded symbol */
					break;
				}
			}
			else
			{
				/* Wrong or unknown coded symbol */
				break;
			}

			/* Correct pointer for any coded symbol */
			pStr += 3;
			dstStr++;
			size--;
		}
		else
		{
			/* Another symbols are forbidden. Stop the parsing of the string */
			break;
		}
	}

	if(trim)
	{
		/* Correct the string pointer */
		*str = pStr;
	}
	/* Store terminal symbol */
	if(size) *dstStr = 0;
	return true;
}

bool GetNumFromTxtField(const char** str, int32_t* num, bool trim)
{
	// Exceptions for the first element of string
	if(**str == 0)
	{
		// End of the string
		return false;
	}

	// Store current string pointer
	const char* pStr = *str;

	// Skip all separators at the start of the string
	for(;;)
	{
		if((*pStr == '.') || (*pStr == '+'))
		{
			pStr++;
		}
		else if(*pStr == '%')
		{
			// Correct the pointer for any coded symbol
			pStr += 3;
		}
		else
		{
			// All kinds of separators are skipped
			break;
		}
	}

	// Check for the minus symbol
	bool sign = false;
	if(*pStr == '-')
	{
		sign = true;
		pStr++;
	}

	// Reset the number
	*num = 0;
	uint8_t digCounter = 0;
	for(;;)
	{
		// Watch for the digit
		if((*pStr >= '0') && (*pStr <= '9'))
		{
			// Watch for overflowing (2^32 = 4294967296 - 10 dig max)
			if(digCounter < 10)
			{
				// Store the digit with shifting the result to left
				*num *= 10;
				*num += (uint8_t)(*pStr) - '0';

				// Inc the digit counter
				digCounter++;
			}

			// Go to the next symbol
			pStr++;
		}
		// Watch for uncoded separators
		else if(*pStr == '+')
		{
			// Correct the pointer
			pStr++;

			if(digCounter == 0)
			{
				// There is no valid number, skip the separator
			}
			else
			{
				// There is valid number, stop the parsing of the string
				break;
			}
		}
		// Watch for coded separators
		else if(*pStr == '%')
		{
			// Create flag for the separator
			bool separ = false;
			if((*(pStr + 1) == '2') && (*(pStr + 2) == 'C'))
			{
				// It is comma. Set the separator flag
				separ = true;
			}
			else if((*(pStr + 1) == '3') && (*(pStr + 2) == 'A'))
			{
				// It is colon. Set the separator flag
				separ = true;
			}
			else if((*(pStr + 1) == '3') && (*(pStr + 2) == 'B'))
			{
				// It is semicolon. Set the separator flag
				separ = true;
			}

			// Correct pointer for any coded symbol
			pStr += 3;

			if(separ)
			{
				if(digCounter == 0)
				{
					// There is no valid number, skip the separator
				}
				else
				{
					// There is valid number, stop the parsing of the string
					break;
				}
			}
		}
		else
		{
			// Another symbols are forbidden. Stop the parsing of the string
			break;
		}
	}

	if(trim)
	{
		// Correct the string pointer
		*str = pStr;
	}

	if(digCounter == 0)
	{
		// There is no valid number
		return false;
	}

	// Check for minus flag
	if(sign) *num = -(*num);

	return true;
}

void RemoveForbiddenSymbols(char* str, char* forbStr)
{
	/* Exceptions for the first element of string */
	if(*str == 0)
	{
		/* End of the string */
		return;
	}
	if(*forbStr == 0)
	{
		/* Empty the string of forbidden symbols */
		return;
	}

	char* storedForbStr = forbStr;
	char* storedStr = str;
	bool shifted;
	for(;;)
	{
		if(*str == 0)
		{
			/* End of the string */
			break;
		}
		forbStr = storedForbStr;
		shifted = false;
		while(*forbStr)
		{
			if(*str == *forbStr)
			{
				/* Store current pointer */
				storedStr = str;
				while(*storedStr)
				{
					/* Shift forbidden symbol */
					*storedStr = *(storedStr + 1);
					storedStr++;
				}
				shifted = true;
				break;
			}
			forbStr++;
		}
		if(shifted == false) str++;
	}

	/* Store terminal symbol */
	*str = 0;
}

bool QueryCmp(const char** str, const char* cmp_str)
{
	for(uint16_t i = 0; i < 1000; i++)
	{
		if(*cmp_str == 0)
		{	
			if((*(*str + i) == 0) || (*(*str + i) == '?') ||
			  /*(*(*str + i) == ';') || */(*(*str + i) == ' '))
			{
				*str += i;
				return true;
			}
			return false;
		}
		if(*(*str + i) == *cmp_str) cmp_str++;
		else return false;
	}
	return false;
}

bool ParamIsEqu(const char** str, const char* cmp_str)
{
	for(uint16_t i = 0; i < 1000; i++)
	{
		if(*cmp_str == 0)
		{	
			if(*(*str + i) == '=') 
			{
				*str += i + 1;
				return true;
			}
			return false;
		}
		if(*(*str + i) == *cmp_str) cmp_str++;
		else return false;
	}
	return false;
}

bool SearchForStartParameter(const char** str)
{
	for(uint16_t i = 0; i < 1000; i++)
	{
		if((**str == 0) ||(**str == ' ')) return false;
		if(**str == '?') 
		{
			(*str)++;
			return true;
		}
		(*str)++;
	}
	return false;
}

bool SearchForNextParameter(const char** str)
{
	for(uint16_t i = 0; i < 1000; i++)
	{
		if((**str == 0) ||(**str == ' ')) return false;
		if(**str == '&') 
		{
			(*str)++;
			return true;
		}
		(*str)++;
	}
	return false;
}

uint32_t PowBase10(uint32_t power)
{
	uint32_t result = 1;
	while(power)
	{
		result = result * 10;
		power--;
	}
	return result;
}

/* Private functions -------------------------------------------------------- */

