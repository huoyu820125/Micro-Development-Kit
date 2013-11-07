// ConfigFile.h: interface for the ConfigFile class.
//
//////////////////////////////////////////////////////////////////////
/*
	可识别的文件格式：
	注释行以#或者//开头
	配置以section段划分
	格式为：［段名］配置内容［/段名］
	段名内可以存在任意个空格制表符，但两端的空白与制表符会被丢弃
	例如：[ ser config ]读出来就是"ser config"

	配置内容为一行一条
	配置格式为key=value
	可以存在任意个空格制表符
	key与value字符串内容中包含的空格与制表符被保留，但两端的空格与制表符会被丢弃
	例如:"  ip list = \t 192.168.0.1 \t 192.168.0.2 \t "
	读出来结果就是
		key ="ip list"
		value="192.168.0.1 \t 192.168.0.2"

	使用范例
	配置文件内容
	[ser config]
	ip = 192.168.0.1
	port = 8888
	[/ser config]

	ConfigFile cfg( "./test.cfg" );
	string ip = cfg["ser config"]["ip"];
	cfg["ser config"]["ip"] = "127.0.0.1";
	cfg["ser config"]["port"] = 8080;//可以直接使用char*、string、int、float等进行赋值
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
	friend class CFGSection;
public:
	bool IsNull();
	operator std::string();
	operator char();
	operator unsigned char();
	operator short();
	operator unsigned short();
	operator int();
	operator unsigned int();
	operator int64();
	operator uint64();
	operator float();
	operator double();
	CFGItem& operator = ( double value );
	CFGItem& operator = ( int value );
	CFGItem& operator = ( std::string value );
	//设置注释，不分windows linux，一律使用\n作为换行符
	void SetDescription( const char *description );
	virtual ~CFGItem();
private:
	CFGItem();
	std::string m_value;
	std::string m_description;
	int m_index;
	bool m_valid;
};
typedef std::map<std::string,CFGItem> ConfigMap;

class CFGSection
{
	friend class ConfigFile;
public:
	CFGSection(const char *name, int index);
	~CFGSection();
public:
	//读或写配置值
	CFGItem& operator []( std::string key );
	//设置注释，不分windows linux，一律使用\n作为换行符
	void SetDescription( const char *description );
	bool Save(FILE *pFile);

private:
	std::string m_name;
	ConfigMap m_content;
	std::string m_description;
	int m_index;

};

typedef std::map<std::string,CFGSection> CFGSectionMap;
class ConfigFile  
{
public:
	ConfigFile();
	ConfigFile( const char *strName );
	virtual ~ConfigFile();
	
public:
	//读或写配置值
	CFGSection& operator []( std::string name );
	//读取配置,只能成功读取1次,一旦读取成功,就不能再读取,再调用都将返回false
	bool ReadConfig( const char *fileName );
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
	CFGSectionMap m_sections;
};

}//namespace mdk

#endif // MDK_CONFIGFILE_H
