// mapi.h: interface for the mapi class.
//
//////////////////////////////////////////////////////////////////////

#ifndef MDK_MAPI_H
#define MDK_MAPI_H

#include <string>
#include "FixLengthInt.h"

namespace mdk
{
	//Ë¯Ãß
	void m_sleep( long lMillSecond );
	//µØÖ·±£´æµ½int64
	bool addrToI64(uint64 &addr64, const char* ip, int port);
	//int64½âÎö³öµØÖ·
	void i64ToAddr(char* ip, int &port, uint64 addr64);
	//Ñ¹Ëõdel°üº¬µÄ×Ö·û
	void TrimString( std::string &str, std::string del );
	//Ñ¹ËõÓÒdel°üº¬µÄ×Ö·û
	void TrimStringLeft( std::string &str, std::string del );
	//Ñ¹Ëõ×ódel°üº¬µÄ×Ö·û
	void TrimStringRight( std::string &str, std::string del );
	//Ñ¹Ëõ¿Õ°××Ö·û
	char* Trim( char *str );
	//Ñ¹ËõÓÒ¿Õ°××Ö·û
	char* TrimRight( char *str );
	//Ñ¹Ëõ×ó¿Õ°××Ö·û
	char* TrimLeft( char *str );
	//×Ö½Ú¸ßµÍÎ»Ë³Ğò·­×ª
	int reversal(int i);
}

#endif // !defined MDK_MAPI_H 
