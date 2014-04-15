COBJECT=fy_modules.o fy_framework.o fy_conf.o fy_logger.o fy_time.o fy_event.o fy_info.o fy_util.o modules/fy_fcgi_accept_module.o modules/fy_fcgi_layout_module.o modules/fy_fcgi_finish_module.o #modules/fy_fcgi_getinfo_module.o
CC=gcc -g
FLAG=-DTEST_FRAMEWORK
DEBUG=-DADS_DEBUG

IFCGI=-I /usr/local/fcgi/include
LFCGI=/usr/local/fcgi/lib/libfcgi.a
LJSON=/opt/download/jsoncpp-src-0.5.0/libs/linux-gcc-4.1.2/libjson_linux-gcc-4.1.2_libmt.a

IMODULE=-I ./modules

ITHRIFT=-I/usr/local/thrift/include/ -I/usr/local/thrift/include/thrift/
LTHRIFT=/usr/local/thrift/lib/libthrift.a
IBOOST=-I/usr/include/boost/

all: fly
$(COBJECT):%.o:%.c
	$(CC) -I. $(IFCGI) $(IMODULE) $(FLAG) $(DEBUG) -c $< -o $@
AdS:$(COBJECT)
	$(CC) -o fly ${COBJECT} $(LFCGI) $(LTHRIFT) ${LJSON} -lpthread -ldl -lmxml

clean:
	$(RM) *.o ./modules/*.o fly 
