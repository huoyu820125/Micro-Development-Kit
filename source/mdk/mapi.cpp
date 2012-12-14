// mapi.cpp: implementation of the mapi class.
//
//////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstring>
#include "../../include/mdk/mapi.h"
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

using namespace std;

namespace mdk
{

void m_sleep( long lMillSecond )
{
#ifndef WIN32
	usleep( lMillSecond * 1000 );
#else
	Sleep( lMillSecond );
#endif
}
	
bool addrToI64(uint64 &addr64, const char* ip, int port)
{
	unsigned char addr[8];
	int nIP[4];
	sscanf(ip, "%d.%d.%d.%d", &nIP[0], &nIP[1], &nIP[2], &nIP[3]);
	addr[0] = nIP[0];
	addr[1] = nIP[1];
	addr[2] = nIP[2];
	addr[3] = nIP[3];
	char checkIP[25];
	sprintf(checkIP, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	if ( 0 != strcmp(checkIP, ip) ) return false;
	memcpy( &addr[4], &port, 4);
	memcpy(&addr64, addr, 8);
	
	return true;
}

void i64ToAddr(char* ip, int &port, uint64 addr64)
{
	unsigned char addr[8];
	memcpy(addr, &addr64, 8);
	sprintf(ip, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	memcpy(&port, &addr[4], 4);
}

void TrimString( string &str, string del )
{
	int nPos = 0;
	unsigned int i = 0;
	for ( ; i < del.size(); i++ )
	{
		while ( true )
		{
			nPos = str.find( del.c_str()[i], 0 );
			if ( -1 == nPos ) break;
			str.replace( str.begin() + nPos, str.begin() + nPos+1, "" );
		}
	}
}

void TrimStringLeft( string &str, string del )
{
	unsigned int i = 0;
	bool bTrim = false;
	for ( ; i < str.size(); i++ )
	{
		if ( string::npos == del.find(str.c_str()[i]) ) break;
		bTrim = true;
	}
	if ( !bTrim ) return;
	str.replace( str.begin(), str.begin() + i, "" );
}

void TrimStringRight( string &str, string del )
{
	int i = str.size() - 1;
	bool bTrim = false;
	for ( ; i >= 0; i-- )
	{
		if ( string::npos == del.find(str.c_str()[i]) ) break;
		bTrim = true;
	}
	if ( !bTrim ) return;
	str.replace( str.begin() + i + 1, str.end(), "" );
}

//Ñ¹Ëõ¿Õ°××Ö·û
char* Trim( char *str )
{
	if ( NULL == str || '\0' == str[0] ) return str;
	int i = 0;
	char *src = str;
	char strTemp[256];
	for ( i = 0; '\0' != *src ; src++ )
	{
		if ( ' ' == *src || '\t' == *src ) continue;
		strTemp[i++] = *src;
	}
	strTemp[i++] = '\0';
	strcpy( str, strTemp );
	return str;
}

//Ñ¹Ëõ¿Õ°××Ö·û
char* TrimRight( char *str )
{
	if ( NULL == str || '\0' == str[0] ) return str;
	int i = 0;
	char *src = str;
	for ( i = 0; '\0' != *src ; src++ )
	{
		if ( ' ' == *src || '\t' == *src ) 
		{
			i++;
			continue;
		}
		i = 0;
	}
	str[strlen(str) - i] = 0;
	return str;
}

//Ñ¹Ëõ¿Õ°××Ö·û
char* TrimLeft( char *str )
{
	if ( NULL == str || '\0' == str[0] ) return str;
	char *src = str;
	for ( ; '\0' != *src ; src++ )
	{
		if ( ' ' != *src && '\t' != *src ) break;
	}
	char strTemp[256];
	strcpy( strTemp, src );
	strcpy( str, strTemp );
	return str;
}

int reversal(int i)
{
	int out = 0;
	out = i << 24;
	out += i >> 24;
	out += ((i >> 8) << 24) >> 8;
	out += ((i >> 16) << 24) >> 16;
	return out;
}

}
