############################################################
## makefile of libunisql.a
############################################################
.SUFFIXES: .cpp

SOURCES  = 	../share/xml/xmlparser.cpp ../share/xml/httpcaller.cpp ../share/TProto/ProtoParse.cpp ../share/cache/msgcache.cpp   
SOURCES += pccserver.cpp pasclient.cpp pconvert.cpp msgclient.cpp  systemenv.cpp pccmain.cpp 
SOURCES += statinfo.cpp postquery.cpp whitelist.cpp userloader.cpp filecacheex.cpp servicecaller.cpp httpquery.cpp         
PROGRAM  = jt809v2_cli
CLIB     = 

##SPECIAL_MACRO_FLAGS = -D_PCC_DEBUG_VER
SPECIAL_CCFLAGS = 
SPECIAL_INCL_PATH = -I../include -I../rediscache -I../share/TProto -I../share/node -I../share/xml -I../share/cache
SPECIAL_LIB_PATH = -L../rediscache -L$(DBLIBS_HOME)/redis/lib

SPECIAL_LIBS = -lctfobase -lrediscache -lhiredis -lcurl
#SPECIAL_LIBS = -lutils -llog -lthshare -lsocketex -lnetbaseex -lhttpagent -ltinyxml 

include ${MTRANS_PRJ_HOME}/makefile.include
