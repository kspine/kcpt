/*
 * userloader.cpp
 *
 *  Created on: 2011-12-29
 *      Author: Administrator
 */

#include <stdio.h>
#include "userloader.h"
#include <comlog.h>
#include <tools.h>

#include <vector>
using std::vector;
#include <fstream>
using std::ifstream;

#include "../tools/utils.h"

// 转换在整型变量
static int my_atoi( const char *ptr ) {
	if ( ptr == NULL ) {
		return 0 ;
	}
	int len = strlen( ptr ) ;
	for ( int i = 0; i < len; ++ i ) {
		if ( ptr[i] >= '0' && ptr[i] <= '9' ) {
			continue ;
		}
		return 0 ;
	}
	return atoi( ptr ) ;
}

bool CUserLoader::Init( ISystemEnv *pEnv )
{
	_pEnv = pEnv;

	return true;
}

//
bool CUserLoader::loadSubscribe(const string &code, const string &path, Macid2Channel &macid2Channel)
{
	vector<string> simids;
	vector<string>::iterator itVs;
	string strVal;
	vector<string> fields;

	set<string> channels;
	pair<Macid2Channel::iterator, bool> ret;

	channels.clear();
	channels.insert(code);

	string name = "FORWARD.LIST." + code;
	_pEnv->GetRedisCache()->HKeys(name.c_str(), simids);
	for(itVs = simids.begin(); itVs != simids.end(); ++itVs) {
		string &simid = *itVs;

		strVal = "";
		_pEnv->GetRedisCache()->HGet(name.c_str(), simid.c_str(), strVal);
		if(strVal != "0") {
			continue; //0正常转发，其它暂停
		}

		strVal = "";
		_pEnv->GetRedisCache()->HGet("KCTX.SECURE", simid.c_str(), strVal);
		fields.clear();
		if(Utils::splitStr(strVal, fields, ',') != 10) {
			continue;
		}

		string macid = fields[6] + "_" + simid;
		ret = macid2Channel.insert(make_pair(macid, channels));
		if(ret.second == false) {
			ret.first->second.insert(code);
		}
	}

	return true;
}

// 加载文件数据
bool CUserLoader::LoadFile( const char *file, const char *path, CGroupUsers &users )
{
	if ( file == NULL )
		return false ;

	char buf[1024] = {0};
	FILE *fp = NULL;
	fp = fopen( file, "r" );
	if (fp == NULL) {
		OUT_ERROR( NULL, 0, NULL, "Load pcc user file %s failed", file ) ;
		return false;
	}

	CGroupUsers::iterator it ;

	list<string> channelNames;
	Macid2Channel macid2channel;

	int count = 0 ;
	while (fgets(buf, sizeof(buf), fp)) {
		unsigned int i = 0;
		while (i < sizeof(buf)) {
			if (!isspace(buf[i]))
				break;
			i++;
		}
		if (buf[i] == '#')
			continue;

		char temp[1024] = {0};
		for (int i = 0, j = 0; i < (int)strlen(buf); ++ i ) {
			if (buf[i] != ' ' && buf[i] != '\r' && buf[i] != '\n') {
				temp[j++] = buf[i];
			}
		}

		string line = temp;
		//1:10.1.99.115:8880:user_name:user_password:A3
		vector<string> vec_line ;
		if ( ! splitvector( line, vec_line, ":" , 0 ) ){
			continue ;
		}
		if ( vec_line.size() < 7 ) continue ;
		// PASCLIENT:110:192.168.5.45:9880:701115:701115:701115
		IUserNotify::_UserInfo  info ;
		info.tag  	 = vec_line[0] ;
		info.code 	 = vec_line[1] ;
		info.ip   	 = vec_line[2] ;
		info.port 	 = atoi( vec_line[3].c_str() ) ;
		info.user 	 = vec_line[4] ;
		info.pwd  	 = vec_line[5] ;
		info.type 	 = vec_line[6] ;

		// 处理接入码操作
		int accesscode = my_atoi( info.type.c_str() ) ;
		if ( accesscode > 0 ) {
			bool encrpty = false ;
			// 参数个数是否大于7个参数
			if ( vec_line.size() > 7 ) {
				// 解析密钥 M1_IA1_IC1
				vector<string> vec2 ;
				splitvector( vec_line[7], vec2, "_" , 0 ) ;
				if ( vec2.size() == 3 ) { // 主要针对同步程序的导致的数据错误
					if ( ! vec2[0].empty() && ! vec2[1].empty() && ! vec2[2].empty() ) {

						_userkey.AddKey( accesscode,
								atoi( vec2[0].c_str() ) ,
								atoi( vec2[1].c_str() ) ,
								atoi( vec2[2].c_str() ) ) ;

						encrpty = true ;
					}
				}
			}
			// 如果为不加密就直接去掉了
			if ( ! encrpty ) {
				// 如果去掉加密处理就清除密钥
				_userkey.DelKey( accesscode ) ;
			}

			loadSubscribe(info.code, path, macid2channel);
			channelNames.push_back(info.type + "_" + info.code);
		}

		string key = info.tag + info.code ;
		// 添加到用户队列中
		it = users.find( vec_line[0] ) ;
		if ( it != users.end() ) {
			CUserMap &mp = it->second ;
			CUserMap::iterator itx = mp.find( key ) ;
			if ( itx != mp.end() ) {
				itx->second = info ;
			} else {
				mp.insert( make_pair( key, info ) ) ;
			}
		} else {
			CUserMap mp ;
			mp.insert( make_pair( key, info )  ) ;
			users.insert( make_pair( info.tag, mp ) ) ;
		}

		++ count ;
	}
	fclose(fp);
	fp = NULL;

	if(macid2channel.empty() == false) {
		share::RWGuard guard(_rwMutex, true);

		list<string> diff;
		Macid2Channel::iterator itMssSrc;
		Macid2Channel::iterator itMssDst;

		diff.clear();
		for(itMssSrc = macid2channel.begin(); itMssSrc != macid2channel.end(); ++itMssSrc) {
			if((itMssDst = _macid2channel.find(itMssSrc->first)) != _macid2channel.end()) {
				continue;
			}

			diff.push_back(itMssSrc->first); //新增的终端
		}
		_pEnv->GetMsgClient()->updateSub(diff, 1);

		diff.clear();
		for(itMssSrc = _macid2channel.begin(); itMssSrc != _macid2channel.end(); ++itMssSrc) {
			if((itMssDst = macid2channel.find(itMssSrc->first)) != macid2channel.end()) {
				continue;
			}

			diff.push_back(itMssSrc->first); //移除的终端
		}
		_pEnv->GetMsgClient()->updateSub(diff, 0);

		_macid2channel = macid2channel; //更新转发规则
		_channelNames = channelNames;   //更新通道名称
	}

	// OUT_PRINT( NULL, 0, NULL, "load pcc user success %s, count %d" , file , count ) ;

	return true ;
}

// 添加组对象
bool CUserLoader::SetNotify( const char *tag, IUserNotify *notify )
{
	share::Guard guard( _mutex ) ;

	// 查找是否已经存在
	CGroupMap::iterator it = _groupuser.find( tag ) ;
	if ( it == _groupuser.end() ) {
		CUserMgr mgr ;
		mgr.SetNotify( notify ) ;
		// 处理添加用户操作
		_groupuser.insert( make_pair(tag, mgr) ) ;
	} else {
		it->second.SetNotify( notify ) ;
	}

	return true ;
}

// 加载用户数据
bool CUserLoader::LoadUser( const char *file, const char *path )
{
	CGroupUsers users ;
	if ( ! LoadFile( file, path, users) ) {
		return false ;
	}

	share::Guard guard( _mutex ) ;
	if ( users.empty() )
		return false ;

	// 处理所有用户处理
	CGroupUsers::iterator it ;
	CGroupMap::iterator  itx ;

	for ( it = users.begin(); it != users.end(); ++ it ) {
		CUserMap &user = it->second ;

		itx = _groupuser.find( it->first ) ;
		if ( itx != _groupuser.end() ) {
			itx->second.Add( user ) ;
		} else {
			CUserMgr mgr ;
			mgr.Add( user ) ;
			_groupuser.insert( make_pair(it->first, mgr ) ) ;
		}
	}

	return true ;
}

// 添加用户数据
void CUserLoader::CUserMgr::Add( CUserMap &users )
{
	if ( users.empty() )
		return ;

	int flag = 0 ;

	CUserMap::iterator it , itx ;
	// 处理添加和修改用户
	for ( it = users.begin(); it != users.end(); ++ it ) {
		itx = _users.find( it->first ) ;
		if ( itx != _users.end() ) {
			// 如果用户数据发生变化
			if ( IUserNotify::Compare( itx->second, it->second ) ) {
				continue ;
			}
			itx->second = it->second ;
			flag = USER_CHGED ;
		} else {
			_users.insert( make_pair( it->first, it->second ) ) ;
			flag = USER_ADDED ;
		}

		if( _notify ) {
			// 通知外部新增用户处理
			_notify->NotifyUser( it->second, flag ) ;
		}
	}

	// 处得删除用户
	for ( it = _users.begin(); it != _users.end(); ) {
		itx = users.find( it->first ) ;
		if ( itx != users.end() ) {
			++ it ;
			continue ;
		}
		if( _notify ) {
			// 通知外部删除用户处理
			_notify->NotifyUser( it->second, USER_DELED ) ;
		}
		_users.erase( it ++ ) ;
	}
}

// 设置通知回调队象
void CUserLoader::CUserMgr::SetNotify( IUserNotify *notify )
{
	// 更改回调对象要处理其中添加的用户
	_notify = notify ;

	if ( _users.empty() )
		return ;

	CUserMap::iterator it ;
	for ( it = _users.begin(); it != _users.end(); ++ it ) {
		// 通知外部新增用户处理
		_notify->NotifyUser( it->second, USER_ADDED ) ;
	}
}

bool CUserLoader::getChannels(const string &macid, set<string> &channels)
{
	share::RWGuard guard(_rwMutex, false);

	Macid2Channel::iterator it;
	if((it = _macid2channel.find(macid)) == _macid2channel.end()) {
		return false;
	}
	channels = it->second;

	return true;
}

bool CUserLoader::getSubscribe(list<string> &macids)
{
	share::RWGuard guard(_rwMutex, false);

	Macid2Channel::iterator it;
	for(it = _macid2channel.begin(); it != _macid2channel.end(); ++it) {
		macids.push_back(it->first); //订阅的手机号码
	}

	//809接入通道的接入码_编号用于MSG订阅关键字
	copy(_channelNames.begin(), _channelNames.end(), back_inserter(macids));

	return true;
}
