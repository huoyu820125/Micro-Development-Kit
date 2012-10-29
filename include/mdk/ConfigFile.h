// ConfigFile.h: interface for the ConfigFile class.
//
//////////////////////////////////////////////////////////////////////
/*
	可识别的文件格式：
	注释行以#或者//开头
	配置一行一条
	配置格式为key=value
	可以存在任意个空格制表符
	key与value字符串内容中包含的空格与制表符被保留，但两端的空格与制表符会被丢弃
	例如:"  ip list = \t 192.168.0.1 \t 192.168.0.2 \t "
	读出来结果就是
		key ="ip list"
		value="192.168.0.1 \t 192.168.0.2"

	使用范例
	ConfigFile cfg( "./test.cfg" );
	string ip = cfg["ip"];
	cfg["ip"] = "127.0.0.1";
	cfg["port"] = 8080;//可以直接使用char*、string、int、float等进行赋值
	cfg.save();
*/
#ifndef MDK_CONFIGFILE_H
#define MDK_CONFIGFILE_H

#include "FixLengthInt.h"

#include <stdio.h>
#include <string>
#include <map>

namespace mdk
{

class CFGItem
{
	friend class ConfigFile;
public:
	operator std::string();
	CFGItem& operator = ( double value );
	CFGItem& operator = ( int value );
	CFGItem& operator = ( std::string value );
	virtual ~CFGItem();
private:
	CFGItem();
	std::string m_value;
	std::string m_description;
};
typedef std::map<std::string,CFGItem> ConfigMap;

class ConfigFile  
{
public:
	ConfigFile( const char *strName );
	virtual ~ConfigFile();
	
public:
	//读或写配置值
	CFGItem& operator []( std::string key );
	bool Save();
private:
	//打开文件
	bool Open( bool bRead );
	//关闭文件
	void Close();
	bool ReadFile();

private:
	std::string m_strName;
	FILE *m_pFile;
	ConfigMap m_content;
	
};

}//namespace mdk

#endif // MDK_CONFIGFILE_H
